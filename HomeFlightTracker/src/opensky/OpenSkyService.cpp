#include "OpenSkyService.h"

#include <WiFi.h>
#include <math.h>

#include "../diag/HeapReport.h"
#include "../net/TimeSync.h"
#include "Quota.h"

namespace {

// Hält Mutex während des Gültigkeitsbereichs.
class Lock {
 public:
  explicit Lock(SemaphoreHandle_t mutex) : mutex_(mutex) { xSemaphoreTake(mutex_, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(mutex_); }

 private:
  SemaphoreHandle_t mutex_;
};

}  // namespace

void OpenSkyService::begin(const AppConfig& config) {
  config_ = config;
  mutex_ = xSemaphoreCreateMutex();
  status_.authenticated = config_.hasOpenskyCredentials();
  if (status_.authenticated) client_.setCredentials(config_.openskyClientId, config_.openskyClientSecret);
  box_ = geo::boundingBox({config_.latitude, config_.longitude}, config_.rangeKm * opensky::kQueryRadiusFactor);
  Serial.printf("[opensky] Box %.4f..%.4f / %.4f..%.4f, %.1f Quadratgrad, %u Credit(s) je Abruf, %s\n", box_.latMin,
                box_.latMax, box_.lonMin, box_.lonMax, box_.areaSquareDegrees(),
                opensky::creditsPerRequest(box_.areaSquareDegrees()),
                status_.authenticated ? "mit Zugang" : "anonym");
  xTaskCreatePinnedToCore(taskEntry, "opensky", kTaskStackBytes, this, 1, nullptr, 0);
}

OpenSkyService::Status OpenSkyService::status() const {
  Lock lock(mutex_);
  Status copy = status_;
  const int32_t remainingMs = static_cast<int32_t>(nextFetchMs_ - millis());
  copy.nextFetchInS = remainingMs > 0 ? remainingMs / 1000 : 0;
  return copy;
}

bool OpenSkyService::copyFlights(FlightList& out, uint32_t& knownSequence) const {
  Lock lock(mutex_);
  if (status_.sequence == knownSequence) return false;
  out = published_;
  knownSequence = status_.sequence;
  return true;
}

void OpenSkyService::requestCredentialTest(const char* clientId, const char* clientSecret) {
  strlcpy(testClientId_, clientId, sizeof(testClientId_));
  strlcpy(testClientSecret_, clientSecret, sizeof(testClientSecret_));
  testMessage_ = "Test laeuft";
  testState_ = TestState::Running;
  testRequested_ = true;
}

void OpenSkyService::taskEntry(void* self) {
  static_cast<OpenSkyService*>(self)->run();
}

void OpenSkyService::run() {
  time_sync::begin();
  for (;;) {
    if (testRequested_) runCredentialTest();

    if (WiFi.status() != WL_CONNECTED) {
      setState(State::WaitingForWifi, "warte auf WLAN");
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    if (!time_sync::isSynced()) {
      setState(State::WaitingForTime, "warte auf Uhrzeit (NTP)");
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    if (static_cast<int32_t>(millis() - nextFetchMs_) < 0) {
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    applyPendingRange();
    const uint32_t startedMs = millis();
    OpenSkyClient::Result result;
    opensky::ParseStats stats;
    const bool ok = fetchOnce(result, stats);
    const uint32_t durationMs = millis() - startedMs;

    if (ok) {
      consecutiveFailures_ = 0;
      authRetried_ = false;
      // Takt ab Startzeitpunkt: exakt planbarer Credit-Verbrauch.
      nextFetchMs_ = startedMs + config_.intervalS * 1000UL;
      publish(stats, durationMs);
      continue;
    }

    if (result == OpenSkyClient::Result::AuthFailed && status_.authenticated && !authRetried_) {
      // Token abgelaufen oder widerrufen: genau einmal neu anmelden und sofort wiederholen.
      Serial.println("[opensky] 401 - Token wird erneuert");
      client_.invalidateToken();
      authRetried_ = true;
      continue;
    }
    scheduleAfterFailure(result, startedMs);
  }
}

bool OpenSkyService::fetchOnce(OpenSkyClient::Result& result, opensky::ParseStats& stats) {
  if (status_.authenticated && !client_.tokenValid()) {
    setState(State::Authenticating, "Anmeldung bei OpenSky");
    result = client_.fetchToken();
    Serial.printf("[opensky] Token: %s (HTTP %d)\n", OpenSkyClient::describe(result), client_.lastHttpCode());
    if (result != OpenSkyClient::Result::Ok) return false;
  }

  working_.clear();
  const geo::LatLon center{config_.latitude, config_.longitude};
  if (!box_.crossesAntimeridian) {
    result = client_.fetchStates(box_, center, config_.rangeKm, working_, stats);
  } else {
    // Box über die Datumsgrenze: zwei Teilabfragen, Ergebnisse zusammenführen.
    geo::BoundingBox west = box_;
    geo::BoundingBox east = box_;
    west.lonMax = 180.0;
    east.lonMin = -180.0;
    west.crossesAntimeridian = east.crossesAntimeridian = false;
    opensky::ParseStats eastStats;
    result = client_.fetchStates(west, center, config_.rangeKm, working_, stats);
    if (result == OpenSkyClient::Result::Ok) {
      result = client_.fetchStates(east, center, config_.rangeKm, working_, eastStats);
      stats.received += eastStats.received;
      stats.withPosition += eastStats.withPosition;
      stats.inRange += eastStats.inRange;
    }
  }
  if (result != OpenSkyClient::Result::Ok) {
    Serial.printf("[opensky] Abruf: %s (HTTP %d)\n", OpenSkyClient::describe(result), client_.lastHttpCode());
    return false;
  }
  working_.sortByDistance();
  return true;
}

void OpenSkyService::publish(const opensky::ParseStats& stats, uint32_t durationMs) {
  {
    Lock lock(mutex_);
    published_ = working_;
    status_.state = State::Running;
    strlcpy(status_.message, "ok", sizeof(status_.message));
    status_.sequence++;
    status_.lastSuccess = time(nullptr);
    status_.apiTime = stats.apiTime;
    status_.aircraftInRange = published_.size();
    status_.receivedInBox = stats.received;
    status_.remainingCredits = client_.remainingCredits();
    status_.lastDurationMs = durationMs;
  }
  Serial.printf("[opensky] %u im Radius (%u in der Box), Credits uebrig %ld, %lu ms\n", working_.size(),
                stats.received, static_cast<long>(client_.remainingCredits()), static_cast<unsigned long>(durationMs));
  logFlights();
  printHeapSnapshot(Serial, "nach OpenSky-Abruf");
}

void OpenSkyService::setState(State state, const char* message) {
  Lock lock(mutex_);
  if (status_.state == state && strcmp(status_.message, message) == 0) return;
  status_.state = state;
  strlcpy(status_.message, message, sizeof(status_.message));
}

void OpenSkyService::scheduleAfterFailure(OpenSkyClient::Result result, uint32_t startedMs) {
  ++consecutiveFailures_;
  uint32_t delayMs;
  State state;
  char message[40];
  switch (result) {
    case OpenSkyClient::Result::AuthFailed:
    case OpenSkyClient::Result::Forbidden:
      state = State::AuthError;
      delayMs = kAuthErrorRetryMs;
      snprintf(message, sizeof(message), "%s (HTTP %d)", OpenSkyClient::describe(result), client_.lastHttpCode());
      break;
    case OpenSkyClient::Result::RateLimited: {
      state = State::RateLimited;
      const uint32_t retryS = client_.retryAfterS() > 0 ? client_.retryAfterS() : 3600;
      delayMs = max<uint32_t>(retryS, config_.intervalS) * 1000UL;
      snprintf(message, sizeof(message), "Kontingent leer, Pause %lu min", static_cast<unsigned long>(retryS / 60));
      break;
    }
    default: {
      state = State::Error;
      // Exponentieller Backoff ab dem regulären Intervall, gedeckelt.
      const uint8_t exponent = consecutiveFailures_ > 5 ? 5 : consecutiveFailures_ - 1;
      delayMs = min<uint32_t>(config_.intervalS * 1000UL << exponent, kMaxBackoffMs);
      snprintf(message, sizeof(message), "%s (HTTP %d)", OpenSkyClient::describe(result), client_.lastHttpCode());
      break;
    }
  }
  nextFetchMs_ = startedMs + delayMs;
  authRetried_ = false;
  setState(state, message);
  {
    Lock lock(mutex_);
    status_.failures++;
  }
  Serial.printf("[opensky] %s - naechster Versuch in %lu s\n", message, static_cast<unsigned long>(delayMs / 1000));
}

void OpenSkyService::applyPendingRange() {
  const uint16_t rangeKm = pendingRangeKm_;
  if (rangeKm == 0) return;
  pendingRangeKm_ = 0;
  config_.rangeKm = rangeKm;
  box_ = geo::boundingBox({config_.latitude, config_.longitude}, config_.rangeKm * opensky::kQueryRadiusFactor);
  Serial.printf("[opensky] Reichweite %u km, %.1f Quadratgrad\n", rangeKm, box_.areaSquareDegrees());
}

void OpenSkyService::runCredentialTest() {
  testRequested_ = false;
  if (WiFi.status() != WL_CONNECTED || !time_sync::isSynced()) {
    testMessage_ = "Kein Internet oder Uhrzeit noch nicht synchron";
    testState_ = TestState::Failed;
    return;
  }
  OpenSkyClient testClient;
  testClient.setCredentials(testClientId_, testClientSecret_);
  memset(testClientSecret_, 0, sizeof(testClientSecret_));
  const OpenSkyClient::Result result = testClient.fetchToken();
  testClient.invalidateToken();
  testState_ = result == OpenSkyClient::Result::Ok ? TestState::Succeeded : TestState::Failed;
  testMessage_ = result == OpenSkyClient::Result::Ok ? "Anmeldung erfolgreich" : OpenSkyClient::describe(result);
  Serial.printf("[opensky] Zugangstest: %s (HTTP %d)\n", testMessage_, testClient.lastHttpCode());
}

void OpenSkyService::logFlights() const {
  const uint16_t count = working_.size() < 10 ? working_.size() : 10;
  for (uint16_t i = 0; i < count; ++i) {
    const Aircraft& a = working_[i];
    Serial.printf("  %-8s %s %6.1f km %5.0f m %4.0f km/h Kurs %3.0f\n", a.callsign[0] ? a.callsign : "-", a.icao24,
                  a.distanceKm, isnan(a.baroAltitudeM) ? -1.0f : a.baroAltitudeM,
                  isnan(a.velocityMs) ? -1.0f : a.velocityMs * 3.6f, isnan(a.trackDeg) ? -1.0f : a.trackDeg);
  }
}

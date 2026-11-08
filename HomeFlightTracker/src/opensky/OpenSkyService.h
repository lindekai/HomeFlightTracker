// Hintergrund-Task (Core 0) für den OpenSky-Abruf: Zeitplanung nach Intervall, Token-Erneuerung,
// Fehlerbehandlung mit Backoff und Rate-Limit-Auswertung. Ergebnisse werden als Snapshot
// veröffentlicht; die Oberfläche liest nur diesen Snapshot und löst nie selbst einen Abruf aus.
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <time.h>

#include "../config/AppConfig.h"
#include "../model/FlightList.h"
#include "OpenSkyClient.h"

class OpenSkyService {
 public:
  enum class State : uint8_t {
    Stopped,
    WaitingForWifi,
    WaitingForTime,
    Authenticating,
    Running,
    AuthError,
    RateLimited,
    Error,
  };

  struct Status {
    State state = State::Stopped;
    char message[40] = "";
    bool authenticated = false;  // mit OpenSky-Zugang (sonst anonym)
    uint32_t sequence = 0;       // steigt mit jedem erfolgreichen Abruf
    time_t lastSuccess = 0;      // Unix-Zeit des letzten erfolgreichen Abrufs
    int64_t apiTime = 0;         // Zeitstempel der Daten laut OpenSky
    uint16_t aircraftInRange = 0;
    uint16_t receivedInBox = 0;
    int32_t remainingCredits = -1;
    uint32_t nextFetchInS = 0;
    uint32_t lastDurationMs = 0;
  };

  enum class TestState : uint8_t { Idle, Running, Succeeded, Failed };

  void begin(const AppConfig& config);

  Status status() const;

  // Kopiert die aktuelle Flugliste, falls sie neuer als knownSequence ist. Liefert true bei Kopie.
  bool copyFlights(FlightList& out, uint32_t& knownSequence) const;

  // Neue Reichweite; wirkt ab dem nächsten regulären Abruf (löst selbst keinen Abruf aus).
  void setRangeKm(uint16_t rangeKm) { pendingRangeKm_ = rangeKm; }

  // Test von Zugangsdaten (Token-Abruf) aus der Weboberfläche; läuft im Task.
  void requestCredentialTest(const char* clientId, const char* clientSecret);
  TestState testState() const { return testState_; }
  const char* testMessage() const { return testMessage_; }

 private:
  static constexpr uint32_t kTaskStackBytes = 12288;
  static constexpr uint32_t kAuthErrorRetryMs = 10UL * 60UL * 1000UL;
  static constexpr uint32_t kMaxBackoffMs = 15UL * 60UL * 1000UL;

  static void taskEntry(void* self);
  void run();
  void runCredentialTest();
  bool fetchOnce(OpenSkyClient::Result& result, opensky::ParseStats& stats);
  void publish(const opensky::ParseStats& stats, uint32_t durationMs);
  void setState(State state, const char* message);
  void scheduleAfterFailure(OpenSkyClient::Result result, uint32_t startedMs);
  void logFlights() const;
  void applyPendingRange();

  AppConfig config_;
  OpenSkyClient client_;
  geo::BoundingBox box_{};
  FlightList working_;
  FlightList published_;
  Status status_;
  mutable SemaphoreHandle_t mutex_ = nullptr;

  uint32_t nextFetchMs_ = 0;
  uint8_t consecutiveFailures_ = 0;
  bool authRetried_ = false;

  volatile uint16_t pendingRangeKm_ = 0;
  volatile bool testRequested_ = false;
  volatile TestState testState_ = TestState::Idle;
  const char* testMessage_ = "";
  char testClientId_[96] = "";
  char testClientSecret_[128] = "";
};

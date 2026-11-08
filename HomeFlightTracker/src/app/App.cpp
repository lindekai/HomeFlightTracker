#include "App.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <math.h>

#include "../config/HardwareSettings.h"
#include "../diag/HeapReport.h"
#include "../net/TimeSync.h"
#include "../opensky/Quota.h"
#include "../ui/Widgets.h"
#include "BootRequest.h"
#include "Version.h"

namespace {

const char* const kRadarButtons[] = {"-", "Liste", "Menü", "+"};
const char* const kDetailButtons[] = {"< Radar", "Liste"};
const char* const kListButtons[] = {"< Radar", "Zurück", "Weiter"};
const char* const kMenuButtons[] = {"Radar", "Setup", "Kalibr.", "Diag."};
const char* const kConfirmButtons[] = {"Ja", "Nein"};
const char* const kSetupButtonsCancel[] = {"Abbrechen"};

constexpr int16_t kRadarTop = ui::kStatusBarHeight;
constexpr int16_t kFooterY = kRadarTop + RadarView::kSize + 1;
constexpr int16_t kListTop = 56;
constexpr int16_t kListRowHeight = 28;

const char* compassPoint(float bearingDeg) {
  static const char* const kPoints[] = {"N", "NO", "O", "SO", "S", "SW", "W", "NW"};
  return kPoints[static_cast<int>(lroundf(bearingDeg / 45.0f)) % 8];
}

const char* positionSourceName(uint8_t source) {
  switch (source) {
    case 0:
      return "ADS-B";
    case 1:
      return "ASTERIX";
    case 2:
      return "MLAT";
    case 3:
      return "FLARM";
    default:
      return "-";
  }
}

void formatAltitude(char* buffer, size_t size, float meters) {
  if (isnan(meters)) {
    snprintf(buffer, size, "-");
  } else {
    snprintf(buffer, size, "%.0f m", meters);
  }
}

}  // namespace

void App::begin(bool setupRequested) {
  tft_.init();
  touch_.begin();
  rotation_ = hardware_settings::loadRotation();
  tft_.setRotation(rotation_);

  // Nach dem Tiefschlaf ohne Startbildschirm: schnell wieder anzeigen; die weckende Berührung
  // darf weder als Recovery noch als Bedienung zählen.
  const bool fromSleep = PowerManager::wokeFromDeepSleep();
  const bool calibrate = !fromSleep && runBootRecovery();
  if (calibrate || !touch_.applyStoredCalibration(rotation_)) touch_.calibrate(rotation_);
  if (fromSleep) {
    Serial.printf("[power] aufgewacht (%s)\n", PowerManager::wokeByTouch() ? "Beruehrung" : "Timer");
    touch_.waitForRelease();
  }

  const bool configLoaded = app_config::load(config_);
  Serial.printf("[config] %s\n", !configLoaded          ? "keine Konfiguration gespeichert"
                                 : config_.isComplete() ? "vollstaendig"
                                                        : "unvollstaendig");
  if (setupRequested || !config_.isComplete()) {
    startSetupMode();
  } else {
    startNormalMode();
  }
  printHeapSnapshot(Serial, "nach App-Start");
}

void App::loop() {
  const uint32_t now = millis();
  wifi_.loop();
  portal_.loop();
  handleSerial();
  handleTouch(touch_.poll());

  if (portal_.restartRequested() && restartAtMs_ == 0) scheduleRestart("Gespeichert", "Neustart ...");
  if (restartAtMs_ != 0 && static_cast<int32_t>(now - restartAtMs_) >= 0) ESP.restart();

  if (mode_ == Mode::Setup) {
    if (now - setupStartedMs_ >= kSetupTimeoutMs) {
      // Ohne Abschluss endet der Setup-Modus; bei unvollständiger Konfiguration startet er mit neuem Passwort neu.
      Serial.println("[app] Setup-Zeit abgelaufen");
      ESP.restart();
    }
    if (now - lastRefreshMs_ >= kRefreshIntervalMs) {
      lastRefreshMs_ = now;
      if (screen_ == Screen::Setup) updateSetupScreen();
    }
    delay(2);
    return;
  }

  if (!mdnsStarted_ && wifi_.stationState() == WifiManager::StationState::Connected) {
    mdnsStarted_ = MDNS.begin(WifiManager::kHostname);
    if (mdnsStarted_) MDNS.addService("http", "tcp", 80);
    Serial.printf("[mdns] %s.local %s\n", WifiManager::kHostname, mdnsStarted_ ? "aktiv" : "FEHLER");
  }

  power_.loop(tft_);  // kehrt in der Nachtruhe nach der Wachphase nicht zurück (Tiefschlaf)

  // Neue Daten sofort übernehmen, Zeitangaben einmal pro Sekunde aktualisieren.
  if (updateFlights()) {
    lastRefreshMs_ = now;
    refresh();
  } else if (now - lastRefreshMs_ >= kRefreshIntervalMs) {
    lastRefreshMs_ = now;
    refresh();
  }
  delay(2);
}

// ---------------------------------------------------------------------------------------------
// Start und Betriebsarten

bool App::runBootRecovery() {
  const int16_t cx = tft_.width() / 2;
  const int16_t cy = tft_.height() / 2;
  tft_.fillScreen(ui::kColorBackground);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(ui::kColorAccent, ui::kColorBackground);
  tft_.drawString("HomeFlightTracker", cx, cy - 50, ui::fittingFont(tft_, "HomeFlightTracker"));
  tft_.setTextColor(ui::kColorText, ui::kColorBackground);
  tft_.drawString(String("Version ") + kFirmwareVersion, cx, cy - 22, 2);
  tft_.setTextColor(ui::kColorMuted, ui::kColorBackground);
  tft_.drawString(String("von ") + kFirmwareAuthor, cx, cy - 4, 2);
  tft_.setTextColor(ui::kColorStale, ui::kColorBackground);
  tft_.drawString("Antippen: Touch kalibrieren", cx, cy + 26, 2);
  tft_.drawString("8 s halten: Werksreset", cx, cy + 44, 2);

  const uint32_t start = millis();
  while (millis() - start < kRecoveryWindowMs) {
    if (!touch_.isRawTouched()) {
      delay(20);
      continue;
    }
    const uint32_t pressStart = millis();
    int32_t shownSeconds = -1;
    while (touch_.isRawTouched()) {
      const uint32_t held = millis() - pressStart;
      if (held >= kFactoryResetHoldMs) runFactoryReset();  // kehrt nicht zurück
      const int32_t remaining = (kFactoryResetHoldMs - held + 999) / 1000;
      if (remaining != shownSeconds && held > 1000) {
        shownSeconds = remaining;
        char line[40];
        snprintf(line, sizeof(line), "Werksreset in %ld s", static_cast<long>(remaining));
        tft_.setTextColor(ui::kColorWarning, ui::kColorBackground);
        tft_.setTextPadding(tft_.width());
        tft_.drawString(line, cx, cy + 80, 4);
        tft_.setTextColor(ui::kColorMuted, ui::kColorBackground);
        tft_.drawString("Loslassen = abbrechen", cx, cy + 108, 2);
        tft_.setTextPadding(0);
      }
      delay(20);
    }
    Serial.println("[touch] Neukalibrierung beim Start angefordert");
    return true;
  }
  return false;
}

void App::runFactoryReset() {
  Serial.println("[app] Werksreset");
  app_config::factoryReset();
  ui::drawMessage(tft_, "Werksreset", "Alle Einstellungen gelöscht", ui::kColorWarning);
  touch_.waitForRelease();
  delay(1500);
  ESP.restart();
}

void App::startSetupMode() {
  mode_ = Mode::Setup;
  wifi_.startAccessPoint();
  portal_.begin(SetupPortal::Mode::Setup, kFirmwareVersion);
  setupStartedMs_ = millis();
  showScreen(Screen::Setup);
}

void App::startNormalMode() {
  mode_ = Mode::Normal;
  // Sprite vor WLAN/TLS reservieren, solange der Heap noch unfragmentiert ist.
  radar_.begin(kRadarTop);
  wifi_.startStation(config_.wifiSsid, config_.wifiPassword);
  portal_.setOpenSkyService(&service_);
  portal_.begin(SetupPortal::Mode::Lan, kFirmwareVersion);
  service_.begin(config_);
  power_.begin(config_);
  showScreen(Screen::Radar);
}

// ---------------------------------------------------------------------------------------------
// Daten und Bildschirmaktualisierung

bool App::updateFlights() {
  if (!service_.copyFlights(flights_, flightsSequence_)) return false;
  const Aircraft* selected = findSelected();
  if (selected != nullptr) {
    detail_ = *selected;
    detailInData_ = true;
  } else if (selectedIcao24_[0] != '\0') {
    detailInData_ = false;
    if (screen_ == Screen::Radar) selectedIcao24_[0] = '\0';  // nicht mehr im Radius
  }
  if (screen_ == Screen::Details) drawDetails();
  if (screen_ == Screen::List) drawList();
  if (screen_ == Screen::Radar) drawRadar();
  return true;
}

RadarView::DataState App::dataState(const OpenSkyService::Status& status) const {
  if (status.lastSuccess == 0) return RadarView::DataState::Waiting;
  const time_t age = time(nullptr) - status.lastSuccess;
  const time_t staleAfter = max<time_t>(3 * config_.intervalS, 180);
  const time_t expireAfter = max<time_t>(10 * config_.intervalS, 900);
  if (age > expireAfter) return RadarView::DataState::Expired;
  if (age > staleAfter) return RadarView::DataState::Stale;
  return RadarView::DataState::Fresh;
}

void App::refresh() {
  switch (screen_) {
    case Screen::Radar: {
      const OpenSkyService::Status status = service_.status();
      const RadarView::DataState state = dataState(status);
      if (state != lastDataState_ || state == RadarView::DataState::Waiting) {
        drawRadar();  // Wechsel der Datenlage oder Wartetext aktualisieren
      } else {
        drawTopBar(status);
        drawRadarFooter(status);
      }
      break;
    }
    case Screen::Details:
      updateDetailsAge();
      break;
    case Screen::Menu:
      updateMenu();
      break;
    case Screen::Setup:
      updateSetupScreen();
      break;
    default:
      break;
  }
}

void App::showScreen(Screen screen) {
  screen_ = screen;
  switch (screen) {
    case Screen::Radar:
      tft_.fillScreen(ui::kColorBackground);
      ui::drawButtonBar(tft_, kRadarButtons, 4);
      drawRadar();
      break;
    case Screen::Details:
      drawDetails();
      break;
    case Screen::List:
      drawList();
      break;
    case Screen::Menu:
      drawMenu();
      break;
    case Screen::ConfirmSetup:
      drawConfirmSetupScreen();
      break;
    case Screen::Setup:
      drawSetupScreen();
      break;
    case Screen::Message:
      break;
  }
}

void App::drawTopBar(const OpenSkyService::Status& status) {
  const bool wifiOk = wifi_.stationState() == WifiManager::StationState::Connected;
  const uint16_t openskyColor = status.state == OpenSkyService::State::Running ? ui::kColorAccent
                                : status.state == OpenSkyService::State::AuthError ||
                                        status.state == OpenSkyService::State::Error ||
                                        status.state == OpenSkyService::State::RateLimited
                                    ? ui::kColorError
                                    : ui::kColorWarning;
  ui::drawStatusBar(tft_, wifiOk ? ui::kColorAccent : ui::kColorWarning, "WLAN", "");
  tft_.fillCircle(62, ui::kStatusBarHeight / 2, 4, openskyColor);
  tft_.setTextColor(ui::kColorText, ui::kColorBar);
  ui::drawText(tft_, status.authenticated ? "OpenSky" : "OpenSky (anonym)", 71, 3);
  if (time_sync::isSynced()) {
    char clock[8];
    time_sync::formatLocalTime(time(nullptr), clock, sizeof(clock), false);
    ui::drawText(tft_, clock, tft_.width() - 6, 3, 2, true);
  }
}

void App::drawRadar() {
  const OpenSkyService::Status status = service_.status();
  lastDataState_ = dataState(status);
  drawTopBar(status);

  RadarView::Info info;
  info.rangeKm = config_.rangeKm;
  info.dataState = lastDataState_;
  info.apiTime = status.apiTime;
  info.waitingText = status.message[0] != '\0' ? status.message : "Starte ...";
  // Kurzinfo: ausgewähltes Flugzeug, sonst das nächstgelegene im Radius (Liste ist nach Entfernung sortiert).
  const Aircraft* selected = findSelected();
  if (selected != nullptr) {
    strlcpy(labelIcao24_, selected->icao24, sizeof(labelIcao24_));
    info.labelIsSelection = true;
  } else if (flights_.size() > 0 && flights_[0].distanceKm <= config_.rangeKm) {
    strlcpy(labelIcao24_, flights_[0].icao24, sizeof(labelIcao24_));
  } else {
    labelIcao24_[0] = '\0';
  }
  info.labelIcao24 = labelIcao24_;
  radar_.draw(flights_, info);
  drawRadarFooter(status);
}

void App::drawRadarFooter(const OpenSkyService::Status& status) {
  char line[64];
  uint16_t color = ui::kColorMuted;
  if (noticeUntilMs_ != 0 && static_cast<int32_t>(millis() - noticeUntilMs_) < 0) {
    strlcpy(line, notice_, sizeof(line));
    color = ui::kColorWarning;
  } else if (power_.nightActive()) {
    snprintf(line, sizeof(line), "Nachtruhe · Tiefschlaf in %lu s",
             static_cast<unsigned long>(power_.secondsUntilSleep()));
    color = ui::kColorWarning;
  } else if (status.lastSuccess == 0) {
    strlcpy(line, status.message, sizeof(line));
  } else {
    char clock[10];
    time_sync::formatLocalTime(status.lastSuccess, clock, sizeof(clock), true);
    const long age = static_cast<long>(time(nullptr) - status.lastSuccess);
    if (status.state == OpenSkyService::State::Running) {
      snprintf(line, sizeof(line), "Stand %s · vor %ld s", clock, age);
    } else {
      snprintf(line, sizeof(line), "Stand %s · %s", clock, status.message);
      color = ui::kColorWarning;
    }
  }
  tft_.fillRect(0, kFooterY, tft_.width(), 16, ui::kColorBackground);
  tft_.setTextColor(color, ui::kColorBackground);
  ui::drawText(tft_, line, tft_.width() / 2 - ui::textWidth(tft_, line) / 2, kFooterY);
}

void App::drawDetails() {
  const Aircraft& a = detail_;
  tft_.fillScreen(ui::kColorBackground);
  drawTopBar(service_.status());

  tft_.setTextDatum(TL_DATUM);
  tft_.setTextColor(ui::kColorSelectedText, ui::kColorBackground);
  tft_.drawString(a.callsign[0] ? a.callsign : "ohne Callsign", ui::kMargin, 28, 4);
  char line[64];
  snprintf(line, sizeof(line), "ICAO24 %s · %s", a.icao24, a.originCountry);
  ui::drawLine(tft_, 56, line, ui::kColorMuted);
  tft_.drawFastHLine(ui::kMargin, 74, tft_.width() - 2 * ui::kMargin, ui::kColorCross);

  char value[32];
  char number[16];
  struct Row {
    const char* label;
    char text[32];
  } rows[10];
  uint8_t n = 0;
  auto add = [&](const char* label, const char* text) {
    rows[n].label = label;
    strlcpy(rows[n].text, text, sizeof(rows[n].text));
    ++n;
  };
  formatAltitude(value, sizeof(value), a.baroAltitudeM);
  add("Höhe (baro)", value);
  formatAltitude(value, sizeof(value), a.geoAltitudeM);
  add("Höhe (geo)", value);
  if (isnan(a.velocityMs)) {
    snprintf(value, sizeof(value), "-");
  } else {
    snprintf(value, sizeof(value), "%.0f km/h", a.velocityMs * 3.6f);
  }
  add("Geschwindigkeit", value);
  if (isnan(a.trackDeg)) {
    snprintf(value, sizeof(value), "-");
  } else {
    snprintf(value, sizeof(value), "%.0f°", a.trackDeg);
  }
  add("Kurs", value);
  if (isnan(a.verticalRateMs)) {
    snprintf(value, sizeof(value), "-");
  } else {
    ui::formatDecimal(number, sizeof(number), a.verticalRateMs, 1);
    snprintf(value, sizeof(value), "%s%s m/s", a.verticalRateMs > 0.05f ? "+" : "", number);
  }
  add("Steigrate", value);
  ui::formatDecimal(number, sizeof(number), a.distanceKm, 1);
  snprintf(value, sizeof(value), "%s km", number);
  add("Entfernung", value);
  snprintf(value, sizeof(value), "%.0f° (%s)", a.bearingDeg, compassPoint(a.bearingDeg));
  add("Peilung", value);
  add("Squawk", a.squawk[0] ? a.squawk : "-");
  snprintf(value, sizeof(value), "%s%s", positionSourceName(a.positionSource), a.onGround ? ", am Boden" : "");
  add("Quelle", value);
  add("Letzter Kontakt", "");  // wird sekündlich aktualisiert

  int16_t y = 80;
  for (uint8_t i = 0; i < n; ++i, y += 16) {
    tft_.setTextColor(ui::kColorMuted, ui::kColorBackground);
    ui::drawText(tft_, rows[i].label, ui::kMargin, y);
    tft_.setTextColor(ui::kColorText, ui::kColorBackground);
    ui::drawText(tft_, rows[i].text, tft_.width() - ui::kMargin, y, 2, true);
  }
  updateDetailsAge();

  tft_.drawFastHLine(ui::kMargin, 244, tft_.width() - 2 * ui::kMargin, ui::kColorCross);
  tft_.setTextColor(ui::kColorStale, ui::kColorBackground);
  ui::drawText(tft_, "Typ, Registrierung, Start/Ziel:", ui::kMargin, 248);
  ui::drawText(tft_, "von OpenSky nicht geliefert", ui::kMargin, 262);
  ui::drawButtonBar(tft_, kDetailButtons, 2);
}

void App::updateDetailsAge() {
  char value[32];
  if (!detailInData_) {
    snprintf(value, sizeof(value), "nicht mehr im Radius");
  } else if (detail_.lastContact > 0 && time_sync::isSynced()) {
    snprintf(value, sizeof(value), "vor %ld s", static_cast<long>(time(nullptr) - detail_.lastContact));
  } else {
    snprintf(value, sizeof(value), "-");
  }
  const int16_t y = 80 + 9 * 16;
  tft_.fillRect(tft_.width() / 2, y, tft_.width() / 2 - ui::kMargin, 16, ui::kColorBackground);
  tft_.setTextColor(detailInData_ ? ui::kColorText : ui::kColorWarning, ui::kColorBackground);
  ui::drawText(tft_, value, tft_.width() - ui::kMargin, y, 2, true);
}

void App::drawList() {
  tft_.fillScreen(ui::kColorBackground);
  drawTopBar(service_.status());
  const uint16_t total = flights_.size();
  const uint8_t pages = total == 0 ? 1 : (total + kListRowsPerPage - 1) / kListRowsPerPage;
  if (listPage_ >= pages) listPage_ = pages - 1;

  char line[48];
  snprintf(line, sizeof(line), "%u Flugzeuge", total);
  tft_.setTextColor(ui::kColorText, ui::kColorBackground);
  ui::drawText(tft_, line, ui::kMargin, 26);
  tft_.setTextColor(ui::kColorMuted, ui::kColorBackground);
  ui::drawText(tft_, "nach Entfernung", tft_.width() - ui::kMargin, 26, 2, true);
  tft_.setTextColor(ui::kColorStale, ui::kColorBackground);
  ui::drawText(tft_, "Callsign", 30, 40);
  ui::drawText(tft_, "Höhe", 160, 40, 2, true);
  ui::drawText(tft_, "Entf.", tft_.width() - ui::kMargin, 40, 2, true);

  for (uint8_t row = 0; row < kListRowsPerPage; ++row) {
    const uint16_t index = listPage_ * kListRowsPerPage + row;
    if (index >= total) break;
    const Aircraft& a = flights_[index];
    const int16_t y = kListTop + row * kListRowHeight;
    const bool selected = strcmp(a.icao24, selectedIcao24_) == 0;
    tft_.fillRoundRect(4, y, tft_.width() - 8, kListRowHeight - 2, 3, selected ? ui::kColorSelectedBg : ui::kColorRow);
    const uint16_t color = selected ? ui::kColorSelectedText : ui::kColorText;
    tft_.setTextColor(color, selected ? ui::kColorSelectedBg : ui::kColorRow);
    ui::drawText(tft_, a.callsign[0] ? a.callsign : "-", 30, y + 6);
    char value[16];
    formatAltitude(value, sizeof(value), a.baroAltitudeM);
    ui::drawText(tft_, value, 160, y + 6, 2, true);
    char number[12];
    ui::formatDecimal(number, sizeof(number), a.distanceKm, 1);
    snprintf(value, sizeof(value), "%s km", number);
    ui::drawText(tft_, value, tft_.width() - ui::kMargin, y + 6, 2, true);
    // Richtungspfeil
    if (!isnan(a.trackDeg)) {
      const float r = a.trackDeg * static_cast<float>(M_PI / 180.0);
      const int16_t cx = 16, cy = y + kListRowHeight / 2 - 1;
      tft_.fillTriangle(cx + lroundf(7 * sinf(r)), cy - lroundf(7 * cosf(r)), cx + lroundf(5 * sinf(r + 2.4f)),
                        cy - lroundf(5 * cosf(r + 2.4f)), cx + lroundf(5 * sinf(r - 2.4f)),
                        cy - lroundf(5 * cosf(r - 2.4f)), selected ? ui::kColorSelected : ui::kColorAircraft);
    }
  }
  if (total == 0) {
    tft_.setTextColor(ui::kColorMuted, ui::kColorBackground);
    ui::drawText(tft_, "Keine Flugzeuge im Radius", ui::kMargin, kListTop + 8);
  } else {
    const uint16_t first = listPage_ * kListRowsPerPage + 1;
    const uint16_t last = min<uint16_t>(first + kListRowsPerPage - 1, total);
    snprintf(line, sizeof(line), "%u-%u von %u", first, last, total);
    tft_.setTextColor(ui::kColorMuted, ui::kColorBackground);
    ui::drawText(tft_, line, tft_.width() / 2 - ui::textWidth(tft_, line) / 2, kListTop + kListRowsPerPage * kListRowHeight + 2);
  }
  ui::drawButtonBar(tft_, kListButtons, 3);
}

void App::drawMenu() {
  tft_.fillScreen(ui::kColorBackground);
  updateMenu();
  ui::drawButtonBar(tft_, kMenuButtons, 4);
}

void App::updateMenu() {
  const OpenSkyService::Status status = service_.status();
  drawTopBar(status);
  const bool connected = wifi_.stationState() == WifiManager::StationState::Connected;
  char line[64];
  int16_t y = ui::kStatusBarHeight + 6;
  auto put = [&](const char* text, uint16_t color) {
    ui::drawLine(tft_, y, text, color);
    y += ui::kLineHeight;
  };

  if (connected) {
    snprintf(line, sizeof(line), "WLAN %s (%d dBm)", config_.wifiSsid, wifi_.rssi());
  } else {
    snprintf(line, sizeof(line), "WLAN %s: %s", config_.wifiSsid,
             wifi_.stationState() == WifiManager::StationState::Connecting ? "verbinde ..." : "getrennt");
  }
  put(line, connected ? ui::kColorText : ui::kColorWarning);
  snprintf(line, sizeof(line), "http://%s", connected ? wifi_.stationIp().toString().c_str() : "-");
  put(line, ui::kColorText);
  if (config_.locationName[0] != '\0') {
    snprintf(line, sizeof(line), "Standort: %s", config_.locationName);
  } else {
    snprintf(line, sizeof(line), "Standort: %.4f, %.4f", config_.latitude, config_.longitude);
  }
  put(line, ui::kColorText);
  snprintf(line, sizeof(line), "Radius %u km, alle %u s", config_.rangeKm, config_.intervalS);
  put(line, ui::kColorText);
  if (config_.nightEnabled) {
    snprintf(line, sizeof(line), "Nachtruhe %02u:%02u-%02u:%02u (Tiefschlaf)", config_.nightStartMin / 60,
             config_.nightStartMin % 60, config_.nightEndMin / 60, config_.nightEndMin % 60);
  } else {
    snprintf(line, sizeof(line), "Nachtruhe aus");
  }
  put(line, ui::kColorText);
  y += 6;

  put("OpenSky:", ui::kColorMuted);
  put(status.message, status.state == OpenSkyService::State::Running ? ui::kColorAccent : ui::kColorWarning);
  if (status.lastSuccess != 0) {
    char clock[10];
    char seconds[12];
    time_sync::formatLocalTime(status.lastSuccess, clock, sizeof(clock), true);
    ui::formatDecimal(seconds, sizeof(seconds), status.lastDurationMs / 1000.0, 1);
    snprintf(line, sizeof(line), "Letzter Abruf %s (%s s)", clock, seconds);
  } else {
    snprintf(line, sizeof(line), "Noch kein Abruf");
  }
  put(line, ui::kColorText);
  snprintf(line, sizeof(line), "Im Radius %u, in der Box %u", status.aircraftInRange, status.receivedInBox);
  put(line, ui::kColorText);
  if (status.remainingCredits >= 0) {
    snprintf(line, sizeof(line), "Credits übrig: %ld", static_cast<long>(status.remainingCredits));
  } else {
    snprintf(line, sizeof(line), "Credits übrig: unbekannt");
  }
  put(line, ui::kColorText);
  snprintf(line, sizeof(line), "Nächster Abruf in %lu s", static_cast<unsigned long>(status.nextFetchInS));
  put(line, ui::kColorText);
  const uint32_t minutes = millis() / 60000;
  snprintf(line, sizeof(line), "Laufzeit %lu:%02lu h, Abrufe %lu, Fehler %lu", static_cast<unsigned long>(minutes / 60),
           static_cast<unsigned long>(minutes % 60), static_cast<unsigned long>(status.sequence),
           static_cast<unsigned long>(status.failures));
  put(line, ui::kColorMuted);
  snprintf(line, sizeof(line), "FW %s · Heap %lu KB (min %lu KB)", kFirmwareVersion,
           static_cast<unsigned long>(ESP.getFreeHeap() / 1024),
           static_cast<unsigned long>(ESP.getMinFreeHeap() / 1024));
  put(line, ui::kColorMuted);
}

void App::drawSetupScreen() {
  tft_.fillScreen(ui::kColorBackground);
  ui::drawStatusBar(tft_, ui::kColorWarning, "Setup-AP aktiv", "");

  const int16_t cx = tft_.width() / 2;
  int16_t y = ui::kStatusBarHeight + 10;
  tft_.setTextDatum(TC_DATUM);
  tft_.setTextColor(ui::kColorWarning, ui::kColorBackground);
  const char* title = config_.isComplete() ? "Setup-Modus" : "Setup erforderlich";
  tft_.drawString(title, cx, y, ui::fittingFont(tft_, title));
  y += 34;

  ui::drawLine(tft_, y, "1. Mit diesem WLAN verbinden:", ui::kColorText);
  y += ui::kLineHeight;
  ui::drawLine(tft_, y, WifiManager::kSetupSsid, ui::kColorAccent);
  y += ui::kLineHeight + 8;
  ui::drawLine(tft_, y, "2. WLAN-Passwort:", ui::kColorText);
  y += ui::kLineHeight + 2;
  tft_.drawRoundRect(ui::kMargin, y, tft_.width() - 2 * ui::kMargin, 32, 4, ui::kColorAccent);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(ui::kColorText, ui::kColorBackground);
  tft_.drawString(wifi_.setupPassword(), cx, y + 17, 4);
  y += 32 + 10;
  ui::drawLine(tft_, y, "3. Seite öffnet sich, sonst:", ui::kColorText);
  y += ui::kLineHeight;
  char line[40];
  snprintf(line, sizeof(line), "http://%s", wifi_.accessPointIp().toString().c_str());
  ui::drawLine(tft_, y, line, ui::kColorAccent);

  if (config_.isComplete()) ui::drawButtonBar(tft_, kSetupButtonsCancel, 1);
  updateSetupScreen();
}

void App::updateSetupScreen() {
  const uint32_t elapsed = millis() - setupStartedMs_;
  const uint32_t remaining = elapsed >= kSetupTimeoutMs ? 0 : (kSetupTimeoutMs - elapsed) / 1000;
  char right[16];
  snprintf(right, sizeof(right), "noch %02lu:%02lu", static_cast<unsigned long>(remaining / 60),
           static_cast<unsigned long>(remaining % 60));
  tft_.fillRect(tft_.width() - 90, 0, 90, ui::kStatusBarHeight, ui::kColorBar);
  tft_.setTextColor(ui::kColorText, ui::kColorBar);
  ui::drawText(tft_, right, tft_.width() - 6, 3, 2, true);

  char line[40];
  const uint8_t clients = wifi_.accessPointClients();
  snprintf(line, sizeof(line), "Verbundene Geräte: %u", clients);
  const int16_t y = tft_.height() - ui::kButtonBarHeight - ui::kLineHeight - 6;
  ui::drawLine(tft_, y, line, clients > 0 ? ui::kColorAccent : ui::kColorMuted);
}

void App::drawConfirmSetupScreen() {
  ui::drawMessage(tft_, "Setup starten?", "WLAN-Verbindung wird getrennt", ui::kColorWarning);
  ui::drawButtonBar(tft_, kConfirmButtons, 2);
}

// ---------------------------------------------------------------------------------------------
// Bedienung

void App::handleTouch(const TouchState& touch) {
  if (!touch.pressed || restartAtMs_ != 0) return;
  // Gedimmtes oder dunkles Display: die Berührung weckt nur.
  if (mode_ == Mode::Normal && power_.onTouch()) return;
  switch (screen_) {
    case Screen::Radar:
      handleRadarTouch(touch.x, touch.y);
      break;
    case Screen::Details:
      switch (ui::buttonAt(tft_, touch.x, touch.y, 2)) {
        case 0:
          showScreen(Screen::Radar);
          break;
        case 1:
          showScreen(Screen::List);
          break;
        default:
          break;
      }
      break;
    case Screen::List: {
      const int8_t button = ui::buttonAt(tft_, touch.x, touch.y, 3);
      const uint8_t pages = flights_.size() == 0 ? 1 : (flights_.size() + kListRowsPerPage - 1) / kListRowsPerPage;
      if (button == 0) {
        showScreen(Screen::Radar);
      } else if (button == 1 && listPage_ > 0) {
        --listPage_;
        drawList();
      } else if (button == 2 && listPage_ + 1 < pages) {
        ++listPage_;
        drawList();
      } else if (button < 0 && touch.y >= kListTop) {
        const uint16_t row = (touch.y - kListTop) / kListRowHeight;
        const uint16_t index = listPage_ * kListRowsPerPage + row;
        if (row < kListRowsPerPage && index < flights_.size()) {
          strlcpy(selectedIcao24_, flights_[index].icao24, sizeof(selectedIcao24_));
          detail_ = flights_[index];
          detailInData_ = true;
          showScreen(Screen::Details);
        }
      }
      break;
    }
    case Screen::Menu:
      switch (ui::buttonAt(tft_, touch.x, touch.y, 4)) {
        case 0:
          showScreen(Screen::Radar);
          break;
        case 1:
          showScreen(Screen::ConfirmSetup);
          break;
        case 2:
          touch_.calibrate(rotation_);
          showScreen(Screen::Menu);
          break;
        case 3:
          restartWith(BootRequest::Diagnostics);
        default:
          break;
      }
      break;
    case Screen::ConfirmSetup:
      switch (ui::buttonAt(tft_, touch.x, touch.y, 2)) {
        case 0:
          ui::drawMessage(tft_, "Setup", "Neustart ...", ui::kColorWarning);
          restartWith(BootRequest::Setup);
        case 1:
          showScreen(mode_ == Mode::Normal ? Screen::Menu : Screen::Setup);
          break;
        default:
          break;
      }
      break;
    case Screen::Setup:
      if (config_.isComplete() && ui::buttonAt(tft_, touch.x, touch.y, 1) == 0) {
        scheduleRestart("Abgebrochen", "Neustart ...");
      }
      break;
    case Screen::Message:
      break;
  }
}

void App::handleRadarTouch(uint16_t x, uint16_t y) {
  switch (ui::buttonAt(tft_, x, y, 4)) {
    case 0:
      changeRange(+1);  // „−“: herauszoomen, größerer Radius
      return;
    case 1:
      listPage_ = 0;
      showScreen(Screen::List);
      return;
    case 2:
      showScreen(Screen::Menu);
      return;
    case 3:
      changeRange(-1);  // „+“: hineinzoomen, kleinerer Radius
      return;
    default:
      break;
  }
  if (!radar_.contains(x, y)) return;

  // Kurzinfo antippen öffnet die Details (auch die automatische des nächstgelegenen Flugzeugs).
  if (radar_.labelHit(x, y) && labelIcao24_[0] != '\0') {
    for (uint16_t i = 0; i < flights_.size(); ++i) {
      if (strcmp(flights_[i].icao24, labelIcao24_) != 0) continue;
      strlcpy(selectedIcao24_, labelIcao24_, sizeof(selectedIcao24_));
      detail_ = flights_[i];
      detailInData_ = true;
      showScreen(Screen::Details);
      return;
    }
  }
  uint16_t candidates[8];
  const uint8_t count = radar_.hitTest(flights_, config_.rangeKm, x, y, candidates, 8);
  if (count == 0) {
    selectedIcao24_[0] = '\0';
  } else {
    // Überlappende Symbole: erneutes Antippen wechselt zum nächsten Kandidaten;
    // ist der einzige Kandidat bereits ausgewählt, öffnen sich die Details.
    int8_t current = -1;
    for (uint8_t i = 0; i < count; ++i) {
      if (strcmp(flights_[candidates[i]].icao24, selectedIcao24_) == 0) current = i;
    }
    if (current >= 0 && count == 1) {
      showScreen(Screen::Details);
      return;
    }
    const uint16_t next = candidates[current < 0 ? 0 : (current + 1) % count];
    strlcpy(selectedIcao24_, flights_[next].icao24, sizeof(selectedIcao24_));
    detail_ = flights_[next];
    detailInData_ = true;
  }
  drawRadar();
}

void App::changeRange(int8_t direction) {
  constexpr size_t kCount = sizeof(app_config::kAllowedRangesKm) / sizeof(app_config::kAllowedRangesKm[0]);
  size_t index = 0;
  while (index < kCount && app_config::kAllowedRangesKm[index] != config_.rangeKm) ++index;
  if (index >= kCount) return;
  const int next = static_cast<int>(index) + direction;
  if (next < 0 || next >= static_cast<int>(kCount)) return;

  AppConfig candidate = config_;
  candidate.rangeKm = app_config::kAllowedRangesKm[next];
  const char* error = app_config::validate(candidate);
  if (error != nullptr) {
    showNotice("Reichweite: Kontingent reicht nicht");
    drawRadarFooter(service_.status());
    return;
  }
  const bool larger = candidate.rangeKm > config_.rangeKm;
  config_.rangeKm = candidate.rangeKm;
  app_config::save(config_);
  service_.setRangeKm(config_.rangeKm);  // gilt ab dem nächsten regulären Abruf
  if (larger) showNotice("Größerer Radius ab nächstem Abruf");
  drawRadar();
}

void App::showNotice(const char* text) {
  strlcpy(notice_, text, sizeof(notice_));
  noticeUntilMs_ = millis() + kNoticeDurationMs;
}

const Aircraft* App::findSelected() const {
  if (selectedIcao24_[0] == '\0') return nullptr;
  for (uint16_t i = 0; i < flights_.size(); ++i) {
    if (strcmp(flights_[i].icao24, selectedIcao24_) == 0) return &flights_[i];
  }
  return nullptr;
}

void App::handleSerial() {
  while (Serial.available() > 0) {
    switch (Serial.read()) {
      case 'h':
        printHeapSnapshot(Serial, "manuell");
        break;
      case 's':
        Serial.println("[app] Setup-Modus angefordert");
        restartWith(BootRequest::Setup);
      case 'd':
        restartWith(BootRequest::Diagnostics);
      case '?':
        Serial.println("Befehle: h=Heap  s=Setup-Modus  d=Diagnose  ?=Hilfe");
        break;
      default:
        break;
    }
  }
}

void App::scheduleRestart(const char* title, const char* line) {
  screen_ = Screen::Message;
  ui::drawMessage(tft_, title, line, ui::kColorAccent);
  restartAtMs_ = millis() + kRestartDelayMs;
}

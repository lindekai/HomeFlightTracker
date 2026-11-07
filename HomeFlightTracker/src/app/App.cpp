#include "App.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include "../config/HardwareSettings.h"
#include "../diag/HeapReport.h"
#include "../opensky/Quota.h"
#include "../ui/Widgets.h"
#include "BootRequest.h"
#include "Version.h"

namespace {

const char* const kStatusButtons[] = {"Setup", "Kalibr.", "Diagnose"};
const char* const kConfirmButtons[] = {"Ja", "Nein"};
const char* const kSetupButtonsCancel[] = {"Abbrechen"};
constexpr uint8_t kStatusButtonCount = 3;
constexpr uint8_t kConfirmButtonCount = 2;

}  // namespace

void App::begin(bool setupRequested) {
  tft_.init();
  touch_.begin();
  rotation_ = hardware_settings::loadRotation();
  tft_.setRotation(rotation_);

  const bool calibrate = runBootRecovery();
  if (calibrate || !touch_.applyStoredCalibration(rotation_)) touch_.calibrate(rotation_);

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

  if (mode_ == Mode::Setup && now - setupStartedMs_ >= kSetupTimeoutMs) {
    // Ohne Abschluss endet der Setup-Modus; bei unvollständiger Konfiguration startet er mit neuem Passwort neu.
    Serial.println("[app] Setup-Zeit abgelaufen");
    ESP.restart();
  }

  if (mode_ == Mode::Normal && !mdnsStarted_ && wifi_.stationState() == WifiManager::StationState::Connected) {
    mdnsStarted_ = MDNS.begin(WifiManager::kHostname);
    if (mdnsStarted_) MDNS.addService("http", "tcp", 80);
    Serial.printf("[mdns] %s.local %s\n", WifiManager::kHostname, mdnsStarted_ ? "aktiv" : "FEHLER");
  }

  if (now - lastRefreshMs_ >= kRefreshIntervalMs) {
    lastRefreshMs_ = now;
    if (screen_ == Screen::Status) updateStatusScreen();
    if (screen_ == Screen::Setup) updateSetupScreen();
  }
  delay(2);
}

bool App::runBootRecovery() {
  const int16_t cx = tft_.width() / 2;
  const int16_t cy = tft_.height() / 2;
  tft_.fillScreen(ui::kColorBackground);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(ui::kColorAccent, ui::kColorBackground);
  tft_.drawString("HomeFlightTracker", cx, cy - 50, ui::fittingFont(tft_, "HomeFlightTracker"));
  tft_.setTextColor(ui::kColorMuted, ui::kColorBackground);
  tft_.drawString(String("Firmware ") + kFirmwareVersion, cx, cy - 22, 2);
  tft_.drawString("Antippen: Touch kalibrieren", cx, cy + 20, 2);
  tft_.drawString("8 s halten: Werksreset", cx, cy + 40, 2);

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
      if (held >= kFactoryResetHoldMs) {
        runFactoryReset();  // kehrt nicht zurück
      }
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
  ui::drawMessage(tft_, "Werksreset", "Alle Einstellungen geloescht", ui::kColorWarning);
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
  wifi_.startStation(config_.wifiSsid, config_.wifiPassword);
  portal_.begin(SetupPortal::Mode::Lan, kFirmwareVersion);
  showScreen(Screen::Status);
}

void App::showScreen(Screen screen) {
  screen_ = screen;
  switch (screen) {
    case Screen::Status:
      drawStatusScreen();
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

void App::drawStatusScreen() {
  tft_.fillScreen(ui::kColorBackground);
  updateStatusScreen();
  ui::drawButtonBar(tft_, kStatusButtons, kStatusButtonCount);
}

void App::updateStatusScreen() {
  const WifiManager::StationState state = wifi_.stationState();
  const bool connected = state == WifiManager::StationState::Connected;
  const uint16_t dot = connected ? ui::kColorAccent
                       : state == WifiManager::StationState::Connecting ? ui::kColorWarning
                                                                         : ui::kColorError;
  ui::drawStatusBar(tft_, dot, "WLAN", kFirmwareVersion);

  char line[48];
  int16_t y = ui::kStatusBarHeight + 8;
  snprintf(line, sizeof(line), "WLAN: %s", config_.wifiSsid);
  ui::drawLine(tft_, y, line, ui::kColorText);
  y += ui::kLineHeight;
  if (connected) {
    snprintf(line, sizeof(line), "verbunden (%d dBm)", wifi_.rssi());
  } else {
    snprintf(line, sizeof(line), "%s", state == WifiManager::StationState::Connecting ? "verbinde ..." : "nicht verbunden");
  }
  ui::drawLine(tft_, y, line, connected ? ui::kColorAccent : ui::kColorWarning);
  y += ui::kLineHeight + 6;

  ui::drawLine(tft_, y, "Weboberflaeche:", ui::kColorMuted);
  y += ui::kLineHeight;
  if (connected) {
    snprintf(line, sizeof(line), "http://%s", wifi_.stationIp().toString().c_str());
  } else {
    snprintf(line, sizeof(line), "-");
  }
  ui::drawLine(tft_, y, line, ui::kColorText);
  y += ui::kLineHeight;
  snprintf(line, sizeof(line), "%s.local", WifiManager::kHostname);
  ui::drawLine(tft_, y, mdnsStarted_ ? line : "", ui::kColorText);
  y += ui::kLineHeight + 6;

  ui::drawLine(tft_, y, "Standort:", ui::kColorMuted);
  y += ui::kLineHeight;
  snprintf(line, sizeof(line), "%.4f, %.4f", config_.latitude, config_.longitude);
  ui::drawLine(tft_, y, line, ui::kColorText);
  y += ui::kLineHeight;
  ui::drawLine(tft_, y, config_.locationName, ui::kColorText);
  y += ui::kLineHeight + 6;

  const bool authenticated = config_.hasOpenskyCredentials();
  const uint8_t credits = opensky::creditsPerRequest(app_config::queryAreaSquareDegrees(config_));
  snprintf(line, sizeof(line), "Radius %u km, alle %u s", config_.rangeKm, config_.intervalS);
  ui::drawLine(tft_, y, line, ui::kColorText);
  y += ui::kLineHeight;
  snprintf(line, sizeof(line), "%s: ~%lu Credits/Tag", authenticated ? "OpenSky" : "anonym",
           static_cast<unsigned long>(opensky::dailyCredits(config_.intervalS, credits)));
  ui::drawLine(tft_, y, line, authenticated ? ui::kColorText : ui::kColorWarning);
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
  ui::drawLine(tft_, y, "3. Seite oeffnet sich, sonst:", ui::kColorText);
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
  tft_.setTextDatum(MR_DATUM);
  tft_.setTextColor(ui::kColorText, ui::kColorBar);
  tft_.setTextPadding(80);
  tft_.drawString(right, tft_.width() - 6, ui::kStatusBarHeight / 2, 2);
  tft_.setTextPadding(0);

  char line[40];
  const uint8_t clients = wifi_.accessPointClients();
  snprintf(line, sizeof(line), "Verbundene Geraete: %u", clients);
  const int16_t y = tft_.height() - ui::kButtonBarHeight - ui::kLineHeight - 6;
  ui::drawLine(tft_, y, line, clients > 0 ? ui::kColorAccent : ui::kColorMuted);
}

void App::drawConfirmSetupScreen() {
  ui::drawMessage(tft_, "Setup starten?", "WLAN-Verbindung wird getrennt", ui::kColorWarning);
  ui::drawButtonBar(tft_, kConfirmButtons, kConfirmButtonCount);
}

void App::handleTouch(const TouchState& touch) {
  if (!touch.pressed || restartAtMs_ != 0) return;
  switch (screen_) {
    case Screen::Status:
      switch (ui::buttonAt(tft_, touch.x, touch.y, kStatusButtonCount)) {
        case 0:
          showScreen(Screen::ConfirmSetup);
          break;
        case 1:
          touch_.calibrate(rotation_);
          showScreen(Screen::Status);
          break;
        case 2:
          restartWith(BootRequest::Diagnostics);
        default:
          break;
      }
      break;
    case Screen::ConfirmSetup:
      switch (ui::buttonAt(tft_, touch.x, touch.y, kConfirmButtonCount)) {
        case 0:
          ui::drawMessage(tft_, "Setup", "Neustart ...", ui::kColorWarning);
          restartWith(BootRequest::Setup);
        case 1:
          showScreen(Screen::Status);
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

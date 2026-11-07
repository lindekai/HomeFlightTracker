#include "WifiManager.h"

#include <WiFi.h>
#include <esp_random.h>
#include <string.h>

void WifiManager::startStation(const char* ssid, const char* password) {
  strlcpy(ssid_, ssid, sizeof(ssid_));
  strlcpy(password_, password, sizeof(password_));
  WiFi.persistent(false);  // Zugangsdaten nur in unserer eigenen Konfiguration halten
  WiFi.setHostname(kHostname);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid_, password_);
  stationMode_ = true;
  stationState_ = StationState::Connecting;
  lastAttemptMs_ = millis();
  Serial.printf("[wifi] verbinde mit \"%s\"\n", ssid_);
}

bool WifiManager::startAccessPoint() {
  WiFi.persistent(false);
  WiFi.setHostname(kHostname);
  WiFi.mode(WIFI_AP_STA);  // erst Funk einschalten: esp_random() liefert dann echte Zufallszahlen
  generateSetupPassword();
  const bool ok = WiFi.softAP(kSetupSsid, setupPassword_);
  Serial.printf("[wifi] Setup-AP \"%s\" %s, IP %s\n", kSetupSsid, ok ? "aktiv" : "FEHLER",
                WiFi.softAPIP().toString().c_str());
  return ok;
}

void WifiManager::loop() {
  updateTest();
  if (!stationMode_) return;

  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    if (stationState_ != StationState::Connected) {
      Serial.printf("[wifi] verbunden, IP %s, RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    }
    stationState_ = StationState::Connected;
    return;
  }

  if (stationState_ == StationState::Connected) {
    Serial.println("[wifi] Verbindung verloren");
    stationState_ = StationState::Disconnected;
    lastAttemptMs_ = millis();
  }
  // Eigener Neuversuch zusätzlich zum Auto-Reconnect, z. B. nach Router-Neustart oder Passwortfehler.
  if (millis() - lastAttemptMs_ >= kReconnectIntervalMs) {
    Serial.println("[wifi] neuer Verbindungsversuch");
    WiFi.disconnect();
    WiFi.begin(ssid_, password_);
    stationState_ = StationState::Connecting;
    lastAttemptMs_ = millis();
  } else if (stationState_ == StationState::Connecting &&
             (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL)) {
    stationState_ = StationState::Disconnected;
  }
}

IPAddress WifiManager::stationIp() const {
  return WiFi.localIP();
}

int8_t WifiManager::rssi() const {
  return WiFi.RSSI();
}

IPAddress WifiManager::accessPointIp() const {
  return WiFi.softAPIP();
}

uint8_t WifiManager::accessPointClients() const {
  return WiFi.softAPgetStationNum();
}

int16_t WifiManager::scan() {
  if (!scanRunning_) {
    WiFi.scanNetworks(true);
    scanRunning_ = true;
    return -1;
  }
  const int16_t result = WiFi.scanComplete();
  if (result == WIFI_SCAN_RUNNING) return -1;
  if (result < 0) {
    // Fehlgeschlagen: beim nächsten Aufruf neu starten.
    scanRunning_ = false;
    return 0;
  }
  return result;
}

void WifiManager::clearScan() {
  WiFi.scanDelete();
  scanRunning_ = false;
}

void WifiManager::beginTest(const char* ssid, const char* password) {
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  testState_ = TestState::Running;
  testStartMs_ = millis();
  testMessage_ = "Verbindung wird getestet";
  Serial.printf("[wifi] Test der Verbindung mit \"%s\"\n", ssid);
}

void WifiManager::updateTest() {
  if (testState_ != TestState::Running) return;
  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    testState_ = TestState::Succeeded;
    testMessage_ = "Verbindung erfolgreich";
  } else if (status == WL_NO_SSID_AVAIL && millis() - testStartMs_ > 5000) {
    testState_ = TestState::Failed;
    testMessage_ = "WLAN nicht gefunden";
  } else if (millis() - testStartMs_ >= kTestTimeoutMs) {
    testState_ = TestState::Failed;
    testMessage_ = status == WL_CONNECT_FAILED ? "Verbindung abgelehnt (Passwort?)"
                                               : "Keine Verbindung (Passwort oder Reichweite?)";
  } else {
    return;
  }
  Serial.printf("[wifi] Testergebnis: %s\n", testMessage_);
  if (testState_ == TestState::Failed) WiFi.disconnect();
}

void WifiManager::generateSetupPassword() {
  // Ohne leicht verwechselbare Zeichen (0/O, 1/I/L); Format XXXX-XXXX.
  static const char kAlphabet[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
  constexpr size_t kAlphabetSize = sizeof(kAlphabet) - 1;
  size_t pos = 0;
  for (int i = 0; i < 8; ++i) {
    if (i == 4) setupPassword_[pos++] = '-';
    setupPassword_[pos++] = kAlphabet[esp_random() % kAlphabetSize];
  }
  setupPassword_[pos] = '\0';
}

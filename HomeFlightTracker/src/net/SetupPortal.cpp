#include "SetupPortal.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <string.h>

#include "../geo/Geo.h"
#include "../opensky/Quota.h"
#include "PortalPage.h"

namespace {

constexpr uint16_t kDnsPort = 53;
const char* kCollectedHeaders[] = {"X-HFT"};  // WebServer::collectHeaders erwartet const char**

}  // namespace

void SetupPortal::begin(Mode mode, const char* firmwareVersion) {
  mode_ = mode;
  firmwareVersion_ = firmwareVersion;

  server_.collectHeaders(kCollectedHeaders, sizeof(kCollectedHeaders) / sizeof(kCollectedHeaders[0]));
  server_.on("/", HTTP_GET, [this] { handleRoot(); });
  server_.on("/api/state", HTTP_GET, [this] { handleState(); });
  server_.on("/api/estimate", HTTP_GET, [this] { handleEstimate(); });
  server_.on("/api/scan", HTTP_GET, [this] { handleScan(); });
  server_.on("/api/wifi/test", HTTP_POST, [this] { handleWifiTestStart(); });
  server_.on("/api/wifi/test", HTTP_GET, [this] { handleWifiTestStatus(); });
  server_.on("/api/config", HTTP_POST, [this] { handleConfig(); });
  server_.onNotFound([this] { handleNotFound(); });
  server_.begin();

  if (mode_ == Mode::Setup) {
    // Captive Portal: alle DNS-Anfragen auf den Access-Point umleiten.
    dns_.setErrorReplyCode(DNSReplyCode::NoError);
    dns_.start(kDnsPort, "*", wifi_.accessPointIp());
  }
  Serial.printf("[web] Weboberflaeche aktiv (%s)\n", mode_ == Mode::Setup ? "Setup" : "Heimnetz, Digest-Auth");
}

void SetupPortal::loop() {
  if (mode_ == Mode::Setup) dns_.processNextRequest();
  server_.handleClient();
}

// Im Heimnetz ist jeder Zugriff passwortgeschützt. Im Setup-Modus schützt das zufällige
// WPA2-Passwort des Access-Points, das nur am Gerät angezeigt wird.
bool SetupPortal::authorize() {
  if (mode_ == Mode::Setup) return true;
  if (config_.webPassword[0] != '\0' && server_.authenticate(kAuthUser, config_.webPassword)) return true;
  server_.requestAuthentication(DIGEST_AUTH, kAuthRealm, "Anmeldung erforderlich");
  return false;
}

// Schutz gegen Cross-Site-Requests: ein Browser sendet den Zusatz-Header nur von unserer eigenen Seite,
// denn fremde Seiten bräuchten dafür eine CORS-Freigabe, die dieser Server nie erteilt.
bool SetupPortal::checkPostRequest() {
  if (server_.header("X-HFT") == "1") return true;
  sendError(403, "Anfrage abgelehnt");
  return false;
}

void SetupPortal::sendJson(int code, const String& body) {
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(code, "application/json", body);
}

void SetupPortal::sendError(int code, const char* message) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = message;
  String body;
  serializeJson(doc, body);
  sendJson(code, body);
}

void SetupPortal::handleRoot() {
  if (!authorize()) return;
  server_.sendHeader("Cache-Control", "no-store");
  server_.send_P(200, "text/html; charset=utf-8", kPortalPage);
}

void SetupPortal::handleState() {
  if (!authorize()) return;
  JsonDocument doc;
  doc["mode"] = mode_ == Mode::Setup ? "setup" : "lan";
  doc["version"] = firmwareVersion_;
  doc["configured"] = config_.isComplete();

  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["ssid"] = config_.wifiSsid;
  wifi["hasPassword"] = config_.wifiPassword[0] != '\0';
  wifi["connected"] = WiFi.status() == WL_CONNECTED;
  wifi["ip"] = WiFi.localIP().toString();

  JsonObject location = doc["location"].to<JsonObject>();
  location["set"] = config_.hasLocation;
  location["lat"] = config_.latitude;
  location["lon"] = config_.longitude;
  location["name"] = config_.locationName;

  JsonObject openskyInfo = doc["opensky"].to<JsonObject>();
  openskyInfo["clientId"] = config_.openskyClientId;
  openskyInfo["hasSecret"] = config_.openskyClientSecret[0] != '\0';

  doc["rangeKm"] = config_.rangeKm;
  doc["intervalS"] = config_.intervalS;
  JsonArray ranges = doc["ranges"].to<JsonArray>();
  for (uint16_t km : app_config::kAllowedRangesKm) ranges.add(km);
  doc["hasWebPassword"] = config_.webPassword[0] != '\0';

  String body;
  serializeJson(doc, body);
  sendJson(200, body);
}

void SetupPortal::handleEstimate() {
  if (!authorize()) return;
  AppConfig candidate = config_;
  candidate.latitude = server_.arg("lat").toDouble();
  candidate.longitude = server_.arg("lon").toDouble();
  candidate.rangeKm = server_.arg("range").toInt();
  const uint16_t interval = server_.arg("interval").toInt();
  const bool authenticated = server_.arg("auth") == "1";
  if (!geo::isValid({candidate.latitude, candidate.longitude})) {
    sendError(400, "Koordinaten ungültig");
    return;
  }

  const double area = app_config::queryAreaSquareDegrees(candidate);
  const uint8_t credits = opensky::creditsPerRequest(area);
  const uint32_t budget = authenticated ? opensky::kDailyCreditsAuthenticated : opensky::kDailyCreditsAnonymous;
  const uint32_t daily = opensky::dailyCredits(interval, credits);

  JsonDocument doc;
  doc["area"] = area;
  doc["creditsPerRequest"] = credits;
  doc["dailyCredits"] = daily;
  doc["budget"] = budget;
  doc["percent"] = interval > 0 ? daily * 100 / budget : 0;
  doc["minInterval"] = opensky::minIntervalSeconds(area, authenticated);
  doc["allowed"] = opensky::isIntervalAllowed(interval, area, authenticated);
  String body;
  serializeJson(doc, body);
  sendJson(200, body);
}

void SetupPortal::handleScan() {
  if (!authorize()) return;
  const int16_t count = wifi_.scan();
  JsonDocument doc;
  if (count < 0) {
    doc["state"] = "running";
  } else {
    doc["state"] = "done";
    JsonArray networks = doc["networks"].to<JsonArray>();
    for (int16_t i = 0; i < count; ++i) {
      const String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) continue;  // versteckte Netze
      JsonObject network = networks.add<JsonObject>();
      network["ssid"] = ssid;
      network["rssi"] = WiFi.RSSI(i);
      network["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    wifi_.clearScan();
  }
  String body;
  serializeJson(doc, body);
  sendJson(200, body);
}

void SetupPortal::handleWifiTestStart() {
  if (!authorize() || !checkPostRequest()) return;
  if (mode_ != Mode::Setup) {
    sendError(409, "Verbindungstest nur im Setup-Modus");
    return;
  }
  JsonDocument request;
  if (deserializeJson(request, server_.arg("plain"))) {
    sendError(400, "Ungültige Anfrage");
    return;
  }
  const char* ssid = request["ssid"] | "";
  const char* password = request["password"] | "";
  const bool open = request["open"] | false;
  if (ssid[0] == '\0' || strlen(ssid) > 32) {
    sendError(400, "WLAN-Name fehlt");
    return;
  }
  // Leeres Passwort beim gespeicherten Netz bedeutet „gespeichertes Passwort verwenden“.
  if (!open && password[0] == '\0' && strcmp(ssid, config_.wifiSsid) == 0) password = config_.wifiPassword;
  if (open) password = "";
  wifi_.beginTest(ssid, password);
  sendJson(202, "{\"ok\":true}");
}

void SetupPortal::handleWifiTestStatus() {
  if (!authorize()) return;
  JsonDocument doc;
  switch (wifi_.testState()) {
    case WifiManager::TestState::Idle:
      doc["state"] = "idle";
      break;
    case WifiManager::TestState::Running:
      doc["state"] = "running";
      break;
    case WifiManager::TestState::Succeeded:
      doc["state"] = "ok";
      break;
    case WifiManager::TestState::Failed:
      doc["state"] = "failed";
      break;
  }
  doc["message"] = wifi_.testMessage();
  String body;
  serializeJson(doc, body);
  sendJson(200, body);
}

void SetupPortal::handleConfig() {
  if (!authorize() || !checkPostRequest()) return;
  JsonDocument request;
  if (deserializeJson(request, server_.arg("plain"))) {
    sendError(400, "Ungültige Anfrage");
    return;
  }

  AppConfig updated = config_;

  const char* ssid = request["wifiSsid"] | "";
  const char* wifiPassword = request["wifiPassword"] | "";
  const bool wifiOpen = request["wifiOpen"] | false;
  if (strlen(ssid) > 32 || strlen(wifiPassword) > 63) {
    sendError(400, "WLAN-Angaben zu lang");
    return;
  }
  const bool ssidChanged = strcmp(ssid, updated.wifiSsid) != 0;
  strlcpy(updated.wifiSsid, ssid, sizeof(updated.wifiSsid));
  if (wifiOpen) {
    updated.wifiPassword[0] = '\0';
  } else if (wifiPassword[0] != '\0') {
    strlcpy(updated.wifiPassword, wifiPassword, sizeof(updated.wifiPassword));
  } else if (ssidChanged) {
    sendError(400, "WLAN-Passwort fehlt (oder „offenes Netz“ wählen)");
    return;
  }

  if (request["lat"].is<double>() && request["lon"].is<double>()) {
    updated.hasLocation = true;
    updated.latitude = request["lat"].as<double>();
    updated.longitude = request["lon"].as<double>();
  } else {
    updated.hasLocation = false;
  }
  strlcpy(updated.locationName, request["locationName"] | "", sizeof(updated.locationName));

  if (request["clearOpensky"] | false) {
    updated.openskyClientId[0] = '\0';
    updated.openskyClientSecret[0] = '\0';
  } else {
    const char* clientId = request["clientId"] | "";
    const char* clientSecret = request["clientSecret"] | "";
    if (strlen(clientId) >= sizeof(updated.openskyClientId) ||
        strlen(clientSecret) >= sizeof(updated.openskyClientSecret)) {
      sendError(400, "OpenSky-Angaben zu lang");
      return;
    }
    if (strcmp(clientId, updated.openskyClientId) != 0) updated.openskyClientSecret[0] = '\0';
    strlcpy(updated.openskyClientId, clientId, sizeof(updated.openskyClientId));
    if (clientSecret[0] != '\0') strlcpy(updated.openskyClientSecret, clientSecret, sizeof(updated.openskyClientSecret));
  }

  updated.rangeKm = request["rangeKm"] | 0;
  updated.intervalS = request["intervalS"] | 0;

  const char* webPassword = request["webPassword"] | "";
  if (strlen(webPassword) > 63) {
    sendError(400, "Geräte-Passwort zu lang");
    return;
  }
  if (webPassword[0] != '\0') strlcpy(updated.webPassword, webPassword, sizeof(updated.webPassword));

  const char* error = app_config::validate(updated);
  if (error != nullptr) {
    sendError(400, error);
    return;
  }
  if (!app_config::save(updated)) {
    sendError(500, "Speichern fehlgeschlagen");
    return;
  }
  config_ = updated;
  Serial.println("[web] Konfiguration gespeichert");
  sendJson(200, "{\"ok\":true}");
  restartRequested_ = true;
}

void SetupPortal::handleNotFound() {
  if (mode_ == Mode::Setup) {
    // Captive-Portal-Erkennung der Betriebssysteme (z. B. /generate_204, /hotspot-detect.html)
    // und alle anderen Adressen auf die Einrichtungsseite umleiten.
    server_.sendHeader("Location", "http://" + wifi_.accessPointIp().toString() + "/", true);
    server_.send(302, "text/plain", "");
    return;
  }
  if (!authorize()) return;
  server_.send(404, "text/plain", "Nicht gefunden");
}

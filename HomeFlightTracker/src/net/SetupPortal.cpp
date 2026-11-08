#include "SetupPortal.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <string.h>

#include "../config/Profiles.h"
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
  server_.on("/api/profiles", HTTP_POST, [this] { handleProfiles(); });
  server_.on("/api/opensky/test", HTTP_POST, [this] { handleOpenSkyTestStart(); });
  server_.on("/api/opensky/test", HTTP_GET, [this] { handleOpenSkyTestStatus(); });
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

  JsonObject power = doc["power"].to<JsonObject>();
  power["dimAfterS"] = config_.dimAfterS;
  power["dimPercent"] = config_.dimPercent;
  power["offAfterMin"] = config_.offAfterMin;
  power["nightEnabled"] = config_.nightEnabled;
  power["nightStartMin"] = config_.nightStartMin;
  power["nightEndMin"] = config_.nightEndMin;

  // Profile ohne Passwörter
  ProfileStore store;
  profiles::load(store);
  JsonObject profileInfo = doc["profiles"].to<JsonObject>();
  profileInfo["max"] = ProfileStore::kMax;
  profileInfo["active"] = store.active;
  JsonArray items = profileInfo["items"].to<JsonArray>();
  for (uint8_t i = 0; i < store.count; ++i) {
    JsonObject item = items.add<JsonObject>();
    item["name"] = store.items[i].name;
    item["ssid"] = store.items[i].wifiSsid;
    item["location"] = store.items[i].locationName;
  }

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

  if (request["power"].is<JsonObjectConst>()) {
    JsonObjectConst power = request["power"];
    updated.dimAfterS = power["dimAfterS"] | updated.dimAfterS;
    updated.dimPercent = power["dimPercent"] | updated.dimPercent;
    updated.offAfterMin = power["offAfterMin"] | updated.offAfterMin;
    updated.nightEnabled = power["nightEnabled"] | updated.nightEnabled;
    updated.nightStartMin = power["nightStartMin"] | updated.nightStartMin;
    updated.nightEndMin = power["nightEndMin"] | updated.nightEndMin;
  }

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
  profiles::updateActive(config_);
  Serial.println("[web] Konfiguration gespeichert");
  sendJson(200, "{\"ok\":true}");
  restartRequested_ = true;
}

// Profile: {"action":"add"|"delete"|"activate", ...}. Aktivieren übernimmt WLAN und Standort
// in die Konfiguration und startet neu.
void SetupPortal::handleProfiles() {
  if (!authorize() || !checkPostRequest()) return;
  JsonDocument request;
  if (deserializeJson(request, server_.arg("plain"))) {
    sendError(400, "Ungültige Anfrage");
    return;
  }
  ProfileStore store;
  if (!profiles::load(store)) {
    sendError(409, "Erst die Konfiguration speichern");
    return;
  }
  const char* action = request["action"] | "";

  if (strcmp(action, "add") == 0) {
    if (store.count >= ProfileStore::kMax) {
      sendError(400, "Höchstens 5 Profile");
      return;
    }
    const char* name = request["name"] | "";
    const char* ssid = request["wifiSsid"] | "";
    const char* password = request["wifiPassword"] | "";
    const bool open = request["wifiOpen"] | false;
    if (strlen(name) >= sizeof(Profile::name) || strlen(ssid) > 32 || strlen(password) > 63) {
      sendError(400, "Angaben zu lang");
      return;
    }
    if (!request["lat"].is<double>() || !request["lon"].is<double>()) {
      sendError(400, "Standort fehlt");
      return;
    }
    for (uint8_t i = 0; i < store.count; ++i) {
      if (strcmp(store.items[i].name, name) == 0) {
        sendError(400, "Profilname schon vergeben");
        return;
      }
    }
    Profile profile{};
    strlcpy(profile.name, name, sizeof(profile.name));
    strlcpy(profile.wifiSsid, ssid, sizeof(profile.wifiSsid));
    if (!open && password[0] == '\0') {
      // Leeres Passwort: gespeichertes Passwort eines Profils mit demselben WLAN übernehmen.
      for (uint8_t i = 0; i < store.count && password[0] == '\0'; ++i) {
        if (strcmp(store.items[i].wifiSsid, ssid) == 0) password = store.items[i].wifiPassword;
      }
      if (password[0] == '\0') {
        sendError(400, "WLAN-Passwort fehlt (oder „offenes Netz“ wählen)");
        return;
      }
    }
    strlcpy(profile.wifiPassword, open ? "" : password, sizeof(profile.wifiPassword));
    profile.latitude = request["lat"].as<double>();
    profile.longitude = request["lon"].as<double>();
    strlcpy(profile.locationName, request["locationName"] | "", sizeof(profile.locationName));
    const char* error = profiles::validate(profile, config_);
    if (error != nullptr) {
      sendError(400, error);
      return;
    }
    store.items[store.count++] = profile;
    if (!profiles::save(store)) {
      sendError(500, "Speichern fehlgeschlagen");
      return;
    }
    Serial.printf("[profile] angelegt: %s\n", profile.name);
    sendJson(200, "{\"ok\":true}");
    return;
  }

  const int index = request["index"] | -1;
  if (index < 0 || index >= store.count) {
    sendError(400, "Profil nicht gefunden");
    return;
  }
  if (strcmp(action, "delete") == 0) {
    if (index == store.active) {
      sendError(400, "Das aktive Profil kann nicht gelöscht werden");
      return;
    }
    if (!profiles::remove(store, index)) {
      sendError(500, "Löschen fehlgeschlagen");
      return;
    }
    sendJson(200, "{\"ok\":true}");
  } else if (strcmp(action, "activate") == 0) {
    const char* error = profiles::validate(store.items[index], config_);
    if (error != nullptr) {
      sendError(400, error);
      return;
    }
    AppConfig updated = config_;
    profiles::applyTo(store.items[index], updated);
    store.active = index;
    if (!app_config::save(updated) || !profiles::save(store)) {
      sendError(500, "Speichern fehlgeschlagen");
      return;
    }
    config_ = updated;
    Serial.printf("[profile] aktiviert: %s\n", store.items[index].name);
    sendJson(200, "{\"ok\":true}");
    restartRequested_ = true;
  } else {
    sendError(400, "Unbekannte Aktion");
  }
}

void SetupPortal::handleOpenSkyTestStart() {
  if (!authorize() || !checkPostRequest()) return;
  if (mode_ != Mode::Lan || opensky_ == nullptr) {
    sendError(409, "Test erst nach dem Speichern im Heimnetz möglich");
    return;
  }
  JsonDocument request;
  if (deserializeJson(request, server_.arg("plain"))) {
    sendError(400, "Ungültige Anfrage");
    return;
  }
  const char* clientId = request["clientId"] | "";
  const char* clientSecret = request["clientSecret"] | "";
  // Leeres Secret bei unveränderter Client-ID: gespeichertes Secret verwenden.
  if (clientSecret[0] == '\0' && strcmp(clientId, config_.openskyClientId) == 0) {
    clientSecret = config_.openskyClientSecret;
  }
  if (clientId[0] == '\0' || clientSecret[0] == '\0') {
    sendError(400, "Client-ID und Client-Secret angeben");
    return;
  }
  opensky_->requestCredentialTest(clientId, clientSecret);
  sendJson(202, "{\"ok\":true}");
}

void SetupPortal::handleOpenSkyTestStatus() {
  if (!authorize()) return;
  JsonDocument doc;
  if (opensky_ == nullptr) {
    doc["state"] = "idle";
  } else {
    switch (opensky_->testState()) {
      case OpenSkyService::TestState::Idle:
        doc["state"] = "idle";
        break;
      case OpenSkyService::TestState::Running:
        doc["state"] = "running";
        break;
      case OpenSkyService::TestState::Succeeded:
        doc["state"] = "ok";
        break;
      case OpenSkyService::TestState::Failed:
        doc["state"] = "failed";
        break;
    }
    doc["message"] = opensky_->testMessage();
  }
  String body;
  serializeJson(doc, body);
  sendJson(200, body);
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

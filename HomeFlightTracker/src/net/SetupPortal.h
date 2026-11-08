// Weboberfläche mit JSON-API für Einrichtung (Setup-AP mit Captive Portal) und spätere
// Konfiguration im Heimnetz (geschützt per HTTP-Digest-Authentifizierung).
#pragma once

#include <DNSServer.h>
#include <WebServer.h>

#include "../config/AppConfig.h"
#include "../opensky/OpenSkyService.h"
#include "WifiManager.h"

class SetupPortal {
 public:
  enum class Mode : uint8_t { Setup, Lan };

  SetupPortal(AppConfig& config, WifiManager& wifi) : config_(config), wifi_(wifi), server_(80) {}

  void begin(Mode mode, const char* firmwareVersion);
  // Im Heimnetz: Dienst für den Test der OpenSky-Zugangsdaten.
  void setOpenSkyService(OpenSkyService* service) { opensky_ = service; }
  void loop();

  // true, sobald eine Konfiguration gespeichert wurde und ein Neustart ansteht.
  bool restartRequested() const { return restartRequested_; }

 private:
  static constexpr char kAuthUser[] = "admin";
  static constexpr char kAuthRealm[] = "HomeFlightTracker";

  bool authorize();
  bool checkPostRequest();
  void sendJson(int code, const String& body);
  void sendError(int code, const char* message);

  void handleRoot();
  void handleState();
  void handleEstimate();
  void handleScan();
  void handleWifiTestStart();
  void handleWifiTestStatus();
  void handleConfig();
  void handleProfiles();
  void handleOpenSkyTestStart();
  void handleOpenSkyTestStatus();
  void handleNotFound();

  AppConfig& config_;
  WifiManager& wifi_;
  OpenSkyService* opensky_ = nullptr;
  WebServer server_;
  DNSServer dns_;
  Mode mode_ = Mode::Lan;
  const char* firmwareVersion_ = "";
  bool restartRequested_ = false;
};

// WLAN: Station-Betrieb mit Reconnect, geschützter Setup-Access-Point, Netzwerksuche und
// Verbindungstest während der Einrichtung.
#pragma once

#include <IPAddress.h>
#include <stdint.h>

class WifiManager {
 public:
  enum class StationState : uint8_t { Off, Connecting, Connected, Disconnected };
  enum class TestState : uint8_t { Idle, Running, Succeeded, Failed };

  static constexpr char kHostname[] = "homeflighttracker";
  static constexpr char kSetupSsid[] = "HomeFlightTracker-Setup";

  // Normalbetrieb: verbindet sich mit dem gespeicherten WLAN und hält die Verbindung.
  void startStation(const char* ssid, const char* password);

  // Setup-Betrieb: WPA2-geschützter Access-Point mit zufälligem Passwort (AP+STA für Scan und Test).
  // Das erzeugte Passwort steht danach in setupPassword().
  bool startAccessPoint();

  void loop();

  StationState stationState() const { return stationState_; }
  IPAddress stationIp() const;
  int8_t rssi() const;
  IPAddress accessPointIp() const;
  uint8_t accessPointClients() const;
  const char* setupPassword() const { return setupPassword_; }

  // Asynchrone Netzwerksuche: liefert -1 solange sie läuft, sonst die Anzahl der Netze.
  int16_t scan();
  void clearScan();

  // Verbindungstest im Setup-Betrieb (blockiert nicht).
  void beginTest(const char* ssid, const char* password);
  TestState testState() const { return testState_; }
  const char* testMessage() const { return testMessage_; }

 private:
  static constexpr uint32_t kReconnectIntervalMs = 30000;
  static constexpr uint32_t kTestTimeoutMs = 20000;

  void generateSetupPassword();
  void updateTest();

  bool stationMode_ = false;
  StationState stationState_ = StationState::Off;
  uint32_t lastAttemptMs_ = 0;
  char ssid_[33] = "";
  char password_[64] = "";

  char setupPassword_[10] = "";
  bool scanRunning_ = false;

  TestState testState_ = TestState::Idle;
  uint32_t testStartMs_ = 0;
  const char* testMessage_ = "";
};

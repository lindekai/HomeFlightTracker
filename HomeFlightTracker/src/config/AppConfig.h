// Anwendungskonfiguration (WLAN, Standort, OpenSky-Zugang, Radar, Weboberfläche) im ESP32-NVS.
// Hinweis: NVS ist ohne aktivierte Flash-Verschlüsselung NICHT verschlüsselt (siehe README, Sicherheit).
#pragma once

#include <stddef.h>
#include <stdint.h>

struct AppConfig {
  char wifiSsid[33] = "";
  char wifiPassword[64] = "";

  bool hasLocation = false;
  double latitude = 0.0;
  double longitude = 0.0;
  char locationName[48] = "";

  char openskyClientId[96] = "";
  char openskyClientSecret[128] = "";

  uint16_t rangeKm = 50;
  uint16_t intervalS = 60;

  char webPassword[64] = "";

  // Energie
  uint16_t dimAfterS = 120;      // Display dimmen nach Inaktivität, 0 = nie
  uint8_t dimPercent = 20;       // Helligkeit im gedimmten Zustand
  uint8_t offAfterMin = 10;      // Display aus nach Inaktivität, 0 = nie
  bool nightEnabled = true;      // Nachtruhe mit Tiefschlaf
  uint16_t nightStartMin = 23 * 60;  // Minuten nach Mitternacht (Ortszeit)
  uint16_t nightEndMin = 6 * 60;

  bool hasOpenskyCredentials() const { return openskyClientId[0] != '\0' && openskyClientSecret[0] != '\0'; }
  // Mindestangaben für den Normalbetrieb.
  bool isComplete() const { return wifiSsid[0] != '\0' && hasLocation && webPassword[0] != '\0'; }
};

namespace app_config {

constexpr uint16_t kAllowedRangesKm[] = {25, 50, 100};
constexpr size_t kMinWebPasswordLength = 8;

bool load(AppConfig& out);
bool save(const AppConfig& config);

// Prüft alle Felder; liefert nullptr bei Erfolg, sonst eine deutsche Fehlermeldung.
const char* validate(const AppConfig& config);

// Fläche der Abfrage-Box in Quadratgrad für Standort und Reichweite.
double queryAreaSquareDegrees(const AppConfig& config);

// Löscht die Anwendungskonfiguration und die Hardware-Einstellungen (Werksreset).
bool factoryReset();

}  // namespace app_config

// Profile aus WLAN und Standort (z. B. „Zuhause“, „Büro“) im ESP32-NVS.
// AppConfig bleibt die Kopie des aktiven Profils; OpenSky-Abruf und Radar lesen nur AppConfig.
#pragma once

#include <stdint.h>

#include "AppConfig.h"

struct Profile {
  char name[25];
  char wifiSsid[33];
  char wifiPassword[64];
  double latitude;
  double longitude;
  char locationName[48];
};

struct ProfileStore {
  static constexpr uint8_t kMax = 5;
  Profile items[kMax];
  uint8_t count = 0;
  int8_t active = -1;
};

namespace profiles {

bool load(ProfileStore& out);  // false = noch keine Profile gespeichert (out ist dann leer)
bool save(const ProfileStore& store);

Profile fromConfig(const AppConfig& config, const char* name);
void applyTo(const Profile& profile, AppConfig& config);

// Übernimmt WLAN und Standort aus config ins aktive Profil. Gibt es noch keine Profile,
// entsteht daraus Profil 1 (Name = Ortsname). Aufruf nach jedem Speichern der Konfiguration.
bool updateActive(const AppConfig& config);

// Prüft, ob die Konfiguration mit diesem Profil zulässig wäre (z. B. Kontingent am anderen Standort).
const char* validate(const Profile& profile, const AppConfig& config);

bool remove(ProfileStore& store, uint8_t index);  // nicht für das aktive Profil
bool clear();

}  // namespace profiles

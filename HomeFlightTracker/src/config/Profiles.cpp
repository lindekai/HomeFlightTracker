#include "Profiles.h"

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

namespace {

constexpr char kNamespace[] = "hft-prof";
constexpr char kKeyStore[] = "store";
constexpr uint8_t kSchemaVersion = 1;

// Gespeichertes Abbild: Version, Anzahl, aktives Profil, Profile.
struct StoredProfiles {
  uint8_t version;
  uint8_t count;
  int8_t active;
  Profile items[ProfileStore::kMax];
};

void terminate(Profile& profile) {
  profile.name[sizeof(profile.name) - 1] = '\0';
  profile.wifiSsid[sizeof(profile.wifiSsid) - 1] = '\0';
  profile.wifiPassword[sizeof(profile.wifiPassword) - 1] = '\0';
  profile.locationName[sizeof(profile.locationName) - 1] = '\0';
}

bool sameProfile(const Profile& a, const Profile& b) {
  return strcmp(a.name, b.name) == 0 && strcmp(a.wifiSsid, b.wifiSsid) == 0 &&
         strcmp(a.wifiPassword, b.wifiPassword) == 0 && a.latitude == b.latitude && a.longitude == b.longitude &&
         strcmp(a.locationName, b.locationName) == 0;
}

}  // namespace

bool profiles::load(ProfileStore& out) {
  out = ProfileStore{};
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return false;
  StoredProfiles stored{};
  const bool ok = prefs.getBytesLength(kKeyStore) == sizeof(stored) &&
                  prefs.getBytes(kKeyStore, &stored, sizeof(stored)) == sizeof(stored);
  prefs.end();
  if (!ok || stored.version != kSchemaVersion || stored.count == 0 || stored.count > ProfileStore::kMax) return false;

  out.count = stored.count;
  out.active = stored.active >= 0 && stored.active < stored.count ? stored.active : 0;
  for (uint8_t i = 0; i < out.count; ++i) {
    out.items[i] = stored.items[i];
    terminate(out.items[i]);
  }
  return true;
}

bool profiles::save(const ProfileStore& store) {
  StoredProfiles stored{};
  stored.version = kSchemaVersion;
  stored.count = store.count;
  stored.active = store.active;
  for (uint8_t i = 0; i < store.count && i < ProfileStore::kMax; ++i) stored.items[i] = store.items[i];
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  const bool ok = prefs.putBytes(kKeyStore, &stored, sizeof(stored)) == sizeof(stored);
  prefs.end();
  return ok;
}

Profile profiles::fromConfig(const AppConfig& config, const char* name) {
  Profile profile{};
  strlcpy(profile.name, name, sizeof(profile.name));
  strlcpy(profile.wifiSsid, config.wifiSsid, sizeof(profile.wifiSsid));
  strlcpy(profile.wifiPassword, config.wifiPassword, sizeof(profile.wifiPassword));
  profile.latitude = config.latitude;
  profile.longitude = config.longitude;
  strlcpy(profile.locationName, config.locationName, sizeof(profile.locationName));
  return profile;
}

void profiles::applyTo(const Profile& profile, AppConfig& config) {
  strlcpy(config.wifiSsid, profile.wifiSsid, sizeof(config.wifiSsid));
  strlcpy(config.wifiPassword, profile.wifiPassword, sizeof(config.wifiPassword));
  config.hasLocation = true;
  config.latitude = profile.latitude;
  config.longitude = profile.longitude;
  strlcpy(config.locationName, profile.locationName, sizeof(config.locationName));
}

bool profiles::updateActive(const AppConfig& config) {
  if (!config.isComplete()) return false;
  ProfileStore store;
  if (!load(store)) {
    store.items[0] = fromConfig(config, config.locationName[0] != '\0' ? config.locationName : "Profil 1");
    store.count = 1;
    store.active = 0;
    Serial.printf("[profile] Profil 1 aus der Konfiguration angelegt: %s\n", store.items[0].name);
    return save(store);
  }
  Profile& active = store.items[store.active];
  const Profile updated = fromConfig(config, active.name);
  if (sameProfile(updated, active)) return true;
  active = updated;
  return save(store);
}

const char* profiles::validate(const Profile& profile, const AppConfig& config) {
  if (profile.name[0] == '\0') return "Profilname fehlt";
  AppConfig candidate = config;
  applyTo(profile, candidate);
  return app_config::validate(candidate);
}

bool profiles::remove(ProfileStore& store, uint8_t index) {
  if (index >= store.count || index == store.active) return false;
  for (uint8_t i = index; i + 1 < store.count; ++i) store.items[i] = store.items[i + 1];
  --store.count;
  if (store.active > index) --store.active;
  return save(store);
}

bool profiles::clear() {
  Preferences prefs;
  const bool ok = prefs.begin(kNamespace, false) && prefs.clear();
  prefs.end();
  return ok;
}

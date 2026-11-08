#include "AppConfig.h"

#include <Preferences.h>
#include <string.h>

#include "../geo/Geo.h"
#include "../opensky/Quota.h"
#include "HardwareSettings.h"

namespace {

constexpr char kNamespace[] = "hft-cfg";
constexpr uint8_t kSchemaVersion = 1;

constexpr char kKeyVersion[] = "ver";
constexpr char kKeyWifiSsid[] = "wifi_ssid";
constexpr char kKeyWifiPassword[] = "wifi_pass";
constexpr char kKeyHasLocation[] = "has_loc";
constexpr char kKeyLatitude[] = "lat";
constexpr char kKeyLongitude[] = "lon";
constexpr char kKeyLocationName[] = "loc_name";
constexpr char kKeyClientId[] = "os_id";
constexpr char kKeyClientSecret[] = "os_secret";
constexpr char kKeyRange[] = "range_km";
constexpr char kKeyInterval[] = "interval_s";
constexpr char kKeyWebPassword[] = "web_pass";
constexpr char kKeyDimAfter[] = "dim_after";
constexpr char kKeyDimPercent[] = "dim_pct";
constexpr char kKeyOffAfter[] = "off_after";
constexpr char kKeyNightEnabled[] = "night_on";
constexpr char kKeyNightStart[] = "night_start";
constexpr char kKeyNightEnd[] = "night_end";

void readString(Preferences& prefs, const char* key, char* target, size_t size) {
  target[0] = '\0';
  if (prefs.isKey(key)) prefs.getString(key, target, size);
}

bool isAllowedRange(uint16_t rangeKm) {
  for (uint16_t allowed : app_config::kAllowedRangesKm) {
    if (allowed == rangeKm) return true;
  }
  return false;
}

}  // namespace

bool app_config::load(AppConfig& out) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return false;
  if (prefs.getUChar(kKeyVersion, 0) != kSchemaVersion) {
    prefs.end();
    return false;
  }

  AppConfig config;
  readString(prefs, kKeyWifiSsid, config.wifiSsid, sizeof(config.wifiSsid));
  readString(prefs, kKeyWifiPassword, config.wifiPassword, sizeof(config.wifiPassword));
  config.hasLocation = prefs.getBool(kKeyHasLocation, false);
  config.latitude = prefs.getDouble(kKeyLatitude, 0.0);
  config.longitude = prefs.getDouble(kKeyLongitude, 0.0);
  readString(prefs, kKeyLocationName, config.locationName, sizeof(config.locationName));
  readString(prefs, kKeyClientId, config.openskyClientId, sizeof(config.openskyClientId));
  readString(prefs, kKeyClientSecret, config.openskyClientSecret, sizeof(config.openskyClientSecret));
  config.rangeKm = prefs.getUShort(kKeyRange, config.rangeKm);
  config.intervalS = prefs.getUShort(kKeyInterval, config.intervalS);
  readString(prefs, kKeyWebPassword, config.webPassword, sizeof(config.webPassword));
  // Energie-Einstellungen kamen nach Schema 1 hinzu; fehlende Schlüssel liefern die Standardwerte.
  config.dimAfterS = prefs.getUShort(kKeyDimAfter, config.dimAfterS);
  config.dimPercent = prefs.getUChar(kKeyDimPercent, config.dimPercent);
  config.offAfterMin = prefs.getUChar(kKeyOffAfter, config.offAfterMin);
  config.nightEnabled = prefs.getBool(kKeyNightEnabled, config.nightEnabled);
  config.nightStartMin = prefs.getUShort(kKeyNightStart, config.nightStartMin);
  config.nightEndMin = prefs.getUShort(kKeyNightEnd, config.nightEndMin);
  prefs.end();

  out = config;
  return true;
}

bool app_config::save(const AppConfig& config) {
  if (validate(config) != nullptr) return false;
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  bool ok = true;
  ok &= prefs.putString(kKeyWifiSsid, config.wifiSsid) == strlen(config.wifiSsid);
  ok &= prefs.putString(kKeyWifiPassword, config.wifiPassword) == strlen(config.wifiPassword);
  ok &= prefs.putBool(kKeyHasLocation, config.hasLocation) > 0;
  ok &= prefs.putDouble(kKeyLatitude, config.latitude) > 0;
  ok &= prefs.putDouble(kKeyLongitude, config.longitude) > 0;
  ok &= prefs.putString(kKeyLocationName, config.locationName) == strlen(config.locationName);
  ok &= prefs.putString(kKeyClientId, config.openskyClientId) == strlen(config.openskyClientId);
  ok &= prefs.putString(kKeyClientSecret, config.openskyClientSecret) == strlen(config.openskyClientSecret);
  ok &= prefs.putUShort(kKeyRange, config.rangeKm) > 0;
  ok &= prefs.putUShort(kKeyInterval, config.intervalS) > 0;
  ok &= prefs.putString(kKeyWebPassword, config.webPassword) == strlen(config.webPassword);
  ok &= prefs.putUShort(kKeyDimAfter, config.dimAfterS) > 0;
  ok &= prefs.putUChar(kKeyDimPercent, config.dimPercent) > 0;
  ok &= prefs.putUChar(kKeyOffAfter, config.offAfterMin) > 0;
  ok &= prefs.putBool(kKeyNightEnabled, config.nightEnabled) > 0;
  ok &= prefs.putUShort(kKeyNightStart, config.nightStartMin) > 0;
  ok &= prefs.putUShort(kKeyNightEnd, config.nightEndMin) > 0;
  // Version zuletzt schreiben: ein unterbrochener Speichervorgang bleibt als „nicht konfiguriert“ erkennbar.
  ok &= prefs.putUChar(kKeyVersion, kSchemaVersion) > 0;
  prefs.end();
  return ok;
}

const char* app_config::validate(const AppConfig& config) {
  if (config.wifiSsid[0] == '\0') return "WLAN-Name fehlt";
  const size_t wifiPasswordLength = strlen(config.wifiPassword);
  if (wifiPasswordLength > 0 && wifiPasswordLength < 8) return "WLAN-Passwort muss mindestens 8 Zeichen haben";
  if (!config.hasLocation) return "Standort fehlt";
  if (!geo::isValid({config.latitude, config.longitude})) return "Koordinaten ungültig (Breite -90…90, Länge -180…180)";
  if (!isAllowedRange(config.rangeKm)) return "Reichweite nicht zulässig";
  const bool authenticated = config.hasOpenskyCredentials();
  if ((config.openskyClientId[0] == '\0') != (config.openskyClientSecret[0] == '\0')) {
    return "OpenSky: Client-ID und Client-Secret nur gemeinsam angeben";
  }
  if (!opensky::isIntervalAllowed(config.intervalS, queryAreaSquareDegrees(config), authenticated)) {
    return authenticated ? "Abrufintervall überschreitet das OpenSky-Tageskontingent"
                         : "Ohne OpenSky-Zugang ist ein Intervall von mindestens 300 s nötig";
  }
  if (strlen(config.webPassword) < kMinWebPasswordLength) return "Geräte-Passwort muss mindestens 8 Zeichen haben";
  if (config.dimAfterS > 3600) return "Dimmen: höchstens 3600 s";
  if (config.dimPercent < 5 || config.dimPercent > 100) return "Helligkeit gedimmt: 5 bis 100 %";
  if (config.offAfterMin > 240) return "Display aus: höchstens 240 min";
  if (config.nightStartMin >= 24 * 60 || config.nightEndMin >= 24 * 60) return "Nachtruhe: ungültige Uhrzeit";
  if (config.nightEnabled && config.nightStartMin == config.nightEndMin) return "Nachtruhe: Beginn und Ende gleich";
  return nullptr;
}

double app_config::queryAreaSquareDegrees(const AppConfig& config) {
  const geo::BoundingBox box =
      geo::boundingBox({config.latitude, config.longitude}, config.rangeKm * opensky::kQueryRadiusFactor);
  return box.areaSquareDegrees();
}

bool app_config::factoryReset() {
  Preferences prefs;
  bool ok = prefs.begin(kNamespace, false) && prefs.clear();
  prefs.end();
  ok &= hardware_settings::clearAll();
  return ok;
}

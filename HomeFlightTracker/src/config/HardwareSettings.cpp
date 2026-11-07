#include "HardwareSettings.h"

#include <Preferences.h>

namespace {

constexpr char kNamespace[] = "hft-hw";
constexpr char kKeyRotation[] = "rotation";
constexpr char kKeyTouchCalibration[] = "touch_cal";

constexpr uint8_t kTouchCalibrationVersion = 1;
constexpr uint16_t kRawMax = 4095;     // XPT2046 liefert 12-Bit-Rohwerte
constexpr uint16_t kMinRawSpan = 500;  // kleinere Spannen deuten auf eine fehlerhafte Kalibrierung hin

struct StoredTouchCalibration {
  uint8_t version;
  uint8_t rotation;
  uint16_t data[5];
};

}  // namespace

uint8_t hardware_settings::loadRotation() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return kDefaultRotation;
  const uint8_t rotation = prefs.getUChar(kKeyRotation, kDefaultRotation);
  prefs.end();
  return rotation <= 3 ? rotation : kDefaultRotation;
}

bool hardware_settings::saveRotation(uint8_t rotation) {
  if (rotation > 3) return false;
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  const bool ok = prefs.putUChar(kKeyRotation, rotation) == sizeof(uint8_t);
  prefs.end();
  return ok;
}

bool hardware_settings::loadTouchCalibration(TouchCalibration& out) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return false;
  StoredTouchCalibration stored{};
  const size_t length = prefs.getBytes(kKeyTouchCalibration, &stored, sizeof(stored));
  prefs.end();
  if (length != sizeof(stored) || stored.version != kTouchCalibrationVersion) return false;

  TouchCalibration calibration;
  calibration.rotation = stored.rotation;
  for (int i = 0; i < 5; ++i) calibration.data[i] = stored.data[i];
  if (!isPlausible(calibration)) return false;

  out = calibration;
  return true;
}

bool hardware_settings::saveTouchCalibration(const TouchCalibration& calibration) {
  if (!isPlausible(calibration)) return false;
  StoredTouchCalibration stored{};
  stored.version = kTouchCalibrationVersion;
  stored.rotation = calibration.rotation;
  for (int i = 0; i < 5; ++i) stored.data[i] = calibration.data[i];

  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  const bool ok = prefs.putBytes(kKeyTouchCalibration, &stored, sizeof(stored)) == sizeof(stored);
  prefs.end();
  return ok;
}

bool hardware_settings::isPlausible(const TouchCalibration& calibration) {
  if (calibration.rotation > 3) return false;
  const uint32_t x0 = calibration.data[0];
  const uint32_t xSpan = calibration.data[1];
  const uint32_t y0 = calibration.data[2];
  const uint32_t ySpan = calibration.data[3];
  if (xSpan < kMinRawSpan || ySpan < kMinRawSpan) return false;
  if (x0 + xSpan > kRawMax || y0 + ySpan > kRawMax) return false;
  return calibration.data[4] <= 0x07;
}

bool hardware_settings::clearAll() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  const bool ok = prefs.clear();
  prefs.end();
  return ok;
}

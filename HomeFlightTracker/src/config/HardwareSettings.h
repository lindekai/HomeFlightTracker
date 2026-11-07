// Dauerhaft gespeicherte Hardware-Einstellungen (ESP32 NVS über Preferences):
// Display-Rotation und Touch-Kalibrierung. Die Kalibrierung gilt nur für die Rotation,
// in der sie ermittelt wurde.
#pragma once

#include <stdint.h>

// Kalibrierwerte im Format von TFT_eSPI::calibrateTouch():
// data[0] = x0, data[1] = x-Spanne, data[2] = y0, data[3] = y-Spanne,
// data[4] = Flags (Bit 0 Achsentausch, Bit 1 x invertiert, Bit 2 y invertiert)
struct TouchCalibration {
  uint8_t rotation = 0;
  uint16_t data[5] = {};
};

namespace hardware_settings {

constexpr uint8_t kDefaultRotation = 0;  // Hochformat

uint8_t loadRotation();
bool saveRotation(uint8_t rotation);

bool loadTouchCalibration(TouchCalibration& out);
bool saveTouchCalibration(const TouchCalibration& calibration);
bool isPlausible(const TouchCalibration& calibration);

// Löscht Rotation und Kalibrierung (Recovery).
bool clearAll();

}  // namespace hardware_settings

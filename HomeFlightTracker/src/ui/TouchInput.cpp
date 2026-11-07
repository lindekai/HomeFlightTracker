#include "TouchInput.h"

#include "../board/BoardPins.h"

void TouchInput::begin() {
  pinMode(board::kPinTouchIrq, INPUT_PULLUP);
}

bool TouchInput::applyStoredCalibration(uint8_t rotation) {
  TouchCalibration stored;
  if (!hardware_settings::loadTouchCalibration(stored) || stored.rotation != rotation) return false;
  calibration_ = stored;
  tft_.setTouch(calibration_.data);
  return true;
}

void TouchInput::calibrate(uint8_t rotation) {
  waitForRelease();
  tft_.setRotation(rotation);

  for (;;) {
    const int16_t cx = tft_.width() / 2;
    const int16_t cy = tft_.height() / 2;
    tft_.fillScreen(TFT_BLACK);
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextColor(TFT_WHITE, TFT_BLACK);
    tft_.drawString("Kalibrierung", cx, cy - 30, 4);
    tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft_.drawString("Die markierte Ecke", cx, cy + 5, 2);
    tft_.drawString("jeweils genau antippen", cx, cy + 25, 2);

    TouchCalibration candidate;
    candidate.rotation = rotation;
    tft_.calibrateTouch(candidate.data, TFT_MAGENTA, TFT_BLACK, 15);
    waitForRelease();

    if (hardware_settings::isPlausible(candidate)) {
      calibration_ = candidate;
      tft_.setTouch(calibration_.data);
      if (!hardware_settings::saveTouchCalibration(calibration_)) {
        Serial.println("[touch] Kalibrierung konnte nicht gespeichert werden");
      }
      Serial.printf("[touch] Kalibrierung R%u: %u %u %u %u 0x%02X\n", rotation, calibration_.data[0],
                    calibration_.data[1], calibration_.data[2], calibration_.data[3], calibration_.data[4]);
      return;
    }

    Serial.println("[touch] Kalibrierung unplausibel, Wiederholung");
    tft_.fillScreen(TFT_BLACK);
    tft_.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft_.drawString("Unplausibel -", cx, cy - 10, 4);
    tft_.drawString("bitte wiederholen", cx, cy + 20, 2);
    delay(1500);
  }
}

TouchState TouchInput::poll() {
  TouchState state;
  uint16_t x = 0;
  uint16_t y = 0;
  const bool touched = tft_.getTouch(&x, &y, kPressThreshold);

  if (touched) {
    lastX_ = x;
    lastY_ = y;
    emptySamples_ = 0;
    state.pressed = !down_;
    down_ = true;
  } else if (down_ && ++emptySamples_ >= kReleaseSamples) {
    down_ = false;
    state.released = true;
  }

  state.down = down_;
  state.x = lastX_;
  state.y = lastY_;
  return state;
}

bool TouchInput::isRawTouched() {
  return tft_.getTouchRawZ() > kPressThreshold;
}

void TouchInput::waitForRelease() {
  uint8_t quiet = 0;
  while (quiet < 5) {
    quiet = isRawTouched() ? 0 : quiet + 1;
    delay(10);
  }
  down_ = false;
  emptySamples_ = 0;
}

bool TouchInput::irqActive() const {
  return digitalRead(board::kPinTouchIrq) == LOW;
}

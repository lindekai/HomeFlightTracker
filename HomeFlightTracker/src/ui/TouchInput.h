// Touch-Eingabe (XPT2046 über TFT_eSPI): Kalibrierung, Persistenz und Ereigniserkennung.
#pragma once

#include <TFT_eSPI.h>

#include "../config/HardwareSettings.h"

struct TouchState {
  bool down = false;      // aktuell berührt
  bool pressed = false;   // in diesem Aufruf neu berührt
  bool released = false;  // in diesem Aufruf losgelassen
  uint16_t x = 0;         // kalibrierte Bildschirmkoordinaten (letzter gültiger Wert)
  uint16_t y = 0;
};

class TouchInput {
 public:
  explicit TouchInput(TFT_eSPI& tft) : tft_(tft) {}

  void begin();

  // Übernimmt eine gespeicherte Kalibrierung, wenn sie zur Rotation passt.
  bool applyStoredCalibration(uint8_t rotation);

  // Interaktive Eck-Kalibrierung in der angegebenen Rotation; speichert das Ergebnis.
  void calibrate(uint8_t rotation);

  TouchState poll();

  // Druckmessung ohne Kalibrierung, z. B. für die Recovery beim Start.
  bool isRawTouched();
  void waitForRelease();

  // Pegel der PENIRQ-Leitung (nur zur Hardwarediagnose).
  bool irqActive() const;

  const TouchCalibration& calibration() const { return calibration_; }

 private:
  static constexpr uint16_t kPressThreshold = 600;  // Standard-Schwelle von TFT_eSPI::getTouch()
  static constexpr uint8_t kReleaseSamples = 3;     // aufeinanderfolgende Leer-Messungen bis „losgelassen“

  TFT_eSPI& tft_;
  TouchCalibration calibration_;
  bool down_ = false;
  uint8_t emptySamples_ = 0;
  uint16_t lastX_ = 0;
  uint16_t lastY_ = 0;
};

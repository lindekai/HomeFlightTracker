// Energiesparen: Hintergrundbeleuchtung per PWM dimmen und abschalten, Nachtruhe mit Tiefschlaf.
// Im Tiefschlaf weckt eine Berührung (PENIRQ des Touchcontrollers an GPIO27) oder der Timer zum
// Ende der Nachtruhe. Nach dem Aufwachen startet die Firmware neu.
#pragma once

#include <TFT_eSPI.h>
#include <time.h>

#include "../config/AppConfig.h"

class PowerManager {
 public:
  enum class Backlight : uint8_t { Full, Dimmed, Off };

  // Früh in setup(): Pin-Haltefunktionen aus dem Tiefschlaf lösen.
  static void releaseSleepHolds();
  static bool wokeFromDeepSleep();
  static bool wokeByTouch();

  void begin(const AppConfig& config);

  // Bei jeder Berührung aufrufen. Liefert true, wenn das Display dabei nur geweckt wurde
  // (die Berührung soll dann keine Aktion auslösen).
  bool onTouch();

  // Dimmen/Abschalten nach Inaktivität; in der Nachtruhe nach der Wachphase Tiefschlaf.
  void loop(TFT_eSPI& tft);

  bool nightActive() const;
  uint32_t secondsUntilSleep() const;
  Backlight backlight() const { return backlight_; }

 private:
  static constexpr uint32_t kPwmFrequencyHz = 5000;
  static constexpr uint8_t kPwmBits = 8;
  static constexpr uint32_t kNightAwakeMs = 120000;  // Wachphase nach Berührung in der Nachtruhe

  void setBrightness(uint8_t percent);
  void setBacklight(Backlight state);
  [[noreturn]] void enterDeepSleep(TFT_eSPI& tft, uint32_t seconds);

  const AppConfig* config_ = nullptr;
  bool pwmReady_ = false;
  Backlight backlight_ = Backlight::Full;
  uint32_t lastActivityMs_ = 0;
};

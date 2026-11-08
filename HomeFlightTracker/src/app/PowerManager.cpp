#include "PowerManager.h"

#include <WiFi.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include "../board/BoardPins.h"
#include "../net/TimeSync.h"
#include "NightSchedule.h"

namespace {

constexpr uint8_t kIliDisplayOff = 0x28;
constexpr uint8_t kIliSleepIn = 0x10;

// Pins, deren Pegel im Tiefschlaf gehalten werden muss: Backlight aus (PNP, HIGH = aus),
// Piezo stumm, beide Chip-Selects inaktiv.
const gpio_num_t kHeldPins[] = {
    static_cast<gpio_num_t>(board::kPinTftBacklight),
    static_cast<gpio_num_t>(board::kPinBeeper),
    static_cast<gpio_num_t>(board::kPinTftCs),
    static_cast<gpio_num_t>(board::kPinTouchCs),
};

uint32_t localSecondOfDay() {
  const time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  return local.tm_hour * 3600UL + local.tm_min * 60UL + local.tm_sec;
}

}  // namespace

void PowerManager::releaseSleepHolds() {
  for (gpio_num_t pin : kHeldPins) gpio_hold_dis(pin);
  gpio_deep_sleep_hold_dis();
}

bool PowerManager::wokeFromDeepSleep() {
  return esp_reset_reason() == ESP_RST_DEEPSLEEP;
}

bool PowerManager::wokeByTouch() {
  return wokeFromDeepSleep() && esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
}

void PowerManager::begin(const AppConfig& config) {
  config_ = &config;
  // TFT_eSPI hat den Pin bereits als Ausgang auf LOW (= an) gesetzt; ab jetzt steuert die PWM.
  pwmReady_ = ledcAttach(board::kPinTftBacklight, kPwmFrequencyHz, kPwmBits);
  lastActivityMs_ = millis();
  setBacklight(Backlight::Full);
  Serial.printf("[power] Dimmen nach %u s auf %u %%, aus nach %u min, Nachtruhe %s %02u:%02u-%02u:%02u\n",
                config.dimAfterS, config.dimPercent, config.offAfterMin, config.nightEnabled ? "an" : "aus",
                config.nightStartMin / 60, config.nightStartMin % 60, config.nightEndMin / 60,
                config.nightEndMin % 60);
}

bool PowerManager::onTouch() {
  lastActivityMs_ = millis();
  if (backlight_ == Backlight::Full) return false;
  setBacklight(Backlight::Full);
  return true;
}

void PowerManager::loop(TFT_eSPI& tft) {
  if (config_ == nullptr) return;
  const uint32_t idleMs = millis() - lastActivityMs_;

  if (nightActive() && idleMs >= kNightAwakeMs) {
    enterDeepSleep(tft, night_schedule::secondsUntil(localSecondOfDay(), config_->nightEndMin));
  }

  Backlight target = Backlight::Full;
  if (config_->offAfterMin > 0 && idleMs >= config_->offAfterMin * 60000UL) {
    target = Backlight::Off;
  } else if (config_->dimAfterS > 0 && idleMs >= config_->dimAfterS * 1000UL) {
    target = Backlight::Dimmed;
  }
  if (target != backlight_) setBacklight(target);
}

bool PowerManager::nightActive() const {
  return config_ != nullptr && config_->nightEnabled && time_sync::isSynced() &&
         night_schedule::isInWindow(localSecondOfDay(), config_->nightStartMin, config_->nightEndMin);
}

uint32_t PowerManager::secondsUntilSleep() const {
  const uint32_t idleMs = millis() - lastActivityMs_;
  return idleMs >= kNightAwakeMs ? 0 : (kNightAwakeMs - idleMs) / 1000;
}

void PowerManager::setBrightness(uint8_t percent) {
  if (!pwmReady_) {
    digitalWrite(board::kPinTftBacklight, percent > 0 ? LOW : HIGH);
    return;
  }
  // Aktiv LOW: Tastgrad = Anteil „aus“. 2^Bits entspricht dauerhaft HIGH.
  const uint32_t full = 1UL << kPwmBits;
  ledcWrite(board::kPinTftBacklight, full - full * percent / 100);
}

void PowerManager::setBacklight(Backlight state) {
  backlight_ = state;
  switch (state) {
    case Backlight::Full:
      setBrightness(100);
      break;
    case Backlight::Dimmed:
      setBrightness(config_ != nullptr ? config_->dimPercent : 20);
      break;
    case Backlight::Off:
      setBrightness(0);
      break;
  }
}

void PowerManager::enterDeepSleep(TFT_eSPI& tft, uint32_t seconds) {
  Serial.printf("[power] Nachtruhe: Tiefschlaf fuer %lu min\n", static_cast<unsigned long>(seconds / 60));
  Serial.flush();

  // Display schlafen legen, Funk aus.
  tft.writecommand(kIliDisplayOff);
  tft.writecommand(kIliSleepIn);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // Pegel festlegen und über den Tiefschlaf halten.
  if (pwmReady_) ledcDetach(board::kPinTftBacklight);
  pinMode(board::kPinTftBacklight, OUTPUT);
  digitalWrite(board::kPinTftBacklight, HIGH);
  digitalWrite(board::kPinBeeper, LOW);
  digitalWrite(board::kPinTftCs, HIGH);
  digitalWrite(board::kPinTouchCs, HIGH);
  for (gpio_num_t pin : kHeldPins) gpio_hold_en(pin);
  gpio_deep_sleep_hold_en();

  // Wecken: Ende der Nachtruhe per Timer, vorher per Berührung (PENIRQ aktiv LOW).
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
  const bool irqIdle = digitalRead(board::kPinTouchIrq) == HIGH;
  const gpio_num_t irq = static_cast<gpio_num_t>(board::kPinTouchIrq);
  rtc_gpio_pullup_en(irq);
  rtc_gpio_pulldown_dis(irq);
  if (irqIdle) {
    esp_sleep_enable_ext0_wakeup(irq, 0);
  } else {
    // Leitung bereits LOW (Berührung oder Störung): nur Timer, sonst sofortiges Wiederaufwachen.
    Serial.println("[power] Touch-IRQ ist LOW - Wecken nur per Timer");
  }
  esp_deep_sleep_start();
}

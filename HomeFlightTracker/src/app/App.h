// Ablaufsteuerung des Geräts: Start mit Recovery, Setup-Modus (Access-Point + Captive Portal)
// und Normalbetrieb (WLAN, Weboberfläche im Heimnetz, Statusanzeige). Das Radar folgt in M4.
#pragma once

#include <TFT_eSPI.h>

#include "../config/AppConfig.h"
#include "../net/SetupPortal.h"
#include "../net/WifiManager.h"
#include "../ui/TouchInput.h"

class App {
 public:
  explicit App(TFT_eSPI& tft) : tft_(tft), touch_(tft), portal_(config_, wifi_) {}

  void begin(bool setupRequested);
  void loop();

 private:
  enum class Mode : uint8_t { Setup, Normal };
  enum class Screen : uint8_t { Status, ConfirmSetup, Setup, Message };

  static constexpr uint32_t kRecoveryWindowMs = 2500;
  static constexpr uint32_t kFactoryResetHoldMs = 8000;
  static constexpr uint32_t kSetupTimeoutMs = 15UL * 60UL * 1000UL;
  static constexpr uint32_t kRefreshIntervalMs = 1000;
  static constexpr uint32_t kRestartDelayMs = 2000;

  // Startbildschirm mit Recovery: kurz antippen = Touch neu kalibrieren,
  // 8 s halten = Werksreset. Liefert true, wenn neu kalibriert werden soll.
  bool runBootRecovery();
  void runFactoryReset();

  void startSetupMode();
  void startNormalMode();

  void showScreen(Screen screen);
  void drawStatusScreen();
  void updateStatusScreen();
  void drawSetupScreen();
  void updateSetupScreen();
  void drawConfirmSetupScreen();

  void handleTouch(const TouchState& touch);
  void handleSerial();
  void scheduleRestart(const char* title, const char* line);

  TFT_eSPI& tft_;
  TouchInput touch_;
  AppConfig config_;
  WifiManager wifi_;
  SetupPortal portal_;

  Mode mode_ = Mode::Normal;
  Screen screen_ = Screen::Status;
  uint8_t rotation_ = 0;
  bool mdnsStarted_ = false;
  uint32_t setupStartedMs_ = 0;
  uint32_t lastRefreshMs_ = 0;
  uint32_t restartAtMs_ = 0;
};

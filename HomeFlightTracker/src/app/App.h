// Ablaufsteuerung des Geräts: Start mit Recovery, Setup-Modus (Access-Point + Captive Portal)
// und Normalbetrieb (WLAN, OpenSky-Abruf im Hintergrund, Radar, Details, Liste, Menü).
#pragma once

#include <TFT_eSPI.h>

#include "../config/AppConfig.h"
#include "../model/FlightList.h"
#include "../net/SetupPortal.h"
#include "../net/WifiManager.h"
#include "../opensky/OpenSkyService.h"
#include "../ui/RadarView.h"
#include "../ui/TouchInput.h"
#include "PowerManager.h"

class App {
 public:
  explicit App(TFT_eSPI& tft) : tft_(tft), touch_(tft), portal_(config_, wifi_), radar_(tft) {}

  void begin(bool setupRequested);
  void loop();

 private:
  enum class Mode : uint8_t { Setup, Normal };
  enum class Screen : uint8_t { Radar, Details, List, Menu, ConfirmSetup, Setup, Message };

  static constexpr uint32_t kRecoveryWindowMs = 2500;
  static constexpr uint32_t kFactoryResetHoldMs = 8000;
  static constexpr uint32_t kSetupTimeoutMs = 15UL * 60UL * 1000UL;
  static constexpr uint32_t kRefreshIntervalMs = 1000;
  static constexpr uint32_t kRestartDelayMs = 2000;
  static constexpr uint32_t kNoticeDurationMs = 4000;
  static constexpr uint8_t kListRowsPerPage = 7;

  // Startbildschirm mit Recovery: kurz antippen = Touch neu kalibrieren,
  // 8 s halten = Werksreset. Liefert true, wenn neu kalibriert werden soll.
  bool runBootRecovery();
  void runFactoryReset();

  void startSetupMode();
  void startNormalMode();

  void showScreen(Screen screen);
  void refresh();
  bool updateFlights();
  RadarView::DataState dataState(const OpenSkyService::Status& status) const;

  void drawTopBar(const OpenSkyService::Status& status);
  void drawRadar();
  void drawRadarFooter(const OpenSkyService::Status& status);
  void drawDetails();
  void updateDetailsAge();
  void drawList();
  void drawMenu();
  void updateMenu();
  void drawSetupScreen();
  void updateSetupScreen();
  void drawConfirmSetupScreen();

  void handleTouch(const TouchState& touch);
  void handleRadarTouch(uint16_t x, uint16_t y);
  void changeRange(int8_t direction);
  void showNotice(const char* text);
  void handleSerial();
  void scheduleRestart(const char* title, const char* line);
  const Aircraft* findSelected() const;

  TFT_eSPI& tft_;
  TouchInput touch_;
  AppConfig config_;
  WifiManager wifi_;
  SetupPortal portal_;
  OpenSkyService service_;
  RadarView radar_;
  PowerManager power_;

  FlightList flights_;
  uint32_t flightsSequence_ = 0;
  char selectedIcao24_[7] = "";
  char labelIcao24_[7] = "";  // Flugzeug mit Kurzinfo im zuletzt gezeichneten Radar
  Aircraft detail_{};
  bool detailInData_ = false;
  uint8_t listPage_ = 0;
  RadarView::DataState lastDataState_ = RadarView::DataState::Waiting;
  char notice_[48] = "";
  uint32_t noticeUntilMs_ = 0;

  Mode mode_ = Mode::Normal;
  Screen screen_ = Screen::Radar;
  uint8_t rotation_ = 0;
  bool mdnsStarted_ = false;
  uint32_t setupStartedMs_ = 0;
  uint32_t lastRefreshMs_ = 0;
  uint32_t restartAtMs_ = 0;
};

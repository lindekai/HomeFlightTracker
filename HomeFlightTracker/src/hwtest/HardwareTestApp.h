// Hardware-Machbarkeitstest (Meilenstein M1): Testbild und Rotation, Hintergrundbeleuchtung,
// Touch-Kalibrierung mit Trefferzonen sowie Heap-Basiswerte inkl. Sprite-Machbarkeit.
// Wird ab M4 durch die eigentliche Radar-Oberfläche ersetzt.
#pragma once

#include <TFT_eSPI.h>

#include "../ui/TouchInput.h"

class HardwareTestApp {
 public:
  explicit HardwareTestApp(TFT_eSPI& tft) : tft_(tft), touch_(tft) {}

  void begin();
  void loop();

 private:
  enum class Screen : uint8_t { TestPattern, TouchTest, Info };

  struct SpriteProbe {
    uint8_t colorDepth = 0;
    bool ok = false;
    uint32_t bytes = 0;  // vom Sprite belegter Heap
  };

  static constexpr int16_t kBarHeight = 40;
  static constexpr int16_t kCellSize = 80;
  static constexpr int16_t kRadarSpriteSize = 240;
  static constexpr uint32_t kHeapLogIntervalMs = 60000;
  static constexpr uint32_t kRecoveryWindowMs = 2500;

  bool verifyLibrarySetup();
  bool showBootScreenAndCheckRecovery();

  void showScreen(Screen screen);
  void drawTestPattern();
  void drawTouchTest();
  void drawTouchGrid(int8_t highlightedCell);
  void drawTouchStatus();
  void drawInfo();
  void drawInfoValues();
  void drawButtonBar(const char* const* labels, uint8_t count);

  void handleTouch(const TouchState& touch);
  void handleTouchTest(const TouchState& touch);
  void handleSerial();
  void printHelp();
  void printCalibration();

  void rotate();
  void recalibrate();
  void testBacklight();
  SpriteProbe probeSprite(uint8_t colorDepth);

  int16_t gridRows() const;
  int8_t cellAt(uint16_t x, uint16_t y) const;
  int8_t buttonAt(uint16_t x, uint16_t y, uint8_t count) const;

  TFT_eSPI& tft_;
  TouchInput touch_;
  Screen screen_ = Screen::TestPattern;
  uint8_t rotation_ = 0;
  bool librarySetupOk_ = false;
  SpriteProbe sprite8_;
  SpriteProbe sprite16_;
  uint32_t lastHeapLogMs_ = 0;
  uint32_t lastInfoRefreshMs_ = 0;
  uint32_t lastStatusMs_ = 0;
};

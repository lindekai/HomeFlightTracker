#include "HardwareTestApp.h"

#include <Esp.h>

#include "../board/BoardPins.h"
#include "../config/HardwareSettings.h"
#include "../diag/HeapReport.h"

namespace {

const char* const kTouchTestButtons[] = {"Testbild", "Kalibr.", "Drehen", "Info"};
const char* const kInfoButtons[] = {"Touch", "Licht", "Neustart"};
constexpr uint8_t kTouchTestButtonCount = sizeof(kTouchTestButtons) / sizeof(kTouchTestButtons[0]);
constexpr uint8_t kInfoButtonCount = sizeof(kInfoButtons) / sizeof(kInfoButtons[0]);

}  // namespace

void HardwareTestApp::begin() {
  printHeapSnapshot(Serial, "Start");
  tft_.init();  // schaltet über TFT_BL auch die Hintergrundbeleuchtung ein
  touch_.begin();
  librarySetupOk_ = verifyLibrarySetup();

  rotation_ = hardware_settings::loadRotation();
  tft_.setRotation(rotation_);
  printHeapSnapshot(Serial, "nach Display-Init");

  const bool forceCalibration = showBootScreenAndCheckRecovery();
  if (forceCalibration || !touch_.applyStoredCalibration(rotation_)) {
    touch_.calibrate(rotation_);
  } else {
    Serial.println("[touch] gespeicherte Kalibrierung geladen");
  }
  printCalibration();

  sprite8_ = probeSprite(8);
  sprite16_ = probeSprite(16);
  printHeapSnapshot(Serial, "nach Sprite-Test");

  printHelp();
  showScreen(Screen::TestPattern);
}

void HardwareTestApp::loop() {
  handleSerial();
  handleTouch(touch_.poll());

  const uint32_t now = millis();
  if (screen_ == Screen::Info && now - lastInfoRefreshMs_ >= 1000) {
    lastInfoRefreshMs_ = now;
    drawInfoValues();
  }
  if (now - lastHeapLogMs_ >= kHeapLogIntervalMs) {
    lastHeapLogMs_ = now;
    printHeapSnapshot(Serial, "periodisch");
  }
  delay(5);
}

// Prüft zur Laufzeit, ob die kompilierte Bibliothek wirklich tft_setup.h verwendet hat.
bool HardwareTestApp::verifyLibrarySetup() {
  setup_t setup;
  tft_.getSetup(setup);
  const bool ok = setup.pin_tft_mosi == board::kPinSpiMosi && setup.pin_tft_miso == board::kPinSpiMiso &&
                  setup.pin_tft_clk == board::kPinSpiSck && setup.pin_tft_cs == board::kPinTftCs &&
                  setup.pin_tft_dc == board::kPinTftDc && setup.pin_tft_rst == board::kPinTftReset &&
                  setup.pin_tch_cs == board::kPinTouchCs;
  Serial.printf("[setup] TFT_eSPI %s, Treiber 0x%04X, SPI %lu Hz: %s\n", setup.version.c_str(), setup.tft_driver,
                static_cast<unsigned long>(setup.tft_spi_freq) * 100000UL,
                ok ? "Pins stimmen mit BoardPins.h ueberein" : "PIN-ABWEICHUNG - docs/setup.md pruefen!");
  return ok;
}

bool HardwareTestApp::showBootScreenAndCheckRecovery() {
  const int16_t cx = tft_.width() / 2;
  const int16_t cy = tft_.height() / 2;
  tft_.fillScreen(TFT_BLACK);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(TFT_GREEN, TFT_BLACK);
  tft_.drawString("HomeFlightTracker", cx, cy - 40, 2);
  tft_.setTextColor(TFT_WHITE, TFT_BLACK);
  tft_.drawString("Diagnose", cx, cy - 10, 4);
  tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft_.drawString("Neu kalibrieren:", cx, cy + 30, 2);
  tft_.drawString("jetzt Display antippen", cx, cy + 50, 2);
  if (!librarySetupOk_) {
    tft_.setTextColor(TFT_RED, TFT_BLACK);
    tft_.drawString("Pin-Konfiguration pruefen!", cx, cy + 80, 2);
  }

  const uint32_t start = millis();
  while (millis() - start < kRecoveryWindowMs) {
    if (touch_.isRawTouched()) {
      Serial.println("[touch] Neukalibrierung beim Start angefordert");
      return true;
    }
    delay(20);
  }
  return false;
}

void HardwareTestApp::showScreen(Screen screen) {
  screen_ = screen;
  switch (screen) {
    case Screen::TestPattern:
      drawTestPattern();
      break;
    case Screen::TouchTest:
      drawTouchTest();
      break;
    case Screen::Info:
      drawInfo();
      lastInfoRefreshMs_ = millis();
      break;
  }
}

void HardwareTestApp::drawTestPattern() {
  const int16_t w = tft_.width();
  const int16_t h = tft_.height();
  tft_.fillScreen(TFT_BLACK);

  // Randlinie: alle vier Kanten müssen vollständig sichtbar sein (kein Versatz, kein Abschneiden).
  tft_.drawRect(0, 0, w, h, TFT_WHITE);

  // Eckmarken in festen Farben: zeigen Rotation und Spiegelung eindeutig an.
  constexpr int16_t m = 14;
  tft_.fillRect(1, 1, m, m, TFT_RED);
  tft_.fillRect(w - m - 1, 1, m, m, TFT_GREEN);
  tft_.fillRect(1, h - m - 1, m, m, TFT_BLUE);
  tft_.fillRect(w - m - 1, h - m - 1, m, m, TFT_WHITE);

  // Richtungspfeil „oben“
  const int16_t cx = w / 2;
  tft_.fillTriangle(cx, 8, cx - 12, 28, cx + 12, 28, TFT_YELLOW);
  tft_.setTextDatum(TC_DATUM);
  tft_.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft_.drawString("OBEN", cx, 32, 2);

  // Farbbalken mit Beschriftung: prüft RGB/BGR-Reihenfolge und Invertierung.
  const uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_YELLOW, TFT_CYAN, TFT_MAGENTA};
  const char* const names[] = {"R", "G", "B", "W", "Y", "C", "M"};
  constexpr int16_t barCount = 7;
  const int16_t barWidth = (w - 20) / barCount;
  const int16_t barTop = 60;
  for (int16_t i = 0; i < barCount; ++i) {
    const int16_t x = 10 + i * barWidth;
    tft_.fillRect(x, barTop, barWidth - 2, 50, colors[i]);
    tft_.setTextColor(TFT_WHITE, TFT_BLACK);
    tft_.drawString(names[i], x + barWidth / 2, barTop + 54, 2);
  }

  // Graustufenverlauf: zeigt Abstufungen und Kontrast.
  const int16_t grayTop = barTop + 80;
  for (int16_t i = 0; i < 16; ++i) {
    const uint8_t v = i * 17;
    const int16_t x = 10 + i * (w - 20) / 16;
    const int16_t next = 10 + (i + 1) * (w - 20) / 16;
    tft_.fillRect(x, grayTop, next - x, 24, tft_.color565(v, v, v));
  }

  char line[32];
  snprintf(line, sizeof(line), "Rotation %u  %dx%d", rotation_, w, h);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(TFT_WHITE, TFT_BLACK);
  tft_.drawString(line, cx, grayTop + 50, 2);
  tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft_.drawString("Tippen: weiter zum Touch-Test", cx, h - 30, 2);
}

void HardwareTestApp::drawTouchTest() {
  tft_.fillScreen(TFT_BLACK);
  tft_.setTextDatum(TL_DATUM);
  tft_.setTextColor(TFT_WHITE, TFT_BLACK);
  char line[32];
  snprintf(line, sizeof(line), "Touch-Test  R%u", rotation_);
  tft_.drawString(line, 4, 4, 2);
  tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft_.drawString("Zone antippen", 4, 22, 2);
  drawTouchGrid(-1);
  drawButtonBar(kTouchTestButtons, kTouchTestButtonCount);
}

void HardwareTestApp::drawTouchGrid(int8_t highlightedCell) {
  const int16_t cols = tft_.width() / kCellSize;
  const int16_t rows = gridRows();
  tft_.fillRect(0, kBarHeight, tft_.width(), rows * kCellSize, TFT_BLACK);
  tft_.setTextDatum(MC_DATUM);
  for (int16_t r = 0; r < rows; ++r) {
    for (int16_t c = 0; c < cols; ++c) {
      const int16_t index = r * cols + c;
      const int16_t x = c * kCellSize;
      const int16_t y = kBarHeight + r * kCellSize;
      const bool highlighted = index == highlightedCell;
      if (highlighted) tft_.fillRect(x + 2, y + 2, kCellSize - 4, kCellSize - 4, TFT_DARKGREEN);
      tft_.drawRect(x + 1, y + 1, kCellSize - 2, kCellSize - 2, TFT_DARKGREY);
      tft_.setTextColor(highlighted ? TFT_WHITE : TFT_DARKGREY, highlighted ? TFT_DARKGREEN : TFT_BLACK);
      tft_.drawNumber(index + 1, x + kCellSize / 2, y + kCellSize / 2, 4);
    }
  }
}

void HardwareTestApp::drawTouchStatus() {
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  tft_.getTouchRaw(&rawX, &rawY);
  const uint16_t z = tft_.getTouchRawZ();
  uint16_t x = 0;
  uint16_t y = 0;
  const bool valid = tft_.getTouch(&x, &y);

  char line[40];
  tft_.setTextDatum(TL_DATUM);
  tft_.setTextColor(TFT_WHITE, TFT_BLACK);
  tft_.setTextPadding(tft_.width() - 8);
  if (valid) {
    snprintf(line, sizeof(line), "x=%3u y=%3u z=%4u", x, y, z);
  } else {
    snprintf(line, sizeof(line), "kein gueltiger Punkt z=%4u", z);
  }
  tft_.drawString(line, 4, 4, 2);
  tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  snprintf(line, sizeof(line), "roh %4u/%4u  IRQ=%s", rawX, rawY, touch_.irqActive() ? "aktiv" : "inaktiv");
  tft_.drawString(line, 4, 22, 2);
  tft_.setTextPadding(0);
}

void HardwareTestApp::drawInfo() {
  tft_.fillScreen(TFT_BLACK);
  tft_.setTextDatum(TL_DATUM);
  tft_.setTextColor(TFT_GREEN, TFT_BLACK);
  tft_.drawString("Systeminfo", 8, 6, 4);

  char line[48];
  int16_t y = 40;
  constexpr int16_t kLine = 18;
  tft_.setTextColor(TFT_WHITE, TFT_BLACK);

  snprintf(line, sizeof(line), "Chip: %s rev %u", ESP.getChipModel(), ESP.getChipRevision());
  tft_.drawString(line, 8, y, 2);
  y += kLine;
  snprintf(line, sizeof(line), "CPU: %lu MHz, %u Kerne", static_cast<unsigned long>(ESP.getCpuFreqMHz()),
           ESP.getChipCores());
  tft_.drawString(line, 8, y, 2);
  y += kLine;
  snprintf(line, sizeof(line), "Flash: %lu MB  PSRAM: %lu KB",
           static_cast<unsigned long>(ESP.getFlashChipSize() / (1024 * 1024)),
           static_cast<unsigned long>(ESP.getPsramSize() / 1024));
  tft_.drawString(line, 8, y, 2);
  y += kLine;
  snprintf(line, sizeof(line), "Rotation: %u (%dx%d)", rotation_, tft_.width(), tft_.height());
  tft_.drawString(line, 8, y, 2);
  y += kLine;
  const TouchCalibration& cal = touch_.calibration();
  snprintf(line, sizeof(line), "Kal.: %u %u %u %u 0x%02X", cal.data[0], cal.data[1], cal.data[2], cal.data[3],
           cal.data[4]);
  tft_.drawString(line, 8, y, 2);
  y += kLine;

  const SpriteProbe probes[] = {sprite8_, sprite16_};
  for (const SpriteProbe& probe : probes) {
    if (probe.ok) {
      snprintf(line, sizeof(line), "Sprite %u Bit: OK (%lu KB)", probe.colorDepth,
               static_cast<unsigned long>(probe.bytes / 1024));
    } else {
      snprintf(line, sizeof(line), "Sprite %u Bit: FEHLER", probe.colorDepth);
    }
    tft_.setTextColor(probe.ok ? TFT_WHITE : TFT_ORANGE, TFT_BLACK);
    tft_.drawString(line, 8, y, 2);
    y += kLine;
  }

  if (!librarySetupOk_) {
    tft_.setTextColor(TFT_RED, TFT_BLACK);
    tft_.drawString("Pin-Konfiguration pruefen!", 8, y, 2);
  }

  drawInfoValues();
  drawButtonBar(kInfoButtons, kInfoButtonCount);
}

void HardwareTestApp::drawInfoValues() {
  const HeapSnapshot heap = takeHeapSnapshot();
  const uint32_t seconds = millis() / 1000;
  char line[40];
  int16_t y = tft_.height() - kBarHeight - 4 * 18 - 4;

  tft_.setTextDatum(TL_DATUM);
  tft_.setTextColor(TFT_CYAN, TFT_BLACK);
  tft_.setTextPadding(tft_.width() - 16);
  snprintf(line, sizeof(line), "Heap frei: %lu B", static_cast<unsigned long>(heap.freeBytes));
  tft_.drawString(line, 8, y, 2);
  y += 18;
  snprintf(line, sizeof(line), "Heap min:  %lu B", static_cast<unsigned long>(heap.minFreeBytes));
  tft_.drawString(line, 8, y, 2);
  y += 18;
  snprintf(line, sizeof(line), "Max. Block: %lu B", static_cast<unsigned long>(heap.maxAllocBytes));
  tft_.drawString(line, 8, y, 2);
  y += 18;
  snprintf(line, sizeof(line), "Laufzeit: %02lu:%02lu:%02lu", static_cast<unsigned long>(seconds / 3600),
           static_cast<unsigned long>((seconds / 60) % 60), static_cast<unsigned long>(seconds % 60));
  tft_.drawString(line, 8, y, 2);
  tft_.setTextPadding(0);
}

void HardwareTestApp::drawButtonBar(const char* const* labels, uint8_t count) {
  const int16_t w = tft_.width();
  const int16_t top = tft_.height() - kBarHeight;
  tft_.fillRect(0, top, w, kBarHeight, TFT_BLACK);
  tft_.setTextDatum(MC_DATUM);
  for (uint8_t i = 0; i < count; ++i) {
    const int16_t x = i * w / count;
    const int16_t buttonWidth = (i + 1) * w / count - x;
    tft_.fillRoundRect(x + 2, top + 2, buttonWidth - 4, kBarHeight - 4, 6, TFT_NAVY);
    tft_.drawRoundRect(x + 2, top + 2, buttonWidth - 4, kBarHeight - 4, 6, TFT_SKYBLUE);
    tft_.setTextColor(TFT_WHITE, TFT_NAVY);
    tft_.drawString(labels[i], x + buttonWidth / 2, top + kBarHeight / 2, 2);
  }
}

void HardwareTestApp::handleTouch(const TouchState& touch) {
  switch (screen_) {
    case Screen::TestPattern:
      if (touch.pressed) showScreen(Screen::TouchTest);
      break;
    case Screen::TouchTest:
      handleTouchTest(touch);
      break;
    case Screen::Info:
      if (!touch.pressed) break;
      switch (buttonAt(touch.x, touch.y, kInfoButtonCount)) {
        case 0:
          showScreen(Screen::TouchTest);
          break;
        case 1:
          testBacklight();
          break;
        case 2:
          ESP.restart();
          break;
        default:
          break;
      }
      break;
  }
}

void HardwareTestApp::handleTouchTest(const TouchState& touch) {
  if (touch.pressed) {
    switch (buttonAt(touch.x, touch.y, kTouchTestButtonCount)) {
      case 0:
        showScreen(Screen::TestPattern);
        return;
      case 1:
        recalibrate();
        return;
      case 2:
        rotate();
        return;
      case 3:
        showScreen(Screen::Info);
        return;
      default:
        break;
    }
    const int8_t cell = cellAt(touch.x, touch.y);
    if (cell >= 0) {
      drawTouchGrid(cell);
      Serial.printf("[touch] Zone %d bei x=%u y=%u\n", cell + 1, touch.x, touch.y);
    }
  }

  if (touch.down) {
    // Spur der gemessenen Punkte zeigt Genauigkeit und Streuung.
    if (cellAt(touch.x, touch.y) >= 0) tft_.fillCircle(touch.x, touch.y, 2, TFT_YELLOW);
    const uint32_t now = millis();
    if (now - lastStatusMs_ >= 100) {
      lastStatusMs_ = now;
      drawTouchStatus();
    }
  }
}

void HardwareTestApp::handleSerial() {
  while (Serial.available() > 0) {
    switch (Serial.read()) {
      case 'c':
        recalibrate();
        break;
      case 'r':
        rotate();
        break;
      case 'b':
        testBacklight();
        break;
      case 'h':
        printHeapSnapshot(Serial, "manuell");
        break;
      case 'k':
        printCalibration();
        break;
      case 'x':
        hardware_settings::clearAll();
        Serial.println("[setup] Rotation und Kalibrierung geloescht - Neustart");
        delay(200);
        ESP.restart();
        break;
      case '?':
        printHelp();
        break;
      default:
        break;
    }
  }
}

void HardwareTestApp::printHelp() {
  Serial.println("Befehle: c=kalibrieren  r=drehen  b=Licht-Test  h=Heap  k=Kalibrierwerte  x=Reset  ?=Hilfe");
}

void HardwareTestApp::printCalibration() {
  const TouchCalibration& cal = touch_.calibration();
  Serial.printf("[touch] Rotation %u, Kalibrierung: %u %u %u %u 0x%02X\n", cal.rotation, cal.data[0], cal.data[1],
                cal.data[2], cal.data[3], cal.data[4]);
}

void HardwareTestApp::rotate() {
  rotation_ = (rotation_ + 1) % 4;
  hardware_settings::saveRotation(rotation_);
  tft_.setRotation(rotation_);
  Serial.printf("[display] Rotation %u\n", rotation_);
  // Die Kalibrierung gilt nur für eine Rotation; nach dem Drehen wird neu kalibriert.
  if (!touch_.applyStoredCalibration(rotation_)) touch_.calibrate(rotation_);
  showScreen(Screen::TestPattern);
}

void HardwareTestApp::recalibrate() {
  touch_.calibrate(rotation_);
  showScreen(Screen::TouchTest);
}

void HardwareTestApp::testBacklight() {
  Serial.println("[display] Hintergrundbeleuchtung 1,5 s aus");
  digitalWrite(board::kPinTftBacklight, !TFT_BACKLIGHT_ON);
  delay(1500);
  digitalWrite(board::kPinTftBacklight, TFT_BACKLIGHT_ON);
  Serial.println("[display] Hintergrundbeleuchtung an");
}

HardwareTestApp::SpriteProbe HardwareTestApp::probeSprite(uint8_t colorDepth) {
  SpriteProbe probe;
  probe.colorDepth = colorDepth;
  TFT_eSprite sprite(&tft_);
  sprite.setColorDepth(colorDepth);
  const uint32_t before = ESP.getFreeHeap();
  probe.ok = sprite.createSprite(kRadarSpriteSize, kRadarSpriteSize) != nullptr;
  if (probe.ok) probe.bytes = before - ESP.getFreeHeap();
  sprite.deleteSprite();
  Serial.printf("[sprite] %ux%u, %u Bit: %s (%lu Byte)\n", kRadarSpriteSize, kRadarSpriteSize, colorDepth,
                probe.ok ? "OK" : "FEHLER", static_cast<unsigned long>(probe.bytes));
  return probe;
}

int16_t HardwareTestApp::gridRows() const {
  return (tft_.height() - 2 * kBarHeight) / kCellSize;
}

int8_t HardwareTestApp::cellAt(uint16_t x, uint16_t y) const {
  const int16_t cols = tft_.width() / kCellSize;
  const int16_t rows = gridRows();
  if (y < kBarHeight || y >= kBarHeight + rows * kCellSize) return -1;
  const int16_t col = x / kCellSize;
  if (col >= cols) return -1;
  return (y - kBarHeight) / kCellSize * cols + col;
}

int8_t HardwareTestApp::buttonAt(uint16_t x, uint16_t y, uint8_t count) const {
  if (y < tft_.height() - kBarHeight || x >= tft_.width()) return -1;
  return x * count / tft_.width();
}

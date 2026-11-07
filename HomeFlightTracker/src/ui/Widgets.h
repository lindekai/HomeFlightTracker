// Gemeinsame Bedienelemente der Geräteoberfläche.
#pragma once

#include <TFT_eSPI.h>

namespace ui {

constexpr int16_t kStatusBarHeight = 22;
constexpr int16_t kButtonBarHeight = 44;
constexpr int16_t kLineHeight = 18;
constexpr int16_t kMargin = 8;

constexpr uint16_t kColorBackground = 0x0042;  // #050B10
constexpr uint16_t kColorBar = 0x08C4;         // #0B1820
constexpr uint16_t kColorText = 0xDF3B;        // #D8E6DC
constexpr uint16_t kColorMuted = 0x8D94;       // #8FB3A0
constexpr uint16_t kColorAccent = 0x3EF0;      // #3DDC84
constexpr uint16_t kColorWarning = 0xFD84;     // #FFB020
constexpr uint16_t kColorError = 0xFAC9;       // #FF5A4F
constexpr uint16_t kColorButton = 0x0906;      // #0D2233
constexpr uint16_t kColorButtonBorder = 0x2B73;  // #2A6F99

// Statusleiste mit Ampelpunkt, Text links und rechts.
void drawStatusBar(TFT_eSPI& tft, uint16_t dotColor, const char* left, const char* right);

// Leiste gleich breiter Schaltflächen am unteren Rand.
void drawButtonBar(TFT_eSPI& tft, const char* const* labels, uint8_t count);

// Index der getroffenen Schaltfläche oder -1.
int8_t buttonAt(TFT_eSPI& tft, uint16_t x, uint16_t y, uint8_t count);

// Textzeile mit Hintergrund-Auffüllung (für zyklisch aktualisierte Werte).
void drawLine(TFT_eSPI& tft, int16_t y, const char* text, uint16_t color, uint8_t font = 2);

// Font 4, falls der Text in die Displaybreite passt, sonst Font 2.
uint8_t fittingFont(TFT_eSPI& tft, const char* text);

// Zentrierte Meldung mit Titel und optionaler zweiter Zeile.
void drawMessage(TFT_eSPI& tft, const char* title, const char* line, uint16_t titleColor);

}  // namespace ui

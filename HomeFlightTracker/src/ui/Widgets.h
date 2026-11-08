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
constexpr uint16_t kColorRing = 0x1B69;          // #1F6F4A
constexpr uint16_t kColorRingDim = 0x1266;       // #164F36
constexpr uint16_t kColorCross = 0x11C5;         // #10392A
constexpr uint16_t kColorRingLabel = 0x2C6C;     // #2F8F63
constexpr uint16_t kColorAircraft = 0xFE89;      // #FFD34D
constexpr uint16_t kColorSelected = 0xFBC7;      // #FF7A3D
constexpr uint16_t kColorSelectedBg = 0x1881;    // #1A1208
constexpr uint16_t kColorSelectedText = 0xFDB1;  // #FFB68F
constexpr uint16_t kColorStale = 0x5B4C;         // #5D6B66
constexpr uint16_t kColorGround = 0x4C94;        // #4A90A0
constexpr uint16_t kColorRow = 0x08E2;           // #0F1D16

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

// UTF-8-Text mit deutschen Umlauten. Font 2 hat keine Umlaute: ä/ö/ü/Ä/Ö/Ü werden als Grundbuchstabe
// mit zwei Punkten gezeichnet, ß als „ss“, ° über das Gradzeichen von Font 2. Andere Fonts erhalten
// die Umschreibung (ae, oe, ue). x/y ist die linke obere Ecke; rightAlign richtet am x-Wert rechtsbündig aus.
// Liefert die gezeichnete Breite. Textfarbe und Hintergrund kommen aus tft.setTextColor().
int16_t drawText(TFT_eSPI& tft, const char* utf8, int16_t x, int16_t y, uint8_t font = 2, bool rightAlign = false);
int16_t textWidth(TFT_eSPI& tft, const char* utf8, uint8_t font = 2);

// Dezimalzahl mit deutschem Komma, z. B. formatDecimal(18.37, 1) → "18,4".
void formatDecimal(char* buffer, size_t size, double value, uint8_t decimals);

// Zentrierte Meldung mit Titel und optionaler zweiter Zeile.
void drawMessage(TFT_eSPI& tft, const char* title, const char* line, uint16_t titleColor);

}  // namespace ui

#include "Widgets.h"

#include <stdio.h>
#include <string.h>

namespace {

// Zeichen nach UTF-8-Dekodierung: ASCII-Zeichen und ggf. Umlaut-Punkte darüber.
struct Glyph {
  char base;
  char extra;     // zweites ASCII-Zeichen (ß → "ss", Umschreibung ae/oe/ue) oder 0
  bool dots;      // Umlaut-Punkte zeichnen (nur Font 2)
  bool capital;   // Großbuchstabe: Punkte höher setzen
};

// Liest ein UTF-8-Zeichen ab text und rückt text weiter.
Glyph nextGlyph(const char*& text, uint8_t font) {
  const uint8_t c = static_cast<uint8_t>(*text++);
  if (c < 0x80) return {static_cast<char>(c), 0, false, false};
  if (c == 0xC2 || c == 0xC3) {
    const uint8_t c2 = static_cast<uint8_t>(*text);
    if (c2 != 0) ++text;
    const bool font2 = font == 2;
    if (c == 0xC2 && c2 == 0xB0) return {font2 ? '`' : 'o', 0, false, false};  // °
    if (c == 0xC2 && c2 == 0xB7) return {'-', 0, false, false};                // ·
    if (c == 0xC3) {
      char base = 0;
      bool capital = false;
      switch (c2) {
        case 0xA4: base = 'a'; break;
        case 0xB6: base = 'o'; break;
        case 0xBC: base = 'u'; break;
        case 0x84: base = 'A'; capital = true; break;
        case 0x96: base = 'O'; capital = true; break;
        case 0x9C: base = 'U'; capital = true; break;
        case 0x9F: return {'s', 's', false, false};  // ß
        default: break;
      }
      if (base != 0) return font2 ? Glyph{base, 0, true, capital} : Glyph{base, 'e', false, false};
    }
    return {'?', 0, false, false};
  }
  // Weitere Mehrbyte-Folgen überspringen.
  while ((static_cast<uint8_t>(*text) & 0xC0) == 0x80) ++text;
  return {'?', 0, false, false};
}

}  // namespace

void ui::drawStatusBar(TFT_eSPI& tft, uint16_t dotColor, const char* left, const char* right) {
  tft.fillRect(0, 0, tft.width(), kStatusBarHeight, kColorBar);
  tft.fillCircle(9, kStatusBarHeight / 2, 4, dotColor);
  tft.setTextColor(kColorText, kColorBar);
  drawText(tft, left, 18, 3);
  drawText(tft, right, tft.width() - 6, 3, 2, true);
}

void ui::drawButtonBar(TFT_eSPI& tft, const char* const* labels, uint8_t count) {
  const int16_t w = tft.width();
  const int16_t top = tft.height() - kButtonBarHeight;
  tft.fillRect(0, top, w, kButtonBarHeight, kColorBackground);
  tft.setTextDatum(MC_DATUM);
  for (uint8_t i = 0; i < count; ++i) {
    const int16_t x = i * w / count;
    const int16_t width = (i + 1) * w / count - x;
    tft.fillRoundRect(x + 2, top + 3, width - 4, kButtonBarHeight - 5, 6, kColorButton);
    tft.drawRoundRect(x + 2, top + 3, width - 4, kButtonBarHeight - 5, 6, kColorButtonBorder);
    tft.setTextColor(kColorText, kColorButton);
    drawText(tft, labels[i], x + width / 2 - textWidth(tft, labels[i]) / 2, top + kButtonBarHeight / 2 - 7);
  }
}

int8_t ui::buttonAt(TFT_eSPI& tft, uint16_t x, uint16_t y, uint8_t count) {
  if (count == 0 || y < tft.height() - kButtonBarHeight || x >= tft.width()) return -1;
  return x * count / tft.width();
}

void ui::drawLine(TFT_eSPI& tft, int16_t y, const char* text, uint16_t color, uint8_t font) {
  tft.fillRect(kMargin, y, tft.width() - 2 * kMargin, tft.fontHeight(font), kColorBackground);
  tft.setTextColor(color, kColorBackground);
  drawText(tft, text, kMargin, y, font);
}

int16_t ui::textWidth(TFT_eSPI& tft, const char* utf8, uint8_t font) {
  int16_t width = 0;
  const char* p = utf8;
  while (*p) {
    const Glyph glyph = nextGlyph(p, font);
    width += tft.textWidth(String(glyph.base), font);
    if (glyph.extra) width += tft.textWidth(String(glyph.extra), font);
  }
  return width;
}

int16_t ui::drawText(TFT_eSPI& tft, const char* utf8, int16_t x, int16_t y, uint8_t font, bool rightAlign) {
  if (rightAlign) x -= textWidth(tft, utf8, font);
  const int16_t start = x;
  const char* p = utf8;
  while (*p) {
    const Glyph glyph = nextGlyph(p, font);
    const int16_t width = tft.drawChar(glyph.base, x, y, font);
    if (glyph.dots) {
      // Font 2: Kleinbuchstaben beginnen in Zeile 7, Großbuchstaben in Zeile 4 der 16er-Zelle.
      const int16_t dotY = y + (glyph.capital ? 1 : 4);
      const int16_t left = x + width / 2 - 2;
      tft.drawPixel(left, dotY, tft.textcolor);
      tft.drawPixel(left + 3, dotY, tft.textcolor);
    }
    x += width;
    if (glyph.extra) x += tft.drawChar(glyph.extra, x, y, font);
  }
  return x - start;
}

void ui::formatDecimal(char* buffer, size_t size, double value, uint8_t decimals) {
  snprintf(buffer, size, "%.*f", decimals, value);
  for (char* p = buffer; *p; ++p) {
    if (*p == '.') *p = ',';
  }
}

uint8_t ui::fittingFont(TFT_eSPI& tft, const char* text) {
  return tft.textWidth(text, 4) <= tft.width() - 2 * kMargin ? 4 : 2;
}

void ui::drawMessage(TFT_eSPI& tft, const char* title, const char* line, uint16_t titleColor) {
  const int16_t cx = tft.width() / 2;
  const int16_t cy = tft.height() / 2;
  tft.fillScreen(kColorBackground);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(titleColor, kColorBackground);
  tft.drawString(title, cx, cy - 14, fittingFont(tft, title));
  if (line != nullptr) {
    tft.setTextColor(kColorMuted, kColorBackground);
    drawText(tft, line, cx - textWidth(tft, line) / 2, cy + 8);
  }
}

#include "Widgets.h"

void ui::drawStatusBar(TFT_eSPI& tft, uint16_t dotColor, const char* left, const char* right) {
  tft.fillRect(0, 0, tft.width(), kStatusBarHeight, kColorBar);
  tft.fillCircle(9, kStatusBarHeight / 2, 4, dotColor);
  tft.setTextColor(kColorText, kColorBar);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(left, 18, kStatusBarHeight / 2, 2);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(right, tft.width() - 6, kStatusBarHeight / 2, 2);
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
    tft.drawString(labels[i], x + width / 2, top + kButtonBarHeight / 2 + 1, 2);
  }
}

int8_t ui::buttonAt(TFT_eSPI& tft, uint16_t x, uint16_t y, uint8_t count) {
  if (count == 0 || y < tft.height() - kButtonBarHeight || x >= tft.width()) return -1;
  return x * count / tft.width();
}

void ui::drawLine(TFT_eSPI& tft, int16_t y, const char* text, uint16_t color, uint8_t font) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, kColorBackground);
  tft.setTextPadding(tft.width() - 2 * kMargin);
  tft.drawString(text, kMargin, y, font);
  tft.setTextPadding(0);
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
    tft.drawString(line, cx, cy + 16, 2);
  }
}

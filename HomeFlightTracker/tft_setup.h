// TFT_eSPI-Konfiguration für HomeFlightTracker
// Hardware: AZ-Touch MOD V01-03 (Schaltplan V01-03-01) mit ESP32 DevKit C (ESP32-WROOM-32),
//           2,4-Zoll-TFT ILI9341 + Touchcontroller XPT2046 am gemeinsamen SPI-Bus (VSPI).
//
// TFT_eSPI (ab 2.5.x) lädt diese Datei automatisch per __has_include(<tft_setup.h>), weil der
// ESP32-Arduino-Core den Sketch-Ordner in den Include-Pfad aufnimmt. Die Bibliothek selbst
// (User_Setup.h / User_Setup_Select.h) wird dadurch NICHT verändert.
//
// Pinquelle und Prüfstatus: docs/hardware.md. Änderungen hier immer mit src/board/BoardPins.h abgleichen.
#pragma once

#define HOMEFLIGHTTRACKER_TFT_SETUP 1

#define ILI9341_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// Gemeinsamer SPI-Bus (TFT + Touch)
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18

// Display
#define TFT_CS   5   // Strapping-Pin, interner Pull-up beim Booten unkritisch
#define TFT_DC   4
#define TFT_RST  22

// Hintergrundbeleuchtung: GPIO15 -> 1 kOhm -> PNP BC807-40 (Emitter an +3V3) => aktiv LOW
#define TFT_BL           15
#define TFT_BACKLIGHT_ON LOW

// Touch (XPT2046)
#define TOUCH_CS 14

// Schriften (nur Flash-Verbrauch)
#define LOAD_GLCD   // Font 1, 8 px
#define LOAD_FONT2  // Font 2, 16 px
#define LOAD_FONT4  // Font 4, 26 px
#define LOAD_GFXFF  // FreeFonts

// SPI-Takte: konservativ gewählt, Erhöhung erst nach Stabilitätstest
#define SPI_FREQUENCY       27000000
#define SPI_READ_FREQUENCY  16000000
#define SPI_TOUCH_FREQUENCY  2500000

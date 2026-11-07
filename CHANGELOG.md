# Changelog

Format angelehnt an [Keep a Changelog](https://keepachangelog.com/de/1.1.0/).

## [Unveröffentlicht]

### M1 – Hardware-Machbarkeitstest

#### Hinzugefügt

- Test-Firmware mit folgenden Teilen:
  - Testbild: Randlinie, Eckmarken, Farbbalken, Graustufen
  - Umschalten der Rotation, gespeichert im NVS
  - Test der Hintergrundbeleuchtung
- Touch-Kalibrierung:
  - gespeichert im NVS mit Plausibilitätsprüfung
  - Recovery durch Antippen beim Start
- Touch-Test mit Trefferzonen, Koordinaten, Rohwerten, Druck und IRQ-Pegel.
- Heap-Messung (Start, nach Display-Init, nach Sprite-Test, periodisch) und Machbarkeitstest für ein Radar-Sprite 240 × 240 bei 8 und 16 Bit.
- Prüfung der Pin-Konfiguration beim Kompilieren (`static_assert`) und zur Laufzeit (`TFT_eSPI::getSetup`).

### M0 – Projektgerüst

#### Hinzugefügt

- Arduino-Sketch-Struktur mit Modulen unter `src/`.
- `tft_setup.h`: TFT_eSPI-Konfiguration im Sketch-Ordner, ohne Änderung der Bibliothek.
- Dokumentation:
  - `README.md`
  - `docs/hardware.md` (Pin-Matrix mit Quellen)
  - `docs/setup.md`
  - `docs/architecture.md`
  - `docs/opensky.md`
  - `docs/testing.md`
  - `docs/ui.md`
- UI-Mockups für Radar, Details, Liste, Setup-Hinweis und Einrichtungsportal.
- `.gitignore`.

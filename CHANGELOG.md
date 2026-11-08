# Changelog

Format angelehnt an [Keep a Changelog](https://keepachangelog.com/de/1.1.0/).

## [Unveröffentlicht]

### Projekt

- MIT-Lizenz hinzugefügt.

### M4 – Radar-Oberfläche

#### Hinzugefügt

- Radar im 8-Bit-Sprite (flackerfrei):
  - Norden oben, vier Entfernungsringe mit Beschriftung, Himmelsrichtungen
  - Flugzeugsilhouetten in Bewegungsrichtung
  - Kennzeichnung für „am Boden“ und veraltete Positionen
  - Auswahl mit Kurzinfo
- Robuste Auswahl bei überlappenden Symbolen (Durchschalten), Details per Kurzinfo oder erneutem Antippen.
- Detailansicht mit eindeutigen Einheiten und dem Hinweis, dass Typ, Registrierung und Start/Ziel nicht geliefert werden.
- Flugliste nach Entfernung mit Richtungspfeilen und Blättern.
- Menü mit WLAN-, OpenSky- und Credit-Status.
- Zoom 25/50/100 km mit Kontingentprüfung; wirkt ab dem nächsten regulären Abruf.
- Statusleiste mit WLAN- und OpenSky-Ampel und Uhrzeit; Fußzeile mit „Stand … · vor n s“.
- Umlaute auf dem Display über eigene UTF-8-Textausgabe.

### M3 – OpenSky-Abruf

#### Hinzugefügt

- NTP-Zeit (Europa/Berlin).
- TLS mit Zertifikatsprüfung über das eingebettete Root-CA-Bundle.
- OAuth2 Client Credentials mit Token-Erneuerung vor Ablauf und einmaliger Neuanmeldung bei 401.
- `/states/all` mit Bounding Box (inkl. Datumsgrenze), Streaming-Parser für beliebig viele Flugzeuge, Tabelle der 100 nächsten.
- Hintergrund-Task mit exaktem Takt, Rate-Limit-Auswertung, Backoff bei 429/5xx/Netzwerkfehlern, Auth-Fehlerstatus.
- „Zugang testen“ für OpenSky in der Weboberfläche.
- Host-Tests für Parser und Flugliste (Null-Felder, fehlende Koordinaten, doppelte Callsigns, Kurs 0°/359°, leere Ergebnisse, abgebrochene Antworten); insgesamt 84 Prüfungen.

### M2 – Konfiguration und WLAN-Einrichtung

#### Hinzugefügt

- Setup-Modus:
  - WPA2-Access-Point `HomeFlightTracker-Setup` mit zufälligem Passwort pro Start, Anzeige nur am Display
  - Captive Portal, Ende nach 15 Minuten
- Weboberfläche (Smartphone-tauglich, Hell-/Dunkelmodus):
  - WLAN-Suche und Verbindungstest
  - Standort per Koordinaten
  - OpenSky-Client-ID und -Secret
  - Reichweite und Intervall mit Live-Schätzung des Credit-Verbrauchs
  - Geräte-Passwort
- Konfiguration im NVS mit Plausibilitätsprüfung: Koordinaten, Reichweite, Passwortlängen, Intervall gegen das Tageskontingent.
- Normalbetrieb:
  - WLAN mit automatischer Wiederverbindung
  - Weboberfläche im Heimnetz mit Digest-Anmeldung und Schutz gegen Cross-Site-Requests
  - mDNS `homeflighttracker.local`
  - Statusbildschirm mit IP, Standort und Credit-Verbrauch
- Werksreset lokal per 8 s Halten beim Start, Neukalibrierung per Antippen beim Start.
- Diagnosemodus (Hardwaretest aus M1) über den Statusbildschirm.
- Rechenmodule `geo` (Distanz, Peilung, Bounding Box inkl. Datumsgrenze und Polnähe) und `opensky/Quota` (Credits, zulässige Intervalle).
- Host-Unit-Tests (`tests/host/run.sh`, 50 Prüfungen).

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

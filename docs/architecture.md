# Architektur

## Überblick

```text
                 ┌──────────────────────── ESP32-WROOM-32 ────────────────────────┐
                 │                                                                 │
  OpenSky  ◄─────┤  Netzwerk-Task (Core 0)             UI-Task (Core 1)            │
  (HTTPS)        │  ├─ WLAN / Reconnect                ├─ Radar-Renderer ──► TFT   │
  NTP      ◄─────┤  ├─ NTP-Zeit                        ├─ Bildschirme / Zustände   │
                 │  ├─ OAuth2-Token                    └─ Touch-Eingabe ◄── XPT2046│
  Smartphone ◄──►┤  ├─ States-Abruf + JSON-Filter               ▲                 │
  (Setup/Web-UI) │  ├─ Kontingent / Backoff                     │                 │
                 │  └─ Web-UI / Setup-Portal                    │                 │
                 │            │                                 │                 │
                 │            └──► Flugdaten-Snapshot (Mutex) ──┘                 │
                 │                                                                 │
                 │  Konfiguration (NVS)   Geometrie (rein rechnerisch)   Diagnose   │
                 └─────────────────────────────────────────────────────────────────┘
```

Grundregeln:

- **Abfrage und Anzeige sind strikt getrennt.**
  - Die Netzwerk-Task schreibt nach jedem erfolgreichen Abruf einen vollständigen Snapshot.
  - Die UI-Task liest nur diesen Snapshot.
  - Touch-Aktionen lösen niemals einen REST-Aufruf aus.
  - Ein HTTPS-Abruf blockiert nie die Anzeige.
- **Keine Scheinbewegung.** Positionen werden nicht interpoliert. Die Anzeige ändert sich nur, wenn neue Daten eintreffen, und das Alter der Daten ist immer sichtbar.
- **Speicherschonend.**
  - Die Flugzeugtabelle hat eine feste Größe.
  - JSON wird mit Filter gestreamt.
  - Dauerhafte dynamische Allokationen gibt es nur beim Start.

## Module und Verzeichnisstruktur

```text
HomeFlightTracker/                    Sketch-Ordner
├── HomeFlightTracker.ino             Einstieg, Pinprüfung, Start der App
├── tft_setup.h                       TFT_eSPI-Pins (siehe docs/setup.md)
└── src/
    ├── board/BoardPins.h             feste Pinbelegung der Trägerplatine          [M1]
    ├── config/HardwareSettings.*     Rotation + Touch-Kalibrierung im NVS         [M1]
    ├── config/AppConfig.*            WLAN, Standort, OpenSky, Reichweite, Intervall [M2]
    ├── diag/HeapReport.*             Heap-Messung                                  [M1]
    ├── hwtest/HardwareTestApp.*      Hardware-Machbarkeitstest                     [M1]
    ├── net/WifiManager.*             Verbindung, Reconnect, Setup-AP               [M2]
    ├── net/SetupPortal.*             Captive Portal, Web-UI, Authentifizierung     [M2]
    ├── net/TimeSync.*                NTP                                           [M3]
    ├── opensky/TokenClient.*         OAuth2 Client Credentials, Token-Erneuerung   [M3]
    ├── opensky/StatesClient.*        /states/all, JSON-Filter, Fehlerbehandlung    [M3]
    ├── opensky/QuotaTracker.*        Credit-Budget, Rate-Limit-Header, Backoff     [M3]
    ├── model/FlightStore.*           feste Tabelle (max. 100), Snapshot            [M3]
    ├── geo/Geo.*                     Distanz, Peilung, Bounding Box, Projektion    [M3]
    ├── ui/TouchInput.*               Kalibrierung, Touch-Ereignisse                [M1]
    ├── ui/RadarRenderer.*            Radar, Symbole, Status                        [M4]
    ├── ui/Screens.*                  Details, Liste, Menü, Statusbildschirme       [M4]
    └── app/App.*                     Zustandsautomat der Betriebszustände          [M4]
```

Die Module ohne Arduino-Abhängigkeit (`geo/`, Parser-Logik, `QuotaTracker`) werden so geschrieben, dass sie sich auf dem Entwicklungsrechner mit einem normalen C++-Compiler testen lassen (siehe [testing.md](testing.md)).

## Betriebszustände (ab M4)

`Start` → `Setup erforderlich` → `WLAN verbinden` → `API verbinden` → `Radar aktiv`

Daneben gibt es die Zustände `Keine Flugzeuge`, `Daten veraltet`, `Auth-/Kontingentfehler` und `Einstellungen`.

## Geometrie

- Distanz mit der Haversine-Formel, Richtung als Anfangspeilung vom Standort aus. Die Darstellung ist azimutal äquidistant: Abstand und Richtung vom Zentrum sind exakt.
- Die Bounding Box für die Abfrage ergibt sich aus Radius und Puffer. Der Längengrad-Abstand wird mit 1/cos(Breite) skaliert. Polnähe und Datumsgrenze werden gesondert behandelt; an der Datumsgrenze sind zwei Abfragen nötig, oder die Box wird begrenzt.
- Nach dem Laden wird die tatsächliche Entfernung zum Zentrum geprüft. Flugzeuge außerhalb des Radius werden verworfen.

## Ressourcen (Stand M1, Build mit „Minimal SPIFFS“)

| Größe | Wert |
|---|---|
| Programmspeicher | 343 352 Byte (17 % von 1 966 080) |
| Globale Variablen | 22 516 Byte (6 % von 327 680) |
| Freier Heap nach Display-Init (ohne WLAN) | 333 156 Byte, größter Block 110 580 Byte |
| Radar-Sprite 240 × 240 | 8 Bit möglich (59 396 Byte), 16 Bit nicht allozierbar |

## Entscheidungen

| Nr. | Entscheidung | Begründung |
|---|---|---|
| E1 | Arduino IDE statt PlatformIO | Vorgabe der Entwicklungsumgebung. Abhängigkeiten werden versioniert in `setup.md` dokumentiert. |
| E2 | TFT_eSPI mit eingebautem XPT2046-Touch | auf der Zielhardware erprobt; gemeinsamer SPI-Bus wird unterstützt |
| E3 | `tft_setup.h` im Sketch statt geänderter Bibliotheksdateien | reproduzierbar, übersteht Bibliotheks-Updates, liegt versioniert im Repo |
| E4 | Kein PSRAM: Radar-Sprite mit 8 Bit (ca. 58 KB), sonst Neuzeichnen geänderter Bereiche | M1-Messung: 16 Bit nicht allozierbar (größter Block 110 KB); Neubewertung nach TLS-Messung in M3 |
| E5 | Setup-Portal und OpenSky-Betrieb nicht gleichzeitig | spart Heap (TLS und Webserver parallel sind auf dem WROOM-32 knapp) |
| E6 | TLS mit Zertifikatsprüfung über das Root-CA-Bundle des ESP32-Cores | keine globale Abschaltung der Prüfung, robuster als ein festgepinntes Zertifikat |
| E7 | Kein eigener Proxy und kein Cloud-Backend | Das Gerät arbeitet selbstständig. Ein Proxy wird nur erwogen, wenn sich ein belegter Engpass zeigt. |

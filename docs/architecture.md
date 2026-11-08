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
├── HomeFlightTracker.ino             Einstieg, Pinprüfung, Wahl Normal-/Diagnosemodus
├── tft_setup.h                       TFT_eSPI-Pins (siehe docs/setup.md)
└── src/
    ├── app/App.*                     Ablaufsteuerung: Start/Recovery, Setup-, Normalbetrieb     [M2]
    ├── app/BootRequest.*             Startwunsch über Software-Neustart (RTC-Speicher)         [M2]
    ├── app/PowerManager.*            Dimmen per PWM, Display aus, Nachtruhe mit Tiefschlaf      [Energie]
    ├── app/NightSchedule.*           Zeitfenster der Nachtruhe (host-getestet)                  [Energie]
    ├── app/ProfileSelect.*           automatische Profilwahl nach sichtbaren WLANs (host-getestet)
    ├── board/BoardPins.h             feste Pinbelegung der Trägerplatine                       [M1]
    ├── config/HardwareSettings.*     Rotation + Touch-Kalibrierung im NVS                      [M1]
    ├── config/AppConfig.*            WLAN, Standort, OpenSky, Radar, Geräte-Passwort im NVS    [M2]
    ├── config/Profiles.*             Profile aus WLAN + Standort (NVS „hft-prof“, max. 5)
    ├── diag/HeapReport.*             Heap-Messung                                               [M1]
    ├── geo/Geo.*                     Distanz, Peilung, Bounding Box (host-getestet)             [M2]
    ├── hwtest/HardwareTestApp.*      Diagnosemodus (Hardwaretest aus M1)                        [M1]
    ├── net/WifiManager.*             Station mit Reconnect, Setup-AP, Scan, Verbindungstest     [M2]
    ├── net/SetupPortal.*             Captive Portal, Web-UI, JSON-API, Digest-Auth             [M2]
    ├── net/PortalPage.h              HTML/JS der Weboberfläche                                  [M2]
    ├── opensky/Quota.*               Credit-Berechnung, zulässige Intervalle (host-getestet)    [M2]
    ├── ui/TouchInput.*               Kalibrierung, Touch-Ereignisse                             [M1]
    ├── ui/Widgets.*                  Statusleiste, Schaltflächen, UTF-8-Text mit Umlauten       [M2/M4]
    ├── net/TimeSync.*                NTP, Zeitzone Europa/Berlin                                [M3]
    ├── model/FlightList.*            Flugzeugtabelle fester Größe (max. 100, nächste zuerst)    [M3]
    ├── model/AircraftCategory.h      ADS-B-Kategorie → Symbolform und Klartext
    ├── model/Units.h                 Umrechnung in Luftfahrt-Einheiten (ft, kt, ft/min, NM, FL)
    ├── opensky/StatesParser.*        Streaming-Parser für /states/all (host-getestet)           [M3]
    ├── opensky/OpenSkyClient.*       TLS mit CA-Bundle, OAuth2-Token, /states/all, Rate-Limit   [M3]
    ├── opensky/OpenSkyService.*      Task auf Core 0: Takt, Token-Erneuerung, Backoff, Snapshot [M3]
    └── ui/RadarView.*                Radar im 8-Bit-Sprite, Projektion, Symbole, Trefferlogik   [M4]
```

Die Module ohne Arduino-Abhängigkeit (`geo/`, Parser-Logik, `QuotaTracker`) werden so geschrieben, dass sie sich auf dem Entwicklungsrechner mit einem normalen C++-Compiler testen lassen (siehe [testing.md](testing.md)).

## Betriebsarten (Stand M2)

| Betriebsart | Auslöser | Verhalten |
|---|---|---|
| Setup | keine vollständige Konfiguration, „Setup“ im Statusbildschirm oder seriell `s` | WPA2-Access-Point mit zufälligem Passwort, Captive Portal, Weboberfläche ohne Anmeldung; endet nach 15 min |
| Normal | vollständige Konfiguration | WLAN-Station mit Reconnect, Weboberfläche im Heimnetz mit Digest-Anmeldung, mDNS, Statusbildschirm |
| Diagnose | „Diagnose“ im Statusbildschirm oder seriell `d` | Hardwaretest aus M1; „Neustart“ kehrt zurück |

Setup- und Diagnosewunsch werden über einen Software-Neustart im RTC-Speicher weitergereicht. Nach einem Stromausfall startet das Gerät deshalb immer im Normalbetrieb (sofern konfiguriert) und nie mit einem Setup-WLAN.

## Weboberfläche und API

| Methode | Pfad | Zweck |
|---|---|---|
| GET | `/` | Einrichtungs- bzw. Konfigurationsseite |
| GET | `/api/state` | aktuelle Einstellungen ohne Geheimnisse (nur „gespeichert ja/nein“) |
| GET | `/api/estimate` | Credit-Schätzung für Standort, Reichweite, Intervall, Nutzerstufe |
| GET | `/api/scan` | WLAN-Suche (asynchron, wird abgefragt) |
| POST/GET | `/api/wifi/test` | Verbindungstest starten / Ergebnis (nur Setup-Modus) |
| POST | `/api/config` | Einstellungen prüfen, speichern (auch ins aktive Profil), Neustart |
| POST | `/api/profiles` | Profil anlegen (`add`), löschen (`delete`) oder aktivieren (`activate`, mit Neustart) |

- POST-Anfragen müssen den Header `X-HFT: 1` tragen. Fremde Webseiten können ihn ohne CORS-Freigabe nicht setzen; das schützt gegen Cross-Site-Requests.
- Im Heimnetz verlangt jede Anfrage eine HTTP-Digest-Anmeldung (`admin` + Geräte-Passwort). Das Passwort wird dabei nicht im Klartext übertragen.
- Die Verbindung selbst ist **unverschlüsselt (HTTP)**: Neu eingegebene Geheimnisse (z. B. ein geändertes OpenSky-Secret) laufen im Heimnetz im Klartext. Im Setup-Modus schützt die WPA2-Verschlüsselung des Access-Points.

## OpenSky-Abruf (M3)

```text
OpenSkyService (Task, Core 0)                      App (loop, Core 1)
  warte auf WLAN → warte auf NTP                     │
  Token gültig? sonst OAuth2-Token holen             │
  /states/all (HTTPS, HTTP/1.0, Stream)              │
    └─ StatesParser: Vektor für Vektor ─► FlightList (working)
  sortieren, veröffentlichen ───── Mutex ─────► copyFlights() bei neuer Sequenznummer
  nächster Abruf = Start + Intervall                 └─ Radar / Liste / Details neu zeichnen
```

- **Takt:** Der nächste Abruf ist exakt „Startzeit + Intervall“. Der Credit-Verbrauch ist damit planbar. Touch-Bedienung löst nie einen Abruf aus.
- **Token:** wird bei weniger als 5 Minuten Restlaufzeit erneuert. Bei `401` genau einmal sofort neu anmelden, danach Fehlerstatus mit 10 Minuten Pause.
- **Fehler:**
  - `429`: Pause gemäß `X-Rate-Limit-Retry-After-Seconds`.
  - `5xx`, Netzwerk und Parser: exponentieller Backoff ab dem Intervall, höchstens 15 Minuten.
  - `401`/`403`: Auth-Fehler, Pause 10 Minuten.
- **Speicher:** Die Antwort wird nicht als Ganzes geladen. Jeder State-Vektor wird einzeln mit ArduinoJson gelesen; der Speicherbedarf ist unabhängig von der Anzahl der Flugzeuge. Gespeichert werden höchstens die 100 nächsten Flugzeuge.
- **Datumsgrenze:** Die Box wird in zwei Teilabfragen zerlegt. In Polnähe umfasst die Box alle Längengrade.

## Energiesparen

| Zustand | Auslöser | Verhalten |
|---|---|---|
| Voll | Berührung | Hintergrundbeleuchtung 100 % (PWM 5 kHz auf GPIO15, aktiv LOW) |
| Gedimmt | `dimAfterS` ohne Berührung | Beleuchtung `dimPercent` |
| Aus | `offAfterMin` ohne Berührung | Beleuchtung aus; Abrufe laufen weiter |
| Tiefschlaf | Nachtruhe aktiv und 2 min ohne Berührung | ESP32 im Deep Sleep, Display im Sleep-Modus (ILI9341 `DISPOFF`/`SLPIN`), WLAN aus |

- **Wecken aus dem Tiefschlaf:**
  - per Timer zum Ende der Nachtruhe,
  - per Berührung über `ext0` an GPIO27 (PENIRQ des XPT2046, aktiv LOW, RTC-Pull-up). TFT_eSPI lässt den Touchcontroller nach jeder Messung im Power-Down-Modus mit aktivem PENIRQ (Kommando `0x90`, PD-Bits 00).
  - Ist die IRQ-Leitung beim Einschlafen bereits LOW, wird nur der Timer verwendet, damit das Gerät nicht sofort wieder aufwacht.
- **Gehaltene Pegel im Tiefschlaf** (`gpio_hold_en` + `gpio_deep_sleep_hold_en`): Backlight HIGH (aus), Piezo LOW, TFT-CS und Touch-CS HIGH. Sie werden zu Beginn von `setup()` wieder freigegeben.
- **Nach dem Aufwachen:** Die Firmware startet neu, ohne Startbildschirm und Recovery-Fenster. Die weckende Berührung wird verworfen.
- **Restverbrauch im Tiefschlaf:** Spannungsregler, USB-Seriell-Wandler und Power-LED des DevKit sowie das Display ziehen weiter Strom. Der tatsächliche Wert ist noch zu messen.

## Radar-Anzeige (M4)

| Datenlage | Bedingung | Darstellung |
|---|---|---|
| Warten | noch kein erfolgreicher Abruf | Ringe, Statusmeldung in der Mitte |
| Aktuell | letzter Abruf jünger als max(3 × Intervall, 180 s) | Symbole gelb, am Boden blaugrau, ausgewählt orange |
| Veraltet | älter | alle Symbole grau ohne Richtung, Hinweis „Daten veraltet“ |
| Abgelaufen | älter als max(10 × Intervall, 15 min) | keine Symbole, „Keine aktuellen Daten“ |

- Einzelne Positionen, die laut Zeitstempel älter als 60 s sind, werden grau dargestellt.
- Flugzeuge ohne Kursangabe erscheinen als Kreis statt als Richtungssymbol.
- Zwischen den Abrufen bewegt sich nichts: Es gibt keine Interpolation.

## Geometrie

- Distanz mit der Haversine-Formel, Richtung als Anfangspeilung vom Standort aus. Die Darstellung ist azimutal äquidistant: Abstand und Richtung vom Zentrum sind exakt.
- Die Bounding Box für die Abfrage ergibt sich aus Radius und Puffer. Der Längengrad-Abstand wird mit 1/cos(Breite) skaliert. Polnähe und Datumsgrenze werden gesondert behandelt; an der Datumsgrenze sind zwei Abfragen nötig, oder die Box wird begrenzt.
- Nach dem Laden wird die tatsächliche Entfernung zum Zentrum geprüft. Flugzeuge außerhalb des Radius werden verworfen.

## Ressourcen (Build mit „Minimal SPIFFS“)

| Größe | M1 | M2 |
|---|---|---|
| Programmspeicher | 343 352 Byte (17 %) | 1 097 164 Byte (55 % von 1 966 080) |
| Globale Variablen | 22 516 Byte (6 %) | 50 568 Byte (15 % von 327 680) |
| Freier Heap nach Start im Setup-Modus (AP + Webserver) | – | 220 244 Byte, größter Block 110 580 Byte |

| Größe (M3/M4, Normalbetrieb) | Wert |
|---|---|
| Programmspeicher | 1 352 348 Byte (68 % von 1 966 080) |
| Globale Variablen | 85 808 Byte (26 %), davon 3 Fluglisten à ca. 10 KB |
| Freier Heap nach Start (Sprite reserviert, WLAN verbunden) | 112 148 Byte |
| Freier Heap nach OpenSky-Abrufen | stabil ca. 104 000 Byte, größter Block 38 900 Byte |
| Minimum während TLS-Handshake | ca. 42 900 Byte |
| Dauer eines Abrufs (inkl. TLS-Handshake) | 0,6–1,5 s |

Weitere M1-Messung: freier Heap nach Display-Init (ohne WLAN) 333 156 Byte; Radar-Sprite 240 × 240 mit 8 Bit möglich (59 396 Byte), mit 16 Bit nicht allozierbar.

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
| E8 | Weboberfläche über HTTP mit Digest-Anmeldung statt HTTPS | Ein selbstsigniertes Zertifikat auf dem Gerät erzeugt Browserwarnungen und kostet RAM; die Unverschlüsselung im Heimnetz ist dokumentiert |
| E10 | Umlaute auf dem Display über eigene UTF-8-Ausgabe (Grundbuchstabe + Punkte) | Font 2 hat keine Umlaute; ein zusätzlicher Font mit Latin-1 kostet Flash und Konvertierungsaufwand |
| E11 | Zoom ändert die Abfragereichweite erst beim nächsten regulären Abruf | Touch löst keinen Abruf aus; der Credit-Verbrauch bleibt planbar |
| E12 | Nachtruhe als Tiefschlaf mit Touch-Wecken statt nur „Display aus“ | Gerät läuft zu Hause am Akku; der größte Gewinn liegt in den Nachtstunden ohne Nutzen |
| E14 | Profile als Kopiervorlage: `AppConfig` bleibt die Kopie des aktiven Profils | OpenSky-Dienst, Radar und Validierung bleiben unverändert; ein Wechsel ist ein Speichern mit Neustart |
| E15 | Profile anlegen nur in der Weboberfläche, am Gerät nur wählen | Eine Bildschirmtastatur auf 240 px Breite wäre mühsam und aufwendig; Passwörter tippt man am Handy einfacher |
| E13 | Wecken in der Nachtruhe startet den regulären Abruftakt | Nach dem Wecken sollen aktuelle Daten erscheinen; der Verbrauch ist auf 1 Credit je Intervall während der 2-minütigen Wachphase begrenzt |
| E9 | Werksreset nur lokal (8 s Halten beim Start) | kein Reset über das Netz möglich, auch nicht bei verlorenem Passwort |

# Einrichtung der Entwicklungsumgebung

Das Projekt wird mit der **Arduino IDE** (2.x) gebaut und geflasht.

## 1. Boardpaket

*Werkzeuge → Board → Boardverwalter*: **esp32 von Espressif Systems**, Version **3.3.8** installieren.

## 2. Bibliotheken

*Werkzeuge → Bibliotheken verwalten*:

| Bibliothek | Version | Zweck | ab Meilenstein |
|---|---|---|---|
| TFT_eSPI (Bodmer) | 2.5.43 | Display und Touch | M1 |
| ArduinoJson (Benoit Blanchon) | 7.4.3 | OpenSky-JSON, Web-UI | M3 |

Die Versionen sind getestet. Neuere Versionen erst nach einem erneuten Hardwaretest übernehmen.

### TFT_eSPI-Konfiguration – keine Änderung an der Bibliothek nötig

Die Pin-Konfiguration steht in [`HomeFlightTracker/tft_setup.h`](../HomeFlightTracker/tft_setup.h). TFT_eSPI (ab 2.5) bindet eine Datei mit diesem Namen automatisch ein (`__has_include(<tft_setup.h>)`). Das funktioniert, weil der ESP32-Arduino-Core den Sketch-Ordner in den Include-Pfad aller Übersetzungseinheiten aufnimmt, auch in den der Bibliothek.

`User_Setup.h` und `User_Setup_Select.h` der Bibliothek bleiben unverändert. Ein Update von TFT_eSPI überschreibt also nichts.

Absicherung gegen eine falsche Konfiguration:

- **Beim Kompilieren:** `HomeFlightTracker.ino` bricht mit einer Fehlermeldung ab, wenn `tft_setup.h` nicht geladen wurde oder von `src/board/BoardPins.h` abweicht.
- **Zur Laufzeit:** Die Firmware liest die tatsächlich in die Bibliothek kompilierten Pins aus (`tft.getSetup()`). Bei einer Abweichung meldet sie das seriell und auf dem Display.

## 3. Board-Einstellungen (Menü *Werkzeuge*)

| Einstellung | Wert |
|---|---|
| Board | ESP32 Dev Module |
| CPU Frequency | 240 MHz (WiFi/BT) |
| Flash Size | 4MB (32Mb) |
| Partition Scheme | **Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)** |
| Upload Speed | 921600 (bei Problemen 460800) |
| Core Debug Level | None |

Zum Partitionsschema: Das Standardschema bietet nur 1,2 MB für die Firmware. Mit WLAN, TLS, Webserver und Grafik wird das ab M3 knapp. „Minimal SPIFFS“ bietet 1,9 MB und lässt OTA-Updates für später offen.

## 4. Kompilieren, Hochladen, serieller Monitor

1. `HomeFlightTracker/HomeFlightTracker.ino` öffnen.
2. Port des DevKit wählen.
3. *Hochladen*. Falls der Upload nicht startet, beim Verbinden die **BOOT**-Taste am DevKit gedrückt halten.
4. *Serieller Monitor* mit **115200 Baud** öffnen.

Optional geht das Kompilieren auch auf der Kommandozeile mit `arduino-cli`:

```bash
arduino-cli compile -b esp32:esp32:esp32:PartitionScheme=min_spiffs HomeFlightTracker
```

## 5. Recovery

| Problem | Lösung |
|---|---|
| Touch reagiert falsch oder Schaltflächen sind nicht treffbar | Beim Start, solange „Neu kalibrieren: jetzt Display antippen“ angezeigt wird (2,5 s), das Display berühren. |
| Hardware-Einstellungen zurücksetzen | Seriell `x` senden: Rotation und Kalibrierung werden gelöscht, danach Neustart. |
| Vollständiger Werksreset | Folgt in M2 (lokal am Gerät auslösbar). |

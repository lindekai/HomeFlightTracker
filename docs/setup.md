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

## 5. Gerät einrichten (ohne PC)

1. Gerät einschalten. Ohne vollständige Konfiguration zeigt es **„Setup erforderlich“** mit:
   - WLAN-Name `HomeFlightTracker-Setup`
   - einem **zufälligen WLAN-Passwort** (bei jedem Setup-Start neu, nur am Display sichtbar)
   - der Adresse `http://192.168.4.1`
2. Mit dem Smartphone in diesem WLAN anmelden. Die Einrichtungsseite öffnet sich automatisch (Captive Portal). Falls nicht: im Browser `http://192.168.4.1` öffnen.
3. Auf der Einrichtungsseite ausfüllen:
   - **WLAN:** Netz suchen oder eingeben, Passwort, optional „Verbindung testen“.
   - **Standort:** Breite und Länge in Dezimalgrad, optional eine Bezeichnung. Die Ortssuche folgt in M5.
   - **OpenSky:** Client-ID und Client-Secret des API-Clients aus dem OpenSky-Konto. Ohne Zugang läuft nur der anonyme Notbetrieb mit mindestens 300 s Intervall.
   - **Radar:** Reichweite und Abrufintervall. Die Seite rechnet den Tagesverbrauch an Credits live aus. Intervalle, die mehr als 80 % des Tageskontingents verplanen, werden abgelehnt.
   - **Geräte-Passwort:** mindestens 8 Zeichen. Schützt die Weboberfläche im Heimnetz (Benutzer `admin`).
4. **Speichern & neu starten.** Das Gerät verbindet sich mit dem Heimnetz und zeigt den Statusbildschirm.

Der Setup-Modus endet nach **15 Minuten** ohne Abschluss. Bei unvollständiger Konfiguration startet er dann mit einem neuen Passwort neu.

### Energie

| Einstellung | Standard | Wirkung |
|---|---|---|
| Dimmen nach | 120 s | Hintergrundbeleuchtung nach dieser Zeit ohne Berührung auf die gedimmte Helligkeit (0 = nie) |
| Helligkeit gedimmt | 20 % | 5–100 % |
| Display aus nach | 10 min | Beleuchtung aus (0 = nie); das Gerät ruft weiter Daten ab |
| Nachtruhe | an, 23:00–06:00 | ESP32 im Tiefschlaf: keine Abrufe, Weboberfläche nicht erreichbar |

Die erste Berührung eines gedimmten oder dunklen Displays weckt nur und löst keine Aktion aus.

In der Nachtruhe weckt eine Berührung das Gerät. Es startet neu, verbindet sich, ruft im regulären Takt ab und schläft nach 2 Minuten ohne Berührung wieder ein; jedes Wecken kostet also mindestens 1 Credit. Am Ende der Nachtruhe wacht es per Timer auf. Die Nachtruhe greift erst, wenn die Uhrzeit per NTP bekannt ist.

### Spätere Änderungen

- Im Heimnetz `http://<IP-Adresse>` oder `http://homeflighttracker.local` öffnen. Die IP-Adresse steht im Statusbildschirm. mDNS funktioniert nicht in jedem Netz und auf jedem Gerät; die IP-Adresse geht immer.
- Anmelden mit Benutzer `admin` und dem Geräte-Passwort.
- Leere Passwort- und Secret-Felder bedeuten „unverändert“. Gespeicherte Geheimnisse werden nie angezeigt.
- Neues WLAN, wenn das alte nicht mehr erreichbar ist: im Statusbildschirm **Setup** → **Ja**.

## 6. Recovery

| Problem | Lösung |
|---|---|
| Touch reagiert falsch oder Schaltflächen sind nicht treffbar | Beim Start (2,5 s lang) das Display kurz antippen: Neukalibrierung. |
| Geräte-Passwort vergessen oder Gerät unbrauchbar konfiguriert | **Werksreset:** beim Start das Display antippen und **8 Sekunden gedrückt halten** (Countdown). Löscht alle Einstellungen inkl. Kalibrierung. Loslassen vorher bricht ab. |
| Diagnose (Testbild, Touch-Test, Heap) | Statusbildschirm → **Diagnose**. Zurück über Info → **Neustart**. |

Der Werksreset ist absichtlich nur lokal am Gerät auslösbar und nicht über das Netz.

Serielle Befehle (115200 Baud) im Normalbetrieb: `h` Heap, `s` Setup-Modus, `d` Diagnose, `?` Hilfe.

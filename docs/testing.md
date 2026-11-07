# Tests

## Teststrategie

| Ebene | Werkzeug | Inhalt |
|---|---|---|
| Build | Arduino IDE / `arduino-cli compile` | fehlerfrei, keine Warnungen im eigenen Code |
| Host-Unit-Tests (ab M3) | C++-Compiler auf dem Entwicklungsrechner | Geometrie, JSON-Auswertung mit Mock-Daten, Kontingentlogik |
| Hardwaretests | Firmware auf dem Gerät + serieller Monitor | Display, Touch, Speicher, WLAN, TLS, OpenSky |
| Langzeittest (M5) | Gerät ≥ 24 h im Dauerbetrieb | Heap-Verlauf, Reconnects, Token-Erneuerung |

## M1 – Hardware-Machbarkeitstest

**Vorbereitung:** Firmware gemäß [setup.md](setup.md) flashen und den seriellen Monitor mit 115200 Baud öffnen.

**Bedienung der Test-Firmware:**

| Bildschirm | Inhalt | Bedienung |
|---|---|---|
| Startbild | Hinweis „Neu kalibrieren“ | innerhalb von 2,5 s antippen erzwingt die Kalibrierung |
| Kalibrierung | vier Eckmarken nacheinander | jeweils genau antippen |
| Testbild | Randlinie, farbige Eckmarken, Pfeil „OBEN“, Farbbalken, Graustufen | Tippen → Touch-Test |
| Touch-Test | Raster aus Trefferzonen (80 × 80 px), Koordinaten, Rohwerte, Druck, IRQ-Pegel | Zone antippen; Schaltflächen Testbild / Kalibr. / Drehen / Info |
| Info | Chip, Flash, PSRAM, Rotation, Kalibrierwerte, Sprite-Test, Heap, Laufzeit | Touch / Licht / Testbild |

Serielle Befehle: `c` kalibrieren, `r` drehen, `b` Licht-Test, `h` Heap, `k` Kalibrierwerte, `x` Einstellungen löschen und neu starten, `?` Hilfe.

### Prüfschritte und Abnahmekriterien

| Nr. | Prüfung | Erwartung | Ergebnis |
|---|---|---|---|
| 1.1 | Platinenrevision und ESP32 | V01-03, ESP32-WROOM-32 | ✅ abgelesen |
| 1.2 | Pinprüfung | seriell: „Pins stimmen mit BoardPins.h ueberein“ | ✅ TFT_eSPI 2.5.43, Treiber 0x9341, 27 MHz |
| 2.1 | Testbild in Rotation 0 | Randlinie an allen vier Kanten vollständig, „OBEN“ zeigt im Gehäuse nach oben | ☐ |
| 2.2 | Farben | Balken R, G, B, W, Y, C, M in der beschrifteten Farbe; Graustufen gleichmäßig | ☐ |
| 2.3 | Rotation | mit „Drehen“ alle vier Rotationen; Eckmarken wandern korrekt mit; Rotation bleibt nach Neustart erhalten | ☐ |
| 2.4 | Hintergrundbeleuchtung | an nach dem Start; „Licht“ schaltet 1,5 s aus und wieder an | ☐ |
| 3.1 | Kalibrierung | wird beim ersten Start automatisch angeboten; Werte plausibel (seriell `k`) | ✅ R0: 244 3564 247 3468 0x02 (x invertiert) |
| 3.2 | Trefferzonen | jede Zone (Hochformat 3 × 3, Querformat 4 × 2) wird beim Antippen der Mitte korrekt erkannt; die Punktspur liegt unter dem Finger | ☐ |
| 3.3 | Persistenz | nach Strom aus/an keine erneute Kalibrierung nötig, Trefferzonen weiterhin korrekt | ☐ |
| 3.4 | Recovery | Antippen beim Start erzwingt die Kalibrierung; `x` setzt zurück | ☐ |
| 3.5 | IRQ-Leitung | Anzeige wechselt bei Berührung zwischen „aktiv“ und „inaktiv“ (nur informativ) | ☐ |
| 7.1 | Heap-Basiswert | Werte „Start“, „nach Display-Init“ und „nach Sprite-Test“ protokolliert | ✅ siehe Messprotokoll |
| 7.2 | Sprite-Machbarkeit | Ergebnis 240 × 240 bei 8 und 16 Bit protokolliert | ✅ 8 Bit ok, 16 Bit nicht möglich |
| 7.3 | Stabilität | 30 min Laufzeit: Werte „periodisch“ ohne fallenden Trend | ☐ |

### Messprotokoll M1

| Messpunkt | frei (B) | min (B) | max. Block (B) |
|---|---|---|---|
| Start | 333 404 | 327 620 | 110 580 |
| nach Display-Init | 333 156 | 327 256 | 110 580 |
| nach Sprite-Test | 333 156 | 273 236 | 110 580 |
| periodisch nach 30 min | | | |

| Sprite 240 × 240 | Ergebnis | belegter Heap |
|---|---|---|
| 8 Bit | ✅ OK | 59 396 B |
| 16 Bit | ❌ nicht allozierbar | – (benötigt 115 200 B, größter freier Block nur 110 580 B) |

Messung vom 2026-11-08 ohne aktives WLAN. **Folgerung:** Ein 16-Bit-Vollsprite für das Radar ist auf dem WROOM-32 nicht möglich. Für M4 ist ein 8-Bit-Sprite (ca. 58 KB) vorgesehen; nach den TLS-Messungen in M3 wird erneut geprüft.

## Spätere Testkataloge (Übersicht)

- **M2:** Start ohne Konfiguration, AP-Passwort, Captive Portal, WLAN-Scan, falsches Passwort, Speichern + Neustart, WLAN-Wechsel, Werksreset, kein offener Endpunkt im LAN.
- **M3:** TLS mit Zertifikatsprüfung, Token holen und erneuern, `/states/all` mit kleiner Bounding Box, Mock-Daten (Null-Felder, fehlende Koordinaten, doppelte Callsigns, Kurs 0°/359°, leere Ergebnisse), HTTP 401/403/429/5xx, Heap über ≥ 100 Abrufe.
- **M4:** Geometrie (Nord, Ost, Süd, West, Entfernung, Radarrand, hohe Breiten), Auswahl bei überlappenden Symbolen, kein Abruf durch Touch, Kennzeichnung veralteter Daten.
- **M5:** Langzeittest ≥ 24 h, Stromunterbrechung, WLAN-Ausfall, Token-Ablauf, unabhängiges Code-Review.

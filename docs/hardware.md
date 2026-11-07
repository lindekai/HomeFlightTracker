# Hardware

## Komponenten

| Komponente | Ausführung | Status |
|---|---|---|
| Trägerplatine | AZ-Touch MOD, aufgedruckte Revision **V01-03** | ✅ am Gerät abgelesen |
| Schaltplan | „AZ-Touch MOD schematic V01-03-01“ (Zihatec, 08.11.2020) | ✅ passt zur Platine |
| Mikrocontroller | ESP32 DevKit C mit **ESP32-WROOM-32** (4 MB Flash, kein PSRAM) | ✅ am Gerät abgelesen |
| Display | 2,4″ TFT 240 × 320, ILI9341, SPI | ✅ laut Schaltplan, Praxistest erfolgreich |
| Touch | XPT2046, resistiv, SPI | ✅ laut Schaltplan, Praxistest erfolgreich |
| Piezo | LS1 LET7525AS-3.6L-2.7-15-R | ⚠️ Ansteuerungsart nicht verifiziert, im MVP ungenutzt |

## Pin-Matrix

Quelle ist ausschließlich der Originalschaltplan V01-03-01, Steckplatz X2 „AZ-Delivery ESP32 DEV KIT C“. Die Pinbelegung wurde zusätzlich in einem Praxistest auf identischer Hardware bestätigt: Display über TFT_eSPI, Hintergrundbeleuchtung über GPIO15 = LOW, Touch am gemeinsamen SPI-Bus.

| Funktion | GPIO | Netz / Ziel im Schaltplan | Pegel / Hinweis | Status |
|---|---|---|---|---|
| SPI MOSI (TFT + Touch T_DIN) | 23 | MOSI → K5/K6 Pin 6 und Pin 12 | VSPI | ✅ |
| SPI MISO (TFT + Touch T_DO) | 19 | MISO → K5/K6 Pin 9 und Pin 13 | VSPI | ✅ |
| SPI SCK (TFT + Touch T_CLK) | 18 | SCK → K5/K6 Pin 7 und Pin 10 | VSPI | ✅ |
| TFT CS | 5 | CS → K5/K6 Pin 3 | **Strapping-Pin** | ✅ |
| TFT DC | 4 | DC → K5/K6 Pin 5 | – | ✅ |
| TFT RESET | 22 | RESET → K5/K6 Pin 4 | – | ✅ |
| Hintergrundbeleuchtung | 15 | LED_32 → R1 1 kΩ → T1 BC807-40 (PNP, Emitter +3V3) → LED Pin 8 | **aktiv LOW**, Strapping-Pin | ✅ |
| Touch CS | 14 | T_CS → K5/K6 Pin 11 | – | ✅ |
| Touch IRQ | 27 | T_IRQ → K5/K6 Pin 14 | PENIRQ, aktiv LOW | ✅ |
| Piezo | 21 | BEEPER → R2 1 kΩ → T2 BC817-40 (NPN), LS1 an +5V | aktiv HIGH | ✅ Pin / ⚠️ Ansteuerung |
| Frei, nur Eingang | 36 (A0), 39, 34, 35 | Stiftleiste K4 | kein interner Pull-up | ✅ |
| Frei, Ein- und Ausgang | 32, 25, 26, 33 | Stiftleiste K7 | – | ✅ |
| UART TX/RX | 1 / 3 | TxD/RxD, auch am D1-Mini-Steckplatz | USB-Seriell des DevKit | ✅ |

K5 ist der Anschluss für das 2,8″-Display, K6 der für das 2,4″-Display. Beide sind parallel verdrahtet.

### Nicht belegte ESP32-Pins auf der Platine

GPIO0, 2, 12, 13, 16 und 17 sowie die Flash-Pins 6–11 sind auf der Trägerplatine nicht beschaltet. GPIO6–11 sind auf dem WROOM-32 intern für den Flash belegt und dürfen nicht verwendet werden.

## Strapping-Pins und Besonderheiten beim Booten

| GPIO | Funktion beim Booten | Auswirkung auf der Platine |
|---|---|---|
| 5 | SDIO-Timing (Pull-up) | TFT CS ist beim Booten inaktiv (HIGH). Unkritisch. |
| 15 | Bootmeldungen über UART0 (Pull-up) | Die Hintergrundbeleuchtung bleibt bis zur Initialisierung aus (HIGH = aus). Unkritisch. |
| 0, 2, 12 | Bootmodus und Flash-Spannung | nicht beschaltet, keine Konflikte |
| 14 | gibt beim Booten kurz PWM aus | Touch CS toggelt kurz. Unkritisch, da der SPI-Bus inaktiv ist. |
| 21 | ohne Pull-down | Die Firmware setzt den Pin sofort auf LOW, damit der Piezo stumm bleibt. |

GPIO 4, 14, 15 und 27 gehören zum ADC2. Der ADC2 ist bei aktivem WLAN nicht nutzbar. Das ist hier ohne Belang, weil diese Pins nur digital verwendet werden.

## Stromversorgung

| Element | Bauteil | Hinweis |
|---|---|---|
| Eingangsklemme K1 | Verpolschutz D2 STPS340U, TVS-Diode D1 SM6T39CA | – |
| Spannungsregler | IC1 LM2576R-05 (Schaltregler) mit L1 100 µH | erzeugt +5V |
| +5V | DevKit-Pin „5V“, Piezo | – |
| +3V3 | Regler des DevKit | versorgt Display, Touch und Backlight-Transistor |

⚠️ **Widerspruch im Datenblatt Rev B:** Seite 1 nennt eine Eingangsspannung von 9–35 V, Seite 2 nennt 15–30 V. Bis zur Klärung ein bereits erprobtes Netzteil verwenden.

⚠️ **Gleichzeitige Versorgung über USB und Klemme** ist nicht geprüft. Ob das DevKit eine Diode zwischen USB-VBUS und dem 5V-Pin hat, hängt von der Ausführung ab. Zum Flashen bei angeschlossenem Netzteil zuerst die Ausführung prüfen oder das Netzteil abziehen.

## Hinweise zur Bestückung

- **Niemals gleichzeitig einen D1 Mini und einen ESP32 stecken.** Beide Steckplätze hängen an denselben Netzen (SPI, CS, DC, RESET, LED, Touch, UART).
- Antennenausrichtung und Steckrichtung des DevKit entsprechend dem Bestückungsdruck einhalten.

## Quellen

- Zihatec, Projektseite AZ-Touch / ArduiTouch ESP: <https://www.hwhardsoft.de/deutsch/projekte/arduitouch-esp/>
  - „Datasheet AZ-Touch MOD Rev B“ (11/2020)
  - „AZ-Touch MOD schematic V01-03-01“
- Referenzkopie des Schaltplans (bytegleich mit der Herstellerdatei geprüft): <https://open-boat-projects.org/wp-content/uploads/2021/01/AZ-Touch-MOD-schematic-V01-03-01-1.pdf>

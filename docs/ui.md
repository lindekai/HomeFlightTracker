# Bedienkonzept und Mockups

Die Mockups zeigen den geplanten Zielzustand (M2/M4) im Maßstab 1:1 zur Displayauflösung (240 × 320 px, Hochformat). Alle Flugdaten darin sind **Beispieldaten**.

## Gerät

| Radar | Details | Liste | Setup erforderlich |
|---|---|---|---|
| <img src="mockups/radar.svg" width="200" alt="Radar"> | <img src="mockups/details.svg" width="200" alt="Details"> | <img src="mockups/list.svg" width="200" alt="Liste"> | <img src="mockups/setup-required.svg" width="200" alt="Setup erforderlich"> |

### Radar

- **Statusleiste:** WLAN- und OpenSky-Status (grün = ok, gelb = eingeschränkt, rot = Fehler) und Uhrzeit. Darunter stehen die Anzahl der Flugzeuge und der Radius.
- **Radar:** Norden oben, vier Entfernungsringe, Standortkreuz in der Mitte. Die Symbole zeigen in Richtung von `true_track`.
- **Auswahl:** Antippen markiert ein Flugzeug (orange) und blendet eine Kurzinfo ein. Ein zweites Antippen öffnet die Details. Liegen Symbole übereinander, wird das nächstgelegene im Trefferradius gewählt; erneutes Antippen wechselt zum nächsten Kandidaten.
- **Veraltete Positionen** werden grau und ohne Bewegung dargestellt und nach einer festgelegten Zeit ausgeblendet.
- **Aktualität:** „Stand hh:mm:ss · vor n s“ ist immer sichtbar.
- **Bedienleiste:** Reichweite − / +, Liste, Menü. Jede Schaltfläche ist mindestens 56 × 30 px groß.

### Details

Es werden nur Werte aus dem OpenSky-State-Vektor angezeigt, mit eindeutigen Einheiten (m, km/h, °, m/s, km). Fehlende Werte erscheinen als „–“. Ein fester Hinweis stellt klar, dass Typ, Registrierung und Start/Ziel nicht geliefert werden.

### Liste

Die Liste ist nach Entfernung sortiert und zeigt sieben Zeilen pro Seite, die mit ▲ / ▼ geblättert werden. Antippen einer Zeile öffnet die Details.

### Setup erforderlich

- Das Gerät zeigt SSID, ein **pro Setup-Start neu erzeugtes WLAN-Passwort** und die Fallback-Adresse an.
- Der Setup-AP beendet sich automatisch nach einer Zeitspanne ohne Abschluss. Ein dauerhaft offenes Konfigurations-WLAN gibt es nicht.

## Einrichtungsportal (Smartphone)

<img src="mockups/setup-portal.svg" width="300" alt="Einrichtungsportal">

Das Portal ist in fünf Schritte gegliedert: WLAN, Standort (Suche oder Koordinaten), OpenSky-Zugang, Radar und Geräte-Passwort.

- Den Tagesverbrauch an Credits rechnet das Portal live aus der Reichweite und dem Intervall aus.
- Geheimnisse werden nach dem Speichern nie wieder angezeigt.

## Umsetzungsstand

- **M2:** Statt des Radars zeigt das Gerät vorerst einen Statusbildschirm (WLAN, IP, Standort, Radius, Credit-Verbrauch) mit den Schaltflächen Setup, Kalibr. und Diagnose.
- **Abweichungen des Portals vom Mockup:**
  - Die Ortssuche (Geocoding) folgt in M5; bis dahin werden Koordinaten eingegeben.
  - „Verbindung testen“ für OpenSky folgt in M3.
- **Ausrichtung:** Die Oberfläche ist für das Hochformat (Rotation 0) ausgelegt.

## Gestaltungsregeln

- Dunkles, sachliches Aviation-Design. Lesbarkeit und Performance haben Vorrang vor Effekten.
- Farben: Hintergrund `#050B10`, Radarringe `#1F6F4A`, Flugzeuge `#FFD34D`, Auswahl `#FF7A3D`, Text `#D8E6DC`, gedämpfter Text `#8FB3A0`, veraltet `#5D6B66`.
- **Schriften:** Die eingebauten Fonts von TFT_eSPI enthalten keine Umlaute. Bis M4 wird ein Font mit Latin-1-Zeichensatz eingebunden. Die M1-Testoberfläche umschreibt Umlaute deshalb noch.

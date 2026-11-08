# Entwurf: Symbole nach Flugzeugkategorie und Profile (WLAN + Standort)

Stand: 2026-11-08, abgestimmt vor der Umsetzung (Firmware 0.7.0 und 0.8.0).

## A – Symbole nach Flugzeugkategorie (0.7.0)

- `/states/all` wird mit `extended=1` abgefragt; Feld 17 enthält die ADS-B-Emitterkategorie. Die Credits ändern sich nicht. Fehlt das Feld, gilt 0 (keine Angabe).
- Drei Symbolformen, jeweils 17 × 17 px in 24 Richtungen mit Kantenglättung, erzeugt von `tools/gen_icons.py`:

  | Form | Kategorien |
  |---|---|
  | Hubschrauber (Rumpf, Heckausleger, Rotorkreis) | 8 Drehflügler |
  | Kleinflugzeug (gerade Flügel, Propeller) | 2 Leicht, 9 Segelflugzeug, 12 Ultraleicht |
  | Verkehrsflugzeug (bisheriges Symbol) | alle übrigen, 0/1 ohne Angabe |

- Die Details zeigen die Kategorie im Klartext („k. A.“ ohne Angabe).
- Zuordnung und Klartext liegen in `src/model/AircraftCategory.h` (Arduino-unabhängig, Host-Tests).

## B – Profile aus WLAN und Standort (0.8.0)

### Daten

- Bis zu 5 Profile, je Name (max. 24 Zeichen), WLAN-Name, WLAN-Passwort, Breite, Länge, Ortsname.
- Gespeichert im NVS-Namespace `hft-prof` als ein Blob (`store`: Version, Anzahl, aktives Profil, Profile).
- `AppConfig` bleibt die Kopie des aktiven Profils. OpenSky-Abruf, Radar und Validierung lesen weiter nur `AppConfig`.
- Migration: Gibt es noch keine Profile, wird aus der vorhandenen Konfiguration Profil 1 (Name = Ortsname, sonst „Profil 1“).
- Speichern im Portal (Abschnitte 1 und 2) aktualisiert zugleich das aktive Profil.

### Weboberfläche

- Neuer Abschnitt „Profile“: Liste mit Name, WLAN und Ort; aktives Profil markiert; Löschen (nicht für das aktive Profil); Aktivieren.
- „Als neues Profil speichern“ legt aus WLAN und Standort der Abschnitte 1 und 2 ein Profil unter einem Namen an. Ein leeres WLAN-Passwortfeld übernimmt das gespeicherte Passwort.
- Passwörter werden nie ausgeliefert.

### Gerät

- Menü-Schaltflächen: „Radar“, „Profil“, „Setup“, „Mehr“. „Mehr“ blendet „Zurück“, „Kalibr.“ und „Diag.“ ein.
- „Profil“ öffnet die Profilliste (aktives Profil hervorgehoben). Antippen → Bestätigung Ja/Nein → `AppConfig` übernehmen, speichern, Neustart.

### Automatische Wahl beim Start

- Vor dem Verbinden ein WLAN-Scan (etwa 2–3 s), nicht nach dem Tiefschlaf.
- Reihenfolge: WLAN des aktiven Profils sichtbar → bleibt. Sonst das Profil mit dem stärksten sichtbaren WLAN. Kein bekanntes WLAN → aktives Profil bleibt, Wiederverbindung wie bisher.
- Die Auswahl ist eine reine Funktion (`profile_select::choose`) und per Host-Test geprüft.

### Kontingent und Fehler

- Ein anderer Standort ändert die Fläche der Abfrage. Jedes Profil wird beim Speichern und vor dem Wechsel mit `app_config::validate` geprüft; ein unzulässiges Profil wird abgelehnt (Portal: Fehlermeldung, Gerät: Hinweis), eine automatische Wahl überspringt es.

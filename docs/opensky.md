# OpenSky Network

Stand der Prüfung: 2026-11-08. Quellen stehen am Ende des Dokuments.

## Endpunkt

`GET https://opensky-network.org/api/states/all?lamin=…&lomin=…&lamax=…&lomax=…`

Die Abfrage wird auf eine Bounding Box um den Standort begrenzt, die aus Radius und Puffer berechnet wird. Nach dem Laden wird zusätzlich die tatsächliche Entfernung geprüft.

## Authentifizierung

- **Nur OAuth2 Client Credentials.** Basic Auth mit Benutzername und Passwort wird nicht mehr akzeptiert.
- Token-Endpunkt: `https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token`
- Ein **Token ist 30 Minuten gültig**. Plan: nach ca. 25 Minuten proaktiv erneuern. Bei `401` genau einmal ein neues Token holen und den Abruf wiederholen. Es wird nicht bei jedem Abruf ein neues Token bezogen.
- Client-ID und Client-Secret gibt der Benutzer im Setup-Portal ein. Sie werden nie geloggt und nie unmaskiert angezeigt.

## Kontingente (Credits)

| Nutzerstufe | Credits | Erneuerung |
|---|---|---|
| Anonym | 400 | täglich |
| Registriert (Standard) | 4 000 | täglich |
| Aktiver Feeder | 8 000 | täglich |

Kosten von `/states/all` je nach Fläche der Bounding Box (Breitenspanne × Längenspanne):

| Fläche | Credits |
|---|---|
| ≤ 25 Quadratgrad | 1 |
| 25–100 | 2 |
| 100–400 | 3 |
| > 400 oder global | 4 |

**Beispiel:** 100 km Radius bei 52,3° N ergibt etwa 1,8° × 2,9° ≈ 5,3 Quadratgrad, also **1 Credit** pro Abruf.

### Abrufintervall und Tagesverbrauch (registriert, 1 Credit/Abruf)

| Intervall | Abrufe pro Tag | Anteil an 4 000 |
|---|---|---|
| 30 s (Minimum) | 2 880 | 72 % |
| **60 s (Standard)** | **1 440** | **36 %** |
| 120 s | 720 | 18 % |
| < ca. 22 s | > 4 000 | Kontingent überschritten → nicht zulässig |

Anonymer Betrieb ist nur als klar gekennzeichneter Notbetrieb vorgesehen: mindestens 300 s Intervall (288 Credits pro Tag). Die zeitliche Auflösung beträgt authentifiziert 5 s und anonym 10 s, kürzere Intervalle bringen also keinen Mehrwert.

## Rate-Limit-Behandlung

- `X-Rate-Limit-Remaining` wird ausgewertet und angezeigt.
- Bei `429 Too Many Requests` gilt die Wartezeit aus `X-Rate-Limit-Retry-After-Seconds`.
- Bei `5xx` und Netzwerkfehlern: exponentieller Backoff mit Obergrenze.
- `401` und `403` werden als Auth- oder Berechtigungsfehler im Status angezeigt, ohne Endlosschleife.

## Datenfelder (State-Vektor)

| Index | Feld | Einheit | Verwendung |
|---|---|---|---|
| 0 | icao24 | – | Schlüssel, Detailansicht |
| 1 | callsign | – | Anzeige (kann null oder leer sein) |
| 2 | origin_country | – | Detailansicht (aus der ICAO-Adresse abgeleitet) |
| 3 | time_position | Unix-s | Alter der Position |
| 4 | last_contact | Unix-s | „Letzter Kontakt“ |
| 5 / 6 | longitude / latitude | ° WGS-84 | Position (null → nicht darstellen) |
| 7 | baro_altitude | m | Höhe |
| 8 | on_ground | bool | Kennzeichnung am Boden |
| 9 | velocity | m/s | Anzeige in km/h |
| 10 | true_track | ° ab Nord, im Uhrzeigersinn | Ausrichtung des Symbols |
| 11 | vertical_rate | m/s | Steigrate |
| 13 | geo_altitude | m | Höhe (geometrisch) |
| 14 | squawk | – | Detailansicht |
| 16 | position_source | 0 ADS-B, 1 ASTERIX, 2 MLAT, 3 FLARM | Detailansicht |

Die API liefert **weder Flugzeugtyp noch Registrierung noch Start- oder Zielflughafen**. Diese Angaben werden weder angezeigt noch aus dem Callsign abgeleitet.

## Nutzungsbedingungen und Attribution

- Laut API-Dokumentation ist die REST-API für **Forschung und nicht-kommerzielle Zwecke** vorgesehen. Für kommerzielle Nutzung ist eine Vereinbarung mit OpenSky nötig.
- In der Oberfläche und im README wird OpenSky als Datenquelle genannt.
- ⚠️ **Offen:** Die Seite <https://opensky-network.org/about/terms-of-use> war bei der Prüfung automatisiert nicht abrufbar (HTTP 403). Sie wird in M3 manuell gelesen, das Ergebnis wird hier festgehalten.

## Quellen

- REST-API-Dokumentation: <https://github.com/openskynetwork/opensky-api/blob/master/docs/free/rest.rst>
- Felder des State-Vektors: <https://github.com/openskynetwork/opensky-api/blob/master/docs/free/states-response.rst>
- API-Übersicht: <https://openskynetwork.github.io/opensky-api>
- FAQ: <https://dev.opensky-network.org/about/faq>

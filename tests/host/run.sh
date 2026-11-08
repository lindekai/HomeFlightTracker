#!/usr/bin/env bash
# Baut und startet die Host-Unit-Tests (Arduino-unabhängige Module).
set -euo pipefail
cd "$(dirname "$0")"
SRC=../../HomeFlightTracker/src
OUT="${TMPDIR:-/tmp}/homeflighttracker-hosttests"
# ArduinoJson ist header-only und läuft auch auf dem Entwicklungsrechner.
ARDUINOJSON="${ARDUINOJSON:-$HOME/Documents/Arduino/libraries/ArduinoJson/src}"
c++ -std=c++17 -Wall -Wextra -Werror -O1 -I"$ARDUINOJSON" -o "$OUT" \
  test_main.cpp "$SRC/geo/Geo.cpp" "$SRC/opensky/Quota.cpp" "$SRC/opensky/StatesParser.cpp" \
  "$SRC/model/FlightList.cpp"
"$OUT"

#include "StatesParser.h"

#include <ArduinoJson.h>
#include <math.h>
#include <string.h>

namespace {

// Indizes im State-Vektor (siehe docs/opensky.md)
constexpr size_t kIcao24 = 0;
constexpr size_t kCallsign = 1;
constexpr size_t kOriginCountry = 2;
constexpr size_t kTimePosition = 3;
constexpr size_t kLastContact = 4;
constexpr size_t kLongitude = 5;
constexpr size_t kLatitude = 6;
constexpr size_t kBaroAltitude = 7;
constexpr size_t kOnGround = 8;
constexpr size_t kVelocity = 9;
constexpr size_t kTrueTrack = 10;
constexpr size_t kVerticalRate = 11;
constexpr size_t kGeoAltitude = 13;
constexpr size_t kSquawk = 14;
constexpr size_t kPositionSource = 16;
constexpr size_t kCategory = 17;  // nur mit extended=1
constexpr size_t kMinimumFields = 17;

void skipWhitespace(opensky::JsonByteSource& in) {
  for (;;) {
    const int c = in.peek();
    if (c != ' ' && c != '\n' && c != '\r' && c != '\t') return;
    in.read();
  }
}

// Liest bis einschließlich token. false, wenn der Strom vorher endet.
bool skipPast(opensky::JsonByteSource& in, const char* token) {
  const size_t length = strlen(token);
  size_t matched = 0;
  for (;;) {
    const int c = in.read();
    if (c < 0) return false;
    if (c == token[matched]) {
      if (++matched == length) return true;
    } else {
      matched = c == token[0] ? 1 : 0;
    }
  }
}

bool readInteger(opensky::JsonByteSource& in, int64_t& value) {
  skipWhitespace(in);
  bool negative = false;
  if (in.peek() == '-') {
    negative = true;
    in.read();
  }
  bool any = false;
  value = 0;
  while (in.peek() >= '0' && in.peek() <= '9') {
    value = value * 10 + (in.read() - '0');
    any = true;
  }
  if (negative) value = -value;
  return any;
}

float optionalFloat(JsonVariantConst value) {
  return value.is<float>() ? value.as<float>() : NAN;
}

void copyString(JsonVariantConst value, char* target, size_t size, bool trimTrailingSpaces) {
  const char* text = value.is<const char*>() ? value.as<const char*>() : "";
  strncpy(target, text, size - 1);
  target[size - 1] = '\0';
  if (!trimTrailingSpaces) return;
  for (size_t i = strlen(target); i > 0 && target[i - 1] == ' '; --i) target[i - 1] = '\0';
}

bool toAircraft(JsonArrayConst state, geo::LatLon center, Aircraft& out) {
  if (state.size() < kMinimumFields) return false;
  if (!state[kLongitude].is<float>() || !state[kLatitude].is<float>()) return false;
  const geo::LatLon position{state[kLatitude].as<double>(), state[kLongitude].as<double>()};
  if (!geo::isValid(position)) return false;

  Aircraft aircraft{};
  copyString(state[kIcao24], aircraft.icao24, sizeof(aircraft.icao24), false);
  copyString(state[kCallsign], aircraft.callsign, sizeof(aircraft.callsign), true);
  copyString(state[kOriginCountry], aircraft.originCountry, sizeof(aircraft.originCountry), false);
  copyString(state[kSquawk], aircraft.squawk, sizeof(aircraft.squawk), false);
  aircraft.latitude = static_cast<float>(position.lat);
  aircraft.longitude = static_cast<float>(position.lon);
  aircraft.baroAltitudeM = optionalFloat(state[kBaroAltitude]);
  aircraft.geoAltitudeM = optionalFloat(state[kGeoAltitude]);
  aircraft.velocityMs = optionalFloat(state[kVelocity]);
  aircraft.trackDeg = optionalFloat(state[kTrueTrack]);
  aircraft.verticalRateMs = optionalFloat(state[kVerticalRate]);
  aircraft.timePosition = state[kTimePosition] | 0;
  aircraft.lastContact = state[kLastContact] | 0;
  aircraft.onGround = state[kOnGround] | false;
  aircraft.positionSource = state[kPositionSource].is<int>() ? state[kPositionSource].as<uint8_t>() : 255;
  aircraft.category = state[kCategory].is<int>() ? state[kCategory].as<uint8_t>() : 0;
  aircraft.distanceKm = static_cast<float>(geo::distanceKm(center, position));
  aircraft.bearingDeg = static_cast<float>(geo::initialBearingDeg(center, position));
  out = aircraft;
  return true;
}

}  // namespace

size_t opensky::JsonByteSource::readBytes(char* buffer, size_t length) {
  size_t count = 0;
  while (count < length) {
    const int c = read();
    if (c < 0) break;
    buffer[count++] = static_cast<char>(c);
  }
  return count;
}

bool opensky::parseStates(JsonByteSource& in, geo::LatLon center, double rangeKm, FlightList& out,
                          ParseStats& stats) {
  stats = ParseStats{};
  // OpenSky liefert {"time":<Unix-Zeit>,"states":[[…],[…]]} bzw. "states":null ohne Treffer.
  if (!skipPast(in, "\"time\":") || !readInteger(in, stats.apiTime)) return false;
  if (!skipPast(in, "\"states\":")) return false;
  skipWhitespace(in);
  if (in.peek() == 'n') return skipPast(in, "null");
  if (in.read() != '[') return false;

  JsonDocument doc;
  for (;;) {
    skipWhitespace(in);
    if (in.peek() == ']') {
      in.read();
      return true;
    }
    if (deserializeJson(doc, in, DeserializationOption::NestingLimit(2))) return false;
    ++stats.received;

    Aircraft aircraft;
    if (doc.is<JsonArrayConst>() && toAircraft(doc.as<JsonArrayConst>(), center, aircraft)) {
      ++stats.withPosition;
      if (aircraft.distanceKm <= rangeKm) {
        ++stats.inRange;
        out.add(aircraft);
      }
    }

    skipWhitespace(in);
    const int separator = in.read();
    if (separator == ']') return true;
    if (separator != ',') return false;
  }
}

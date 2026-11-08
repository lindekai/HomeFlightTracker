// HTTPS-Zugriff auf OpenSky: OAuth2-Token (Client Credentials) und /states/all.
// TLS mit Zertifikatsprüfung über das im ESP32-Core eingebettete Root-CA-Bundle.
#pragma once

#include <Arduino.h>

#include "../geo/Geo.h"
#include "../model/FlightList.h"
#include "StatesParser.h"

class OpenSkyClient {
 public:
  enum class Result : uint8_t { Ok, AuthFailed, Forbidden, RateLimited, ServerError, NetworkError, ParseError };

  void setCredentials(const char* clientId, const char* clientSecret);
  bool hasCredentials() const { return clientId_[0] != '\0'; }

  // Holt ein neues Access-Token. Gültigkeit laut Antwort (OpenSky: 30 min).
  Result fetchToken();
  // true, wenn das Token noch mindestens kTokenRenewMarginS gültig ist.
  bool tokenValid() const;
  void invalidateToken();

  // Ruft /states/all für die Box ab und übernimmt Flugzeuge im Radius in out (out wird nicht geleert).
  Result fetchStates(const geo::BoundingBox& box, geo::LatLon center, double rangeKm, FlightList& out,
                     opensky::ParseStats& stats);

  int lastHttpCode() const { return lastHttpCode_; }
  int32_t remainingCredits() const { return remainingCredits_; }  // -1 = unbekannt
  uint32_t retryAfterS() const { return retryAfterS_; }

  static const char* describe(Result result);

 private:
  static constexpr uint32_t kTokenRenewMarginS = 300;
  static constexpr uint32_t kHttpTimeoutMs = 15000;

  Result classify(int httpCode) const;

  char clientId_[96] = "";
  char clientSecret_[128] = "";
  String token_;
  uint32_t tokenObtainedMs_ = 0;
  uint32_t tokenLifetimeS_ = 0;
  int lastHttpCode_ = 0;
  int32_t remainingCredits_ = -1;
  uint32_t retryAfterS_ = 0;
};

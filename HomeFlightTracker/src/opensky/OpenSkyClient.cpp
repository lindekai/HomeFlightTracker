#include "OpenSkyClient.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

namespace {

constexpr char kTokenUrl[] =
    "https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token";
constexpr char kStatesUrl[] = "https://opensky-network.org/api/states/all";
constexpr char kUserAgent[] = "HomeFlightTracker/1.0 (ESP32)";

// Vollständiges Mozilla-Root-CA-Bundle, das der ESP32-Arduino-Core in die Firmware einbettet.
extern const uint8_t kCaBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t kCaBundleEnd[] asm("_binary_x509_crt_bundle_end");

const char* kCollectedHeaders[] = {"X-Rate-Limit-Remaining", "X-Rate-Limit-Retry-After-Seconds"};

void configureTls(NetworkClientSecure& tls) {
  tls.setCACertBundle(kCaBundleStart, kCaBundleEnd - kCaBundleStart);
  tls.setHandshakeTimeout(15);
}

String urlEncode(const char* text) {
  static const char kHex[] = "0123456789ABCDEF";
  String encoded;
  for (const char* p = text; *p; ++p) {
    const uint8_t c = static_cast<uint8_t>(*p);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += static_cast<char>(c);
    } else {
      encoded += '%';
      encoded += kHex[c >> 4];
      encoded += kHex[c & 0x0F];
    }
  }
  return encoded;
}

// Adapter: HTTP-Datenstrom als Zeichenquelle für den Parser.
class StreamSource : public opensky::JsonByteSource {
 public:
  explicit StreamSource(Stream& stream) : stream_(stream) {}
  int read() override {
    char c;
    return stream_.readBytes(&c, 1) == 1 ? static_cast<uint8_t>(c) : -1;  // wartet bis zum Timeout
  }
  int peek() override {
    const uint32_t start = millis();
    while (stream_.available() == 0) {
      if (millis() - start >= stream_.getTimeout()) return -1;
      delay(1);
    }
    return stream_.peek();
  }

 private:
  Stream& stream_;
};

}  // namespace

void OpenSkyClient::setCredentials(const char* clientId, const char* clientSecret) {
  strlcpy(clientId_, clientId, sizeof(clientId_));
  strlcpy(clientSecret_, clientSecret, sizeof(clientSecret_));
  invalidateToken();
}

OpenSkyClient::Result OpenSkyClient::fetchToken() {
  invalidateToken();
  NetworkClientSecure tls;
  configureTls(tls);
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  http.setUserAgent(kUserAgent);
  if (!http.begin(tls, kTokenUrl)) return Result::NetworkError;
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = "grant_type=client_credentials&client_id=";
  body += urlEncode(clientId_);
  body += "&client_secret=";
  body += urlEncode(clientSecret_);
  lastHttpCode_ = http.POST(body);
  body.clear();

  Result result = classify(lastHttpCode_);
  // Keycloak meldet falsche Zugangsdaten mit 400/401 ("invalid_client").
  if (lastHttpCode_ == 400) result = Result::AuthFailed;
  if (result == Result::Ok) {
    JsonDocument filter;
    filter["access_token"] = true;
    filter["expires_in"] = true;
    JsonDocument doc;
    if (deserializeJson(doc, http.getString(), DeserializationOption::Filter(filter)) ||
        !doc["access_token"].is<const char*>()) {
      result = Result::ParseError;
    } else {
      token_ = doc["access_token"].as<const char*>();
      tokenLifetimeS_ = doc["expires_in"] | 1800;
      tokenObtainedMs_ = millis();
    }
  }
  http.end();
  return result;
}

bool OpenSkyClient::tokenValid() const {
  if (token_.isEmpty()) return false;
  const uint32_t ageS = (millis() - tokenObtainedMs_) / 1000;
  return ageS + kTokenRenewMarginS < tokenLifetimeS_;
}

void OpenSkyClient::invalidateToken() {
  token_ = String();
  tokenLifetimeS_ = 0;
}

OpenSkyClient::Result OpenSkyClient::fetchStates(const geo::BoundingBox& box, geo::LatLon center, double rangeKm,
                                                 FlightList& out, opensky::ParseStats& stats) {
  // Grenzen leicht nach außen runden, damit kein Randbereich verloren geht.
  char url[200];
  snprintf(url, sizeof(url), "%s?lamin=%.4f&lomin=%.4f&lamax=%.4f&lomax=%.4f", kStatesUrl, box.latMin - 0.0001,
           box.lonMin - 0.0001, box.latMax + 0.0001, box.lonMax + 0.0001);

  NetworkClientSecure tls;
  configureTls(tls);
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  http.setUserAgent(kUserAgent);
  http.useHTTP10(true);  // kein Chunked-Encoding: Antwort direkt als Datenstrom lesbar
  if (!http.begin(tls, url)) return Result::NetworkError;
  http.collectHeaders(kCollectedHeaders, sizeof(kCollectedHeaders) / sizeof(kCollectedHeaders[0]));
  if (!token_.isEmpty()) http.addHeader("Authorization", "Bearer " + token_);

  lastHttpCode_ = http.GET();
  if (http.hasHeader("X-Rate-Limit-Remaining")) {
    remainingCredits_ = http.header("X-Rate-Limit-Remaining").toInt();
  }
  retryAfterS_ = http.hasHeader("X-Rate-Limit-Retry-After-Seconds")
                     ? http.header("X-Rate-Limit-Retry-After-Seconds").toInt()
                     : 0;

  Result result = classify(lastHttpCode_);
  if (result == Result::Ok) {
    Stream& stream = http.getStream();
    stream.setTimeout(kHttpTimeoutMs);
    StreamSource source(stream);
    if (!opensky::parseStates(source, center, rangeKm, out, stats)) result = Result::ParseError;
  }
  http.end();
  return result;
}

OpenSkyClient::Result OpenSkyClient::classify(int httpCode) const {
  if (httpCode == 200) return Result::Ok;
  if (httpCode == 401) return Result::AuthFailed;
  if (httpCode == 403) return Result::Forbidden;
  if (httpCode == 429) return Result::RateLimited;
  if (httpCode >= 500) return Result::ServerError;
  if (httpCode < 0) return Result::NetworkError;  // HTTPClient-Fehler (Verbindung, TLS, Timeout)
  return Result::ServerError;
}

const char* OpenSkyClient::describe(Result result) {
  switch (result) {
    case Result::Ok:
      return "ok";
    case Result::AuthFailed:
      return "Anmeldung abgelehnt";
    case Result::Forbidden:
      return "Zugriff verweigert";
    case Result::RateLimited:
      return "Kontingent erschoepft";
    case Result::ServerError:
      return "Serverfehler";
    case Result::NetworkError:
      return "Netzwerk-/TLS-Fehler";
    case Result::ParseError:
      return "Antwort unlesbar";
  }
  return "?";
}

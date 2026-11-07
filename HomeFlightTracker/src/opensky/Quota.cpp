#include "Quota.h"

namespace {

constexpr uint32_t kSecondsPerDay = 86400;

}  // namespace

uint8_t opensky::creditsPerRequest(double areaSquareDegrees) {
  if (areaSquareDegrees <= 25.0) return 1;
  if (areaSquareDegrees <= 100.0) return 2;
  if (areaSquareDegrees <= 400.0) return 3;
  return 4;
}

uint32_t opensky::dailyCredits(uint16_t intervalS, uint8_t creditsPerRequest) {
  if (intervalS == 0) return UINT32_MAX;
  const uint32_t requests = (kSecondsPerDay + intervalS - 1) / intervalS;
  return requests * creditsPerRequest;
}

uint16_t opensky::minIntervalSeconds(double areaSquareDegrees, bool authenticated) {
  const uint32_t budget = authenticated ? kDailyCreditsAuthenticated : kDailyCreditsAnonymous;
  const uint32_t plannedBudget = budget * kMaxPlannedBudgetPercent / 100;
  const uint32_t credits = creditsPerRequest(areaSquareDegrees);
  // Kleinstes Intervall, bei dem ceil(86400 / t) * credits <= plannedBudget gilt.
  const uint32_t maxRequests = plannedBudget / credits;
  uint32_t interval = (kSecondsPerDay + maxRequests - 1) / maxRequests;
  while (dailyCredits(interval, credits) > plannedBudget) ++interval;

  const uint16_t floor = authenticated ? kMinIntervalAuthenticatedS : kMinIntervalAnonymousS;
  return interval < floor ? floor : static_cast<uint16_t>(interval);
}

bool opensky::isIntervalAllowed(uint16_t intervalS, double areaSquareDegrees, bool authenticated) {
  return intervalS >= minIntervalSeconds(areaSquareDegrees, authenticated) && intervalS <= kMaxIntervalS;
}

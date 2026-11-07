#include "BootRequest.h"

#include <Esp.h>
#include <esp_attr.h>
#include <esp_system.h>

namespace {

RTC_NOINIT_ATTR uint32_t rtcBootRequest;

}  // namespace

BootRequest takeBootRequest() {
  const uint32_t stored = rtcBootRequest;
  rtcBootRequest = static_cast<uint32_t>(BootRequest::None);
  if (esp_reset_reason() != ESP_RST_SW) return BootRequest::None;
  switch (static_cast<BootRequest>(stored)) {
    case BootRequest::Setup:
      return BootRequest::Setup;
    case BootRequest::Diagnostics:
      return BootRequest::Diagnostics;
    default:
      return BootRequest::None;
  }
}

void restartWith(BootRequest request) {
  rtcBootRequest = static_cast<uint32_t>(request);
  ESP.restart();
  for (;;) {
  }
}

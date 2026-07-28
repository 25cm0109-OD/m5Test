#pragma once

#include <Arduino.h>

namespace uploadConfig {

constexpr char kWifiSsid[] = "YOUR_WIFI_SSID";
constexpr char kWifiPassword[] = "YOUR_WIFI_PASSWORD";
constexpr char kDeviceId[] = "m5stick-living-room";
constexpr char kApiBaseUrl[] =
    "https://YOUR_ACCOUNT.example.com/ir-signals/api";
constexpr char kApiKey[] = "GENERATE_A_RANDOM_API_KEY";

constexpr uint32_t kWifiTimeoutMs = 15000;
constexpr uint16_t kHttpTimeoutMs = 10000;
constexpr uint32_t kCommandPollIntervalMs = 1000;

}  // namespace uploadConfig

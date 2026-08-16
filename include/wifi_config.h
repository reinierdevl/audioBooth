#pragma once

// Fill these values to let the player join an existing Wi-Fi network.
// Leave WIFI_STA_SSID empty to start the fallback access point immediately.
constexpr char WIFI_STA_SSID[] = "";
constexpr char WIFI_STA_PASSWORD[] = "";

constexpr char WIFI_DEVICE_NAME_PREFIX[] = "audiobooth_";
constexpr char WIFI_AP_PASSWORD[] = "0123456789";

constexpr char WEB_BUSY_MESSAGE[] =
    "user interface not available, device in use";

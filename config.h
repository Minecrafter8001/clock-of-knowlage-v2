// ----------------------------------------------------------
// config.h - User configuration for Clock of Knowledge v2
// ----------------------------------------------------------

#pragma once

// ------------------------ Location / API -------------------------
const float LAT = 0; //put your location here (doesn't need to be exact, the citys cordinites are fine)
const float LON = 0;  //put your location here (doesn't need to be exact, the citys cordinites are fine)
const char* TIMEZONE_API = "Europe/London"; // Timezone string for Open-Meteo

// ------------------------ WiFi ----------------------------
const char* ssid     = "name (ssid) here";
const char* password = "password here";

// ------------------------ NTP ------------------------------
const char* ntpServer = "pool.ntp.org";

// UK timezone with automatic DST
const char* tzUK = "GMT0BST,M3.5.0/1,M10.5.0/2";

// ----------------------------------------------------------
// Clock of Knowledge v2
// ESP32 + GP1294AI VFD + DHT22 + Open-Meteo
// Network-connected clock with internal + external temp/humidity
// ----------------------------------------------------------

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include "DHT.h"
#include "time.h"
#include <WiFiClientSecure.h>

// User configuration
#include "config.h"

// ------------------------ Hardware & Pins ------------------------
#define DHTPIN              13
#define DHTTYPE             DHT22

#define FILAMENT_EN_PIN     16
#define VFD_RESET_PIN       17
#define SPI_CLOCK_PIN       18
#define SPI_DATA_PIN        23
#define SPI_CS_PIN          5
#define SPI_RESET_PIN       33

// ------------------------ Timings ------------------------
const unsigned long RESYNC_INTERVAL_MS   = 3UL * 3600UL * 1000UL; // 3 hours
const unsigned long METEO_INTERVAL_MS    = 60UL * 60UL * 1000UL;  // 1 hour
const unsigned long METEO_RETRY_MS       = 30UL * 1000UL;         // 30 seconds
const unsigned long WIFI_RETRY_MS        = 30UL * 1000UL;         // 30 seconds
#define LOOP_DELAY_MS 50

// ------------------------ Globals ------------------------
DHT dht(DHTPIN, DHTTYPE);

U8G2_GP1294AI_256X48_F_4W_SW_SPI u8g2(
  U8G2_R0, SPI_CLOCK_PIN, SPI_DATA_PIN, SPI_CS_PIN,
  U8X8_PIN_NONE, SPI_RESET_PIN
);

// Bitmaps (shared with draw.ino)
const unsigned char image_wifi_connected_bits[] = {
  0x80,0x0f,0x00,0xe0,0x3f,0x00,0x78,0xf0,0x00,0x9c,0xcf,0x01,0xee,0xbf,0x03,0xf7,0x78,0x07,
  0x3a,0xe7,0x02,0xdc,0xdf,0x01,0xe8,0xb8,0x00,0x70,0x77,0x00,0xa0,0x2f,0x00,0xc0,0x1d,0x00,
  0x80,0x0a,0x00,0x00,0x07,0x00,0x00,0x02,0x00,0x00,0x00,0x00
};
const unsigned char image_humidity_bits[] = {
  0x20,0x00,0x20,0x00,0x30,0x00,0x70,0x00,0x78,0x00,0xf8,0x00,0xfc,0x01,0xfc,0x01,0x7e,0x03,
  0xfe,0x02,0xff,0x06,0xff,0x07,0xfe,0x03,0xfe,0x03,0xfc,0x01,0xf0,0x00
};
const unsigned char image_temperature_bits[] = {
  0x38,0x00,0x44,0x00,0xd4,0x00,0x54,0x00,0xd4,0x00,0x54,0x00,0xd4,0x00,0x54,0x00,0x54,0x00,
  0x92,0x00,0x39,0x01,0x75,0x01,0x7d,0x01,0x39,0x01,0x82,0x00,0x7c,0x00
};
const unsigned char image_wifi_disconnected_bits[] = {
  0x84,0x0f,0x00,0x68,0x30,0x00,0x10,0xc0,0x00,0xa4,0x0f,0x01,0x42,0x30,0x02,0x91,0x40,0x04,
  0x08,0x85,0x00,0xc4,0x1a,0x01,0x20,0x24,0x00,0x10,0x4a,0x00,0x80,0x15,0x00,0x40,0x20,0x00,
  0x00,0x42,0x00,0x00,0x85,0x00,0x00,0x02,0x01,0x00,0x00,0x00
};

// State
unsigned long lastDHT = 0;
float cachedTemp = NAN, cachedHum = NAN;

static int lastSecond = -1;
unsigned long lastResync = 0;

// Open-Meteo state
unsigned long lastMeteo = 0;
float extTemp = NAN;
float extHum  = NAN;

// Wi-Fi retry ticker
unsigned long lastWifiRetry = 0;

// Forward declaration (implemented in draw.ino)
void drawUI(const struct tm& tmNow);

// ------------------------ Helpers ------------------------
bool connectWiFi(unsigned long timeoutMs = 15000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.printf("Connecting to %s", ssid);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    Serial.println("\nWiFi connect failed (timeout).");
    WiFi.disconnect(true);   // reset state
    delay(2000);             // small backoff
    return false;
  }
}

bool syncTime() {
  configTzTime(tzUK, ntpServer); // automatic DST
  struct tm timeinfo;
  bool ok = getLocalTime(&timeinfo, 5000);
  Serial.println(ok ? "Time synced from NTP" : "Failed to get NTP time");
  return ok;
}

void readDHTReliable() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    delay(50);
    t = dht.readTemperature();
    h = dht.readHumidity();
  }
  if (!isnan(t) && !isnan(h)) {
    cachedTemp = t;
    cachedHum  = h;
  }
}

void formatEnv(char* tempBuf, size_t tempLen, char* humBuf, size_t humLen, float t, float h) {
  if (!isnan(t)) snprintf(tempBuf, tempLen, "%.1fC", t);
  else strncpy(tempBuf, "--.-C", tempLen);

  if (!isnan(h)) snprintf(humBuf, humLen, "%.1f%%", h);
  else strncpy(humBuf, "--.-%", humLen);

  tempBuf[tempLen - 1] = 0;
  humBuf[humLen - 1] = 0;
}

const char* dow3(uint8_t wday) {
  static const char* days[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
  if (wday > 6) return "---";
  return days[wday];
}

void fetchOpenMeteo() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Open-Meteo: WiFi not connected; skipping");
    return;
  }

  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(LAT, 6) +
               "&longitude=" + String(LON, 6) +
               "&current=temperature_2m,relative_humidity_2m&timezone=" + TIMEZONE_API;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("Open-Meteo: http.begin() failed");
    lastMeteo = millis() - METEO_INTERVAL_MS + METEO_RETRY_MS;
    return;
  }

  http.addHeader("Accept-Encoding", "identity");
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(12000);

  Serial.println("Fetching Open-Meteo...");
  int code = http.GET();
  if (code != 200) {
    Serial.printf("Open-Meteo HTTP error: %d\n", code);
    http.end();
    lastMeteo = millis() - METEO_INTERVAL_MS + METEO_RETRY_MS;
    return;
  }

  String body = http.getString();
  StaticJsonDocument<1536> doc;
  DeserializationError err = deserializeJson(doc, body);

  if (err) {
    Serial.printf("JSON error: %s\n", err.c_str());
    Serial.printf("Body (first 200): %s\n", body.substring(0, 200).c_str());
    http.end();
    lastMeteo = millis() - METEO_INTERVAL_MS + METEO_RETRY_MS;
    return;
  }

  http.end();

  float t = doc["current"]["temperature_2m"] | NAN;
  float h = doc["current"]["relative_humidity_2m"] | NAN;

  if (!isnan(t)) extTemp = t;
  if (!isnan(h)) extHum  = h;

  Serial.printf("Open-Meteo OK: %.1f °C, %.1f %%\n", extTemp, extHum);
  lastMeteo = millis(); // success
}

// ------------------------ Setup ------------------------
void setup() {
  Serial.begin(115200);

  pinMode(FILAMENT_EN_PIN, OUTPUT);
  pinMode(VFD_RESET_PIN, OUTPUT);
  digitalWrite(FILAMENT_EN_PIN, HIGH);
  digitalWrite(VFD_RESET_PIN, LOW);
  delay(50);
  digitalWrite(VFD_RESET_PIN, HIGH);

  dht.begin();
  u8g2.begin();

  connectWiFi();
  syncTime();
  lastResync = millis();

  readDHTReliable();
  lastMeteo = millis() - METEO_INTERVAL_MS; // trigger fetch immediately
}

// ------------------------ Loop ------------------------
void loop() {
  if (millis() - lastResync > RESYNC_INTERVAL_MS) {
    syncTime();
    lastResync = millis();
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiRetry > WIFI_RETRY_MS) {
      Serial.println("WiFi lost, reconnecting...");
      connectWiFi();
      lastWifiRetry = millis();
    }
  }

  if (millis() - lastMeteo > METEO_INTERVAL_MS) {
    fetchOpenMeteo();
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    delay(250);
    return;
  }

  unsigned long nowMillis = millis();
  if (nowMillis - lastDHT > 2000) {
    lastDHT = nowMillis;
    readDHTReliable();
  }

  if (timeinfo.tm_sec != lastSecond) {
    lastSecond = timeinfo.tm_sec;
    drawUI(timeinfo);
  }

  delay(LOOP_DELAY_MS);
}

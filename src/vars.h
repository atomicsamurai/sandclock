#pragma once

#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoJson.h>
#include <NeoPixelBrightnessBus.h>

/*
 * Firmware version. Bump on every behavior-changing release; the UI shows it
 * (footer + OTA page) and /config reports it, so a flash can be verified
 * from the browser or with `curl http://sandclock.local/config`.
 */
#define FW_VERSION "2.4.0"

/*
 * Shared globals, grouped by concern. Everything here is defined once and
 * read across headers (single translation unit build: main.cpp includes
 * vars.h first, then every other header in dependency order).
 */

// ---- Pins -----------------------------------------------------------------
int sensorPin = A0;   // LDR for auto brightness
int buzzerPin = D7;

// ---- Config (persisted to SPIFFS as /config.json, see config.h) -----------
struct ConfigObject {
  bool autoBrightness;
  byte brightness;
  byte hourFormat;
  byte displayStyle;  // 1 = cycle time/temp (5s/2s), 2 = combined on one screen
  char hassBaseUrl[100];
  char hassEntityId[128];
  char hassAT[512];
  char wifi_ssid[32];
  char wifi_password[32];
  char tzName[32];
  char tzValue[32];
  RgbColor clock_color;
  RgbColor temp_color;
  RgbColor other_color;
  String wifiMacString;
};

ConfigObject config;

// ---- Display state --------------------------------------------------------
int showingTemp = 0;                 // 0 = time screen, 1 = temperature
bool dotsOn = true;                  // colon blink phase
long currentTemp = -1000;            // -1000 sentinel = no HA reading
RgbColor color_black = RgbColor(0, 0, 0);

// Alt-screen cycle: 5s time, 2s temperature
const unsigned long TIME_SCREEN_MS = 5000;
const unsigned long TEMP_SCREEN_MS = 2000;
unsigned long screenStartMs = 0;

// ---- Scroll message state (set by /showmessage, driven by scroll.h) -------
int showingMessage = 0;
char messageText[128] = "";
uint16_t messageScrollDelayMs = 50;  // defaults until /showmessage sets them
uint16_t messageRepeatCount = 1;

// ---- Timing counters (millis() based) --------------------------------------
unsigned long prevTime = 0;              // 1s clock/temp tick
unsigned long prevTimeBrightness = 0;    // 5s brightness tick
unsigned long prevTimeTemperature = 0;   // 60s HA fetch tick
int beepDone = 0;                        // chime latch for checkChime()

// ---- Sound ----------------------------------------------------------------
int sensorValue = 0;  // last LDR reading (also used by handleBrightness)

// ---- Network services ------------------------------------------------------
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdateServer;

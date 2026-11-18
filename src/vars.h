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
#define FW_VERSION "2.12.0"

// Set at build time by tools/git_rev.py (git describe), so /info shows exactly
// which commit is running.
#ifndef GIT_REV
#define GIT_REV "unknown"
#endif

/*
 * Shared globals, grouped by concern. Everything here is defined once and
 * read across headers (single translation unit build: main.cpp includes
 * vars.h first, then every other header in dependency order).
 */

// ---- Pins -----------------------------------------------------------------
int sensorPin = A0;   // LDR for auto brightness
int buzzerPin = D7;

// ---- Config (persisted to the EEPROM sector, see config.h) -----------------
struct ConfigObject {
  bool autoBrightness;
  byte brightness;
  byte hourFormat;
  byte displayStyle;  // 1 = cycle time/temp (5s/2s), 2 = combined on one screen
  bool chimeEnabled;         // hourly/half-hourly chime
  bool chimeMuteDark;        // silence automatic beeps (chime, message start/end) when dark
  uint16_t chimeDarkThreshold;  // smoothed light reading (0-1023) below which it is "dark"
  char hassBaseUrl[100];
  char hassEntityId[128];
  char hassAT[512];
  char wifi_ssid[32];
  char wifi_password[32];
  char tzName[32];
  char tzValue[32];
  char authPassword[32];  // HTTP Basic password for user "admin"; empty = auth off
  char apPassword[16];    // WPA2 password of the setup access point (generated, 10 chars)
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
bool scrollQuiet = false;            // next scroll skips its start/end beeps (system announcements)

// ---- Timing counters (millis() based) --------------------------------------
unsigned long prevTime = 0;              // 1s clock/temp tick
unsigned long prevTimeBrightness = 0;    // 5s brightness tick
unsigned long prevTimeTemperature = 0;   // 60s HA fetch tick
bool tempFetchTried = false;             // false until the first HA fetch, so it runs right after boot
int beepDone = 0;                        // chime latch for checkChime()

// ---- Sound ----------------------------------------------------------------
int sensorValue = 0;  // last LDR reading (also used by handleBrightness)
int lightLevel = -1;  // LDR reading smoothed over ~20s; -1 until the first sample

// ---- Status reported by /info ---------------------------------------------
int haLastCode = 0;                 // last HA fetch: HTTP status, -1 transport error, -2 non-numeric state
unsigned long haLastAttemptMs = 0;  // millis() of the last HA fetch (0 = never)
unsigned long haLastOkMs = 0;       // millis() of the last fetch that returned a temperature
volatile time_t ntpLastSync = 0;    // epoch of the last SNTP time update (0 = never)
volatile uint32_t ntpSyncCount = 0;
uint32_t minFreeHeap = 0xFFFFFFFF;  // lowest free heap seen since boot
bool apActive = false;              // setup AP is up (WiFi not connected)
bool safeMode = false;              // started in safe mode after repeated crashes (see safe_mode.h)
uint32_t crashCount = 0;            // consecutive crashed starts, cleared after 2 minutes of stable running
bool rebootPending = false;         // loop() restarts at rebootAtMs (lets a handler reply first)
unsigned long rebootAtMs = 0;

// ---- Network services ------------------------------------------------------
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdateServer;

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <ArduinoLog.h>
#include <NeoPixelBrightnessBus.h>
/**************************************************************************/

/*
 * Definition of the LED matrix layout 
 */
constexpr uint8_t PanelWidth = 32;
constexpr uint8_t PanelHeight = 8;
constexpr uint16_t PixelCount = PanelWidth * PanelHeight;

NeoTopology<ColumnMajorAlternating180Layout> topo (PanelWidth, PanelHeight);
NeoPixelBrightnessBus<NeoGrbFeature, Neo800KbpsMethod> strip (PixelCount);

/**************************************************************************/

// Dependency order: vars.h defines the shared globals every other header
// reads; each header below declares what it needs so none of them depends
// on include order in this file (except vars.h coming first).
#include "vars.h"
#include "wifi_logo.h"
#include "sounds.h"
#include "display.h"
#include "sensor.h"
#include "scroll.h"
#include "time_ops.h"
#include "api.h"

DNSServer dnsServer;
IPAddress APIP (192, 168, 4, 1);

// Boot waits this long for WiFi before bringing up the setup AP; after that
// the station keeps retrying so a router that boots slower than the clock
// (power outage) is picked up without a reboot.
const unsigned long WIFI_BOOT_WAIT_MS = 20000;
const unsigned long WIFI_RETRY_MS = 30000;
const unsigned long WIFI_AP_AFTER_MS = 120000;  // AP for a WiFi loss after boot

bool wifiWasUp = false;
unsigned long wifiDownSinceMs = 0;
unsigned long lastWifiAttemptMs = 0;

void startAp() {
  WiFi.mode (WIFI_AP_STA);
  WiFi.softAPConfig (APIP, APIP, IPAddress (255, 255, 255, 0));
  WiFi.softAP (F("SandClock"));
  dnsServer.start (53, "*", APIP);
  apActive = true;
  Log.warningln("WiFi not connected: setup AP 'SandClock' is up, still retrying '%s'", config.wifi_ssid);
}

void stopAp() {
  dnsServer.stop ();
  WiFi.softAPdisconnect (true);
  WiFi.mode (WIFI_STA);
  apActive = false;
  Log.noticeln("Setup AP stopped");
}

void onWifiConnected() {
  Log.noticeln("WiFi connected, IP: %p", WiFi.localIP());
  MDNS.begin ("sandclock");
  applyTimezone();  // (re)starts SNTP, so time resyncs after an outage
}

// Called every loop(): detects connect/disconnect transitions, retries the
// station connection, and runs the setup AP only while WiFi is down.
void maintainWifi() {
  unsigned long now = millis();
  bool up = WiFi.status() == WL_CONNECTED;
  if (up) {
    if (!wifiWasUp) {
      if (apActive) {
        stopAp();
      }
      onWifiConnected();
    }
    wifiWasUp = true;
    return;
  }
  if (wifiWasUp) {
    wifiWasUp = false;
    wifiDownSinceMs = now;
    lastWifiAttemptMs = now;
    Log.warningln("WiFi connection lost");
  }
  if (!apActive && now - wifiDownSinceMs >= WIFI_AP_AFTER_MS) {
    startAp();
  }
  if (now - lastWifiAttemptMs >= WIFI_RETRY_MS && config.wifi_ssid[0]) {
    lastWifiAttemptMs = now;
    Log.traceln("Retrying WiFi: %s", config.wifi_ssid);
    WiFi.begin (config.wifi_ssid, config.wifi_password);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  initSound();

  strip.Begin();
  clearDisplay();

  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

  LittleFS.begin();
  if (!loadConfig()) {
    initDefaultConfig();
  }
  delay(200);

  Log.noticeln("Starting up WiFi...");
  WiFi.mode (WIFI_STA);
  WiFi.persistent (false);
  if(strlen(config.wifi_ssid) > 0) {
    Log.traceln("Connecting to %s", config.wifi_ssid);
    WiFi.begin (config.wifi_ssid, config.wifi_password);
  }
  auto timeout = millis () + WIFI_BOOT_WAIT_MS;
  while (WiFi.status () != WL_CONNECTED && millis () < timeout) {
    wifi_logo (RgbColor (0x80, 0x80, 0x80));
    delay (15);
  }

  if (WiFi.status() == WL_CONNECTED) {
    onWifiConnected();
    wifiWasUp = true;
    for (int N = 0x200; N >= 0; N -= 3) {
      wifi_logo (RgbColor (N > 0x180 ? N - 0x180 : 0, N > 0x100 ? 0x80 : (N > 0x80 ? N - 0x80 : 0), N > 0x180 ? N - 0x180 : 0));
      delay (15);
    }
  } else {
    wifiDownSinceMs = millis ();
    lastWifiAttemptMs = millis ();
    startAp();
    for (int N = 0x200; N >= 0; N -= 3) {
      wifi_logo (RgbColor (N > 0x180 ? N - 0x180 : 0, N > 0x180 ? N - 0x180 : 0, N > 0x100 ? 0x80 : (N > 0x80 ? N - 0x80 : 0)));
      delay (15);
    }
  }

  httpUpdateServer.setup(&server, "/update", AUTH_USER, config.authPassword);

  // HTTP Handlers
  setupHandlers();

  // Revalidate on every load so a flashed UI change shows up immediately
  // instead of after the browser's cache expires.
  server.serveStatic("/", LittleFS, "/", "no-cache");
  server.begin();

  Log.traceln("LittleFS contents:");
  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    Log.traceln("FS File: %s, size: %s", fileName.c_str(), String(fileSize).c_str());
  }
}

void loop() {
  unsigned long currentMillis = millis();
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < minFreeHeap) {
    minFreeHeap = freeHeap;
  }
  server.handleClient();
  if (rebootPending && millis() >= rebootAtMs) {
    ESP.restart();
  }
  maintainWifi();
  if (currentMillis - prevTimeBrightness >= 5000) { // every 5 secs
    prevTimeBrightness = currentMillis;
    handleBrightness();
  }
  if (showingMessage != 0) {  // /showmessage arrived: start a non-blocking scroll
    showingMessage = 0;
    beginMessageScroll();
  }
  tickScroll();
  bool online = WiFi.status() == WL_CONNECTED;
  // The RTC keeps time through a WiFi drop, so keep showing it; the logo is
  // only for "no WiFi and no time yet".
  if (online || timeSynced()) {
    if(!scrollActive) {
      if (currentMillis - prevTime >= 1000) { // every sec
          prevTime = currentMillis;
          checkChime();
          warnIfNtpStale();
          refreshTemperatureIfDue();
          clearDisplay();
          if (!timeSynced()) {
            showNoTime();
          } else if (config.displayStyle == 2) {
            // combined: time and temperature on one screen, no cycling
            showCombined();
          } else {
            // cycle: 5s time, 2s temperature
            if (!showingTemp && currentMillis - screenStartMs >= TIME_SCREEN_MS) {
              showingTemp = 1;
              screenStartMs = currentMillis;
            } else if (showingTemp && currentMillis - screenStartMs >= TEMP_SCREEN_MS) {
              showingTemp = 0;
              screenStartMs = currentMillis;
            }
            if (showingTemp) {
              showTemperature();
            } else {
              showTime();
            }
          }
      }
      strip.Show();
    }
  } else {
      wifi_logo (RgbColor (0x80, 0x80, 30));
      delay (15);
  }
  if (online) {
    MDNS.update ();
  }
  if (apActive) {
    dnsServer.processNextRequest ();
  }
}

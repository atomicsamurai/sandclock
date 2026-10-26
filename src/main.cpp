#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <FS.h>
#include <ESP8266HTTPClient.h> // Please read the instructions on http://arduino.esp8266.com/Arduino/versions/2.3.0/doc/filesystem.html#uploading-files-to-file-system
#include <TimeLib.h>
// #include <WiFiManager.h> //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <SD.h>
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

void setup() {
  Serial.begin(115200);
  while (!Serial);

  initSound();

  strip.Begin();
  // strip.SetPixelColor(0, RgbColor(0x40, 0x00, 0x00));
  // strip.Show();
  clearDisplay();

  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

  SPIFFS.begin();
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
  auto timeout = millis () + (10 * 1000);
  while (WiFi.status () != WL_CONNECTED && millis () < timeout) {
    wifi_logo (RgbColor (0x80, 0x80, 0x80));
    delay (15);
  }

  if (WiFi.status() == WL_CONNECTED) {
    // if status is connected, initialize timeClient and sync to ntp
    Log.traceln("Local IP: %p", WiFi.localIP());
    MDNS.begin ("sandclock");
    for (int N = 0x200; N >= 0; N -= 3) {
      wifi_logo (RgbColor (N > 0x180 ? N - 0x180 : 0, N > 0x100 ? 0x80 : (N > 0x80 ? N - 0x80 : 0), N > 0x180 ? N - 0x180 : 0));
      delay (15);
    }
    // timeClient.begin();
    // syncNTP(true);
    applyTimezone();
  } else {
    WiFi.mode (WIFI_AP);
    WiFi.softAPConfig (APIP, APIP, IPAddress (255, 255, 255, 0));
    WiFi.softAP (F("SandClock"));
    dnsServer.start (53, "*", APIP);
    for (int N = 0x200; N >= 0; N -= 3) {
      wifi_logo (RgbColor (N > 0x180 ? N - 0x180 : 0, N > 0x180 ? N - 0x180 : 0, N > 0x100 ? 0x80 : (N > 0x80 ? N - 0x80 : 0)));
      delay (15);
    }
  }

  IPAddress ip = WiFi.localIP();

  httpUpdateServer.setup(&server);

  // HTTP Handlers
  setupHandlers();

  // Before uploading the files with the "ESP8266 Sketch Data Upload" tool, 
  // zip the files with the command "gzip -r ./data/"
  // *.gz files are automatically unpacked and served from your 
  // ESP (so you don't need to create a handler for each file).
  server.serveStatic("/", SPIFFS, "/", "max-age=86400");
  server.begin();

  Log.traceln("SPIFFS contents:");
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    Log.traceln("FS File: %s, size: %s", fileName.c_str(), String(fileSize).c_str());
  }
}

void loop() {
  unsigned long currentMillis = millis();
  server.handleClient();
  if (currentMillis - prevTimeBrightness >= 5000) { // every 5 secs
    prevTimeBrightness = currentMillis;
    handleBrightness();
  }
  if (showingMessage != 0) {  // /showmessage arrived: start a non-blocking scroll
    showingMessage = 0;
    beginMessageScroll();
  }
  tickScroll();
  if (WiFi.status() == WL_CONNECTED) {
    if(!scrollActive) {
      if (currentMillis - prevTime >= 1000) { // every sec
          prevTime = currentMillis;
          checkChime();
          clearDisplay();
          if (config.displayStyle == 2) {
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
      // if (currentMillis - prevTimeNtpSync >= 3600000) { // every hour
      //   prevTimeNtpSync = currentMillis;
      //   syncNTP(true);
      // }
      strip.Show();
      MDNS.update ();
      dnsServer.processNextRequest ();
    }
  } else {
      wifi_logo (RgbColor (0x80, 0x80, 30));
      delay (15);
  }
}

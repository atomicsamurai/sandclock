
#pragma once

#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

int sensorPin = A0; // select the input pin for LDR
int buzzerPin = D7;

int sensorValue = 0;
float voltage = 0;
long timeOffset = 0;

unsigned long prevTime = 0;
unsigned long prevTimeBrightness = 0;
unsigned long prevTimeTemperature = 0;
unsigned long prevTimeNtpSync = 0;
int beepDone = 0;
int showingMessage = 0;
char messageText[128] = "";
uint16_t messageScrollDelayMs = 50;  // defaults until /showmessage sets them
uint16_t messageRepeatCount = 1;

DynamicJsonDocument configDoc(1024);
struct ConfigObject {
  bool autoBrightness;
  byte brightness;
  byte hourFormat;
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

// typedef struct {
//   const char * const name;
//   const char * const value;
// } TimezoneInfo;

ConfigObject config;

int showingTemp = 0;
bool dotsOn = true;
long currentTemp = -1000;
int tempFetchCounter = 100;

// Alt-screen cycle: 5s time, 2s temperature
const unsigned long TIME_SCREEN_MS = 5000;
const unsigned long TEMP_SCREEN_MS = 2000;
unsigned long screenStartMs = 0;

RgbColor color_black = RgbColor(0, 0, 0);

WiFiUDP ntpUDP;
// NTPClient timeClient(ntpUDP, "pool.ntp.org", timeOffset * 60, 60000);

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdateServer;

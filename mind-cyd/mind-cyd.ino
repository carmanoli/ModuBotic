/*
  ModuBotic mind-cyd - ESP32 CYD bridge

  WiFi HTTP:
    Talks to control/server.py.

  Serial2:
    UART link to Arduino body controller.

  Common contract:
    Panel -> CYD -> Arduino: command lines such as HEAD_LEFT or STOP.
    Arduino -> CYD -> Panel: telemetry query lines such as env=..., i2c=..., rgb=...
*/

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#if __has_include("config.h")
#include "config.h"
#else
#define MODUBOTIC_WIFI_SSID "A_TUA_REDE"
#define MODUBOTIC_WIFI_PASSWORD "A_TUA_PASSWORD"
#define MODUBOTIC_CONTROL_BASE_URL "http://192.168.1.20:8080"
#define MODUBOTIC_BODY_RX_PIN 16
#define MODUBOTIC_BODY_TX_PIN 17
#define MODUBOTIC_BODY_BAUD 9600
#define MODUBOTIC_COMMAND_POLL_MS 250
#define MODUBOTIC_TELEMETRY_FLUSH_MS 100
#endif

#if __has_include(<TFT_eSPI.h>)
#include <TFT_eSPI.h>
#define MODUBOTIC_HAS_TFT 1
TFT_eSPI tft;
#else
#define MODUBOTIC_HAS_TFT 0
#endif

HardwareSerial BodySerial(2);

String bodyLine;
String lastCommand = "NONE";
String lastBodyLine = "";
String lastStatus = "boot";
unsigned long lastCommandPollAt = 0;
unsigned long lastTelemetryFlushAt = 0;
unsigned long lastScreenRefreshAt = 0;

void connectWiFi();
void pollCommand();
void readBodySerial();
void forwardTelemetryLine(const String &line);
bool isTelemetryLine(const String &line);
bool isIgnorableCommand(const String &command);
String httpPostText(const String &url);
void setStatus(const String &status);
void updateScreen(bool force = false);

void setup() {
  Serial.begin(115200);
  BodySerial.begin(
    MODUBOTIC_BODY_BAUD,
    SERIAL_8N1,
    MODUBOTIC_BODY_RX_PIN,
    MODUBOTIC_BODY_TX_PIN
  );

#if MODUBOTIC_HAS_TFT
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("ModuBotic CYD", 8, 8);
#endif

  setStatus("wifi");
  connectWiFi();
  BodySerial.println("PING");
  setStatus("ready");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    setStatus("wifi reconnect");
    connectWiFi();
  }

  readBodySerial();

  const unsigned long now = millis();
  if (now - lastCommandPollAt >= MODUBOTIC_COMMAND_POLL_MS) {
    lastCommandPollAt = now;
    pollCommand();
  }

  updateScreen();
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(MODUBOTIC_WIFI_SSID, MODUBOTIC_WIFI_PASSWORD);

  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
    updateScreen(true);
  }

  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void pollCommand() {
  String command = httpPostText(String(MODUBOTIC_CONTROL_BASE_URL) + "/api/command");
  command.trim();
  command.toUpperCase();

  if (command.length() == 0 || isIgnorableCommand(command)) {
    return;
  }

  lastCommand = command;
  BodySerial.println(command);
  Serial.print("PANEL -> BODY: ");
  Serial.println(command);
  setStatus("sent command");
}

void readBodySerial() {
  while (BodySerial.available() > 0) {
    char c = (char)BodySerial.read();
    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      bodyLine.trim();
      if (bodyLine.length() > 0) {
        lastBodyLine = bodyLine;
        Serial.print("BODY -> CYD: ");
        Serial.println(bodyLine);

        if (isTelemetryLine(bodyLine)) {
          forwardTelemetryLine(bodyLine);
        }
      }
      bodyLine = "";
    } else {
      bodyLine += c;
    }
  }
}

void forwardTelemetryLine(const String &line) {
  const unsigned long now = millis();
  if (now - lastTelemetryFlushAt < MODUBOTIC_TELEMETRY_FLUSH_MS) {
    delay(MODUBOTIC_TELEMETRY_FLUSH_MS - (now - lastTelemetryFlushAt));
  }
  lastTelemetryFlushAt = millis();

  String url = String(MODUBOTIC_CONTROL_BASE_URL) + "/api/telemetry?" + line;
  String response = httpPostText(url);
  response.trim();

  Serial.print("TELEMETRY -> PANEL: ");
  Serial.print(line);
  Serial.print(" -> ");
  Serial.println(response);
  setStatus("telemetry");
}

bool isTelemetryLine(const String &line) {
  return line.startsWith("env=") ||
         line.startsWith("i2c=") ||
         line.startsWith("rgb=") ||
         line.startsWith("color=") ||
         line.startsWith("temperature=") ||
         line.startsWith("humidity=") ||
         line.startsWith("light=") ||
         line.startsWith("gas=");
}

bool isIgnorableCommand(const String &command) {
  return command == "NONE" ||
         command == "OK" ||
         command == "ERROR" ||
         command == "TIMEOUT" ||
         command == "REQUEST TIMEOUT";
}

String httpPostText(const String &url) {
  if (WiFi.status() != WL_CONNECTED) {
    return "ERROR";
  }

  HTTPClient http;
  http.setTimeout(1200);
  http.begin(url);

  int code = http.POST("");
  String response = "";
  if (code > 0) {
    response = http.getString();
  } else {
    response = "ERROR";
  }

  http.end();
  return response;
}

void setStatus(const String &status) {
  lastStatus = status;
  updateScreen(true);
}

void updateScreen(bool force) {
  if (!force && millis() - lastScreenRefreshAt < 500) {
    return;
  }
  lastScreenRefreshAt = millis();

#if MODUBOTIC_HAS_TFT
  tft.fillRect(0, 36, 320, 204, TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("WiFi", 8, 42);
  tft.setTextColor(WiFi.status() == WL_CONNECTED ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.drawString(WiFi.status() == WL_CONNECTED ? "OK" : "...", 90, 42);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("IP", 8, 70);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "-", 90, 70);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Cmd", 8, 104);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(lastCommand.substring(0, 20), 90, 104);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Body", 8, 136);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(lastBodyLine.substring(0, 20), 90, 136);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("State", 8, 170);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(lastStatus.substring(0, 20), 90, 170);
#else
  (void)force;
#endif
}

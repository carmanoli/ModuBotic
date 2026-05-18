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
#include <Arduino_GFX_Library.h>
#include <HTTPClient.h>
#include <WiFi.h>

#if __has_include("config.h")
#include "config.h"
#else
#define MODUBOTIC_WIFI_SSID "A_TUA_REDE"
#define MODUBOTIC_WIFI_PASSWORD "A_TUA_PASSWORD"
#define MODUBOTIC_CONTROL_BASE_URL "http://10.0.0.100:8080"
#define MODUBOTIC_BODY_RX_PIN 35
#define MODUBOTIC_BODY_TX_PIN 22
#define MODUBOTIC_BODY_BAUD 9600
#define MODUBOTIC_COMMAND_POLL_MS 250
#define MODUBOTIC_TELEMETRY_FLUSH_MS 100
#define MODUBOTIC_BODY_PING_MS 5000
#define MODUBOTIC_BODY_ONLINE_MS 15000
#define MODUBOTIC_BODY_DEBUG_RAW 1
#define MODUBOTIC_FORWARD_BODY_TELEMETRY 1
#define MODUBOTIC_ROBOT_ID "pink"
#define MODUBOTIC_MIND_TYPE "cyd"
#define MODUBOTIC_MQTT_ENABLED 0
#define MODUBOTIC_MQTT_HOST "10.0.0.90"
#define MODUBOTIC_MQTT_PORT 1883
#define MODUBOTIC_MQTT_BASE_TOPIC "modubotic/robot"
#define MODUBOTIC_MQTT_RETRY_MS 5000
#endif

#if MODUBOTIC_MQTT_ENABLED
#include <PubSubClient.h>
#define MODUBOTIC_HAS_MQTT 1
#define MODUBOTIC_HAS_ARDUINO_MQTT 0
#define MODUBOTIC_HAS_PUBSUB_MQTT 1
#else
#define MODUBOTIC_HAS_MQTT 0
#define MODUBOTIC_HAS_ARDUINO_MQTT 0
#define MODUBOTIC_HAS_PUBSUB_MQTT 0
#endif

#ifndef MODUBOTIC_BODY_USE_USB_UART
#define MODUBOTIC_BODY_USE_USB_UART 0
#endif

#ifndef MODUBOTIC_ENABLE_USB_SERIAL_LOG
#define MODUBOTIC_ENABLE_USB_SERIAL_LOG 1
#endif

#ifndef MODUBOTIC_FORWARD_BODY_TELEMETRY
#define MODUBOTIC_FORWARD_BODY_TELEMETRY 1
#endif

#ifndef MODUBOTIC_MQTT_ENABLED
#define MODUBOTIC_MQTT_ENABLED 0
#endif

#ifndef MODUBOTIC_MQTT_RETRY_MS
#define MODUBOTIC_MQTT_RETRY_MS 5000
#endif

#if MODUBOTIC_ENABLE_USB_SERIAL_LOG
#define LOG_BEGIN(baud) Serial.begin(baud)
#define LOG_PRINT(...) Serial.print(__VA_ARGS__)
#define LOG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define LOG_PRINTLN_EMPTY() Serial.println()
#else
#define LOG_BEGIN(baud) do { (void)(baud); } while (0)
#define LOG_PRINT(...) do { } while (0)
#define LOG_PRINTLN(...) do { } while (0)
#define LOG_PRINTLN_EMPTY() do { } while (0)
#endif

#define MODUBOTIC_HAS_TFT 1
#define MODUBOTIC_BACKLIGHT_PIN CYD_TFT_BL
#define TFT_BACKLIGHT_ON HIGH

Arduino_DataBus *displayBus = new Arduino_ESP32SPI(
  CYD_TFT_DC,
  CYD_TFT_CS,
  CYD_TFT_SCK,
  CYD_TFT_MOSI,
  CYD_TFT_MISO,
  CYD_TFT_SPI_BUS
);

Arduino_GFX *display = new Arduino_ILI9341(
  displayBus,
  -1,
  0,
  false,
  CYD_TFT_HEIGHT,
  CYD_TFT_WIDTH
);

const uint16_t COLOR_BLACK = 0x0000;
const uint16_t COLOR_BLUE = 0x001F;
const uint16_t COLOR_RED = 0xF800;
const uint16_t COLOR_GREEN = 0x07E0;
const uint16_t COLOR_CYAN = 0x07FF;
const uint16_t COLOR_YELLOW = 0xFFE0;
const uint16_t COLOR_WHITE = 0xFFFF;
const uint16_t COLOR_TEXT = COLOR_WHITE;

#if !defined(MODUBOTIC_DISABLE_TOUCH) && __has_include(<XPT2046_Touchscreen.h>)
#include <XPT2046_Touchscreen.h>
#define MODUBOTIC_HAS_TOUCH 1
#define MODUBOTIC_TOUCH_IRQ 36
#define MODUBOTIC_TOUCH_MOSI 32
#define MODUBOTIC_TOUCH_MISO 39
#define MODUBOTIC_TOUCH_CLK 25
#define MODUBOTIC_TOUCH_CS 33
SPIClass touchSpi = SPIClass(VSPI);
XPT2046_Touchscreen touch(MODUBOTIC_TOUCH_CS, MODUBOTIC_TOUCH_IRQ);
#else
#define MODUBOTIC_HAS_TOUCH 0
#endif

#if MODUBOTIC_BODY_USE_USB_UART
#define BodySerial Serial
#else
HardwareSerial BodySerial(2);
#endif

#if MODUBOTIC_HAS_MQTT
WiFiClient mqttNetworkClient;
#if MODUBOTIC_HAS_ARDUINO_MQTT
MqttClient mqttClient(mqttNetworkClient);
#else
PubSubClient mqttClient(mqttNetworkClient);
#endif
#endif

String bodyLine;
String lastCommand = "NONE";
String lastBodyLine = "";
String lastBodyTxLine = "";
String lastBodyCommandTxLine = "";
String lastStatus = "boot";
String mqttClientId;
String mqttState = "off";
unsigned long lastCommandPollAt = 0;
unsigned long lastTelemetryFlushAt = 0;
unsigned long lastScreenRefreshAt = 0;
unsigned long lastTouchAt = 0;
unsigned long lastBodyPingAt = 0;
unsigned long lastBodyPongAt = 0;
unsigned long lastBodyRxAt = 0;
unsigned long lastBodyTxAt = 0;
unsigned long lastMqttConnectAttemptAt = 0;
unsigned long bodyRxByteCount = 0;
unsigned long bodyRxLineCount = 0;
unsigned long bodyTxPingCount = 0;
unsigned long bodyTxCommandCount = 0;
uint8_t lastBodyRxByte = 0;
bool screenOn = true;

void setupHeartbeatLeds();
void heartbeatBlink(int count);
void printDiagnostics();
void connectWiFi();
void setupDisplay();
void runDisplayBootTest();
void setupTouch();
void updateTouch();
void setScreenPower(bool enabled);
void setupMqtt();
void updateMqtt();
bool mqttIsReady();
void handleMqttCommand(const String &command);
#if MODUBOTIC_HAS_ARDUINO_MQTT
void mqttCallback(int messageSize);
#else
void mqttCallback(char *topic, byte *payload, unsigned int length);
#endif
String mqttTopic(const String &suffix);
void publishMqttLine(const String &suffix, const String &line, bool retained = false);
void publishMqttStatus();
void updateBodyHeartbeat();
void sendBodyLine(const String &line, bool commandLine = false);
void pollCommand();
void readBodySerial();
void forwardTelemetryLine(const String &line);
bool isTelemetryLine(const String &line);
bool isIgnorableCommand(const String &command);
String telemetryBaseUrl();
String httpPostText(const String &url);
void setStatus(const String &status);
void updateScreen(bool force = false);

void setup() {
  setupHeartbeatLeds();
  heartbeatBlink(3);

  LOG_BEGIN(115200);
  delay(800);
  printDiagnostics();

  BodySerial.begin(
    MODUBOTIC_BODY_BAUD,
    SERIAL_8N1,
    MODUBOTIC_BODY_RX_PIN,
    MODUBOTIC_BODY_TX_PIN
  );

  setupDisplay();
  setupTouch();
  runDisplayBootTest();

  setStatus("wifi");
  connectWiFi();
  setupMqtt();
  sendBodyLine("PING");
  setStatus("ready");
}

void setupHeartbeatLeds() {
#ifdef CYD_LED_RED
  pinMode(CYD_LED_RED, OUTPUT);
  pinMode(CYD_LED_GREEN, OUTPUT);
  pinMode(CYD_LED_BLUE, OUTPUT);
  CYD_LED_RGB_OFF();
#endif
}

void heartbeatBlink(int count) {
#ifdef CYD_LED_RED
  for (int i = 0; i < count; i++) {
    CYD_LED_BLUE_ON();
    delay(150);
    CYD_LED_BLUE_OFF();
    delay(150);
  }
#else
  (void)count;
#endif
}

void printDiagnostics() {
  LOG_PRINTLN_EMPTY();
  LOG_PRINTLN("=== ModuBotic CYD diagnostics ===");

#ifdef ARDUINO_BOARD
  LOG_PRINT("ARDUINO_BOARD: ");
  LOG_PRINTLN(ARDUINO_BOARD);
#endif

#ifdef ARDUINO_VARIANT
  LOG_PRINT("ARDUINO_VARIANT: ");
  LOG_PRINTLN(ARDUINO_VARIANT);
#endif

#ifdef ARDUINO_ESP32_2432S028R
  LOG_PRINTLN("Board macro: ARDUINO_ESP32_2432S028R");
#endif

  LOG_PRINT("WiFi SSID: ");
  LOG_PRINTLN(MODUBOTIC_WIFI_SSID);
  LOG_PRINT("Control URL: ");
  LOG_PRINTLN(MODUBOTIC_CONTROL_BASE_URL);
  LOG_PRINT("Robot/mind: ");
  LOG_PRINT(MODUBOTIC_ROBOT_ID);
  LOG_PRINT(" ");
  LOG_PRINTLN(MODUBOTIC_MIND_TYPE);
  LOG_PRINT("MQTT: ");
#if MODUBOTIC_HAS_MQTT
  LOG_PRINT(MODUBOTIC_MQTT_HOST);
  LOG_PRINT(":");
  LOG_PRINTLN(MODUBOTIC_MQTT_PORT);
#elif MODUBOTIC_MQTT_ENABLED
  LOG_PRINTLN("ArduinoMqttClient/PubSubClient not installed");
#else
  LOG_PRINTLN("disabled");
#endif
  LOG_PRINT("Body UART RX/TX/baud: ");
  LOG_PRINT(MODUBOTIC_BODY_RX_PIN);
  LOG_PRINT("/");
  LOG_PRINT(MODUBOTIC_BODY_TX_PIN);
  LOG_PRINT("/");
  LOG_PRINTLN(MODUBOTIC_BODY_BAUD);
  LOG_PRINT("Body ping interval/online ms: ");
  LOG_PRINT(MODUBOTIC_BODY_PING_MS);
  LOG_PRINT("/");
  LOG_PRINTLN(MODUBOTIC_BODY_ONLINE_MS);
  LOG_PRINT("Body raw byte debug: ");
#if MODUBOTIC_BODY_DEBUG_RAW
  LOG_PRINTLN("enabled");
#else
  LOG_PRINTLN("disabled");
#endif

#ifdef CYD_TFT_CS
  LOG_PRINTLN("CYD macros detected.");
  LOG_PRINT("CYD_TFT_CS/DC/SCK/MOSI/MISO/BL: ");
  LOG_PRINT(CYD_TFT_CS);
  LOG_PRINT("/");
  LOG_PRINT(CYD_TFT_DC);
  LOG_PRINT("/");
  LOG_PRINT(CYD_TFT_SCK);
  LOG_PRINT("/");
  LOG_PRINT(CYD_TFT_MOSI);
  LOG_PRINT("/");
  LOG_PRINT(CYD_TFT_MISO);
  LOG_PRINT("/");
  LOG_PRINTLN(CYD_TFT_BL);
  LOG_PRINT("CYD_TFT_WIDTH/HEIGHT: ");
  LOG_PRINT(CYD_TFT_WIDTH);
  LOG_PRINT("/");
  LOG_PRINTLN(CYD_TFT_HEIGHT);
#else
  LOG_PRINTLN("CYD macros NOT detected.");
#endif

  LOG_PRINTLN("Display library: Arduino_GFX");

#if MODUBOTIC_HAS_TOUCH
  LOG_PRINTLN("XPT2046_Touchscreen: detected");
#else
  LOG_PRINTLN("XPT2046_Touchscreen: disabled/not detected");
#endif

  LOG_PRINTLN("Display SPI port: CYD_TFT_SPI_BUS");

  LOG_PRINTLN("=================================");
}

void loop() {
  static unsigned long lastHeartbeatAt = 0;
  if (millis() - lastHeartbeatAt > 2000) {
    lastHeartbeatAt = millis();
    heartbeatBlink(1);
  }

  if (WiFi.status() != WL_CONNECTED) {
    setStatus("wifi reconnect");
    connectWiFi();
  }
  updateMqtt();

  const unsigned long now = millis();
  if (!mqttIsReady() && now - lastCommandPollAt >= MODUBOTIC_COMMAND_POLL_MS) {
    lastCommandPollAt = now;
    pollCommand();
  }

  readBodySerial();
  updateTouch();
  updateBodyHeartbeat();
  updateScreen();
}

void setupDisplay() {
#if MODUBOTIC_HAS_TFT
  LOG_PRINTLN("Display setup: begin");
#ifdef MODUBOTIC_BACKLIGHT_PIN
  LOG_PRINT("Display setup: backlight pin ");
  LOG_PRINTLN(MODUBOTIC_BACKLIGHT_PIN);
  pinMode(MODUBOTIC_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(MODUBOTIC_BACKLIGHT_PIN, TFT_BACKLIGHT_ON);
#endif
  LOG_PRINTLN("Display setup: display->begin()");
  if (!display->begin()) {
    LOG_PRINTLN("Display setup: begin failed");
  }
  LOG_PRINTLN("Display setup: begin done");
  display->fillScreen(COLOR_BLUE);
  display->setTextColor(COLOR_WHITE, COLOR_BLUE);
  display->setTextSize(2);
  display->setCursor(8, 8);
  display->println("ModuBotic CYD");
  display->setCursor(8, 34);
  display->println("BOOT");
  delay(1200);
#else
  LOG_PRINTLN("Display desativado.");
#endif
}

void setupTouch() {
#if MODUBOTIC_HAS_TOUCH
  touchSpi.begin(
    MODUBOTIC_TOUCH_CLK,
    MODUBOTIC_TOUCH_MISO,
    MODUBOTIC_TOUCH_MOSI,
    MODUBOTIC_TOUCH_CS
  );
  touch.begin(touchSpi);
  touch.setRotation(1);
  LOG_PRINTLN("Touch XPT2046 pronto.");
#else
  LOG_PRINTLN("XPT2046_Touchscreen nao encontrada; touch desativado.");
#endif
}

void runDisplayBootTest() {
#if MODUBOTIC_HAS_TFT
  LOG_PRINTLN("Display boot test: red");
  display->fillScreen(COLOR_RED);
  delay(700);
  LOG_PRINTLN("Display boot test: green");
  display->fillScreen(COLOR_GREEN);
  delay(700);
  LOG_PRINTLN("Display boot test: blue");
  display->fillScreen(COLOR_BLUE);
  delay(700);
  LOG_PRINTLN("Display boot test: text");
  display->fillScreen(COLOR_BLACK);
  display->setTextColor(COLOR_TEXT, COLOR_BLACK);
  display->setTextSize(2);
  display->setCursor(8, 8);
  display->println("ModuBotic CYD");
  display->setCursor(8, 34);
  display->println("Display OK");
  delay(1200);
#else
  LOG_PRINTLN("Display desativado.");
#endif
}

void updateTouch() {
#if MODUBOTIC_HAS_TOUCH
  if (!touch.touched()) {
    return;
  }

  unsigned long now = millis();
  if (now - lastTouchAt < 600) {
    return;
  }
  lastTouchAt = now;

  setScreenPower(!screenOn);
  LOG_PRINT("Touch -> screen ");
  LOG_PRINTLN(screenOn ? "ON" : "OFF");
#endif
}

void setScreenPower(bool enabled) {
  screenOn = enabled;
#ifdef MODUBOTIC_BACKLIGHT_PIN
  digitalWrite(MODUBOTIC_BACKLIGHT_PIN, enabled ? TFT_BACKLIGHT_ON : !TFT_BACKLIGHT_ON);
#endif
#if MODUBOTIC_HAS_TFT
  if (enabled) {
    updateScreen(true);
  }
#endif
}

void updateBodyHeartbeat() {
  const unsigned long now = millis();
  if (now - lastBodyPingAt < MODUBOTIC_BODY_PING_MS) {
    return;
  }

  lastBodyPingAt = now;
  sendBodyLine("PING");
  LOG_PRINTLN("CYD -> BODY: PING");
  setStatus("body ping");
}

void sendBodyLine(const String &line, bool commandLine) {
  BodySerial.println(line);
  lastBodyTxLine = line;
  lastBodyTxAt = millis();
  if (line == "PING") {
    bodyTxPingCount++;
  }
  if (commandLine) {
    bodyTxCommandCount++;
    lastBodyCommandTxLine = line;
  }
  publishMqttLine("debug/body-tx", line);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(MODUBOTIC_WIFI_SSID, MODUBOTIC_WIFI_PASSWORD);

  LOG_PRINT("WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    LOG_PRINT(".");
    updateScreen(true);
  }

  LOG_PRINTLN_EMPTY();
  LOG_PRINT("IP: ");
  LOG_PRINTLN(WiFi.localIP());
}

void setupMqtt() {
#if MODUBOTIC_HAS_MQTT
  mqttState = "setup";
#if MODUBOTIC_HAS_ARDUINO_MQTT
  mqttClientId = String("modubotic-") + MODUBOTIC_ROBOT_ID;
  mqttClient.setId(mqttClientId.c_str());
  mqttClient.onMessage(mqttCallback);
#else
  mqttClient.setServer(MODUBOTIC_MQTT_HOST, MODUBOTIC_MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
#endif
#elif MODUBOTIC_MQTT_ENABLED
  mqttState = "no lib";
#else
  mqttState = "off";
#endif
}

void updateMqtt() {
#if MODUBOTIC_HAS_MQTT
  if (WiFi.status() != WL_CONNECTED) {
    mqttState = "no wifi";
    return;
  }

  if (!mqttClient.connected()) {
    const unsigned long now = millis();
    if (now - lastMqttConnectAttemptAt < MODUBOTIC_MQTT_RETRY_MS) {
      return;
    }
    lastMqttConnectAttemptAt = now;
    mqttState = "connect";
    String clientId = String("modubotic-") + MODUBOTIC_ROBOT_ID;

#if MODUBOTIC_HAS_ARDUINO_MQTT
    mqttClientId = clientId;
    mqttClient.setId(mqttClientId.c_str());
    if (mqttClient.connect(MODUBOTIC_MQTT_HOST, MODUBOTIC_MQTT_PORT)) {
      String commandTopic = mqttTopic("command");
      mqttClient.subscribe(commandTopic.c_str());
      publishMqttStatus();
      mqttState = "MQTT";
      setStatus("mqtt");
    } else {
      mqttState = "fail";
    }
#else
    String statusTopic = mqttTopic("status");
    String offline = String("{\"robotId\":\"") + MODUBOTIC_ROBOT_ID +
                     "\",\"online\":false}";
    if (mqttClient.connect(clientId.c_str(), statusTopic.c_str(), 0, true, offline.c_str())) {
      mqttClient.subscribe(mqttTopic("command").c_str());
      publishMqttStatus();
      mqttState = "MQTT";
      setStatus("mqtt");
    } else {
      mqttState = "fail";
    }
#endif
    return;
  }

  mqttState = "MQTT";
#if MODUBOTIC_HAS_ARDUINO_MQTT
  mqttClient.poll();
#else
  mqttClient.loop();
#endif
#endif
}

bool mqttIsReady() {
#if MODUBOTIC_HAS_MQTT
  return mqttClient.connected();
#else
  return false;
#endif
}

void handleMqttCommand(const String &rawCommand) {
  String command = rawCommand;
  command.trim();
  command.toUpperCase();
  if (command.length() == 0 || isIgnorableCommand(command)) {
    return;
  }

  lastCommand = command;
  sendBodyLine(command, true);
  publishMqttLine("debug/body-tx", command);
  setStatus("mqtt cmd");
}

#if MODUBOTIC_HAS_ARDUINO_MQTT
void mqttCallback(int messageSize) {
  if (mqttClient.messageTopic() != mqttTopic("command")) {
    while (mqttClient.available()) {
      mqttClient.read();
    }
    return;
  }

  String command;
  while (messageSize-- > 0 && mqttClient.available()) {
    command += (char)mqttClient.read();
  }
  handleMqttCommand(command);
}
#else
void mqttCallback(char *topic, byte *payload, unsigned int length) {
#if MODUBOTIC_HAS_PUBSUB_MQTT
  String topicText = String(topic);
  if (topicText != mqttTopic("command")) {
    return;
  }

  String command;
  for (unsigned int i = 0; i < length; i++) {
    command += (char)payload[i];
  }
  handleMqttCommand(command);
#else
  (void)topic;
  (void)payload;
  (void)length;
#endif
}
#endif

String mqttTopic(const String &suffix) {
  return String(MODUBOTIC_MQTT_BASE_TOPIC) + "/" + MODUBOTIC_ROBOT_ID + "/" + suffix;
}

void publishMqttLine(const String &suffix, const String &line, bool retained) {
#if MODUBOTIC_HAS_MQTT
  if (!mqttClient.connected()) {
    return;
  }
#if MODUBOTIC_HAS_ARDUINO_MQTT
  String topic = mqttTopic(suffix);
  mqttClient.beginMessage(topic.c_str(), retained);
  mqttClient.print(line);
  mqttClient.endMessage();
#else
  mqttClient.publish(mqttTopic(suffix).c_str(), line.c_str(), retained);
#endif
#else
  (void)suffix;
  (void)line;
  (void)retained;
#endif
}

void publishMqttStatus() {
  String payload = String("{\"robotId\":\"") + MODUBOTIC_ROBOT_ID +
                   "\",\"mindType\":\"" + MODUBOTIC_MIND_TYPE +
                   "\",\"ip\":\"" + WiFi.localIP().toString() +
                   "\",\"online\":true}";
  publishMqttLine("status", payload, true);
}

void pollCommand() {
  String url = String(MODUBOTIC_CONTROL_BASE_URL) +
               "/api/command?device=CYD%20ESP32&ip=" +
               WiFi.localIP().toString();
  String command = httpPostText(url);
  command.trim();
  command.toUpperCase();

  if (command.length() == 0 || isIgnorableCommand(command)) {
    return;
  }

  lastCommand = command;
  sendBodyLine(command, true);
  LOG_PRINT("PANEL -> BODY: ");
  LOG_PRINTLN(command);
  setStatus("sent command");
}

void readBodySerial() {
  int bytesRead = 0;
  while (BodySerial.available() > 0 && bytesRead < 64) {
    bytesRead++;
    char c = (char)BodySerial.read();
    bodyRxByteCount++;
    lastBodyRxByte = (uint8_t)c;
#if MODUBOTIC_BODY_DEBUG_RAW
    LOG_PRINT("BODY RX byte 0x");
    if ((uint8_t)c < 16) {
      LOG_PRINT("0");
    }
    LOG_PRINT((uint8_t)c, HEX);
    LOG_PRINT(" '");
    if (c >= 32 && c <= 126) {
      LOG_PRINT(c);
    } else {
      LOG_PRINT(".");
    }
    LOG_PRINTLN("'");
#endif
    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      bodyLine.trim();
      if (bodyLine.length() > 0) {
        bodyRxLineCount++;
        lastBodyLine = bodyLine;
        lastBodyRxAt = millis();
        publishMqttLine("debug/body-rx", bodyLine);
        LOG_PRINT("BODY -> CYD: ");
        LOG_PRINTLN(bodyLine);

        if (bodyLine == "PONG") {
          lastBodyPongAt = millis();
          publishMqttStatus();
          setStatus("body ok");
        }

        if (MODUBOTIC_FORWARD_BODY_TELEMETRY && isTelemetryLine(bodyLine)) {
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

  String url = telemetryBaseUrl() + "&" + line;
  String response = httpPostText(url);
  response.trim();

  LOG_PRINT("TELEMETRY -> PANEL: ");
  LOG_PRINT(line);
  LOG_PRINT(" -> ");
  LOG_PRINTLN(response);
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

String telemetryBaseUrl() {
  return String(MODUBOTIC_CONTROL_BASE_URL) +
         "/api/telemetry?device=CYD%20ESP32&ip=" +
         WiFi.localIP().toString();
}

String httpPostText(const String &url) {
  if (WiFi.status() != WL_CONNECTED) {
    return "ERROR";
  }

  HTTPClient http;
  http.setTimeout(500);
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
  display->fillScreen(COLOR_BLACK);
  display->setTextSize(2);
  display->setTextColor(COLOR_TEXT, COLOR_BLACK);
  display->setCursor(8, 8);
  display->print("Robot ");
  display->println(MODUBOTIC_ROBOT_ID);

  display->setTextColor(COLOR_TEXT, COLOR_BLACK);
  display->setCursor(8, 36);
  display->print("Net ");
  if (WiFi.status() != WL_CONNECTED) {
    display->print("...");
  } else if (mqttIsReady()) {
    display->print("MQTT");
  } else {
    display->print("HTTP ");
    display->print(mqttState.substring(0, 6));
  }

  display->setTextColor(COLOR_TEXT, COLOR_BLACK);
  display->setCursor(8, 64);
  display->print("IP ");
  display->print(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "-");

  display->setTextColor(COLOR_TEXT, COLOR_BLACK);
  display->setCursor(8, 92);
  display->print("Cmd ");
  display->print(lastCommand.substring(0, 16));

  display->setTextColor(COLOR_TEXT, COLOR_BLACK);
  display->setCursor(8, 120);
  display->print("Tx ");
  if (lastBodyCommandTxLine.length() > 0) {
    display->print(lastBodyCommandTxLine.substring(0, 16));
  } else if (lastBodyTxLine.length() > 0) {
    display->print(lastBodyTxLine.substring(0, 16));
  } else {
    display->print("-");
  }

  display->setTextColor(COLOR_TEXT, COLOR_BLACK);
  display->setCursor(8, 148);
  display->print("Rx ");
  if (lastBodyLine.length() > 0) {
    display->print(lastBodyLine.substring(0, 16));
  } else {
    display->print("-");
  }

  display->setTextColor(COLOR_TEXT, COLOR_BLACK);
  display->setCursor(8, 176);
  display->print("Body ");
  if (lastBodyPongAt) {
    display->print("P");
    display->print((millis() - lastBodyPongAt) / 1000);
    display->print("s");
  } else {
    display->print("P-");
  }
  display->print(" ");
  if (lastBodyRxAt) {
    display->print("R");
    display->print((millis() - lastBodyRxAt) / 1000);
    display->print("s");
  } else {
    display->print("R-");
  }

  display->setTextColor(COLOR_TEXT, COLOR_BLACK);
  display->setCursor(8, 204);
  display->print("L");
  display->print(bodyRxLineCount);
  display->print("/");
  display->print(bodyRxByteCount);
  display->print(" T");
  display->print(bodyTxPingCount);
  display->print("/");
  display->print(bodyTxCommandCount);

  display->setTextColor(COLOR_TEXT, COLOR_BLACK);
  display->setCursor(8, 232);
  display->print("State ");
  display->print(lastStatus.substring(0, 14));
#else
  (void)force;
#endif
}

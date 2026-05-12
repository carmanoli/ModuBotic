/*
  ModuBotic body controller - Arduino UNO R4

  USB Serial:
    Debug console in Arduino IDE.

  Serial1:
    UART link to UNIHIKER K10.

  Wiring for UNO R4:
    UNO R4 D1 TX -> K10 RX/P0 or chosen serial RX
    UNO R4 D0 RX <- K10 TX/P1 or chosen serial TX
    UNO R4 GND   -> K10 GND
    UNO R4 D4    -> MAX7219 DIN
    UNO R4 D5    -> MAX7219 CS/LOAD
    UNO R4 D6    -> MAX7219 CLK
    MAX7219      -> 2x 8x8 LED matrix modules in cascade
    LCD I2C SDA  -> UNO R4 SDA
    LCD I2C SCL  -> UNO R4 SCL
    GY-33 VCC    -> 3.3V or 5V
    GY-33 GND    -> GND
    GY-33 SDA/DR -> UNO R4 SDA
    GY-33 SCL/CT -> UNO R4 SCL
    GY-33 S0/SO  -> GND for module I2C mode
    LCD I2C VCC  -> 5V
    LCD I2C GND  -> GND
    AHT20/BMP280 SDA -> UNO R4 SDA
    AHT20/BMP280 SCL -> UNO R4 SCL
    AHT20/BMP280 VCC -> 3.3V or 5V according to module
    AHT20/BMP280 GND -> GND
    Left arm servo       -> UNO R4 D12
    External arm sensors -> UNO R4 SDA/SCL I2C bus

  Protocol:
    K10 -> UNO: HEAD_LEFT
    UNO -> K10: RECEIVED HEAD_LEFT
*/

#include <Servo.h>
#include "eyes.h"
#include "lcd_display.h"
#include "gy33_color_sensor.h"
#include "aht20_bmp280_sensor.h"

const unsigned long BAUD_CONSOLE = 115200;
const unsigned long BAUD_LINK = 9600;

const int LEFT_ARM_SERVO_PIN = 12;
const int HEAD_SERVO_PIN = 11;
const int RIGHT_ARM_SERVO_PIN = 10;
const int LEFT_WHEEL_SERVO_PIN = 9;
const int RIGHT_WHEEL_SERVO_PIN = 8;

const int HEAD_LEFT_ANGLE = 135;
const int HEAD_CENTER_ANGLE = 90;
const int HEAD_RIGHT_ANGLE = 45;

const int LEFT_ARM_UP_ANGLE = 45;
const int LEFT_ARM_CENTER_ANGLE = 90;
const int LEFT_ARM_DOWN_ANGLE = 135;

const int RIGHT_ARM_UP_ANGLE = 135;
const int RIGHT_ARM_CENTER_ANGLE = 90;
const int RIGHT_ARM_DOWN_ANGLE = 45;

const int LEFT_WHEEL_STOP_US = 1500;
const int RIGHT_WHEEL_STOP_US = 1500;
const int LEFT_WHEEL_FORWARD_US = 2000;
const int LEFT_WHEEL_BACK_US = 1000;
const int RIGHT_WHEEL_FORWARD_US = 1000;
const int RIGHT_WHEEL_BACK_US = 2000;
const unsigned long BODY_SERVO_DETACH_DELAY_MS = 700;

String consoleLine;
String k10Line;
String lastCommand = "NONE";
Servo headServo;
Servo leftArmServo;
Servo rightArmServo;
Servo leftWheelServo;
Servo rightWheelServo;
unsigned long lastBodyServoMoveAt = 0;
int activeLeftWheelUs = LEFT_WHEEL_STOP_US;
int activeRightWheelUs = RIGHT_WHEEL_STOP_US;
bool wheelsActive = false;

bool isIgnoredK10Message(const String &command);

void setup() {
  Serial.begin(BAUD_CONSOLE);
  Serial1.begin(BAUD_LINK);

  enableLcd(false);
  detachBodyServos();
  detachWheelServos();
  setupGy33ColorSensor();
  setupAht20Bmp280Sensor();
  setupEyes();
  quietAllServoPins();


  unsigned long serialStart = millis();
  while (!Serial && millis() - serialStart < 1500) {
    delay(10);
  }

  Serial.println("ModuBotic body pronto.");
  Serial.println("Comandos aceites:");
  Serial.println("  HEAD_LEFT, HEAD_CENTER, HEAD_RIGHT");
  Serial.println("  ARM_LEFT_UP, ARM_LEFT_DOWN");
  Serial.println("  ARM_RIGHT_UP, ARM_RIGHT_DOWN");
  Serial.println("  MOVE_FORWARD, MOVE_BACK, TURN_LEFT, TURN_RIGHT, STOP, WHEELS_TEST");
  Serial.println("  LCD_ON, LCD_OFF, EYES_ON, EYES_OFF, EYES_RESET");
  Serial.println("  EYES_ALL_ON, EYES_CLEAR, EYES_SCAN");
  Serial.println("  GY33_SCAN, GY33_DEBUG, GY33_READ, GY33_LED_ON, GY33_LED_OFF, GY33_ON, GY33_OFF");
  Serial.println("  ENV_SCAN, ENV_READ, ENV_ON, ENV_OFF");
  Serial.println();
  Serial.println("Tambem podes escrever um comando nesta consola para testar.");
  Serial.print("LCD I2C: ");
  if (!isLcdEnabled()) {
    Serial.println("desligado no arranque; usa LCD_ON para testar");
  } else if (isLcdAvailable()) {
    Serial.print("encontrado em 0x");
    Serial.println(getLcdAddress(), HEX);
  } else {
    Serial.println("nao encontrado em 0x27 nem 0x3F");
  }

  Serial1.println("BODY_READY");
}

void loop() {
  readConsole();
  readK10();

  updateGy33ColorSensor();
  printGy33ReadingToSerial();
  printGy33TelemetryToStream(Serial1);
  clearGy33ReadingUpdated();
  updateAht20Bmp280Sensor();
  printAht20Bmp280ReadingToSerial();
  printAht20Bmp280TelemetryToStream(Serial1);
  clearAht20Bmp280ReadingUpdated();
  updateLcdTelemetry(getAht20TemperatureC(), getAht20Humidity(), isAht20Bmp280ReadingOk());
  updateEyes();
  detachIdleServos();
}

void readConsole() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      handleCommand(consoleLine, "USB");
      consoleLine = "";
    } else {
      consoleLine += c;
    }
  }
}

void readK10() {
  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();
    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      handleCommand(k10Line, "K10");
      k10Line = "";
    } else {
      k10Line += c;
    }
  }
}

void handleCommand(String command, const char *source) {
  command.trim();
  command.toUpperCase();
  command = normalizeCommand(command);

  if (command.length() == 0) {
    return;
  }

  if (command == "NONE" || isIgnoredK10Message(command)) {
    return;
  }

  if (command == "PING") {
    Serial.println("BODY: PONG");
    Serial1.println("PONG");
    return;
  }

  Serial.print(source);
  Serial.print(" -> BODY: ");
  Serial.println(command);

  if (isKnownCommand(command)) {
    lastCommand = command;
    applyCommand(command);

    Serial.print("BODY: RECEIVED ");
    Serial.println(command);

    if (command != "GY33_READ" && command != "ENV_READ" && command != "AHT20_BMP280_READ") {
      Serial1.print("RECEIVED ");
      Serial1.println(command);
    }
  } else {
    Serial.print("BODY: UNKNOWN ");
    Serial.println(command);

    Serial1.print("UNKNOWN ");
    Serial1.println(command);
  }
}

String normalizeCommand(String command) {
  if (isKnownCommand(command) || command == "NONE" || command == "PING") {
    return command;
  }

  int length = command.length();
  if (length >= 2 && command.charAt(length - 1) == command.charAt(length - 2)) {
    String shortened = command.substring(0, length - 1);
    if (isKnownCommand(shortened) || shortened == "NONE" || shortened == "PING") {
      return shortened;
    }
  }

  return command;
}

bool isIgnoredK10Message(const String &command) {
  return command == "OK" ||
         command == "ERROR" ||
         command == "ERROR ON HTTP REQUEST" ||
         command == "HTTP ERROR" ||
         command == "TIMEOUT" ||
         command == "REQUEST TIMEOUT";
}

bool isKnownCommand(const String &command) {
  return command == "HEAD_LEFT" ||
         command == "HEAD_CENTER" ||
         command == "HEAD_RIGHT" ||
         command == "ARM_LEFT_UP" ||
         command == "ARM_LEFT_CENTER" ||
         command == "ARM_LEFT_DOWN" ||
         command == "ARM_RIGHT_UP" ||
         command == "ARM_RIGHT_CENTER" ||
         command == "ARM_RIGHT_DOWN" ||
         command == "MOVE_FORWARD" ||
         command == "MOVE_BACK" ||
         command == "TURN_LEFT" ||
         command == "TURN_RIGHT" ||
         command == "LEFT_WHEEL_FORWARD" ||
         command == "LEFT_WHEEL_BACK" ||
         command == "RIGHT_WHEEL_FORWARD" ||
         command == "RIGHT_WHEEL_BACK" ||
         command == "WHEELS_TEST" ||
         command == "STOP" ||
         command == "EYES_RESET" ||
         command == "EYES_ON" ||
         command == "EYES_OFF" ||
         command == "LCD_ON" ||
         command == "LCD_OFF" ||
         command == "EYES_ALL_ON" ||
         command == "EYES_CLEAR" ||
         command == "EYES_SCAN" ||
         command == "GY33_SCAN" ||
         command == "GY33_DEBUG" ||
         command == "GY33_READ" ||
         command == "GY33_LED_ON" ||
         command == "GY33_LED_OFF" ||
         command == "GY33_ON" ||
         command == "GY33_OFF" ||
         command == "ENV_SCAN" ||
         command == "ENV_READ" ||
         command == "ENV_ON" ||
         command == "ENV_OFF" ||
         command == "AHT20_BMP280_SCAN" ||
         command == "AHT20_BMP280_READ" ||
         command == "AHT20_BMP280_ON" ||
         command == "AHT20_BMP280_OFF" ||
         command.startsWith("COMMAND ");
}

void applyCommand(const String &command) {
  if (command == "HEAD_LEFT") {
    moveBodyServo(headServo, HEAD_SERVO_PIN, HEAD_LEFT_ANGLE);
  } else if (command == "HEAD_CENTER") {
    moveBodyServo(headServo, HEAD_SERVO_PIN, HEAD_CENTER_ANGLE);
  } else if (command == "HEAD_RIGHT") {
    moveBodyServo(headServo, HEAD_SERVO_PIN, HEAD_RIGHT_ANGLE);
  } else if (command == "ARM_LEFT_UP") {
    moveBodyServo(leftArmServo, LEFT_ARM_SERVO_PIN, LEFT_ARM_UP_ANGLE);
  } else if (command == "ARM_LEFT_CENTER") {
    moveBodyServo(leftArmServo, LEFT_ARM_SERVO_PIN, LEFT_ARM_CENTER_ANGLE);
  } else if (command == "ARM_LEFT_DOWN") {
    moveBodyServo(leftArmServo, LEFT_ARM_SERVO_PIN, LEFT_ARM_DOWN_ANGLE);
  } else if (command == "ARM_RIGHT_UP") {
    moveBodyServo(rightArmServo, RIGHT_ARM_SERVO_PIN, RIGHT_ARM_UP_ANGLE);
  } else if (command == "ARM_RIGHT_CENTER") {
    moveBodyServo(rightArmServo, RIGHT_ARM_SERVO_PIN, RIGHT_ARM_CENTER_ANGLE);
  } else if (command == "ARM_RIGHT_DOWN") {
    moveBodyServo(rightArmServo, RIGHT_ARM_SERVO_PIN, RIGHT_ARM_DOWN_ANGLE);
  } else if (command == "MOVE_FORWARD") {
    enterDriveMode();
    driveWheels(LEFT_WHEEL_FORWARD_US, RIGHT_WHEEL_FORWARD_US);
  } else if (command == "MOVE_BACK") {
    enterDriveMode();
    driveWheels(LEFT_WHEEL_BACK_US, RIGHT_WHEEL_BACK_US);
  } else if (command == "TURN_LEFT") {
    enterDriveMode();
    driveWheels(LEFT_WHEEL_BACK_US, RIGHT_WHEEL_FORWARD_US);
  } else if (command == "TURN_RIGHT") {
    enterDriveMode();
    driveWheels(LEFT_WHEEL_FORWARD_US, RIGHT_WHEEL_BACK_US);
  } else if (command == "LEFT_WHEEL_FORWARD") {
    enterDriveMode();
    driveWheels(LEFT_WHEEL_FORWARD_US, RIGHT_WHEEL_STOP_US);
  } else if (command == "LEFT_WHEEL_BACK") {
    enterDriveMode();
    driveWheels(LEFT_WHEEL_BACK_US, RIGHT_WHEEL_STOP_US);
  } else if (command == "RIGHT_WHEEL_FORWARD") {
    enterDriveMode();
    driveWheels(LEFT_WHEEL_STOP_US, RIGHT_WHEEL_FORWARD_US);
  } else if (command == "RIGHT_WHEEL_BACK") {
    enterDriveMode();
    driveWheels(LEFT_WHEEL_STOP_US, RIGHT_WHEEL_BACK_US);
  } else if (command == "WHEELS_TEST") {
    enterDriveMode();
    driveWheels(LEFT_WHEEL_FORWARD_US, RIGHT_WHEEL_FORWARD_US);
    runWheelsForDuration(1000);
    exitDriveMode();
  } else if (command == "STOP") {
    exitDriveMode();
    centerBody();
  } else if (command == "EYES_RESET") {
    resetEyes();
  } else if (command == "EYES_ON") {
    enableEyes(true);
  } else if (command == "EYES_OFF") {
    enableEyes(false);
  } else if (command == "LCD_ON") {
    enableLcd(true);
    Serial.print("LCD: ");
    if (isLcdAvailable()) {
      Serial.print("ON em 0x");
      Serial.println(getLcdAddress(), HEX);
    } else {
      Serial.println("nao encontrado");
    }
  } else if (command == "LCD_OFF") {
    enableLcd(false);
    Serial.println("LCD: OFF");
  } else if (command == "EYES_ALL_ON") {
    showEyesAllOn();
  } else if (command == "EYES_CLEAR") {
    clearEyesForTest();
  } else if (command == "EYES_SCAN") {
    startEyesScanTest();
  } else if (command == "GY33_SCAN") {
    scanGy33I2cBus();
  } else if (command == "GY33_DEBUG") {
    debugGy33ColorSensor();
  } else if (command == "GY33_READ") {
    requestGy33ColorReading();
    updateGy33ColorSensor();
    printGy33ReadingToSerial();
    printGy33TelemetryToStream(Serial1);
    clearGy33ReadingUpdated();
  } else if (command == "GY33_LED_ON") {
    setGy33LedPower(10);
  } else if (command == "GY33_LED_OFF") {
    setGy33LedPower(0);
  } else if (command == "GY33_ON") {
    enableGy33ColorSensor(true);
  } else if (command == "GY33_OFF") {
    enableGy33ColorSensor(false);
  } else if (command == "ENV_SCAN" || command == "AHT20_BMP280_SCAN") {
    scanAht20Bmp280I2cBus();
  } else if (command == "ENV_READ" || command == "AHT20_BMP280_READ") {
    requestAht20Bmp280Reading();
    updateAht20Bmp280Sensor();
    printAht20Bmp280ReadingToSerial();
    printAht20Bmp280TelemetryToStream(Serial1);
    clearAht20Bmp280ReadingUpdated();
  } else if (command == "ENV_ON" || command == "AHT20_BMP280_ON") {
    enableAht20Bmp280Sensor(true);
  } else if (command == "ENV_OFF" || command == "AHT20_BMP280_OFF") {
    enableAht20Bmp280Sensor(false);
  }
}

void attachServoIfNeeded(Servo &servo, int pin) {
  if (!servo.attached()) {
    servo.attach(pin);
  }
}

void quietServoPin(int pin) {
  digitalWrite(pin, LOW);
  pinMode(pin, OUTPUT);
}

void quietAllServoPins() {
  quietServoPin(LEFT_ARM_SERVO_PIN);
  quietServoPin(HEAD_SERVO_PIN);
  quietServoPin(RIGHT_ARM_SERVO_PIN);
  quietServoPin(LEFT_WHEEL_SERVO_PIN);
  quietServoPin(RIGHT_WHEEL_SERVO_PIN);
}

void moveBodyServo(Servo &servo, int pin, int angle) {
  attachServoIfNeeded(servo, pin);
  servo.write(angle);
  lastBodyServoMoveAt = millis();
}

void detachIdleServos() {
  if (millis() - lastBodyServoMoveAt < BODY_SERVO_DETACH_DELAY_MS) {
    return;
  }

  detachBodyServos();
}

void detachBodyServos() {
  if (headServo.attached()) {
    headServo.detach();
  }
  quietServoPin(HEAD_SERVO_PIN);

  if (leftArmServo.attached()) {
    leftArmServo.detach();
  }
  quietServoPin(LEFT_ARM_SERVO_PIN);

  if (rightArmServo.attached()) {
    rightArmServo.detach();
  }
  quietServoPin(RIGHT_ARM_SERVO_PIN);
}

void enterDriveMode() {
  enableEyes(false);
  detachBodyServos();
}

void exitDriveMode() {
  stopWheels();
  enableEyes(true);
}

void attachWheelServos() {
  if (!leftWheelServo.attached()) {
    leftWheelServo.attach(LEFT_WHEEL_SERVO_PIN, 500, 2500);
  }
  if (!rightWheelServo.attached()) {
    rightWheelServo.attach(RIGHT_WHEEL_SERVO_PIN, 500, 2500);
  }
}

void detachWheelServos() {
  if (leftWheelServo.attached()) {
    leftWheelServo.detach();
  }
  quietServoPin(LEFT_WHEEL_SERVO_PIN);

  if (rightWheelServo.attached()) {
    rightWheelServo.detach();
  }
  quietServoPin(RIGHT_WHEEL_SERVO_PIN);
}

void driveWheels(int leftMicroseconds, int rightMicroseconds) {
  attachWheelServos();
  activeLeftWheelUs = leftMicroseconds;
  activeRightWheelUs = rightMicroseconds;
  wheelsActive = true;
  leftWheelServo.writeMicroseconds(activeLeftWheelUs);
  rightWheelServo.writeMicroseconds(activeRightWheelUs);
}

void stopWheels() {
  attachWheelServos();
  leftWheelServo.writeMicroseconds(LEFT_WHEEL_STOP_US);
  rightWheelServo.writeMicroseconds(RIGHT_WHEEL_STOP_US);
  delay(120);
  wheelsActive = false;
  activeLeftWheelUs = LEFT_WHEEL_STOP_US;
  activeRightWheelUs = RIGHT_WHEEL_STOP_US;
}

void runWheelsForDuration(unsigned long durationMs) {
  unsigned long startedAt = millis();
  while (millis() - startedAt < durationMs) {
    delay(10);
  }
}

void sendWheelPulse(int leftMicroseconds, int rightMicroseconds) {
  attachWheelServos();
  leftWheelServo.writeMicroseconds(leftMicroseconds);
  rightWheelServo.writeMicroseconds(rightMicroseconds);
}

void centerBody() {
  stopWheels();
  moveBodyServo(headServo, HEAD_SERVO_PIN, HEAD_CENTER_ANGLE);
  moveBodyServo(leftArmServo, LEFT_ARM_SERVO_PIN, LEFT_ARM_CENTER_ANGLE);
  moveBodyServo(rightArmServo, RIGHT_ARM_SERVO_PIN, RIGHT_ARM_CENTER_ANGLE);
}

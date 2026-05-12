#include <Servo.h>

const int LEFT_WHEEL_SERVO_PIN = 9;
const int RIGHT_WHEEL_SERVO_PIN = 8;

Servo leftWheel;
Servo rightWheel;

void setup() {
  Serial.begin(115200);
  leftWheel.attach(LEFT_WHEEL_SERVO_PIN, 500, 2500);
  rightWheel.attach(RIGHT_WHEEL_SERVO_PIN, 500, 2500);

  stopWheels();
  Serial.println("Wheel timing test ready.");
}

void loop() {
  Serial.println("Forward: left 2000us, right 1000us");
  leftWheel.writeMicroseconds(2000);
  rightWheel.writeMicroseconds(1000);
  delay(3000);

  Serial.println("Stop: 1500us");
  stopWheels();
  delay(3000);

  Serial.println("Back: left 1000us, right 2000us");
  leftWheel.writeMicroseconds(1000);
  rightWheel.writeMicroseconds(2000);
  delay(3000);

  Serial.println("Stop: 1500us");
  stopWheels();
  delay(3000);
}

void stopWheels() {
  leftWheel.writeMicroseconds(1500);
  rightWheel.writeMicroseconds(1500);
}

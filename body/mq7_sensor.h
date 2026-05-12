#ifndef MODUBOTIC_MQ7_SENSOR_H
#define MODUBOTIC_MQ7_SENSOR_H

void setupMq7Sensor();
void updateMq7Sensor();
void printMq7ReadingToSerial();
void selectMq7LeftArm();
void selectMq7RightArm();
void disableMq7Sensor();
bool hasMq7Reading();
bool isMq7ReadingOk();
int getMq7RawValue();

#endif

#define CLK 13
#define DT 12
#define SW 14
#include <Wire.h>
#include <GyverEncoder.h>

Encoder enc1(CLK, DT, SW);

void isr() {
  enc1.tick();  // отработка в прерывании
}

void setup() {
  Serial.begin(9600);
  attachInterrupt(0, isr, CHANGE);    // прерывание на 2 пине! CLK у энка
}

void loop() {
  enc1.tick();  // отработка
  if (enc1.isRight()) Serial.println("Right");         // если был поворот
  if (enc1.isLeft()) Serial.println("Left");
  if (enc1.isRightH()) Serial.println("Right holded"); // если было удержание + поворот
  if (enc1.isLeftH()) Serial.println("Left holded");
}
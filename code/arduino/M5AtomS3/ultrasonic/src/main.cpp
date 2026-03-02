
/**
 * @author SeanKwok (shaoxiang@m5stack.com)
 * @brief M5AtomS3 Atomic GPS Base Test
 * @version 0.1
 * @date 2023-12-13
 *
 * https://wiki.dfrobot.com/Underwater_Ultrasonic_Obstacle_Avoidance_Sensor_6m_SKU_SEN0599
 *
 * @Hardwares: M5AtomS3 + Atomic GPS Base
 * @Platform Version: Arduino M5Stack Board Manager v2.0.9
 * @Dependent Library:
 * M5GFX: https://github.com/m5stack/M5GFX
 * M5Unified: https://github.com/m5stack/M5Unified
 * M5AtomS3: https://github.com/m5stack/M5AtomS3
 * TinyGPSPlus: https://github.com/mikalhart/TinyGPSPlus
 *
 * AOZ1282CI Simple Buck Regulator. 4.5V to 36V operating input voltage range
 * 1.2A continuous output current
 */

// #include "HardwareSerial.h"
#include <Arduino.h>
// #include <M5Unified.h>
#include "M5AtomS3.h"

unsigned char buffer_RTT[4] = {0};
uint8_t CS;
// #define COM 0x55
// #define COM 0xFF
#define COM 0x00
int Distance = 0;
int distance_mm = 0;

bool readSensor(int &dist);

void setup() {
  delay(1000);
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  AtomS3.begin(cfg);
  // Serial.begin(115200);
  // delay(1000);
  // AtomS3.begin(cfg);

  AtomS3.Display.setTextColor(GREEN);
  AtomS3.Display.setTextDatum(middle_center);
  // AtomS3.Display.setFont(&fonts::FreeSerifBold24pt7b);
  AtomS3.Display.setFont(&fonts::FreeMono9pt7b);
  AtomS3.Display.setTextSize(1);
  AtomS3.Display.drawString("GPS", M5.Display.width() / 2,
                            M5.Display.height() / 2);

  int textsize = AtomS3.Display.height() / 60;
  if (textsize == 0) {
    textsize = 1;
  }
  AtomS3.Display.setTextSize(textsize);

  M5.Log.println("RTC found.");
  // Serial.println(F("GPS data received: check wiring"));
  //   2363][E][esp32-hal-uart.c:285] uartSetPins(): UART2 set pins GPS data
  //   received: check wiring

  // Serial2.begin(4800, SERIAL_8N1, 22, 19);
  // Serial2.begin(4800, SERIAL_8N1, 5, 6);
  Serial2.begin(115200, SERIAL_8N1, 5, 6);
  Serial.println(F("Serial2 connected. Wait data"));
}
int count = 0;

unsigned long nextTime = 0;

void loop() {
  unsigned long msecNow = millis();
  AtomS3.update();
  /*
    AtomS3.update();
    if (AtomS3.BtnA.wasPressed()) {
      AtomS3.Display.clear();
      AtomS3.Display.drawString("Pressed", AtomS3.Display.width() / 2,
                                AtomS3.Display.height() / 2);
      Serial.println("Pressed");
    }
    if (AtomS3.BtnA.wasReleased()) {
      AtomS3.Display.clear();
      AtomS3.Display.drawString("Released", AtomS3.Display.width() / 2,
                                AtomS3.Display.height() / 2);
      Serial.println("Released");
    }
    */
  if (msecNow >= nextTime) {
    nextTime = msecNow + 500;
    Serial2.write(COM);
    if (readSensor(distance_mm)) {
      Serial.print("Distance: ");
      Serial.print(distance_mm);
      Serial.print(" mm  (");
      Serial.print(distance_mm / 10.0, 1);
      Serial.println(" cm)");
    } else {
      // Serial.println("Read error ");
      M5.Log.printf("Read error\n");
    }

    // delay(200); // ~10 readings per second
  }
}

bool readSensor(int &dist) {
  // Wait for header byte
  unsigned long timeout = millis() + 500;
  while (Serial2.available() < 1) {
    if (millis() > timeout) {
      M5.Log.printf("First Timeout ");
      return false;
    }
  }
  M5.Log.printf("First Wait: %d\n", 500 - (timeout - millis()));

  // Flush until we find the 0xFF header
  timeout = millis() + 200;
  while (Serial2.available()) {
    if (Serial2.peek() == 0xFF)
      break;
    if (millis() > timeout) {
      M5.Log.printf("2 Timeout ");
      return false;
    }
    Serial2.read(); // discard non-header bytes
  }

  // Wait for full 4-byte frame
  timeout = millis() + 100;
  while (Serial2.available() < 4) {
    // if (millis() > timeout) return false;
    if (millis() > timeout) {
      M5.Log.printf("3 Timeout ");
      return false;
    }
  }

  byte buf[4];
  Serial2.readBytes(buf, 4);
  M5.Log.printf("Read 4 bytes ");
  for (int i = 0; i < 4; i++) {
    M5.Log.printf(" %d: 0x%02X\t", i, buf[i]);
    // Serial.printf("count:%d %d: 0x%02X\n", i, buffer_RTT[i]);
  }
  M5.Log.printf("\n");

  // Validate header
  if (buf[0] != 0xFF)
    return false;

  // Validate checksum
  byte checksum = (buf[0] + buf[1] + buf[2]) & 0xFF;
  if (checksum != buf[3])
    return false;

  // Calculate distance
  dist = (buf[1] << 8) | buf[2];

  return true;
}

void loop3() {
  Serial2.write(COM);
  delay(500);
  if (Serial2.available() > 0) {
    delay(10);
    buffer_RTT[0] = Serial2.read();
    if (buffer_RTT[0] == 0xff) {
      // Serial.println("Read 0xFF:");
      M5.Log.printf("Read 0xFF: count:%d\n", count++);
      buffer_RTT[0] = 0xff;
      for (int i = 1; i < 4; i++) {
        buffer_RTT[i] = Serial2.read();
        M5.Log.printf(" %d: 0x%02X\t", i, buffer_RTT[i]);
        // Serial.printf("count:%d %d: 0x%02X\n", i, buffer_RTT[i]);
      }
      M5.Log.printf("\n");
      CS = buffer_RTT[0] + buffer_RTT[1] + buffer_RTT[2];
      if (buffer_RTT[3] == CS) {
        Distance = (buffer_RTT[1] << 8) + buffer_RTT[2];
        Serial.print("Distance:");
        Serial.print(Distance);
        Serial.println("mm");
      }
    } else {
      M5.Log.printf("Read  0x%02X\n", buffer_RTT[0]);
    }
  }
}

void loopBack() {
  while (Serial.available() > 0) {
    Serial2.write(Serial.read());
    yield();
  }
  while (Serial2.available() > 0) {
    Serial.write(Serial2.read());
    yield();
  }
}


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
 */

#include <Arduino.h>
#include <M5Unified.h>

unsigned char buffer_RTT[4] = {0};
uint8_t CS;
#define COM 0x55
int Distance = 0;

void setup() {
  delay(1000);
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  // Serial.begin(115200);
  // delay(1000);
  // AtomS3.begin(cfg);

  M5.Display.setTextColor(GREEN);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::FreeSerifBold24pt7b);
  M5.Display.setTextSize(1);
  M5.Display.drawString("GPS", M5.Display.width() / 2, M5.Display.height() / 2);

  M5.Log.println("RTC foggund.");
  // Serial.println(F("GPS data received: check wiring"));
  //   2363][E][esp32-hal-uart.c:285] uartSetPins(): UART2 set pins GPS data
  //   received: check wiring

  // Serial2.begin(4800, SERIAL_8N1, 22, 19);
  // Serial2.begin(4800, SERIAL_8N1, 5, 6);
  Serial2.begin(115200, SERIAL_8N1, 5, 6);
  Serial.println(F("Serial2 connected. Wait data"));
}

void loop() {
  Serial2.write(COM);
  delay(100);
  if (Serial2.available() > 0) {
    delay(4);
    if (Serial2.read() == 0xff) {
      buffer_RTT[0] = 0xff;
      for (int i = 1; i < 4; i++) {
        buffer_RTT[i] = Serial2.read();
      }
      CS = buffer_RTT[0] + buffer_RTT[1] + buffer_RTT[2];
      if (buffer_RTT[3] == CS) {
        Distance = (buffer_RTT[1] << 8) + buffer_RTT[2];
        Serial.print("Distance:");
        Serial.print(Distance);
        Serial.println("mm");
      }
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

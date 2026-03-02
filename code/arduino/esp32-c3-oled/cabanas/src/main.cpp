/*
 * This is a simple example of reading from a serial distance sensor and
 * displaying the result on an OLED screen. The sensor is expected to send
 * 4-byte frames with the following format:
 *
 * Byte 0: 0xFF (header)
 * Byte 1: High byte of distance (in mm)
 * Byte 2: Low byte of distance (in mm)
 * Byte 3: Checksum (sum of bytes 0-2 modulo 256)
 *
 * The code initializes the OLED display, sets up the serial communication,
 * and periodically reads from the sensor. If a valid frame is received, it
 * extracts the distance and displays it on the OLED. If there is an error
 * in reading or validating the frame, it logs an error message.
 *
 * Note: The code assumes that the sensor is connected to Serial1 (RX pin) and
 * that the OLED is connected via I2C with specified SDA and SCL pins.
 *
 * https://wiki.dfrobot.com/Underwater_Ultrasonic_Obstacle_Avoidance_Sensor_6m_SKU_SEN0599#More%20Documents
 */
#include <Arduino.h>
#include <WiFiMulti.h>
// #include <Esp.h>
//  Include the libraries we need
#include "EEPROM.h"
// Displays sensor values on the built-in 72x40 OLED
//
#include <OneBitDisplay.h>
#include <Wire.h>
//
/*
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#include "HardwareSerial.h"
#include "arduino_secrets.h"
*/
// #define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
// #define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

ONE_BIT_DISPLAY obd;
// Need to explicitly set the I2C pin numbers on this C3 board
#define SDA_PIN 5
#define SCL_PIN 6
#define LED_PIN 8
#define BOOTSEL_PIN 9
#define COM 0xFF

static const char *TAG = "EspC3";
const unsigned int samplingInterval = 2000;
unsigned long previousMillis = 0;
bool bootState = true; // active low

int distance_mm = 0;

void setup() {

  Wire.begin(SDA_PIN, SCL_PIN);
  obd.setI2CPins(SDA_PIN, SCL_PIN);
  obd.I2Cbegin(OLED_72x40);
  obd.allocBuffer();
  //  obd.setContrast(50);
  obd.fillScreen(0);
  obd.setFont(FONT_8x8);
  obd.println("Starting...");
  obd.display();
  pinMode(LED_PIN, OUTPUT);
  pinMode(20, INPUT); // RX IN

  digitalWrite(LED_PIN, true); // LED off
  pinMode(BOOTSEL_PIN, INPUT_PULLUP);

  bool ok = false;
  Serial.begin(115200);
  // Serial2.begin(115200, SERIAL_8O2, rxPin, txPin); // UART2 hardware

  Serial1.begin(115200, SERIAL_8N1, RX, TX);
  // while (!Serial);
  //  wait for serial port to connect. Needed for native USB port only
  esp_log_level_set("*", ESP_LOG_INFO); // set all components to ERROR level
#if defined(ESP_PLATFORM)
  ESP_LOGE(TAG,
           "using ESP_LOGE error log."); // Critical errors, software module can
                                         // not recover on its own
  ESP_LOGW(TAG,
           "using ESP_LOGW warn log."); // Error conditions from which recovery
                                        // measures have been taken
  ESP_LOGI(TAG, "using ESP_LOGI info log.");  // Information messages which
                                              // describe normal flow of events
  ESP_LOGD(TAG, "using ESP_LOGD debug log."); // Extra information which is not
                                              // necessary for normal use
                                              // (values, pointers, sizes, etc).
  ESP_LOGV(
      TAG,
      "using ESP_LOGV verbose log."); // Bigger chunks of debugging information,
                                      // or frequent messages which can
                                      // potentially flood the output.
#endif
  ESP_LOGI(TAG, "ESP-32-C3 RX %d", RX);
}

bool readSensor(int &dist) {
  // Wait for header byte
  unsigned long timeout = millis() + 100;
  // Flush any existing data
  while (Serial1.available() > 0) {
    Serial1.read();
    // yield();
  }
  Serial1.write(COM);
  delay(8); // give sensor time to respond
  while (Serial1.available() < 1) {
    if (millis() > timeout) {
      // M5.Log.printf("First Timeout ");
      return false;
    }
  }
  // M5.Log.printf("First Wait: %d\n", 500 - (timeout - millis()));

  // Flush until we find the 0xFF header
  timeout = millis() + 100;
  while (Serial1.available()) {
    if (Serial1.peek() == 0xFF)
      break;
    if (millis() > timeout) {
      // M5.Log.printf("2 Timeout ");
      return false;
    }
    Serial1.read(); // discard non-header bytes
  }

  // Wait for full 4-byte frame
  timeout = millis() + 100;
  while (Serial1.available() < 4) {
    // if (millis() > timeout) return false;
    if (millis() > timeout) {
      // M5.Log.printf("3 Timeout ");
      return false;
    }
  }
  byte buf[4];
  Serial1.readBytes(buf, 4);
  // M5.Log.printf("Read 4 bytes ");

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

void loop() {
  unsigned long currentMillis = millis();
  // ledState = not ledState;
  bootState = digitalRead(BOOTSEL_PIN);
  if (not bootState) {
    digitalWrite(LED_PIN, false); // Turn ON
  } else {
    digitalWrite(LED_PIN, true); // Turn OFF
  }
  if (currentMillis - previousMillis > samplingInterval) {
    previousMillis = currentMillis;
    // ESP_LOGI(TAG, "Dallas Temperature IC Control Library Demo");
    if (readSensor(distance_mm)) {
      ESP_LOGI(TAG, "Distance: %d mm", distance_mm);
      obd.setCursor(0, 0);
      obd.fillScreen(0);
      obd.printf("D: %d ", distance_mm);

      obd.display();
      // M5.Display.width() / 2,
      // M5.Display.height() / 2);
    } else {
      // Serial.println("Read error ");
      ESP_LOGI(TAG, "Read error\n");
    }
  }
}

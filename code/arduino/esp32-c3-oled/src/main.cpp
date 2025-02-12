/**
 * Device 0
0x28, 0x1B, 0x3D, 0x57, 00x4, 0xE1, 0x3C, 0xB9,
Device 1
0x28, 0x47, 0xED, 0xA8, 0xB2, 0x23, 00x6, 0xD7,

 */

#include <Arduino.h>
#include <WiFiMulti.h>
// Include the libraries we need
#include <OneWire.h>
#include <DallasTemperature.h>
#include "arduino_secrets.h"
// Displays sensor values on the built-in 72x40 OLED
//
#include <Wire.h>
#include <OneBitDisplay.h>

ONE_BIT_DISPLAY obd;
// Need to explicitly set the I2C pin numbers on this C3 board
#define SDA_PIN 5
#define SCL_PIN 6

#define DEVICE "ESP32"

#define TZ_INFO "UTC"

#define WIFI_RETRIES 20
WiFiMulti wifiMulti;
uint8_t WiFiStatus = WL_DISCONNECTED;

bool connect_wifi() {
	bool ret = false;
   // M5_LOGI(", Waiting for WiFi... ");
    for(int i = 0; i < WIFI_RETRIES; i++) {
        if(wifiMulti.run() != WL_CONNECTED) {
            //while(wifiMulti.run() != WL_CONNECTED) {
            //M5_LOGW(".");
            Serial.print(".");
            delay(500);
        }
        else {
            Serial.println(" WiFi connected. ");
            //M5_LOGI("IP address: ");
            IPAddress ip = WiFi.localIP();
            //M5_LOGI("%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            //M5.Display.print("WiFi connected.");
            ret = true;
            break;
        }
    }
    if (ret) {
    // Accurate time is necessary for certificate validation and writing in batches
    // We use the NTP servers in your area as provided by: https://www.pool.ntp.org/zone/
    // Syncing progress and the time will be printed to Serial.
        //timeSync(TZ_INFO, "ntp1.tecnico.ulisboa.pt", "pool.ntp.org");
    }
    else
        //M5_LOGE(" Fail to connect WiF, giving up.");
        //M5_LOGE
        Serial.println(" Fail to connect WiFi, giving up.");

    return ret;
}

const unsigned int samplingInterval = 2000;
unsigned long previousMillis = 0;

void lightSleep(uint64_t time_in_ms)
{
#ifdef HAL_ESP32_HAL_H_
  esp_sleep_enable_timer_wakeup(time_in_ms * 1000);
  esp_light_sleep_start();
#else
  delay(time_in_ms);
#endif
} /* lightSleep() */ 
unsigned int count = 0;
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

    bool ok = false;
    Serial.begin(115200);
      while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // attempt to connect to Wifi network:
  Serial.print("Attempting to connect to SSID: ");

    for(int i = 0; i < NUM_SSID; i++) {
        wifiMulti.addAP(ssids[i], pass[i]);
    }
    ok = connect_wifi();
     Serial.println("Dallas Temperature IC Control Library Demo");
}
void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > samplingInterval) {
        previousMillis += samplingInterval;
        // call sensors.requestTemperatures() to issue a global temperature
      obd.setCursor(0,0);
      obd.printf("CO2: %d \n\n", count); // mySensor.getCO2());
      obd.printf("Temp: %d \n\n",  WiFiStatus); // (int)mySensor.getTemperature());
      obd.printf("Hum: %d%%",  count++); // (int)mySensor.getHumidity());
      obd.display();
        WiFiStatus = wifiMulti.run();
        Serial.print(count);
        Serial.println(" Count");

    }

}

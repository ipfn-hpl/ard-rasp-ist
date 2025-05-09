
#include <Arduino.h>
#include <WiFiMulti.h>
#include <Wire.h>

#include "arduino_secrets.h"
// At file scope, define it before including esp_log.h, e.g.:
//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

static const char* TAG = "ESP32lolin";
const int ledPin = LED_BUILTIN;  // the number of the LED pin GPIO 22

bool ledStatus = false;
void setup() {   

    pinMode(LED_BUILTIN, OUTPUT);
    //Wire.begin(SDA_PIN, SCL_PIN);
    bool ok = false;
    Serial.begin(115200);
    esp_log_level_set("*", ESP_LOG_INFO);        // set all components to ERROR LOG_LOCAL_LEVEL
    ESP_LOGI(TAG, "Attempting to connect to SSID: ");
}

void loop() {

    ESP_LOGE(TAG, "Hello Led Pin %d", ledPin);
    ESP_LOGE(TAG, "Critical errors, software module can not recover on its own");
	ESP_LOGW(TAG, "Error conditions from which recovery measures have been taken");
	ESP_LOGI(TAG, "Information messages which describe normal flow of events");
	ESP_LOGD(TAG, "Extra information which is not necessary for normal use (values, pointers, sizes, etc)");
	ESP_LOGV(TAG, "Bigger chunks of debugging information, or frequent messages which can potentially flood the output");

    Serial.println("hello");
    digitalWrite(LED_BUILTIN, ledStatus); 
    ledStatus = not ledStatus;
    delay(1000);
}

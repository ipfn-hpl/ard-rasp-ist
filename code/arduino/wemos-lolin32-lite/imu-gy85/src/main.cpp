//#if (!PLATFORMIO)
// Enable Arduino-ESP32 logging in Arduino IDE
#ifdef CORE_DEBUG_LEVEL
#undef CORE_DEBUG_LEVEL
#endif
#ifdef LOG_LOCAL_LEVEL
#undef LOG_LOCAL_LEVEL
#endif

#define CORE_DEBUG_LEVEL 4
#define LOG_LOCAL_LEVEL CORE_DEBUG_LEVEL
//#endif  

#include <Arduino.h>
#include <Wire.h>

//#include <esp32-hal-log.h>
#include "esp_log.h"
//
#include "I2Cdev.h"
#include "ADXL345.h"
static const char* TAG = "wemos32lolin";

ADXL345 accel;

bool ledStatus = false;

void i2c_scan() {
    byte error, address;
    uint8_t nDevices = 0;
    Serial.println("Scanning for I2C devices ...");
    for (address = 0x01; address < 0x7f; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            Serial.printf("I2C device found at address 0x%02X\n", address);
            nDevices++;
        } else if (error != 2) {
            Serial.printf("Error %d at address 0x%02X\n", error, address);
        }
    }
    if (nDevices == 0) {
        Serial.println("No I2C devices found");
    }
}

void setup() {   

    //  Library pins  SDA 19  SCL 23
    //Wire.begin(SDA_PIN, SCL_PIN);
    Wire.begin();
    pinMode(LED_BUILTIN, OUTPUT);
    bool ok = false;
    Serial.begin(115200);
    // while (!Serial);
    delay(1000);
    i2c_scan();
    Serial.println("Initializing I2C devices...");
    accel.initialize();
    Serial.println(accel.testConnection() ? "ADXL345 connection successful" : "ADXL345 connection failed");
    Serial.printf("LOG_LOCAL_LEVEL %d\n", LOG_LOCAL_LEVEL);
    //                                   esp32-hal-log.h            esp_log.h
    //                       level 0 = ARDUHAL_LOG_LEVEL_NONE    = ESP_LOG_NONE 
    ESP_LOGE(TAG, "ESP_LOGE, level 1 = ARDUHAL_LOG_LEVEL_ERROR   = ESP_LOG_ERROR");
    ESP_LOGW(TAG, "ESP_LOGW, level 2 = ARDUHAL_LOG_LEVEL_WARN    = ESP_LOG_WARN");    
    ESP_LOGI(TAG, "ESP_LOGI, level 3 = ARDUHAL_LOG_LEVEL_INFO    = ESP_LOG_INFO");
    ESP_LOGD(TAG, "ESP_LOGD, level 4 = ARDUHAL_LOG_LEVEL_DEBUG   = ESP_LOG_DEBUG");
    ESP_LOGV(TAG, "ESP_LOGV, level 5 = ARDUHAL_LOG_LEVEL_VERBOSE = ESP_LOG_VERBOSE");
    ESP_LOGE(TAG, "using ESP_LOGE error log.");    // Critical errors, software module can not recover on its own
    ESP_LOGW(TAG, "using ESP_LOGW warn log.");     // Error conditions from which recovery measures have been taken
    ESP_LOGI(TAG, "using ESP_LOGI info log.");     // Information messages which describe normal flow of events
    ESP_LOGD(TAG, "using ESP_LOGD debug log.");    // Extra information which is not necessary for normal use (values, pointers, sizes, etc).
    ESP_LOGV(TAG, "using ESP_LOGV verbose log.");  // Bigger chunks of debugging information, or frequent messages which can potentially flood the output.

    log_i("Info Message");
    log_e("Error Message!");

}

void loop() {
    //uint8_t buffer[6];
    //int16_t ax, ay, az;
    int16_t a[3]; 
    //const 
    char * buffer = (char *) &a[0];
    // read raw accel measurements from device
    //accel.getAcceleration(&ax, &ay, &az);
    accel.getAcceleration(&a[0], &a[1], &a[2]);

    // display comma-separated accel x/y/z values
    //Serial.printf("Accel x,y,z: %d, %d, %d\n", ax, ay, az);
    Serial.printf("Accel x,y,z: %d, %d, %d\n", a[0], a[1], a[2]);
    //Serial.write(buffer, 6); // send binary data
    digitalWrite(LED_BUILTIN, ledStatus); 
    ledStatus = not ledStatus;
    delay(1000);
}

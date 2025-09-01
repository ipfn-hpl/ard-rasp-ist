
#include <Arduino.h>
#include <WiFiMulti.h>
#include <Wire.h>

//#include <DFRobot_ITG3200.h>

#include "arduino_secrets.h"
// At file scope, define it before including esp_log.h, e.g.:
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

static const char* TAG = "ESP32lolin";
const int ledPin = LED_BUILTIN;  // the number of the LED pin GPIO 22
                                 //
// //call setPins() first, so that begin() can be called without arguments from libraries
//  bool setPins(int sda, int scl);
//#define I2C_ADDR 0x68
/*
 * GY-85 BMP085 Sensor Modules 3V-5V 9 Axis Sensor Module (ITG3205 +ADXL345 + HMC5883L)
 * ,6DOF 9DOF IMU Sensor
 */
#include "I2Cdev.h"
#include "ITG3200.h" // ITG-3205
#include "HMC5883L.h"  // HA5883
#include "ADXL345.h"
//#include "AK8975.h"


// class default I2C address is 0x68
// specific I2C addresses may be passed as a parameter here
// AD0 low = 0x68 (default for SparkFun 6DOF board)
// AD0 high = 0x69 (default for SparkFun ITG-3200 standalone board)
ITG3200 gyro;
//DFRobot_ITG3200 gyro = DFRobot_ITG3200(&Wire, I2C_ADDR);
//float xyz[3], temperature;

// class default I2C address is 0x1E
// specific I2C addresses may be passed as a parameter here
// this device only supports one I2C address (0x1E)
// 3-Axis Digital Compass IC 
HMC5883L mag(0x0D);
//AK8975 mag(0x0D);

ADXL345 accel;


bool ledStatus = false;

int16_t gx, gy, gz;
int16_t ax, ay, az;

void setup() {   

    Wire.setPins(13,15);
    Wire.begin();
    pinMode(LED_BUILTIN, OUTPUT);
    //Wire.begin(SDA_PIN, SCL_PIN);
    bool ok = false;
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nI2C Scanner");
    uint8_t nDevices = 0;
    for(int address = 1; address < 127; address++ ) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address<16) {
                Serial.print("0");
            }
            Serial.println(address,HEX);
            nDevices++;
        }
        else if (error==4) {
            Serial.print("Unknown error at address 0x");
            if (address<16) {
                Serial.print("0");
            }
            Serial.println(address,HEX);
        }    
    }
    Serial.println("Initializing I2C devices...");
    gyro.initialize();
    mag.initialize();
    accel.initialize();
    //gyro_begin();
    //gyro.reset();
    //esp_log_level_set("*", ESP_LOG_INFO);        // set all components to ERROR LOG_LOCAL_LEVEL
    //esp_log_level_set("*", ESP_LOG_DEBUG);
    ESP_LOGI(TAG, "Attempting to connect to SSID: ");
    // verify connection
    Serial.println("Testing device connections...");
    Serial.println(gyro.testConnection() ? "ITG3200 connection successful" : "ITG3200 connection failed");
    // ITG3200 connection successful
    //Serial.println(mag.testConnection() ? "AK8975 connection successful" : "AK8975 connection failed");

    Serial.println(mag.testConnection() ? "HMC5883L connection successful" : "HMC5883L connection failed");
    // HMC5883L connection failed
    Serial.println(accel.testConnection() ? "ADXL345 connection successful" : "ADXL345 connection failed");
    // ADXL345 connection successful

}

void loop() {

    //ESP_LOGE(TAG, "Hello Led Pin %d", ledPin);
    ESP_LOGE(TAG, "Critical errors");
    //, software module can not recover on its own");
	ESP_LOGW(TAG, "Error conditions from which recovery measures have been taken");
	ESP_LOGI(TAG, "Information messages which describe normal flow of events");
	ESP_LOGD(TAG, "Extra information which is not necessary for normal use (values, pointers, sizes, etc)");
	ESP_LOGV(TAG, "Bigger chunks of debugging information, or frequent messages which can potentially flood the output");

    // read raw gyro measurements from device
    gyro.getRotation(&gx, &gy, &gz);

    // display tab-separated gyro x/y/z values
    Serial.print("gyro:\t");
    Serial.print(gx); Serial.print("\t");
    Serial.print(gy); Serial.print("\t");
    Serial.println(gz);

    int16_t tp = gyro.getTemperature();
    uint8_t id = gyro.getDeviceID();

    Serial.print(id, HEX);
    Serial.print(" ");
    Serial.print(tp);
    Serial.println(":temp");

    // read raw accel measurements from device
    accel.getAcceleration(&ax, &ay, &az);

    // display tab-separated accel x/y/z values
    Serial.print("accel:\t");
    Serial.print(ax); Serial.print("\t");
    Serial.print(ay); Serial.print("\t");
    Serial.println(az);

     log_i("Found %d devices.", 2);

    //Serial.println("hello");
    digitalWrite(LED_BUILTIN, ledStatus); 
    ledStatus = not ledStatus;
    delay(1000);
}
/*
    if (gyro.isRawdataReady())
    {
        gyro.readGyro(xyz);
        Serial.print("X:");
        Serial.print(xyz[0]);
        Serial.print("  Y:");
        Serial.print(xyz[1]);
        Serial.print("  Z:");
        Serial.println(xyz[2]);
    }
bool gyro_begin(){
    gyro.setSamplerateDiv(NOSRDIVIDER);
    gyro.setFSrange(RANGE2000);
    gyro.setFilterBW(BW256_SR8);
    setClocksource(PLL_XGYRO_REF);
    gyro.setItgready(true);
    gyro.setRawdataReady(true);
    gyro.delay(GYROSTART_UP_DELAY);
    return true;

}
     * I2C device found at address 0x0D
I2C device found at address 0x53
I2C device found at address 0x68
    uint8_t nDevices = 0;
    for(int address = 1; address < 127; address++ ) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address<16) {
                Serial.print("0");
            }
            Serial.println(address,HEX);
            nDevices++;
        }
        else if (error==4) {
            Serial.print("Unknown error at address 0x");
            if (address<16) {
                Serial.print("0");
            }
            Serial.println(address,HEX);
        }    
    }
    if (nDevices == 0) {
        Serial.println("No I2C devices found\n");
    }
    */

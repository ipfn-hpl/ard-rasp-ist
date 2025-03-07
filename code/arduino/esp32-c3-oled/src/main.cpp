/**
 * Device 0
 0x28, 0x1B, 0x3D, 0x57, 00x4, 0xE1, 0x3C, 0xB9,
 Device 1
 0x28, 0x47, 0xED, 0xA8, 0xB2, 0x23, 00x6, 0xD7,

*/

#include <Arduino.h>
#include <WiFiMulti.h>
//#include <Esp.h>
// Include the libraries we need
#include "EEPROM.h"
#include <OneWire.h>
#include <DallasTemperature.h>
// Displays sensor values on the built-in 72x40 OLED
//
#include <Wire.h>
#include <OneBitDisplay.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#include "arduino_secrets.h"

//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
//#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

ONE_BIT_DISPLAY obd;
// Need to explicitly set the I2C pin numbers on this C3 board
#define SDA_PIN 5
#define SCL_PIN 6
#define LED_PIN 8

#define DEVICE "ESP32"

#define TZ_INFO "UTC"

#define WIFI_RETRIES 20
WiFiMulti wifiMulti;
uint8_t WiFiStatus = WL_DISCONNECTED;
bool ledState = false;

// Data wire is plugged into port G33 on the M5Stick
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 12

EEPROMClass SENSORS("eeprom0");

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature dt_oneWire(&oneWire);

// arrays to hold device addresses
const int max_sensors = 8;
DeviceAddress ds18Sensors[max_sensors];
float sensorTemp[max_sensors];
float sensorDiff = 0.0;
int buttonState;
/*
 * Yellow Sensors
 * [EspC3] CHIP MAC: 14b74de385a0

 * [EspC3] 0x28, 0x8B, 0xD0, 0x7F, 0xB2, 0x23, 0x6,0xDA
   [EspC3] 0x28, 0xDB, 0x0B,0xEE,  0xB2, 0x23,0x06,0xB4
   Black Sensors 
   [EspC3] CHIP MAC: 646b4ee385a0
   [EspC3] 0x28, 0xAA, 0xAF, 0x77, 0xB0, 0x23, 0x07, 0x54
   [EspC3] 0x28, 0xA3, 0xA0, 0xB9, 0xB0, 0x23, 0x07, 0x81


 * */

DeviceAddress redThermometers[][8] = {
    {0x28, 0xAA, 0xAF, 0x77, 0xB0, 0x23, 0x07, 0x54},

    {0x28, 0x8B, 0xD0, 0x7F, 0xB2, 0x23, 0x06, 0xDA},
    {0x28, 0x1B, 0x3D, 0x57, 0x04, 0xE1, 0x3C, 0xB9}
};
DeviceAddress redThermometer  =  {0x28, 0x1B, 0x3D, 0x57, 0x04, 0xE1, 0x3C, 0xB9};

//    0x28, 0x46, 0x91, 0x46, 0xD4, 0xB7, 0x1F, 0x7D};
DeviceAddress blueThermometer  = {0x28, 0x47, 0xED, 0xA8, 0xB2, 0x23, 0x06, 0xD7};

#define WRITE_PRECISION WritePrecision::US
#define MAX_BATCH_SIZE 500
// The recommended size is at least 2 x batch size.
#define WRITE_BUFFER_SIZE 3 * MAX_BATCH_SIZE

// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient iflxClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN,
        InfluxDbCloud2CACert);

// Declare Data point
Point iflxSensor("ds18b20Sensors");

bool iflxConnected = false;


//    0x28, 0x91, 0x8F, 0xCB, 0xB0, 0x23, 0x9, 0x25};

uint8_t dsCount = 0;

static const char* TAG = "EspC3";
const unsigned int samplingInterval = 2000;
unsigned long previousMillis = 0;


void printDtAddress(DeviceAddress deviceAddress);

/*/ function to print a Dallas device address
*/
// function to set a Dallas device address
void cpyDtAddress(DeviceAddress dest, DeviceAddress src)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        dest[i] = src[i];
    }
}
void writeDtAddress(DeviceAddress src)
{
    uint8_t addr;
    for (uint8_t i = 0; i < 8; i++)
    {
        addr = src[i];
        SENSORS.writeByte(i, addr);
        ESP_LOGI(TAG, "Saved add %d:%u", i, addr);

    }
}

void readDtAddress(DeviceAddress dest)
{
    uint8_t addr;
    for (uint8_t i = 0; i < 8; i++)
    {
        addr = SENSORS.readByte(i);
        ESP_LOGI(TAG, "read add %d:0x%X", i, addr);
        //dest[i] = addr;
    }
}

bool init_onewire() {
    bool ok = false;

    // Start up the DALLAS Temp library
    dt_oneWire.begin();
    // locate devices on the bus
    dsCount = dt_oneWire.getDeviceCount();
    log_i("Found %d devices.", dsCount);
    //Serial.println(dsCount, DEC);
    ok = (dsCount > 0u);
    if(ok) {
  // method 1: by index
        for (uint8_t i = 0; i < 2; i++) {
            if (!dt_oneWire.getAddress(ds18Sensors[i], i)) 
                ESP_LOGW(TAG, "Unable to find address for Device %d", i);
            else 
                printDtAddress(ds18Sensors[i]);

        }
    }

    // report parasite power requirements
    log_i("Parasite power is: ");
    if (dt_oneWire.isParasitePowerMode())
        ESP_LOGI(TAG, "ON");
        //log_i("ON\n");
    else
        log_i("OFF\n");
    return ok;
}

unsigned setup_dallas(){
    unsigned int found = 0;
    if (!dt_oneWire.isConnected(redThermometer))
    {
        ESP_LOGI(TAG, "Unable to find Device 0 (red)");
    }
    else {
        cpyDtAddress(ds18Sensors[0], redThermometer);
        dt_oneWire.setResolution(ds18Sensors[0], TEMPERATURE_PRECISION);
        ESP_LOGI(TAG, "Red Temp sensor, Resolution: %d",
                dt_oneWire.getResolution(ds18Sensors[0]));
        found++;
    }
    if (!dt_oneWire.isConnected(blueThermometer))
    {
            ESP_LOGI("TAG", "Unable to find Device 1 (blue)");
    }
    else {
        cpyDtAddress(ds18Sensors[1], blueThermometer);
        dt_oneWire.setResolution(ds18Sensors[1], TEMPERATURE_PRECISION);
        ESP_LOGI(TAG, "Blue Temp sensor, Resolution: %d", 
                dt_oneWire.getResolution(ds18Sensors[1]));
        //Serial.println(dt_oneWire.getResolution(ds18Sensors[1]), DEC);
        found++;
    }
    return found;
}


bool send_iflx_data(InfluxDBClient client, Point iflx) {
    iflx.clearFields();
    iflx.addField("tempRed", sensorTemp[0]);
    iflx.addField("tempBlue", sensorTemp[1]);

    return client.writePoint(iflx);
}

bool connect_wifi(int retries) {
    bool ret = false;
    for(int i = 0; i < retries; i++) {
        if(wifiMulti.run() == WL_CONNECTED) {
            //Serial.println(" WiFi connected. ");
            log_i("IP address: ");
            IPAddress ip = WiFi.localIP();
            log_i("%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            //M5.Display.print("WiFi connected.");
            ret = true;
            break;
        }
        else {
            log_i("..");
            delay(500);
        }
    }
    if (ret) {
        // Accurate time is necessary for certificate validation and writing in batches
        // We use the NTP servers in your area as provided by: https://www.pool.ntp.org/zone/
        // Syncing progress and the time will be printed to Serial.
        //timeSync(TZ_INFO, "ntp1.tecnico.ulisboa.pt", "pool.ntp.org");
        configTzTime("UTC", "ntp1.tecnico.ulisboa.pt", "pool.ntp.org", "time.nis.gov");
    }
    else
        ESP_LOGW(TAG, " Fail to connect WiFi, giving up.");

    return ret;
}

void validate_influx() {
    // Check server connection
    if (iflxClient.validateConnection()) {
        ESP_LOGI(TAG, "Connected to InfluxDB: ");
        ESP_LOGI(TAG, "%s", iflxClient.getServerUrl());
        iflxConnected  = true;
    } else {
        //Serial.print("InfluxDB connection failed: ");
        ESP_LOGW(TAG, "InfluxDB Not connected %s", iflxClient.getLastErrorMessage());
    }
}
void lightSleep(uint64_t time_in_ms)
{
#ifdef HAL_ESP32_HAL_H_
        esp_sleep_enable_timer_wakeup(time_in_ms * 1000);
        esp_light_sleep_start();
#else
        delay(time_in_ms);
#endif
} /* lightSleep() */ // function to print a Dallas device address
void printDtAddress(DeviceAddress deviceAddress)
{
    ESP_LOGI(TAG, "0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X", 
            deviceAddress[0], deviceAddress[1], deviceAddress[2],deviceAddress[3],
            deviceAddress[4], deviceAddress[5], deviceAddress[6],deviceAddress[7]);
}
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
        pinMode(LED_PIN, OUTPUT);
        pinMode(20, INPUT); // RX IN

        digitalWrite(LED_PIN, ledState);

        bool ok = false;
        Serial.begin(115200);
        while (!Serial);
            // wait for serial port to connect. Needed for native USB port only
        esp_log_level_set("*", ESP_LOG_INFO);        // set all components to ERROR level
        #if defined ( ESP_PLATFORM )
        ESP_LOGE(TAG, "using ESP_LOGE error log.");    // Critical errors, software module can not recover on its own
        ESP_LOGW(TAG, "using ESP_LOGW warn log.");     // Error conditions from which recovery measures have been taken
        ESP_LOGI(TAG, "using ESP_LOGI info log.");     // Information messages which describe normal flow of events
        ESP_LOGD(TAG, "using ESP_LOGD debug log.");    // Extra information which is not necessary for normal use (values, pointers, sizes, etc).
        ESP_LOGV(TAG, "using ESP_LOGV verbose log.");  // Bigger chunks of debugging information, or frequent messages which can potentially flood the output.
        #endif


        //esp_log_level_set("*", ESP_LOG_VERBOSE);        // set all components to ERROR level
        esp_log_level_set("wifi", ESP_LOG_INFO);      // enable WARN logs from WiFi stack
        esp_log_level_set("dhcpc", ESP_LOG_INFO);     // enable INFO logs from DHCP client
        log_i("hello world!");
        log_e("hello worldE!");
        // attempt to connect to Wifi network:
        delay(1000);

        ESP_LOGI(TAG, "Attempting to connect to SSID: ");
        for(int i = 0; i < NUM_SSID; i++) {
            wifiMulti.addAP(ssids[i], pass[i]);
        }
        if (!SENSORS.begin(0x100)) {
            ESP_LOGE(TAG, "Failed to initialize EEPROM");
            //Serial.println("Restarting...");
            delay(1000);
            ESP.restart();
        }
        else{
            ESP_LOGI(TAG, "Initialize EEPROM OK");
            //writeDtAddress(redThermometer);
            readDtAddress(redThermometer);
            /* CORRUPT HEAP: Bad head at 0x3fcaa9d0. Expected 0xabba1234 got 0x3fc995d4

assert failed: multi_heap_free multi_heap_poisoning.c:259 (head != NULL)

            readDtAddress(SENSORS, redThermometer);
            */
        }
        log_i("OW init. Locating devices...");
        ESP_LOGI(TAG, "CHIP MAC: %012llx", ESP.getEfuseMac());
        init_onewire();
        setup_dallas();
        ok = connect_wifi(WIFI_RETRIES);
        //Serial.println
        // Add constant tags - only once
        if (ok) {
            iflxSensor.addTag("experiment", "calorimetryEsp32C3");
            //validate_influx();
        }

        ESP_LOGI(TAG, "Dallas Temperature IC Control Library Demo");

}

void loop() {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis > samplingInterval) {
            previousMillis += samplingInterval;
            // call sensors.requestTemperatures() to issue a global temperature
            dt_oneWire.requestTemperatures();
            sensorTemp[0] = dt_oneWire.getTempC(ds18Sensors[0]);
            sensorTemp[1] = dt_oneWire.getTempC(ds18Sensors[1]) - sensorDiff;
            ESP_LOGI(TAG, "Tred: %.2fC, Tblue: %.2f",
                    sensorTemp[0],  sensorTemp[1]);     // Information messages which describe normal flow of events
            WiFiStatus = wifiMulti.run();
            obd.setCursor(0,0);
            obd.printf("TRed: %.2f \n\n", sensorTemp[0]);
            obd.printf("TBlue: %.2f \n\n", sensorTemp[1]);
            obd.printf("WiFi: %d", WiFiStatus);
            obd.display();
                /*
            if (WiFiStatus == WL_CONNECTED) {
                /validate_influx();
                if(iflxConnected) {
                    if (!send_iflx_data(iflxClient, iflxSensor)) {
                        ESP_LOGE(TAG, "InfluxDB write failed: ");
                        //Serial.println(client.getLastErrorMessage());
                    }
                }
            }
                */


//            else
//                iflxConnected  = false;
            digitalWrite(LED_PIN, ledState);
            ledState = not ledState;
        }

}

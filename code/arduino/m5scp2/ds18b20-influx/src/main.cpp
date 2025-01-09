/**
 * @Hardwares: M5StickCPlus2
 * @Platform Version: Arduino M5Stack Board Manager v2.0.9
 * @Dependent Library:
 * M5GFX: https://github.com/m5stack/M5GFX
 * M5Unified: https://github.com/m5stack/M5Unified
 * M5StickCPlus2: https://github.com/m5stack/M5StickCPlus2
 */

#include <Arduino.h>
#include <M5Unified.h>

//#include "M5StickCPlus2.h"
#include <WiFiMulti.h>

// Include the libraries we need
#include <OneWire.h>
#include <DallasTemperature.h>

#include "Vrekrer_scpi_parser.h"

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#include "arduino_secrets.h"

#define DEVICE "ESP32"

#define TZ_INFO "UTC"

#define WIFI_RETRIES 20

/*
 * Include definition in include/arduino_secrets.h
 * eg.
#define NUM_SSID 3
const char ssids[][16] = // 10 is the length of the longest string + 1 ( for the '\0' at the end )
{
	"MikroTok",
	"ZZZ",
	"Wifi_BBC"
};

const char pass[][16] = // 10 is the length of the longest string + 1 ( for the '\0' at the end )
{
	"XXXX",
	"ZZZZ",
	"ZZZZZ"
};
#define INFLUXDB_URL ""
#define INFLUXDB_TOKEN "" 
#define INFLUXDB_ORG "67c974ed52348dd6"
#define INFLUXDB_BUCKET "ardu-rasp"
*/


#define WRITE_PRECISION WritePrecision::US
#define MAX_BATCH_SIZE 500
// The recommended size is at least 2 x batch size.
#define WRITE_BUFFER_SIZE 3 * MAX_BATCH_SIZE

// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Declare Data point
Point iflx_sensor("ds18b20Sensors");

WiFiMulti wifiMulti;
uint8_t WiFiStatus = WL_DISCONNECTED;
/*
typedef enum {
  WL_NO_SHIELD = 255,  // for compatibility with WiFi Shield library
  WL_STOPPED = 254,
  WL_IDLE_STATUS = 0,
  WL_NO_SSID_AVAIL = 1,
  WL_SCAN_COMPLETED = 2,
  WL_CONNECTED = 3,
  WL_CONNECT_FAILED = 4,
  WL_CONNECTION_LOST = 5,
  WL_DISCONNECTED = 6
} wl_status_t;

*/

SCPI_Parser my_instrument;

// Data wire is plugged into port G33 on the M5Stick
#define ONE_WIRE_BUS 33
#define TEMPERATURE_PRECISION 12

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature dt_oneWire(&oneWire);

// arrays to hold device addresses
const int max_sensors = 8;
DeviceAddress ds18Sensors[max_sensors];
float sensorTemp[max_sensors];
float sensorDiff = 0.0;

//DeviceAddress redThermometer  = {0x28, 0xC0, 0x48, 0x8B, 0xB0, 0x23, 0x09, 0xAF};
//DeviceAddress blueThermometer  = {0x28, 0x0E, 0x21, 0xBA, 0xB2, 0x23, 0x06, 0x08};
DeviceAddress redThermometer  = {0x28, 0x46, 0x91, 0x46, 0xD4, 0xB7, 0x1F, 0x7D};
DeviceAddress blueThermometer  = {0x28, 0x91, 0x8F, 0xCB, 0xB0, 0x23, 0x09, 0x25};

uint8_t dsCount = 0;
int state;

const unsigned int samplingInterval = 5000; // how often to run the main loop (in ms)
unsigned long previousMillis = 0;

// Function to convert voltage to percentage
int voltageToPercentage(int voltage)
{
    // Assuming 3.7V is 0% and 4.2V is 100%
    int percentage = map(voltage, 3700, 4200, 0, 100);
    return constrain(percentage, 0, 100);
}

int getStableBatteryPercentage() {
    const int numReadings = 10;
    int totalVoltage = 0;

    for (int i = 0; i < numReadings; i++) {
        totalVoltage += M5.Power.getBatteryVoltage();
        delay(10); // Small delay between readings
    }

    int averageVoltage = totalVoltage / numReadings;
    return voltageToPercentage(averageVoltage);
}

bool displayPaused = false;

// function to print a Dallas device address
void printDtAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
    Serial.print(", 0x");
  }
    Serial.println("}");
}
// function to set a Dallas device address
void cpyDtAddress(DeviceAddress dest, DeviceAddress src)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        dest[i] = src[i];
    }
}

bool init_dallas() {
	bool ok = false;

    // Start up the DALLAS Temp library
    dt_oneWire.begin();
     // locate devices on the bus
    Serial.print("Locating devices...");
    Serial.print("Found ");
    dsCount = dt_oneWire.getDeviceCount();
    Serial.print(dsCount, DEC);
    Serial.println(" devices.");
    ok = (dsCount > 0u);

    // report parasite power requirements
    Serial.print("Parasite power is: ");
    if (dt_oneWire.isParasitePowerMode())
        Serial.println("ON");
    else 
	    Serial.println("OFF");
    return ok;
}

unsigned setup_dallas(){
    unsigned int found = 0;
    if (!dt_oneWire.isConnected(redThermometer))
    {
        Serial.println("Unable to find Device 0 (red)");
    }
    else {
        cpyDtAddress(ds18Sensors[0], redThermometer);
        dt_oneWire.setResolution(ds18Sensors[0], TEMPERATURE_PRECISION);
        Serial.print("Red Temp sensor, Resolution: ");
        Serial.println(dt_oneWire.getResolution(ds18Sensors[0]), DEC);
        found++;
    }
    if (!dt_oneWire.isConnected(blueThermometer))
    {
        Serial.println("Unable to find Device 1 (blue)");
    }
    else {
        cpyDtAddress(ds18Sensors[1], blueThermometer);
        dt_oneWire.setResolution(ds18Sensors[1], TEMPERATURE_PRECISION);
        Serial.print("Blue Temp sensor, Resolution: ");
        Serial.println(dt_oneWire.getResolution(ds18Sensors[1]), DEC);
        found++;
    }
    return found;
}

bool connect_wifi() {
	bool ret = false;
    Serial.print(", Waiting for WiFi... ");
    for(int i = 0; i < WIFI_RETRIES; i++) {
        if(wifiMulti.run() != WL_CONNECTED) {
            //while(wifiMulti.run() != WL_CONNECTED) {
            Serial.print(".");
            delay(500);
        }
        else {
            Serial.print(". WiFi connected. ");
            Serial.print("IP address: ");
            IPAddress ip = WiFi.localIP();
            Serial.println(ip);
            //M5.Display.print("WiFi connected.");
            ret = true;
            break;
        }
    }
    // Accurate time is necessary for certificate validation and writing in batches
    // We use the NTP servers in your area as provided by: https://www.pool.ntp.org/zone/
    // Syncing progress and the time will be printed to Serial.
    timeSync(TZ_INFO, "ntp1.tecnico.ulisboa.pt", "pool.ntp.org");
    return ret;
}

bool send_iflx_data() {
    iflx_sensor.clearFields();
    iflx_sensor.addField("tempRed", sensorTemp[0]);
    iflx_sensor.addField("tempBlue", sensorTemp[1]);

    return client.writePoint(iflx_sensor);
}


void update_display() {
    // ColorLCD 135 x 240

    if (!displayPaused) {
        int percentage = getStableBatteryPercentage();
        M5.Display.clear();
        M5.Display.setCursor(0, 20);
        M5.Display.printf("M5S BAT: %3d%%, %.2f V", percentage,
                M5.Power.getBatteryVoltage()/1e3);
        //StickCP2.Display.setTextSize(0.7);
        //M5.Display.printf("%dmV",
        M5.Display.setCursor(10, 40);
        if(WiFiStatus == WL_CONNECTED) 
            M5.Display.printf("WiFi: Connected");
        else
            M5.Display.printf("WiFi: %d", WiFiStatus);
        M5.Display.setCursor(0, 60);
        M5.Display.printf("Tr: %.2f Tb: %.2f ", sensorTemp[0], sensorTemp[1]);
        if(sensorDiff != 0.0) 
            M5.Display.printf("Eq");
//        StickCP2.Display.printf("T0: %.2F T1: %.f\n", sensorTemp[0], sensorTemp[1]);
        //StickCP2.Display.setTextSize(1);
   }
}
void setup() {   
    bool ok = false;
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);
    //Serial.println("M5StickCPlus2 initialized");

    M5.Display.setBrightness(25);
    M5.Display.setRotation(1);
    M5.Display.setTextColor(GREEN);
    //Text alignment middle_center means aligning the center of the text to the specified coordinate position.
    M5.Display.setTextDatum(middle_center);
    // StickCP2.Display.setFont(&fonts::Orbitron_Light_24);
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.setTextSize(1);
    update_display();

    //delay(1000);
    Serial.println("Dallas Temperature Influx Client");


    // connecting to a WiFi network
    for(int i = 0; i < NUM_SSID; i++) {
        wifiMulti.addAP(ssids[i], pass[i]);
    }
    ok = connect_wifi();
    // Add constant tags - only once
    if (ok) {
        iflx_sensor.addTag("experiment", "calorimetryM5S");

        // Check server connection
        if (client.validateConnection()) {
            Serial.print("Connected to InfluxDB: ");
            Serial.println(client.getServerUrl());
        } else {
            Serial.print("InfluxDB connection failed: ");
            Serial.println(client.getLastErrorMessage());
        }
    }

    init_dallas();
    if (setup_dallas() == 0){
        if (!dt_oneWire.getAddress(ds18Sensors[0], 0))
            Serial.println("Unable to find address for Device 0");
        Serial.println("Device 0");
        printDtAddress(ds18Sensors[0]);
        if (!dt_oneWire.getAddress(ds18Sensors[1], 1))
            Serial.println("Unable to find address for Device 1");
        Serial.println("Device 1");
        printDtAddress(ds18Sensors[1]);
    }

}

void loop() {
    unsigned long currentMillis = millis();
    static constexpr const int colors[] = { TFT_WHITE, TFT_CYAN, TFT_RED, TFT_YELLOW, TFT_BLUE, TFT_GREEN };
    static constexpr const char* const names[] = { "none", "wasHold", "wasClicked", "wasPressed", "wasReleased", "wasDeciedCount" };

    // my_instrument.ProcessInput(Serial, "\n");
    //StickCP2.update(); // Update button states
    M5.update(); // Update button states
    /*                   // 
                         if (StickCP2.BtnA.wasPressed()) {
                         displayPaused = !displayPaused; // Toggle display update state
                                                         //Serial.print("StickCP2.BtnA.wasPressed ");
                                                         }
                                                         if (StickCP2.BtnB.wasClicked()) {
                                                         StickCP2.Display.clear();
                                                         StickCP2.Speaker.tone(8000, 20);
                                                         }
    */
    state = M5.BtnA.wasHold() ? 1
        : M5.BtnA.wasClicked() ? 2
        : M5.BtnA.wasPressed() ? 3
        : M5.BtnA.wasReleased() ? 4
        : M5.BtnA.wasDecideClickCount() ? 5
        : 0;
    if (state)
    {
        //M5_LOGI("BtnB:%s  count:%d", names[state], M5.BtnB.getClickCount());
        M5_LOGI("BtnA:%s  count:%d", names[state], M5.BtnA.getClickCount());
        //M5.Display.fillRect(w*2, 0, w-1, h, colors[state]);
        M5.Display.setCursor(state * 10, 80);
        M5.Display.printf("%d", state);
        if (state == 1)
         sensorDiff = dt_oneWire.getTempC(ds18Sensors[1]) - dt_oneWire.getTempC(ds18Sensors[0]);
    }
    state = M5.BtnB.wasHold() ? 1
        : M5.BtnB.wasClicked() ? 2
        : M5.BtnB.wasPressed() ? 3
        : M5.BtnB.wasReleased() ? 4
        : M5.BtnB.wasDecideClickCount() ? 5
        : 0;
    if (state)
    {
        M5_LOGI("BtnB:%s  count:%d", names[state], M5.BtnB.getClickCount());
        M5.Display.setCursor(state * 10, 100);
        M5.Display.printf("%d", state);
    }

    state = M5.BtnC.wasHold() ? 1
        : M5.BtnC.wasClicked() ? 2
        : M5.BtnC.wasPressed() ? 3
        : M5.BtnC.wasReleased() ? 4
        : M5.BtnC.wasDecideClickCount() ? 5
        : 0;
    if (state)
    {
        M5_LOGI("BtnC:%s  count:%d", names[state], M5.BtnC.getClickCount());
        M5.Display.setCursor(state * 10, 120);
        M5.Display.printf("%d", state);
    }

    if (currentMillis - previousMillis > samplingInterval) {
        previousMillis += samplingInterval;
        // call sensors.requestTemperatures() to issue a global temperature
        WiFiStatus = wifiMulti.run();
        dt_oneWire.requestTemperatures();
        sensorTemp[0] = dt_oneWire.getTempC(ds18Sensors[0]);
        //Serial.print("red C: ");
        //Serial.print(sensorTemp[0]);
        sensorTemp[1] = dt_oneWire.getTempC(ds18Sensors[1]) - sensorDiff; 
        //Serial.print(", blue C: ");
    //Serial.println(sensorTemp[1]);
        M5_LOGI("Tred %.2f  Tblue: %.2f", sensorTemp[0], sensorTemp[1]);
        update_display();

        if (wifiMulti.run() == WL_CONNECTED) {
            // Write point
            if (!send_iflx_data()) {
                Serial.print("InfluxDB write failed: ");
                Serial.println(client.getLastErrorMessage());
            }
        }
        else {
            Serial.println("Wifi connection lost");
            connect_wifi();
        }

    }
}
//  vim: syntax=cpp ts=4 sw=4 sts=4 sr et

/**
 * @Hardwares: ;M5StickCPlus2
 * @Platform Version: Arduino M5Stack Board Manager v2.0.9
 * @Dependent Library:
 * M5GFX: https://github.com/m5stack/M5GFX
 * M5Unified: https://github.com/m5stack/M5Unified
 * M5StickCPlus2: https://github.com/m5stack/M5StickCPlus2
 */

#include <Arduino.h>
// #include "M5StickCPlus2.h"
#include <M5Unified.h>
#include <WiFiMulti.h>

// Include the libraries we need
#include <OneWire.h>
#include <DallasTemperature.h>

#include "Vrekrer_scpi_parser.h"

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// Different versions of the framework have different SNTP header file names and availability.
#if __has_include (<esp_sntp.h>)
    #include <esp_sntp.h>
    #define SNTP_ENABLED 1
#elif __has_include (<sntp.h>)
    #include <sntp.h>
    #define SNTP_ENABLED 1
#endif


#include "arduino_secrets.h"

#define DEVICE "ESP32"

#define TZ_INFO "UTC"

#define WIFI_RETRIES 20
#define NTP_SERVER1  "ard-rasp5-ist.local" // "192.168.88.88" // RPI5 Labuino wifi
#define NTP_SERVER2  "ntp1.tecnico.ulisboa.pt"
#define NTP_SERVER3   "1.pool.ntp.org"
#define NTP_TIMEZONE  "UTC"


#define WRITE_PRECISION WritePrecision::US
#define MAX_BATCH_SIZE 500
// The recommended size is at least 2 x batch size.
#define WRITE_BUFFER_SIZE 3 * MAX_BATCH_SIZE

// Declare InfluxDB client instance with preconfigured InfluxCloud V2 certificate

#define URL_LABUINO "http://ard-rasp5-ist.local:8086"
#define ORG_LABUINO "99ec3c8cc9cefdaf"
#define TOKEN_LABUINO "hnU-TlqgnbfExXnRwPOak1-BKa3Ggg-R5h6aRyRvabMGuMPD6bcz5cIxjmzfGjMcd4Mj5M_xVWDFMIYPz1OK4g=="

InfluxDBClient client(URL_LABUINO, ORG_LABUINO, INFLUXDB_BUCKET, TOKEN_LABUINO, InfluxDbCloud2CACert);

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

SCPI_Parser esp32_instrument;

static auto &dsp = (M5.Display);

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
int buttonState;

#define NRSENSORS  2
uint8_t redThermometers[NRSENSORS][8] = {
    {0x28, 0xC0, 0x48, 0x8B, 0xB0, 0x23, 0x09, 0xAF},
    {0x28, 0x46, 0x91, 0x46, 0xD4, 0xB7, 0x1F, 0x7D}
};
uint8_t blueThermometers[NRSENSORS][8] = {
    {0x28, 0x0E, 0x21, 0xBA, 0xB2, 0x23, 0x06, 0x08},
    {0x28, 0x91, 0x8F, 0xCB, 0xB0, 0x23, 0x9, 0x25}
};

DeviceAddress redThermometer;
//DeviceAddress redThermometer  = {0x28, 0xC0, 0x48, 0x8B, 0xB0, 0x23, 0x09, 0xAF};
//DeviceAddress blueThermometer  = {0x28, 0x0E, 0x21, 0xBA, 0xB2, 0x23, 0x06, 0x08};
//DeviceAddress redThermometer  =  {0x28, 0x46, 0x91, 0x46, 0xD4, 0xB7, 0x1F, 0x7D};
DeviceAddress blueThermometer; //  = {0x28, 0x91, 0x8F, 0xCB, 0xB0, 0x23, 0x9, 0x25};

uint8_t dsCount = 0;

const unsigned int samplingInterval = 5000; // how often to run the main loop (in ms)
unsigned long previousMillis = 0;

void update_display();
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
    M5_LOGI("0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X", 
            deviceAddress[0], deviceAddress[1], deviceAddress[2],deviceAddress[3],
            deviceAddress[4], deviceAddress[5], deviceAddress[6],deviceAddress[7]);
}
// function to set a Dallas device address
void cpyDtAddress(DeviceAddress dest, DeviceAddress src)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        dest[i] = src[i];
    }
}

void Identify(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(F("IPFN,SCPI ESP32 Thermo,#00," VREKRER_SCPI_VERSION));  
  //*IDN? Suggested return string should be in the following format:
  // "<vendor>,<model>,<serial number>,<firmware>"
}

bool setup_scpi(){
    bool ok = true;
    /*
       To fix hash crashes, the hashing magic numbers can be changed before 
       registering commands.
       Use prime numbers up to the SCPI_HASH_TYPE size.
       */
    esp32_instrument.hash_magic_number = 37; //Default value = 37
    esp32_instrument.hash_magic_offset = 7;  //Default value = 7

    /*
       Timeout time can be changed even during program execution
       See Error_Handling example for further details.
       */
    esp32_instrument.timeout = 10; //value in miliseconds. Default value = 10
    esp32_instrument.RegisterCommand(F("*IDN?"), &Identify);
    return ok;
}

bool init_onewire() {
    bool ok = false;

    // Start up the DALLAS Temp library
    dt_oneWire.begin();
    // locate devices on the bus
    M5_LOGI("Locating devices...");
    M5_LOGI("Found ");
    dsCount = dt_oneWire.getDeviceCount();
    Serial.print(dsCount, DEC);
    M5_LOGI(" devices.");
    ok = (dsCount > 0u);

    // report parasite power requirements
    M5_LOGI("Parasite power is: ");
    if (dt_oneWire.isParasitePowerMode())
        M5_LOGI("ON\n");
    else
        M5_LOGI("OFF\n");
    return ok;
}

unsigned setup_dallas(){
    unsigned int found = 0;
    for (int s = 0; s < NRSENSORS; s++) {
        if (dt_oneWire.isConnected(redThermometers[s])){
            M5_LOGI("Found Red Sensor index %d", s);
            printDtAddress(redThermometers[s]);
            cpyDtAddress(redThermometer, redThermometers[s]);
            dt_oneWire.setResolution(redThermometer, TEMPERATURE_PRECISION);
            found++;
            break;
        }
    }
    for (int s = 0; s < NRSENSORS; s++) {
        if (dt_oneWire.isConnected(blueThermometers[s])){
            M5_LOGI("Found Blue Sensor index %d", s);
            printDtAddress(blueThermometers[s]);
            cpyDtAddress(blueThermometer, blueThermometers[s]);
            dt_oneWire.setResolution(blueThermometer, TEMPERATURE_PRECISION);
            found++;
            break;
        }
    }
    /*
    if (!dt_oneWire.isConnected(redThermometer))
    {
            M5_LOGE("Unable to find Device 0 (red)");
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
            M5_LOGE("Unable to find Device 1 (blue)");
    }
    else {
        cpyDtAddress(ds18Sensors[1], blueThermometer);
        dt_oneWire.setResolution(ds18Sensors[1], TEMPERATURE_PRECISION);
        Serial.print("Blue Temp sensor, Resolution: ");
        Serial.println(dt_oneWire.getResolution(ds18Sensors[1]), DEC);
        found++;
    }
    */
    return found;
}

int connect_wifi(int retries) {
    int status = WL_IDLE_STATUS; // the Wifi radio's status
                                 //     // setup RTC ( NTP auto setting )
    configTzTime(NTP_TIMEZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
//bool connect_wifi() {
//	bool ret = false;
    M5_LOGI(", Waiting for WiFi... ");
    for(int i = 0; i < retries; i++) {
        status = wifiMulti.run();
        if(status == WL_CONNECTED) {

            M5_LOGI(". WiFi connected. %s", WiFi.SSID().c_str());
            M5_LOGI("IP address: ");
            IPAddress ip = WiFi.localIP();
            M5_LOGI("%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        //timeSync(TZ_INFO, "ntp1.tecnico.ulisboa.pt", "pool.ntp.org");
            //M5.Display.print("WiFi connected.");
            break;
        }
        else {
        //if(wifiMulti.run() != WL_CONNECTED) {
            //while(wifiMulti.run() != WL_CONNECTED) {
            M5_LOGW(".");
            delay(500);
        }
    }
    if (status == WL_CONNECTED) {
        //#if SNTP_ENABLED
        M5.Log.println("SNTP ENABLED.\n");
        while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
        {
            M5.Log.print(".");
            M5.delay(1000);
        }
    }
    else
        M5_LOGE(" Fail to connect WiF, giving up.");
    return status;
}

bool send_iflx_data() {
    iflx_sensor.clearFields();
    iflx_sensor.addField("tempRed", sensorTemp[0]);
    iflx_sensor.addField("tempBlue", sensorTemp[1]);

    return client.writePoint(iflx_sensor);
}

void setup() {   
    bool ok = false;
#if defined ( ESP_PLATFORM )
    ESP_LOGE("TAG", "using ESP_LOGE error log.");    // Critical errors, software module can not recover on its own
    ESP_LOGW("TAG", "using ESP_LOGW warn log.");     // Error conditions from which recovery measures have been taken
    ESP_LOGI("TAG", "using ESP_LOGI info log.");     // Information messages which describe normal flow of events
    ESP_LOGD("TAG", "using ESP_LOGD debug log.");    // Extra information which is not necessary for normal use (values, pointers, sizes, etc).
    ESP_LOGV("TAG", "using ESP_LOGV verbose log.");  // Bigger chunks of debugging information, or frequent messages which can potentially flood the output.
#endif
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);
    M5.Display.setBrightness(25);
    M5.Display.setRotation(1);
    M5.Display.setTextColor(GREEN);
    //Text alignment middle_center means aligning the center of the text to the specified coordinate position.

    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.setTextSize(1);
    dsp.fillScreen(TFT_BLACK);
    update_display();
    /// You can set Log levels for each output destination.
/// ESP_LOG_ERROR / ESP_LOG_WARN / ESP_LOG_INFO / ESP_LOG_DEBUG / ESP_LOG_VERBOSE
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_VERBOSE);
  //M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_DEBUG);
  //M5.Log.setLogLevel(m5::log_target_callback, ESP_LOG_INFO);
  /// You can color the log or not.
    M5.Log.setEnableColor(m5::log_target_serial, true);

    M5_LOGI("M5_LOGI info log");      /// INFO level output with source info


    /// `M5.Log.printf()` is output without log level and without suffix and is output to all serial, display, and callback.
    M5.Log.printf("M5.Log.printf non level output\n");

    //delay(1000);

    // Features: WiFi, BT, Dual Core, 240MHz, Embedded Flash, Embedded PSRAM, VRef calibration in efuse, Coding Scheme None
//Crystal is 40MHz
//MAC: 0c:8b:95:b4:78:64


    init_onewire();
    if (setup_dallas() == 0){
        if (!dt_oneWire.getAddress(ds18Sensors[0], 0))
            M5_LOGW("Unable to find address for Device 0");
        printDtAddress(ds18Sensors[0]);
        if (!dt_oneWire.getAddress(ds18Sensors[1], 1))
            M5_LOGW("Unable to find address for Device 1");
        printDtAddress(ds18Sensors[1]);
    }
    M5_LOGI("Connecting to Rpi Wifi: %s", WiFi.macAddress().c_str());
    wifiMulti.addAP("Labuino", "Arduino25");
    // connecting to  WiFi network
    WiFiStatus = connect_wifi(10);
    if (WiFiStatus == WL_CONNECTED) {
        M5_LOGI("Labuino Wifi OK. Grafana server is http://ard-rasp5-ist.local:3000");
    }
    else {
        M5_LOGW("Labuino Wifi Failed. Trying others.");
         // clears the current list of Multi APs and frees the memory
        //wifiMulti.APlistClean();

        for(int i = 0; i < NUM_SSID; i++) {
            wifiMulti.addAP(ssids[i], pass[i]);
        }
        WiFiStatus = connect_wifi(WIFI_RETRIES);
        if (WiFiStatus == WL_CONNECTED) {
            M5_LOGI("Wifi OK. Grafana server is https://kane584.tecnico.ulisboa.pt:3000");
        // serverCert - Optional. InfluxDB 2 server trusted certificate (or CA certificate) or certificate SHA1 fingerprint.  Should be stored in PROGMEM. Only in case of https connection.
        //
            client.setConnectionParams(INFLUXDB_URL, INFLUXDB_ORG,
                    INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
        }
   // void setConnectionParams(const String &serverUrl, const String &org, const String &bucket, const String &authToken, const char *certInfo = nullptr);

    }
    // Add constant tags - only once
    if (WiFiStatus == WL_CONNECTED) {
        iflx_sensor.addTag("experiment", "calorimetryM5S");

        // Check server connection
        if (client.validateConnection()) {
           M5_LOGI("Connected to InfluxDB: %s", 
                   client.getServerUrl().c_str());
        } else {
            M5_LOGW("InfluxDB connection failed: %s",
                client.getLastErrorMessage());
        }
    }

}
/*
void update_display() {
    // ColorLCD 135 x 240

    if (!displayPaused) {
        int percentage = getStableBatteryPercentage();
        StickCP2.Display.clear();
        StickCP2.Display.setCursor(10, 20);
        StickCP2.Display.printf("BAT: %d%%", percentage);
        StickCP2.Display.setCursor(120, 20);
        //StickCP2.Display.setTextSize(0.7);
        StickCP2.Display.printf("%dmV", StickCP2.Power.getBatteryVoltage());
        StickCP2.Display.setCursor(10, 40);WiFi.macAddress()
        if (WiFiStatus == WL_CONNECTED) 
            StickCP2.Display.printf("WiFi: Connected");
        else
            StickCP2.Display.printf("WiFi: %d", WiFiStatus);

        StickCP2.Display.setCursor(10, 60);
        StickCP2.Display.printf("T0: %.2f C", sensorTemp[0]);
        StickCP2.Display.setCursor(120, 60);
        StickCP2.Display.printf("T1: %.2f C", sensorTemp[1]);
//        StickCP2.Display.printf("T0: %.2F T1: %.f\n", sensorTemp[0], sensorTemp[1]);
        //StickCP2.Display.setTextSize(1);
   }
}
*/
void update_display() {
    // ColorLCD 135 x 240

    if (!displayPaused) {
        int percentage = getStableBatteryPercentage();
        M5.Display.clear();
        M5.Display.setCursor(0, 20);
        M5.Display.printf("M5S BAT: %3d%%, %.2f V", percentage,
                M5.Power.getBatteryVoltage()/1e3);
        //M5.Display.printf("%dmV",
        M5.Display.setCursor(10, 40);
        if(WiFiStatus == WL_CONNECTED) {
            M5.Display.printf("WiFi: Connected  %s", WiFi.SSID().c_str());
            IPAddress ip = WiFi.localIP();
            //M5.Display.printf(" %s", ip.toString());
            //M5.Display.printf("%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        }
        else
            M5.Display.printf("WiFi: %d", WiFiStatus);
        M5.Display.setCursor(0, 60);
        M5.Display.printf("Tr: %.2f Tb: %.2f ", sensorTemp[0], sensorTemp[1]);
        if(sensorDiff != 0.0)
            M5.Display.printf("Eq");
   }
}

void loop() {
    unsigned long currentMillis = millis();
    static constexpr const int colors[] = { TFT_WHITE, TFT_CYAN, TFT_RED, TFT_YELLOW, TFT_BLUE, TFT_GREEN };
    static constexpr const char* const names[] = { "none", "wasHold", "wasClicked", "wasPressed", "wasReleased", "wasDeciedCount" };

    // my_instrument.ProcessInput(Serial, "\n");
    M5.update(); // Update button states
    buttonState = M5.BtnA.wasHold() ? 1
        : M5.BtnA.wasClicked() ? 2
        : M5.BtnA.wasPressed() ? 3
        : M5.BtnA.wasReleased() ? 4
        : M5.BtnA.wasDecideClickCount() ? 5
        : 0;
    if (buttonState)
    {
        //M5_LOGI("BtnB:%s  count:%d", names[buttonState], M5.BtnB.getClickCount());
        M5_LOGI("BtnA:%s  count:%d", names[buttonState], M5.BtnA.getClickCount());
        //M5.Display.fillRect(w*2, 0, w-1, h, colors[state]);
        M5.Display.setCursor(buttonState * 10, 80);
        M5.Display.printf("%d", buttonState);
        if (buttonState == 1)
            sensorDiff = dt_oneWire.getTempC(blueThermometer) - dt_oneWire.getTempC(redThermometer);
    }
    buttonState = M5.BtnB.wasHold() ? 1
        : M5.BtnB.wasClicked() ? 2
        : M5.BtnB.wasPressed() ? 3
        : M5.BtnB.wasReleased() ? 4
        : M5.BtnB.wasDecideClickCount() ? 5
        : 0;
    if (buttonState)
    {
        M5_LOGI("BtnB:%s  count:%d", names[buttonState], M5.BtnB.getClickCount());
        M5.Display.setCursor(buttonState * 10, 100);
        M5.Display.printf("%d", buttonState);
    }

    buttonState = M5.BtnC.wasHold() ? 1
        : M5.BtnC.wasClicked() ? 2
        : M5.BtnC.wasPressed() ? 3
        : M5.BtnC.wasReleased() ? 4
        : M5.BtnC.wasDecideClickCount() ? 5
        : 0;
    if (buttonState)
    {
        M5_LOGI("BtnC:%s  count:%d", names[buttonState], M5.BtnC.getClickCount());
        M5.Display.setCursor(buttonState * 10, 120);
        M5.Display.printf("%d", buttonState);
    }
    if (currentMillis - previousMillis > samplingInterval) {
        previousMillis += samplingInterval;
        // call sensors.requestTemperatures() to issue a global temperature
        //WiFiStatus = wifiMulti.run();
        dt_oneWire.requestTemperatures();
        sensorTemp[0] = dt_oneWire.getTempC(redThermometer);
        sensorTemp[1] = dt_oneWire.getTempC(blueThermometer) - sensorDiff;
        M5_LOGI("Tred %.2f  Tblue: %.2f", sensorTemp[0], sensorTemp[1]);
        update_display();

        //if (wifiMulti.run() == WL_CONNECTED) {
        if (WiFiStatus == WL_CONNECTED) {
            // Write point
            if (!send_iflx_data()) {
                M5_LOGE("InfluxDB write failed: ");
                Serial.println(client.getLastErrorMessage());
            }
        }
        else {
            M5_LOGW("Wifi connection lost");
            //connect_wifi();
        }

    }
}


/*
void loop() {
    unsigned long currentMillis = millis();
    esp32_instrument.ProcessInput(Serial, "\n");
    M5.update(); // Update button states
    static int count_loop;
    if (currentMillis - previousMillis > samplingInterval) {
        previousMillis += samplingInterval;
        // call sensors.requestTemperatures() to issue a global temperature
        WiFiStatus = wifiMulti.run();
        dt_oneWire.requestTemperatures();
        sensorTemp[0] = dt_oneWire.getTempC(ds18Sensors[0]);
        sensorTemp[1] = dt_oneWire.getTempC(ds18Sensors[1]);
        update_display();
        if ((count_loop++) %10 == 0) {
            Serial.print("Temp0 C: ");
            Serial.print(sensorTemp[0]);
            Serial.print(", Temp1 C: ");
            Serial.println(sensorTemp[1]);
        }

        if (WiFiStatus != WL_CONNECTED) {
            Serial.println("Wifi connection lost");
        }
        else if (!send_iflx_data()) {
        // Write point
            Serial.print("InfluxDB write failed: ");
            Serial.println(client.getLastErrorMessage());
        }

    }
    // if (StickCP2.BtnB.wasClicked()) {
    //     StickCP2.Display.clear();
    //     // StickCP2.Speaker.tone(8000, 20);
    // }

    // if (StickCP2.BtnA.wasPressed()) {
    //     displayPaused = !displayPaused; // Toggle display update state
    // }

}
*/
//  vim: syntax=cpp ts=4 sw=4 sts=4 sr et

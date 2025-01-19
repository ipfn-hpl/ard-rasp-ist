/**
 * @Hardwares: M5StickCPlus
 * @Platform Version: Arduino M5Stack Board Manager v2.0.9
 * @Dependent Library:
 * M5GFX: https://github.com/m5stack/M5GFX
 * M5Unified: https://github.com/m5stack/M5Unified
 */

#include <Arduino.h>
// #include "M5StickCPlus2.h"
#include <M5Unified.h>
#include <WiFiMulti.h>

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#include "arduino_secrets.h"

#include <bsec2.h>
//#include <commMux\commMux.h>

Bsec2 envSensor;
TwoWire Wire2 = TwoWire(2);

#define SAMPLE_RATE		BSEC_SAMPLE_RATE_ULP

int i2c_scl_pin = 33; //26;
int i2c_sda_pin = 32; //0;

#define DEVICE "ESP32"

#define TZ_INFO "UTC"

#define WIFI_RETRIES 20

WiFiMulti wifiMulti;
uint8_t WiFiStatus = WL_DISCONNECTED;

// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient iflx_client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Declare Data point
Point iflx_point("bme688Sensors");


void update_display();
const unsigned int samplingInterval = 5000; // how often to run the main loop (in ms)
unsigned long previousMillis = 0;

enum D_STS {
   D_STS_NO_ERR
  ,D_STS_INITIALIZING
  ,D_STS_NO_DEVICE
  ,D_STS_NETWORK_AMB_ERR
  ,D_STS_READ_DATA_ERR
  ,D_STS_MEASURING
  ,D_STS_MAX
};


D_STS currentSts = D_STS_NO_ERR;

int buttonState;

struct {             // Structure declaration
        float iaq;
        int iaq_accuracy;
        float static_iaq;
        float co2_equivalent;
        float breath_voc_equivalent;
        float raw_temperature;
        float raw_pressure;
        float raw_humidity;
        float raw_gas;
        float stabilization_status;
        float run_in_status;
        float sensor_heat_compensated_temperature;
        float sensor_heat_compensated_humidity;
} bmeStructure;


void CheckBsecSts(Bsec2 bsec) {
  if (bsec.status < BSEC_OK) {
    Serial.println("BSEC error code : " + String(bsec.status));
    currentSts = D_STS_READ_DATA_ERR;
  } else if (bsec.status > BSEC_OK) {
    Serial.println("BSEC warning code : " + String(bsec.status));
    currentSts = D_STS_READ_DATA_ERR;
  }
  if (bsec.sensor.status < BME68X_OK) {
    Serial.println("BME68X error code : " + String(bsec.sensor.status));
    currentSts = D_STS_READ_DATA_ERR;
  } else if (bsec.sensor.status > BME68X_OK) {
    Serial.println("BME68X warning code : " + String(bsec.sensor.status));
    currentSts = D_STS_READ_DATA_ERR;
  }
}

void newDataCallback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec)
{
    if (!outputs.nOutputs)
    {
        return;
    }

    Serial.println("BSEC outputs:\n\tTime stamp = " + String((int) (outputs.output[0].time_stamp / INT64_C(1000000))));
    for (uint8_t i = 0; i < outputs.nOutputs; i++)
    {
        const bsecData output  = outputs.output[i];
        switch (output.sensor_id)
        {
            case BSEC_OUTPUT_IAQ:
                Serial.println("\tIAQ = " + String(output.signal));
                Serial.println("\tIAQ accuracy = " + String((int) output.accuracy));
                bmeStructure.iaq = output.signal;
                bmeStructure.iaq_accuracy = output.accuracy;
                break;
            case BSEC_OUTPUT_RAW_TEMPERATURE:
                Serial.println("\tTemperature = " + String(output.signal));
                bmeStructure.raw_temperature = output.signal;
                break;
            case BSEC_OUTPUT_RAW_PRESSURE:
                Serial.println("\tPressure = " + String(output.signal));
                bmeStructure.raw_pressure = output.signal;
                break;
            case BSEC_OUTPUT_RAW_HUMIDITY:
                Serial.println("\tHumidity = " + String(output.signal));
                bmeStructure.raw_humidity = output.signal;
                break;
            case BSEC_OUTPUT_RAW_GAS:
                Serial.println("\tGas resistance = " + String(output.signal));
                bmeStructure.raw_gas = output.signal;
                break;
            case BSEC_OUTPUT_STABILIZATION_STATUS:
                Serial.println("\tStabilization status = " + String(output.signal));
                bmeStructure.stabilization_status = output.signal;
                break;
            case BSEC_OUTPUT_RUN_IN_STATUS:
                Serial.println("\tRun in status = " + String(output.signal));
                bmeStructure.run_in_status = output.signal;
                break;
            case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
                Serial.println("\tCompensated temperature = " + String(output.signal));
                bmeStructure.sensor_heat_compensated_temperature = output.signal;
                break;
            case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
                Serial.println("\tCompensated humidity = " + String(output.signal));
                bmeStructure.sensor_heat_compensated_humidity = output.signal;
                break;
            case BSEC_OUTPUT_STATIC_IAQ:
                Serial.println("\tStatic IAQ = " + String(output.signal));
                break;
            case BSEC_OUTPUT_CO2_EQUIVALENT:
                Serial.println("\tCO2 Equivalent = " + String(output.signal));
                break;
            case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
                Serial.println("\tbVOC equivalent = " + String(output.signal));
                break;
            case BSEC_OUTPUT_GAS_PERCENTAGE:
                Serial.println("\tGas percentage = " + String(output.signal));
                break;
            case BSEC_OUTPUT_COMPENSATED_GAS:
                Serial.println("\tCompensated gas = " + String(output.signal));
                break;
            default:
                break;
        }
    }
}

void initSensor() {
  // Initialize & setup sensor
  Wire2.begin(i2c_sda_pin,i2c_scl_pin);
  if (!envSensor.begin(BME68X_I2C_ADDR_LOW, Wire2)) {
      M5_LOGE("No Sensor");
    CheckBsecSts(envSensor);
    currentSts = D_STS_NO_DEVICE;
    //updateStsWrite(currentSts);
    return;
  }
  M5_LOGI("Found Sensor");

  bsecSensor sensorList[] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_STABILIZATION_STATUS,
    BSEC_OUTPUT_RUN_IN_STATUS,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY
  };
  if (!envSensor.updateSubscription(sensorList, ARRAY_LEN(sensorList), BSEC_SAMPLE_RATE_LP)) {

      M5_LOGE("No updateSubscription");
    CheckBsecSts(envSensor);
    currentSts = D_STS_NO_DEVICE;
   // updateStsWrite(currentSts);
    //return;
  }
  /* Whenever new data is available call the newDataCallback function */
    envSensor.attachCallback(newDataCallback);

  Serial.println("BSEC library version " + \
            String(envSensor.version.major) + "." \
            + String(envSensor.version.minor) + "." \
            + String(envSensor.version.major_bugfix) + "." \
            + String(envSensor.version.minor_bugfix));
}

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

bool connect_wifi() {
    bool ret = false;
    M5_LOGI(", Waiting for WiFi... ");
    for(int i = 0; i < WIFI_RETRIES; i++) {
        if(wifiMulti.run() != WL_CONNECTED) {
            //while(wifiMulti.run() != WL_CONNECTED) {
            M5_LOGW(".");
            delay(500);
        }
        else {
            M5_LOGI(". WiFi connected. ");
            M5_LOGI("IP address: ");
            IPAddress ip = WiFi.localIP();
            M5_LOGI("%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            //M5.Display.print("WiFi connected.");
            ret = true;
            break;
        }
        }
        if (ret) {
            // Accurate time is necessary for certificate validation and writing in batches
            // We use the NTP servers in your area as provided by: https://www.pool.ntp.org/zone/
            // Syncing progress and the time will be printed to Serial.
            timeSync(TZ_INFO, "ntp1.tecnico.ulisboa.pt", "pool.ntp.org");
        }
        else
            M5_LOGE(" Fail to connect WiF, giving up.");
        return ret;
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
        //M5.Display.setBrightness(25);
        M5.Display.setRotation(1);
        M5.Display.setTextColor(GREEN);
        //Text alignment middle_center means aligning the center of the text to the specified coordinate position.

        M5.Display.setTextDatum(middle_center);
        M5.Display.setFont(&fonts::FreeSans9pt7b);
        M5.Display.setTextSize(1);
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

        M5_LOGI("Connecting to Rpi Wifi..");
        /*
        wifiMulti.addAP("Labuino-rpi5", "Arduino25");
        // connecting to  WiFi network
        ok = connect_wifi();
        if (ok) {
            Serial.println("Rpi Wifi OK");
        }
        else {
            M5_LOGW("Rpi Wifi Failed. Trying others.");
            */
            // clears the current list of Multi APs and frees the memory
            //wifiMulti.APlistClean();

            for(int i = 0; i < NUM_SSID; i++) {
                wifiMulti.addAP(ssids[i], pass[i]);
            }
            ok = connect_wifi();
        // }
        // Add constant tags - only once
            if (ok) {
                iflx_point.addTag("client", "bme688M5StickCP");

                // Check server connection
                if (iflx_client.validateConnection()) {
                    M5_LOGI("Connected to InfluxDB: ");
                    M5_LOGI("%s", iflx_client.getServerUrl());
                } else {
                    M5_LOGE("InfluxDB connection failed: ");
                    Serial.println(iflx_client.getLastErrorMessage());
                }
            }
        initSensor();
}

void update_display() {
    // ColorLCD 135 x 240

    int percentage = getStableBatteryPercentage();
    M5.Display.clear();
    M5.Display.setCursor(0, 10);
    M5.Display.printf("M5S BAT: %3d%%, %.2f V", percentage,
            M5.Power.getBatteryVoltage()/1e3);
    //M5.Display.printf("%dmV",
    M5.Display.setCursor(0, 30);
    if(WiFiStatus == WL_CONNECTED) {
        M5.Display.printf("WiFi: Connected ");
        IPAddress ip = WiFi.localIP();
        //M5.Display.printf(" %s", ip.toString());
        M5.Display.setCursor(0, 50);
        M5.Display.printf("IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    }
    else
        M5.Display.printf("WiFi Status: %d", WiFiStatus);
    M5.Display.setCursor(0, 70);
    M5.Display.printf("T: %.2f P: %.2f H: %.1f%% ", 
        bmeStructure.sensor_heat_compensated_temperature,
        bmeStructure.raw_pressure,
        bmeStructure.sensor_heat_compensated_humidity);
    M5.Display.setCursor(0, 90);
    M5.Display.printf("IAQ: %.1f IAQ acq: %d", 
        bmeStructure.iaq,
        bmeStructure.iaq_accuracy);
}

bool send_iflx_data() {
    iflx_point.clearFields();
    iflx_point.addField("Temperature", bmeStructure.sensor_heat_compensated_temperature);
    iflx_point.addField("Humidity", bmeStructure.sensor_heat_compensated_humidity);
    iflx_point.addField("Pressure", bmeStructure.raw_pressure); //sensorTemp[1]);
    iflx_point.addField("IAQ", bmeStructure.iaq); //sensorTemp[1]);
    iflx_point.addField("IAQaccuracy", bmeStructure.iaq_accuracy); //sensorTemp[1]);

    return iflx_client.writePoint(iflx_point);
}

void loop() {
    unsigned long currentMillis = millis();
    static constexpr const int colors[] = { TFT_WHITE, TFT_CYAN, TFT_RED, TFT_YELLOW, TFT_BLUE, TFT_GREEN };
    static constexpr const char* const names[] = { "none", "wasHold", "wasClicked", "wasPressed", "wasReleased", "wasDeciedCount" };
    /* Call the run function often so that the library can 
     * check if it is time to read new data from the sensor  
     * and process it.
     */
    if (!envSensor.run())
    {
        CheckBsecSts(envSensor);
    }
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
        //if (buttonState == 1)
        //    sensorDiff = dt_oneWire.getTempC(ds18Sensors[1]) - dt_oneWire.getTempC(ds18Sensors[0]);
    }
    if (currentMillis - previousMillis > samplingInterval) {
        previousMillis += samplingInterval;
        // call sensors.requestTemperatures() to issue a global temperature
        WiFiStatus = wifiMulti.run();
        //M5_LOGI("Loop ");
        update_display();

        if (WiFiStatus == WL_CONNECTED) {
            // Write point
            if (!send_iflx_data()) {
                 M5_LOGE("InfluxDB write failed: ");
                 Serial.println(iflx_client.getLastErrorMessage());
            }
        }
        else {
            M5_LOGW("Wifi connection lost");
            connect_wifi();
        }

    }
}


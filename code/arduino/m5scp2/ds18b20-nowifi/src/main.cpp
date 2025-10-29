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
// #include <WiFiMulti.h>

// Include the libraries we need
#include <DallasTemperature.h>
#include <OneWire.h>

// #include "Vrekrer_scpi_parser.h"

// #include "arduino_secrets.h"

#define DEVICE "ESP32"

#define TZ_INFO "UTC"

static auto &dsp = (M5.Display);
/*
 * Include definition in include/arduino_secrets.h
 * eg.
#define NUM_SSID 3
const char ssids[][16] = // 10 is the length of the longest string + 1 ( for the
'\0' at the end )
{
"MikroTok",
"ZZZ",
"Wifi_BBC"
};

const char pass[][16] = // 10 is the length of the longest string + 1 ( for the
'\0' at the end )
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

// Data wire is plugged into port G33 on the M5Stick
#define ONE_WIRE_BUS 33
#define TEMPERATURE_PRECISION 12

// Setup a oneWire instance to communicate with any OneWire devices (not just
// Maxim/Dallas temperature ICs)
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
// DeviceAddress redThermometer  = {0x28, 0xC0, 0x48, 0x8B, 0xB0, 0x23, 0x09,
// 0xAF}; DeviceAddress blueThermometer  = {0x28, 0x0E, 0x21, 0xBA, 0xB2, 0x23,
// 0x06, 0x08}; DeviceAddress redThermometer  =  {0x28, 0x46, 0x91, 0x46, 0xD4,
// 0xB7, 0x1F, 0x7D}; DeviceAddress blueThermometer  = {0x28, 0x91, 0x8F, 0xCB,
// 0xB0, 0x23, 0x9, 0x25};
//

DeviceAddress redThermometer = {0x28, 0xD4, 0xAD, 0x91, 0xB0, 0x24, 0x06, 0x2C};

DeviceAddress blueThermometer = {0x28, 0x67, 0x03, 0x5E,
                                 0x9B, 0x23, 0x09, 0x3A};
DeviceAddress redThermometer = {0x28, 0x82, 0xB8, 0x46, 0xA1, 0x23, 0x06, 0x16};
DeviceAddress blueThermometer = {0x28, 0x1B, 0xDC, 0x17,
                                 0xB0, 0x24, 0x06, 0xE7};
DeviceAddress blueThermometer = {0x28, 0x01, 0x22, 0x1D,
                                 0xB0, 0x24, 0x06, 0x6C};
DeviceAddress redThermometer = {0x28, 0xCB, 0xC9, 0x69, 0xB0, 0x24, 0x06, 0x30};
*/

DeviceAddress blueThermometer = {0x28, 0xAA, 0x2B, 0x56,
                                 0xA1, 0x23, 0x06, 0xC0};
DeviceAddress redThermometer = {0x28, 0xCE, 0x8D, 0x77, 0x9B, 0x23, 0x09, 0x7A};

uint8_t dsCount = 0;

const unsigned int samplingInterval =
    2000; // how often to run the main loop (in ms)
unsigned long previousMillis = 0;

void update_display();
// Function to convert voltage to percentage
int voltageToPercentage(int voltage) {
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
void printDtAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16)
      Serial.print("0");
    Serial.print("0x");
    Serial.print(deviceAddress[i], HEX);
    Serial.print(", ");
  }
  Serial.println(" ");
}
// function to set a Dallas device address
void cpyDtAddress(DeviceAddress dest, DeviceAddress src) {
  for (uint8_t i = 0; i < 8; i++) {
    dest[i] = src[i];
  }
}

bool init_dallas() {
  bool ok = false;

  // Start up the DALLAS Temp library
  dt_oneWire.begin();
  // locate devices on the bus
  M5_LOGI("Locating devices...");
  dsCount = dt_oneWire.getDeviceCount();
  M5_LOGI("Found %d devices.", dsCount);
  Serial.print(dsCount, DEC);
  ok = (dsCount > 0u);

  // report parasite power requirements
  M5_LOGI("Parasite power is: ");
  if (dt_oneWire.isParasitePowerMode())
    M5_LOGI("ON\n");
  else
    M5_LOGI("OFF\n");
  return ok;
}

unsigned setup_dallas() {
  unsigned int found = 0;
  if (!dt_oneWire.isConnected(redThermometer)) {
    M5_LOGE("Unable to find Device 0 (red)");
  } else {
    cpyDtAddress(ds18Sensors[0], redThermometer);
    dt_oneWire.setResolution(ds18Sensors[0], TEMPERATURE_PRECISION);
    Serial.print("Red Temp sensor, Resolution: ");
    Serial.println(dt_oneWire.getResolution(ds18Sensors[0]), DEC);
    found++;
  }
  if (!dt_oneWire.isConnected(blueThermometer)) {
    M5_LOGE("Unable to find Device 1 (blue)");
  } else {
    cpyDtAddress(ds18Sensors[1], blueThermometer);
    dt_oneWire.setResolution(ds18Sensors[1], TEMPERATURE_PRECISION);
    Serial.print("Blue Temp sensor, Resolution: ");
    Serial.println(dt_oneWire.getResolution(ds18Sensors[1]), DEC);
    found++;
  }
  return found;
}

void setup() {
  bool ok = false;
#if defined(ESP_PLATFORM)
  ESP_LOGE("TAG",
           "using ESP_LOGE error log."); // Critical errors, software module can
                                         // not recover on its own
  ESP_LOGW("TAG",
           "using ESP_LOGW warn log."); // Error conditions from which recovery
                                        // measures have been taken
  ESP_LOGI("TAG", "using ESP_LOGI info log."); // Information messages which
                                               // describe normal flow of events
  ESP_LOGD("TAG",
           "using ESP_LOGD debug log."); // Extra information which is not
                                         // necessary for normal use (values,
                                         // pointers, sizes, etc).
  ESP_LOGV(
      "TAG",
      "using ESP_LOGV verbose log."); // Bigger chunks of debugging information,
                                      // or frequent messages which can
                                      // potentially flood the output.
#endif
  auto cfg = M5.config();
  M5.begin(cfg);
  dsp.setRotation(dsp.getRotation() ^ 1);
  int32_t w = dsp.width();
  int32_t h = dsp.height();
  if (w < h) { /// Landscape mode.
    dsp.setRotation(dsp.getRotation() ^ 1);
    w = dsp.width();
    h = dsp.height();
  }
  int32_t graph_area_h = ((h - 8) / 18) * 18;
  int32_t text_area_h = h - graph_area_h;
  M5_LOGI("dsp W, H:%d, %d", w, h);
  Serial.begin(115200);
  float fontsize = text_area_h / 8;
  dsp.setTextSize(fontsize);
  dsp.setColor(GREEN);
  dsp.setTextColor(TFT_WHITE, TFT_BLUE);
  dsp.printf("dsp W, H:%d, %d F%.2f", w, h, fontsize);
  // M5.Display.setBrightness(25);
  // M5.Display.setTextColor(GREEN);
  //  Text alignment middle_center means aligning the center of the text to the
  //  specified coordinate position.

  // M5.Display.setTextDatum(middle_center);
  // M6.Display.setFont(&fonts::FreeSans9pt7b);
  // M5.Display.setTextSize(1);
  update_display();
  /// You can set Log levels for each output destination.
  /// ESP_LOG_ERROR / ESP_LOG_WARN / ESP_LOG_INFO / ESP_LOG_DEBUG /
  /// ESP_LOG_VERBOSE
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_VERBOSE);
  // M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_DEBUG);
  // M5.Log.setLogLevel(m5::log_target_callback, ESP_LOG_INFO);
  /// You can color the log or not.
  M5.Log.setEnableColor(m5::log_target_serial, true);

  M5_LOGI("M5_LOGI info log"); /// INFO level output with source info

  /// `M5.Log.printf()` is output without log level and without suffix and is
  /// output to all serial, display, and callback.
  M5.Log.printf("M5.Log.printf non level output\n");
  // Serial.println("Dallas Temperature Influx Client");

  // delay(1000);

  init_dallas();
  if (setup_dallas() == 0) {
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
void update_display() {
  // ColorLCD 135 x 240

  if (!displayPaused) {
    int percentage = getStableBatteryPercentage();
    // M5.Display.clear();
    dsp.clear();
    dsp.setCursor(0, 20);
    dsp.printf("M5S BAT: %3d%%, %.2f V", percentage,
               M5.Power.getBatteryVoltage() / 1e3);
    // M5.Display.printf("%dmV",
    // M5.Display.printf("WiFi: %d", WiFiStatus);
    dsp.setCursor(0, 60);
    dsp.printf("Tr: %.2f Tb: %.2f ", sensorTemp[0], sensorTemp[1]);
    if (sensorDiff != 0.0)
      dsp.printf("Eq");
  }
}

void loop() {
  unsigned long currentMillis = millis();
  static constexpr const int colors[] = {TFT_WHITE,  TFT_CYAN, TFT_RED,
                                         TFT_YELLOW, TFT_BLUE, TFT_GREEN};
  static constexpr const char *const names[] = {
      "none",       "wasHold",     "wasClicked",
      "wasPressed", "wasReleased", "wasDeciedCount"};

  // my_instrument.ProcessInput(Serial, "\n");
  M5.update(); // Update button states
  buttonState = M5.BtnA.wasHold()               ? 1
                : M5.BtnA.wasClicked()          ? 2
                : M5.BtnA.wasPressed()          ? 3
                : M5.BtnA.wasReleased()         ? 4
                : M5.BtnA.wasDecideClickCount() ? 5
                                                : 0;
  if (buttonState) {
    // M5_LOGI("BtnB:%s  count:%d", names[buttonState],
    // M5.BtnB.getClickCount());
    M5_LOGI("BtnA:%s  count:%d", names[buttonState], M5.BtnA.getClickCount());
    // M5.Display.fillRect(w*2, 0, w-1, h, colors[state]);
    M5.Display.setCursor(buttonState * 10, 80);
    M5.Display.printf("%d", buttonState);
    if (buttonState == 1)
      sensorDiff = dt_oneWire.getTempC(ds18Sensors[1]) -
                   dt_oneWire.getTempC(ds18Sensors[0]);
  }
  buttonState = M5.BtnB.wasHold()               ? 1
                : M5.BtnB.wasClicked()          ? 2
                : M5.BtnB.wasPressed()          ? 3
                : M5.BtnB.wasReleased()         ? 4
                : M5.BtnB.wasDecideClickCount() ? 5
                                                : 0;
  if (buttonState) {
    M5_LOGI("BtnB:%s  count:%d", names[buttonState], M5.BtnB.getClickCount());
    M5.Display.setCursor(buttonState * 10, 100);
    M5.Display.printf("%d", buttonState);
  }

  buttonState = M5.BtnC.wasHold()               ? 1
                : M5.BtnC.wasClicked()          ? 2
                : M5.BtnC.wasPressed()          ? 3
                : M5.BtnC.wasReleased()         ? 4
                : M5.BtnC.wasDecideClickCount() ? 5
                                                : 0;
  if (buttonState) {
    M5_LOGI("BtnC:%s  count:%d", names[buttonState], M5.BtnC.getClickCount());
    M5.Display.setCursor(buttonState * 10, 120);
    M5.Display.printf("%d", buttonState);
  }
  if (currentMillis - previousMillis > samplingInterval) {
    previousMillis += samplingInterval;
    // call sensors.requestTemperatures() to issue a global temperature
    // WiFiStatus = wifiMulti.run();
    dt_oneWire.requestTemperatures();
    sensorTemp[0] = dt_oneWire.getTempC(ds18Sensors[0]);
    sensorTemp[1] = dt_oneWire.getTempC(ds18Sensors[1]) - sensorDiff;
    M5_LOGI("Tred %.2f  Tblue: %.2f", sensorTemp[0], sensorTemp[1]);
    update_display();
  }
}

// if (StickCP2.BtnB.wasClicked()) {
//     StickCP2.Display.clear();
//     // StickCP2.Speaker.tone(8000, 20);
// }

// if (StickCP2.BtnA.wasPressed()) {
//     displayPaused = !displayPaused; // Toggle display update state
// }
//  vim: syntax=cpp ts=4 sw=4 sts=4 sr et

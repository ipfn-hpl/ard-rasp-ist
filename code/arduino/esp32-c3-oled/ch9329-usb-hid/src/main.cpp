
// #include "EEPROM.h"
#include "esp_log.h"
#include <Arduino.h>
#include <CH9329_Keyboard.h>
#include <Debounce.h>
#include <OneBitDisplay.h>

// #include <SoftwareSerial.h>
const byte rxPin = 2;
const byte txPin = 3;
// SoftwareSerial mySerial(rxPin, txPin);

#define SDA_PIN 5
#define SCL_PIN 6
#define LED_PIN 8
#define BOOTSEL_PIN 9

ONE_BIT_DISPLAY obd;
bool bootState = true; // active low
bool ledState = true;
const unsigned int samplingInterval = 1000;
unsigned long previousMillis = 0;
static const char *TAG = "EspC3";

void setup() {

  obd.setI2CPins(SDA_PIN, SCL_PIN);
  obd.I2Cbegin(OLED_72x40);
  obd.allocBuffer();
  //  obd.setContrast(50);
  obd.fillScreen(0);
  // obd.setFont(FONT_8x8);
  obd.setFont(FONT_6x8);
  //
  obd.setCursor(0, 22);
  obd.println("Starting...");
  obd.display();
  pinMode(LED_PIN, OUTPUT);
  // pinMode(20, INPUT); // RX IN

  digitalWrite(LED_PIN, ledState); // LED off
  pinMode(BOOTSEL_PIN, INPUT_PULLUP);
  debouncePins(BOOTSEL_PIN, BOOTSEL_PIN);
  bool ok = false;
  Serial.begin(115200);
  Serial1.begin(CH9329_DEFAULT_BAUDRATE, SERIAL_8N1, RX, TX);
  CH9329_Keyboard.begin(Serial);

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

  // esp_log_level_set("*", ESP_LOG_VERBOSE);        // set all components to
  // ERROR level
  esp_log_level_set("wifi", ESP_LOG_INFO); // enable WARN logs from WiFi stack
  log_i("hello world!");
  log_e("hello worldE!");
  // attempt to connect to Wifi network:
  ESP_LOGI(TAG, "CH9329_Keyboard Control Library Demo, RX: %d", RX);
}
void loop() {
  static int i = 0;
  unsigned long currentMillis = millis();
  // ledState = not ledState;
  // bootState = digitalRead(BOOTSEL_PIN);
  bootState = debouncedDigitalRead(BOOTSEL_PIN);
  if (currentMillis - previousMillis > samplingInterval) {
    previousMillis = currentMillis;
    // call sensors.requestTemperatures() to issue a global temperature
    //                iflxConnected  = false;
    // ESP_LOGI(TAG, "CH9329_Keyboard , RX: %d, TX:%d", RX, TX);
    // ESP_LOGI(TAG, "CH9329_Keyboard , CH9329_DEFAULT_BAUDRATE: %d, TX:%d",
    //         CH9329_DEFAULT_BAUDRATE, TX);
    // CH9329_Keyboard , RX: 20, TX:21
    // CH9329_DEFAULT_BAUDRATE: 9600
    ESP_LOGI(TAG, "Sending 'Hello world'...");
    // Serial.println("Sending 'Hello world'...");

    // delay(1000);
    if (!bootState) {
      digitalWrite(LED_PIN, ledState); // LED off
      ledState = !ledState;
      CH9329_Keyboard.print("ThisPasswordIsWeakLikeABaby");
    }
    // obd.setCursor(10, 20);
    // obd.println("Count Led...");
    obd.fillScreen(0);
    obd.setCursor(0, 12);
    obd.print("Count=");
    obd.println(i++, DEC);
    obd.display();
    // Serial.println("Sending Enter key...");
  }
}

//

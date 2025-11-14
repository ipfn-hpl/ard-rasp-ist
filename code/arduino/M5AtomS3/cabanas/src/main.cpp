/*
 *
 * @Hardwares: M5AtomS3 + Atomic GPS Base
 * M5AtomS3: https://github.com/m5stack/M5AtomS3
 * TinyGPSPlus: https://github.com/mikalhart/TinyGPSPlus
 */

#include <Arduino.h>
#include <M5Unified.h>
#include <TinyGPSPlus.h>

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <WiFiMulti.h>

#include <esp_sntp.h>
#define SNTP_ENABLED 1
// #endif

#include "arduino_secrets.h"

#define DEVICE "M5AS3"
#define TZ_INFO "UTC0"

#define WIFI_RETRIES 15
// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET,
                      INFLUXDB_TOKEN, InfluxDbCloud2CACert);

#define NTP_SERVER1 "pool.ntp.org"
#define NTP_SERVER2 "ntp1.tecnico.ulisboa.pt"
#define NTP_SERVER3 "1.pool.ntp.org"
#define NTP_TIMEZONE "UTC"
static constexpr const char *const wd[7] = {"Sun", "Mon", "Tue", "Wed",
                                            "Thr", "Fri", "Sat"};

#define WRITE_PRECISION                                                        \
  WritePrecision::US // MS
                     //

// The TinyGPSPlus object
TinyGPSPlus gps;
static auto &dsp = (M5.Display);
// Declare Data point
Point iflxSensor("wifi_status");

WiFiMulti wifiMulti;
uint8_t WiFiStatus = WL_DISCONNECTED;

int connect_wifi(int retries) {
  int status = WL_IDLE_STATUS; // the Wifi radio's status
                               // setup RTC ( NTP auto setting )
  configTzTime(NTP_TIMEZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);

  for (int i = 0; i < retries; i++) {
    status = wifiMulti.run();
    if (status == WL_CONNECTED) {
      // Serial.println(" WiFi connected. ");
      // log_i("IP address: ");
      IPAddress ip = WiFi.localIP();
      M5_LOGI("IPAddress %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      dsp.setCursor(0, 110);
      dsp.printf("WiFi %s", WiFi.SSID().c_str());
      break;
    } else {
      M5_LOGI("..");
      delay(500);
    }
  }
  if (status == WL_CONNECTED) {
    // Accurate time is necessary for certificate validation and writing in
    // batches We use the NTP servers in your area as provided by:
    // https://www.pool.ntp.org/zone/ Syncing progress and the time will be
    // printed to Serial.
    // timeSync(TZ_INFO,  NTP_SERVER1, NTP_SERVER2);
    // configTzTime(TZ_INFO, "ntp1.tecnico.ulisboa.pt", "pool.ntp.org",
    // "time.nis.gov");
    M5.Log.println("SNTP ENABLED.\n");
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
      M5.Log.print(".");
      M5.delay(1000);
    }
    M5.Log.println("\r\nNTP Connected.");
    time_t t = time(nullptr) + 1; // Advance one second.
    while (t > time(nullptr))
      ; /// Synchronization in seconds
    M5.Rtc.setDateTime(gmtime(&t));

    if (client.validateConnection()) {
      M5_LOGI("Connected to InfluxDB: %s", client.getServerUrl().c_str());
      // Set write precision to milliseconds. Leave other parameters default.
      // Enable messages batching and retry buffer
      client.setWriteOptions(WriteOptions().writePrecision(WRITE_PRECISION));
      // WriteOptions().writePrecision(WRITE_PRECISION).batchSize(MAX_BATCH_SIZE).bufferSize(WRITE_BUFFER_SIZE));
    } else {
      M5_LOGE("InfluxDB connection failed: %s",
              client.getLastErrorMessage().c_str());
    }
  } else {
    M5_LOGE(" Fail to connect WiFi, giving up.");
    // timeSync("UTC", "192.168.88.1", "pool.ntp.org", "time.nis.gov");
  }
  return status;
}

// This custom version of delay() ensures that the gps object
// is being "fed".
static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (Serial2.available())
      gps.encode(Serial2.read());
  } while (millis() - start < ms);
}

void writeWifiPoint() {
  // Write point
  iflxSensor.addField("rssi", WiFi.RSSI());
  if (!client.writePoint(iflxSensor)) {
    M5_LOGE("InfluxDB write failed: %s", client.getLastErrorMessage().c_str());
  }
  // Clear fields for next usage. Tags remain the same.
  iflxSensor.clearFields();
  M5_LOGI("Flushing Wifi data into InfluxDB");
  if (!client.flushBuffer()) {
    M5_LOGW("InfluxDB flush failed: %s", client.getLastErrorMessage().c_str());
    M5_LOGE("Full buffer: %d", client.isBufferFull());
    // ? "Yes" : "No");
  }
}
void setup() {
  delay(1000);
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);

  dsp.setTextColor(GREEN);
  dsp.setTextDatum(middle_center);
  dsp.setFont(&fonts::FreeSerifBold24pt7b);
  dsp.setTextSize(1);
  dsp.drawString("GPS", M5.Display.width() / 2, M5.Display.height() / 2);

  if (!M5.Rtc.isEnabled()) {
    M5.Log.println("RTC not found.");
    for (;;) {
      M5.delay(500);
    }
  }

  M5.Log.println("RTC found.");
  Serial2.begin(4800, SERIAL_8N1, 5, 6);
  Serial.println(F("Serial 2 connected"));
  for (int i = 0; i < NUM_SSID; i++) {
    wifiMulti.addAP(ssids[i], pass[i]);
  }
  WiFiStatus = connect_wifi(WIFI_RETRIES);
  // Serial.println
  //  Add constant tags - only once
  delay(2000);

  // Add tags to the data point
  iflxSensor.addTag("device", DEVICE);
  iflxSensor.addTag("SSID", WiFi.SSID());
}

void loop() {
  static uint32_t next_sec = 0;
  int32_t sec = millis() / 1000;
  // loopNmea();
  M5.update();
  if (sec >= next_sec) {
    // ESP32 internal timer
    auto t = time(nullptr);
    {
      auto tm = gmtime(&t); // for UTC.
                            // M5.Display.setCursor(0,20);
      M5.Log.printf("ESP32 UTC  :%04d/%02d/%02d (%s)  %02d:%02d:%02d\r\n",
                    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                    wd[tm->tm_wday], tm->tm_hour, tm->tm_min, tm->tm_sec);
    }
    // M5.Display.setCursor(0,20);
    // M5.Log.printf("IDB Point UTC  %s:", tC.c_str());
    next_sec = sec + 2;
    if (wifiMulti.run() != WL_CONNECTED) {
      M5_LOGE("Wifi connection lost");
    } else
      writeWifiPoint();
  }
  if ((sec & 7) == 0) { // prevent WDT.
    vTaskDelay(1);
  }

  Serial.println();
  smartDelay(1000);

  if (millis() > 5000 && gps.charsProcessed() < 10)
    M5.Log.println("No GPS data received: check wiring");
}

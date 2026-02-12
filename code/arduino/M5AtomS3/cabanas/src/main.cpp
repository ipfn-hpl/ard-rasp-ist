/*
 *
 * @Hardwares: M5AtomS3 + Atomic GPS Base
 * M5AtomS3: https://github.com/m5stack/M5AtomS3
 * TinyGPSPlus: https://github.com/mikalhart/TinyGPSPlus
 * iIPFN Location: 38.737272,-9.138717

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

// #define DEVICE "M5AS3"
char device_buff[32];
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
TinyGPSCustom zda(gps, "INZDA", 1);  // - Time and Date
TinyGPSCustom mtw(gps, "INMTW", 1);  // Water Temperature
                                     // TinyGPSCustom mtw(gps, "INMTW", 1); //
TinyGPSCustom gcat(gps, "INGGA", 1); //
TinyGPSCustom gllt(gps, "INGLL", 1); //- Geographic Position (latitude)
TinyGPSCustom gllh(gps, "INGLL", 2); //- Geographic Position (N/S)

TinyGPSCustom gllg(gps, "INGLL", 3); //- Geographic Position (longitude)

TinyGPSCustom dpt(gps, "INDPT", 1); //- Depth
                                    //
static auto &dsp = (M5.Display);
// Declare Data point
Point iflxSensor("wifi_status");

WiFiMulti wifiMulti;
uint8_t WiFiStatus = WL_DISCONNECTED;

float boatLat, boatLng, boatDepth;

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
void updateBoatData() {
  if (zda.isUpdated() || gllt.isUpdated() || gllg.isUpdated() ||
      dpt.isUpdated()) {

    boatLat = atof(gllt.value()) / 100.0;
    boatLng = atof(gllg.value()) / 100.0;
    boatDepth = atof(dpt.value());
  }
  if (gps.time.isUpdated()) {
    if (gps.location.isValid()) {
      boatLat = gps.location.lat();
      boatLng = gps.location.lng();
    }
  }
}
void displayInfo() {

  if (gps.time.isUpdated()) {
    Serial.print(F("Location: "));
    if (gps.location.isValid()) {
      Serial.print(gps.location.lat(), 6);
      Serial.print(F(","));
      Serial.print(gps.location.lng(), 6);
    } else {
      Serial.print(F("INVALID"));
    }
    Serial.println();
  }
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

bool sendBoatPoint() {
  Point sensorBoat("boatData");
  sensorBoat.addTag("device", device_buff);
  sensorBoat.addField("Lat", boatLat);
  sensorBoat.addField("Lng", boatLng);
  sensorBoat.addField("Depth", boatDepth);
  if (!client.writePoint(sensorBoat)) {
    M5_LOGE("InfluxDB write failed: %s", client.getLastErrorMessage().c_str());
    return false;
  }

  return true;
}

void sendWifiPoint() {
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
  /*
     if (!M5.Rtc.isEnabled()) {
     M5.Log.println("RTC not found.");
     for (;;) {
     M5.delay(500);
     }
     } e
     M5.Log.println("RTC found.");
     */
  Serial2.begin(4800, SERIAL_8N1, 5, 6);
  Serial.println(F("Serial 2 connected"));
  for (int i = 0; i < NUM_SSID; i++) {
    wifiMulti.addAP(ssids[i], pass[i]);
  }
  sprintf(device_buff, "M5AS3-%s", WiFi.macAddress().c_str());
  WiFiStatus = connect_wifi(WIFI_RETRIES);
  // Serial.println
  //  Add constant tags - only once
  delay(2000);

  // Add tags to the data point
  iflxSensor.addTag("device", device_buff);
  iflxSensor.addTag("SSID", WiFi.SSID());
}

void loop() {
  static uint32_t next_sec = 0;
  static uint32_t next_data = 0;
  int32_t sec = millis() / 1000;
  // loopNmea();
  displayInfo();
  updateBoatData();
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
    next_sec = sec + 60;
    if (wifiMulti.run() != WL_CONNECTED) {
      M5_LOGE("Wifi connection lost");
    } else
      sendWifiPoint();
  }
  if (sec >= next_data) {
    next_data = sec + 30;
    sendBoatPoint();
  }

  if ((sec & 7) == 0) { // prevent WDT.
    vTaskDelay(1);
  }

  Serial.println();
  smartDelay(1000);

  if (millis() > 5000 && gps.charsProcessed() < 10)
    M5.Log.println("No GPS data received: check wiring");
}

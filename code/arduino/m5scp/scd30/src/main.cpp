/*
 * https://www.adafruit.com/product/4867
 *  https://learn.adafruit.com/adafruit-scd30
 */
#include <Arduino.h>
// Include this to enable the M5 global instance.
#include <M5Unified.h>

// Basic demo for readings from Adafruit SCD30
#include <Adafruit_SCD30.h>

#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <esp_sntp.h>
//#include <sntp.h>

#include "arduino_secrets.h"

#define WIFI_RETRIES 15
#define NTP_SERVER1  "ntp04.oal.ul.pt"
#define NTP_SERVER2  "ntp1.tecnico.ulisboa.pt"
#define NTP_SERVER3   "1.pool.ntp.org"
#define NTP_TIMEZONE  "UTC"

#define WRITE_PRECISION WritePrecision::MS

// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);


Adafruit_SCD30  scd30;

static uint32_t next_sec = 0;

static auto &dsp = (M5.Display);
//static rect_t rect_graph_area;
//static rect_t rect_text_area;

//static const uint8_t SDA = 32;
//static const uint8_t SCL = 33;

//My recommendation is to declare and use.
                //
WiFiMulti wifiMulti;
uint8_t WiFiStatus = WL_DISCONNECTED;

int connect_wifi(int retries) {
    int status = WL_IDLE_STATUS; // the Wifi radio's status
    // setup RTC ( NTP auto setting )
    configTzTime(NTP_TIMEZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);

    for(int i = 0; i < retries; i++) {
        status = wifiMulti.run();
        if(status == WL_CONNECTED) {
            //Serial.println(" WiFi connected. ");
            //log_i("IP address: ");
            IPAddress ip = WiFi.localIP();
            M5_LOGI("IPAddress %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            M5.Display.setCursor(0,90);
            dsp.printf("WiFi %s", WiFi.SSID().c_str());
            break;
        }
        else {
            M5_LOGI("..");
            delay(500);
        }
    }
    if (status == WL_CONNECTED) {
        // Accurate time is necessary for certificate validation and writing in batches
        // We use the NTP servers in your area as provided by: https://www.pool.ntp.org/zone/
        // Syncing progress and the time will be printed to Serial.
        //timeSync(TZ_INFO,  NTP_SERVER1, NTP_SERVER2);
        //configTzTime(TZ_INFO, "ntp1.tecnico.ulisboa.pt", "pool.ntp.org", "time.nis.gov");
//#if SNTP_ENABLED
        M5.Log.println("SNTP ENABLED.\n");
        while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
        {
            M5.Log.print(".");
            M5.delay(1000);
        }
        /*
#else
        M5.delay(1600);
        struct tm timeInfo;
        while (!getLocalTime(&timeInfo, 1000))
        {
            M5.Log.print('.');
        };
#endif
*/
        M5.Log.println("\r\nNTP Connected.");
        time_t t = time(nullptr)+1; // Advance one second.
        while (t > time(nullptr));  /// Synchronization in seconds
        M5.Rtc.setDateTime( gmtime( &t ) );

        if (client.validateConnection()) {
            M5_LOGI("Connected to InfluxDB: %s", 
                    client.getServerUrl().c_str());
            // Set write precision to milliseconds. Leave other parameters default.
            // Enable messages batching and retry buffer
            client.setWriteOptions(
                    WriteOptions().writePrecision(WRITE_PRECISION));
        } else {
            M5_LOGE("InfluxDB connection failed: %s",
                    client.getLastErrorMessage().c_str());
        }
    }
    else {
        M5_LOGE(" Fail to connect WiFi, giving up.");
        //timeSync("UTC", "192.168.88.1", "pool.ntp.org", "time.nis.gov");
    }
    return status;
}

void setup(void)
{
  auto cfg = M5.config();

  // If you want to use external IMU, write this
//cfg.external_imu = true;

  M5.begin(cfg);
  // Not needed
  //Wire.begin(SDA, SCL);
  //Wire.begin();  // Init wire and join the I2C network.
  //
  /// You can set Log levels for each output destination.
  /// ESP_LOG_ERROR / ESP_LOG_WARN / ESP_LOG_INFO / ESP_LOG_DEBUG / ESP_LOG_VERBOSE
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_VERBOSE);
  //M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_DEBUG);
  //M5.Log.setLogLevel(m5::log_target_callback, ESP_LOG_INFO);
  /// You can color the log or not.
  M5.Log.setEnableColor(m5::log_target_serial, true);

  M5_LOGI("M5_LOGI info log");      // INFO level output with source INFO

  if (!M5.Rtc.isEnabled())
  {
    M5.Log.println("RTC not found.");
    for (;;) { M5.delay(500); }
  }

  M5.Log.println("RTC found.");
  int32_t w = dsp.width();
  int32_t h = dsp.height();
  if (w < h)
  { /// Landscape mode.
    dsp.setRotation(dsp.getRotation() ^ 1);
    w = dsp.width();
    h = dsp.height();
  }
  M5_LOGI("dsp W, H:%d, %d", w, h);
  int32_t graph_area_h = ((h - 8) / 18) * 18;
  int32_t text_area_h = h - graph_area_h;
  float fontsize = text_area_h / 8;
  M5.Log.printf("text_area_h:%d fontsize:%.2f", text_area_h, fontsize);
  fontsize = 2.0; //text_area_h / 8;
  dsp.setTextSize(fontsize);
  M5.Display.setCursor(0,10);
  //M5.Display.printf("dsp W, H:%d, %d F%.2f", w, h, fontsize);
  dsp.printf("dsp W, H:%d, %d F%.2f", w, h, fontsize);

  if (!scd30.begin()) {
  M5_LOGW("Failed to find SCD30 chip");
    //while (1) { delay(10); }
  }
  M5_LOGI("SCD30 Found!");
  M5.Log.printf("text_area_h:%d fontsize:%.2f", text_area_h, fontsize);
  // M5.Log.printf("SDA %d, SCL %d\n", SDA, SCL);
  // SDA 32, SCL 33

  //rect_graph_area = { 0, 0, w, graph_area_h };
  //rect_text_area = {0, graph_area_h, w, text_area_h };

}

void loop(void) {

    int32_t sec = millis() / 1000;
    if (sec >= next_sec)
    {
        next_sec = sec + 2;
        M5.Log.printf("Hello %d ", sec);
        if (scd30.dataReady()) {
            M5_LOGI("Data available!");
            if (scd30.read()){
                //M5.Log.printf("Temperature: %.3f\n", scd30.temperature);
                M5.Log.printf("Temp:%.2f RH:%.2f  CO2:%.3f ppm\n", scd30.temperature,
                           scd30.relative_humidity, scd30.CO2);
                M5.Display.setCursor(0, 40);
                dsp.printf("                              ");
                M5.Display.setCursor(0, 40);
                dsp.printf("Temp:%.2f RH:%.2f", scd30.temperature,
                        scd30.relative_humidity);
                M5.Display.setCursor(0, 60);
                dsp.printf("CO2:%.3f ppm", scd30.CO2);
            }
            else
                M5_LOGW("Error reading sensor data");
        }
        else
            M5_LOGW("Data Not available!");
        if ((sec & 7) == 0)
        { // prevent WDT.
            vTaskDelay(1);
        }
    }
}


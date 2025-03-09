/*
 *  https://github.com/m5stack/M5Unified/tree/master/examples/Basic/Imu
 *  */
#include <Arduino.h>
// If you use Unit LCD, write this.
//#include <M5UnitLCD.h>
// Include this to enable the M5 global instance.
#include <M5Unified.h>

#include <WiFiMulti.h>
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

#ifndef SNTP_ENABLED
#define SNTP_ENABLED 0
#endif

#include "arduino_secrets.h"

#define DEVICE "ESP32"

#define TZ_INFO "UTC0"

#define WIFI_RETRIES 15
// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

#define NTP_SERVER1  "pool.ntp.org"
#define NTP_SERVER2  "ntp1.tecnico.ulisboa.pt"
#define NTP_SERVER3   "1.pool.ntp.org"
 #define NTP_TIMEZONE  "UTC"

#define WRITE_PRECISION WritePrecision::MS  // MS
#define MAX_BATCH_SIZE 50
// The recommended size is at least 2 x batch size.
#define WRITE_BUFFER_SIZE 3 * MAX_BATCH_SIZE

// Strength of the calibration operation;
// 0: disables calibration.
// 1 is weakest and 255 is strongest.
static constexpr const uint8_t calib_value = 64;


// This sample code performs calibration by clicking on a button or screen.
// After 10 seconds of calibration, the results are stored in NVS.
// The saved calibration values are loaded at the next startup.
// 
// === How to calibration ===
// ※ Calibration method for Accelerometer
//    Change the direction of the main unit by 90 degrees
//     and hold it still for 2 seconds. Repeat multiple times.
//     It is recommended that as many surfaces as possible be on the bottom.
//
// ※ Calibration method for Gyro
//    Simply place the unit on a quiet desk and hold it still.
//    It is recommended that this be done after the accelerometer calibration.
// 
// ※ Calibration method for geomagnetic sensors
//    Rotate the main unit slowly in multiple directions.
//    It is recommended that as many surfaces as possible be oriented to the north.
// 
// Values for extremely large attitude changes are ignored.
// During calibration, it is desirable to move the device as gently as possible.

struct rect_t
{
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
};

static constexpr const uint32_t color_tbl[18] = 
{
0xFF0000u, 0xCCCC00u, 0xCC00FFu,
0xFFCC00u, 0x00FF00u, 0x0088FFu,
0xFF00CCu, 0x00FFCCu, 0x0000FFu,
0xFF0000u, 0xCCCC00u, 0xCC00FFu,
0xFFCC00u, 0x00FF00u, 0x0088FFu,
0xFF00CCu, 0x00FFCCu, 0x0000FFu,
};
static constexpr const float coefficient_tbl[3] = { 0.5f, (1.0f / 256.0f), (1.0f / 1024.0f) };

static auto &dsp = (M5.Display);
static rect_t rect_graph_area;
static rect_t rect_text_area;

static uint8_t calib_countdown = 0;

static int prev_xpos[18];
// Declare Data point
Point iflxSensor("wifi_status");

WiFiMulti wifiMulti;
uint8_t WiFiStatus = WL_DISCONNECTED;

const unsigned long imu_semp_period = 10000; // in us
unsigned int point2Send = 0; //100; // in us
                                             //
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
            //M5.Display.print("WiFi connected.");
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
#if SNTP_ENABLED
        M5.Log.println("SNTP ENABLED.\n");
        while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
        {
            M5.Log.print(".");
            M5.delay(1000);
        }
#else
        M5.delay(1600);
        struct tm timeInfo;
        while (!getLocalTime(&timeInfo, 1000))
        {
            M5.Log.print('.');
        };
#endif
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
                    WriteOptions().writePrecision(WRITE_PRECISION).batchSize(MAX_BATCH_SIZE).bufferSize(WRITE_BUFFER_SIZE));
        } else {
            M5_LOGE("InfluxDB connection failed: %s",
            //Serial.println(
                    client.getLastErrorMessage().c_str());
        }
    }
    else {
        M5_LOGE(" Fail to connect WiFi, giving up.");
        //timeSync("UTC", "192.168.88.1", "pool.ntp.org", "time.nis.gov");
    }
    return status;
}


void drawBar(int32_t ox, int32_t oy, int32_t nx, int32_t px, int32_t h, uint32_t color)
{
  uint32_t bgcolor = (color >> 3) & 0x1F1F1Fu;
  if (px && ((nx < 0) != (px < 0)))
  {
    dsp.fillRect(ox, oy, px, h, bgcolor);
    px = 0;
  }
  if (px != nx)
  {
    if ((nx > px) != (nx < 0))
    {
      bgcolor = color;
    }
    dsp.setColor(bgcolor);
    dsp.fillRect(nx + ox, oy, px - nx, h);
  }
}

void drawGraph(const rect_t& r, const m5::imu_data_t& data)
{
  float aw = (128 * r.w) >> 1;
  float gw = (128 * r.w) / 256.0f;
  float mw = (128 * r.w) / 1024.0f;
  int ox = (r.x + r.w)>>1;
  int oy = r.y;
  int h = (r.h / 18) * (calib_countdown ? 1 : 2);
  int bar_count = 9 * (calib_countdown ? 2 : 1);

  dsp.startWrite();
  for (int index = 0; index < bar_count; ++index)
  {
    float xval;
    if (index < 9)
    {
      auto coe = coefficient_tbl[index / 3] * r.w;
      xval = data.value[index] * coe;
    }
    else
    {
      xval = M5.Imu.getOffsetData(index - 9) * (1.0f / (1 << 19));
    }

    // for Linear scale graph.
    float tmp = xval;

    // The smaller the value, the larger the amount of change in the graph.
//  float tmp = sqrtf(fabsf(xval * 128)) * (signbit(xval) ? -1 : 1);

    int nx = tmp;
    int px = prev_xpos[index];
    if (nx != px)
    prev_xpos[index] = nx;
    drawBar(ox, oy + h * index, nx, px, h - 1, color_tbl[index]);
  }
  dsp.endWrite();
}

void updateCalibration(uint32_t c, bool clear = false)
{
  calib_countdown = c;

  if (c == 0) {
    clear = true;
  }

  if (clear)
  {
    memset(prev_xpos, 0, sizeof(prev_xpos));
    dsp.fillScreen(TFT_BLACK);

    if (c)
    { // Start calibration.
      M5.Imu.setCalibration(calib_value, calib_value, calib_value);
    // ※ The actual calibration operation is performed each time during M5.Imu.update.
    // 
    // There are three arguments, which can be specified in the order of Accelerometer, gyro, and geomagnetic.
    // If you want to calibrate only the Accelerometer, do the following.
    // M5.Imu.setCalibration(100, 0, 0);
    //
    // If you want to calibrate only the gyro, do the following.
    // M5.Imu.setCalibration(0, 100, 0);
    //
    // If you want to calibrate only the geomagnetism, do the following.
    // M5.Imu.setCalibration(0, 0, 100);
    }
    else
    { // Stop calibration. (Continue calibration only for the geomagnetic sensor)
      M5.Imu.setCalibration(0, 0, calib_value);
      // If you want to stop all calibration, write this.
      // M5.Imu.setCalibration(0, 0, 0);

      // save calibration values.
      M5.Imu.saveOffsetToNVS();
    }
  }

  auto backcolor = (c == 0) ? TFT_BLACK : TFT_BLUE;
  dsp.fillRect(rect_text_area.x, rect_text_area.y, rect_text_area.w, rect_text_area.h, backcolor);

  if (c)
  {
    dsp.setCursor(rect_text_area.x + 2, rect_text_area.y + 1);
    dsp.setTextColor(TFT_WHITE, TFT_BLUE);
    dsp.printf("Countdown:%d ", c);
  }
}

void startCalibration(void)
{
  updateCalibration(10, true);
}

void setup(void)
{
  auto cfg = M5.config();

  // If you want to use external IMU, write this
//cfg.external_imu = true;

  M5.begin(cfg);

  /// You can set Log levels for each output destination.
  /// ESP_LOG_ERROR / ESP_LOG_WARN / ESP_LOG_INFO / ESP_LOG_DEBUG / ESP_LOG_VERBOSE
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_VERBOSE);
  //M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_DEBUG);
  //M5.Log.setLogLevel(m5::log_target_callback, ESP_LOG_INFO);
  /// You can color the log or not.
  M5.Log.setEnableColor(m5::log_target_serial, true);

  M5_LOGI("M5_LOGI info log");      // INFO level output with source INFO

  const char* name;
  auto imu_type = M5.Imu.getType();
//[   262][I][main.cpp:214] setup(): imu:mpu6886

 if (!M5.Rtc.isEnabled())
  {
    M5.Log.println("RTC not found.");
    for (;;) { M5.delay(500); }
  }

  M5.Log.println("RTC found.");


  switch (imu_type)
  {
  case m5::imu_none:        name = "not found";   break;
  case m5::imu_sh200q:      name = "sh200q";      break;
  case m5::imu_mpu6050:     name = "mpu6050";     break;
  case m5::imu_mpu6886:     name = "mpu6886";     break;
  case m5::imu_mpu9250:     name = "mpu9250";     break;
  case m5::imu_bmi270:      name = "bmi270";      break;
  default:                  name = "unknown";     break;
  };
  M5_LOGI("imu:%s", name);
  //M5.Display.printf("imu:%s", name);

  if (imu_type == m5::imu_none)
  {
    for (;;) { delay(1); }
  }

//[   264][I][main.cpp:230] setup(): imu W, H:240, 135
  int32_t w = dsp.width();
  int32_t h = dsp.height();
  if (w < h)
  {
    dsp.setRotation(dsp.getRotation() ^ 1);
    w = dsp.width();
    h = dsp.height();
  }
  M5_LOGI("imu W, H:%d, %d", w, h);
  int32_t graph_area_h = ((h - 8) / 18) * 18;
  int32_t text_area_h = h - graph_area_h;
  float fontsize = text_area_h / 8;
  dsp.setTextSize(fontsize);

  rect_graph_area = { 0, 0, w, graph_area_h };
  rect_text_area = {0, graph_area_h, w, text_area_h };

  // Read calibration values from NVS.
  if (!M5.Imu.loadOffsetFromNVS())
  {
    startCalibration();
  }
  // attempt to connect to Wifi network:

  M5_LOGI("Attempting to connect to SSID: ");
  for(int i = 0; i < NUM_SSID; i++) {
      wifiMulti.addAP(ssids[i], pass[i]);
  }
   WiFiStatus = connect_wifi(WIFI_RETRIES);
   //Serial.println
   // Add constant tags - only once
   delay(2000);
   
// Add tags to the data point
    iflxSensor.addTag("device", DEVICE);
    iflxSensor.addTag("SSID", WiFi.SSID());
   /*
   if (WiFiStatus == WL_CONNECTED) {
       //iflxSensor.addTag("experiment", "calorimetryEsp32C3");
       //iflx_sensor.addTag("experiment", buffer); //"calorimetryEsp32C3");
       //validate_influx();
     //  previousMillis = millis() + 10 * samplingInterval;
   }
   */
}

void loop(void)
{
  static uint32_t frame_count = 0;
  static uint32_t prev_sec = 0;
  static constexpr const char* const wd[7] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};

  // To update the IMU value, use M5.Imu.update.
  // If a new value is obtained, the return value is non-zero.
  auto imu_update = M5.Imu.update();
  if (imu_update)
  {
    // Obtain data on the current value of the IMU.
    auto data = M5.Imu.getImuData();
    drawGraph(rect_graph_area, data);
     // Report networks (low priority data) in case we successfully wrote the previous batch
    if (!client.isBufferFull()) {
    if (point2Send > 0) {
        // Set identical time for the whole network scan
        time_t tnow = time(nullptr);
        Point sensorImu("imu_data");
        sensorImu.addTag("device", DEVICE);
        sensorImu.addField("gyro.x", data.gyro.x);
        sensorImu.addField("gyro.y", data.gyro.y);
        sensorImu.addField("gyro.z", data.gyro.z);
        //sensorImu.setTime(tnow);  //set the time
          if (!client.writePoint(sensorImu)) {
              M5_LOGE("InfluxDB write failed: %s",
                      client.getLastErrorMessage().c_str());
          }
          //else
              point2Send--;
    }
    }
     else
         M5_LOGW("IMU reporting skipped due to communication issues");
/*
    // The data obtained by getImuData can be used as follows.
    data.accel.x;      // accel x-axis value.
    data.accel.y;      // accel y-axis value.
    data.accel.z;      // accel z-axis value.
    data.accel.value;  // accel 3values array [0]=x / [1]=y / [2]=z.

    data.gyro.x;      // gyro x-axis value.
    data.gyro.y;      // gyro y-axis value.
    data.gyro.z;      // gyro z-axis value.
    data.gyro.value;  // gyro 3values array [0]=x / [1]=y / [2]=z.

    data.mag.x;       // mag x-axis value.
    data.mag.y;       // mag y-axis value.
    data.mag.z;       // mag z-axis value.
    data.mag.value;   // mag 3values array [0]=x / [1]=y / [2]=z.

    data.value;       // all sensor 9values array [0~2]=accel / [3~5]=gyro / [6~8]=mag

    M5_LOGV("ax:%f  ay:%f  az:%f", data.accel.x, data.accel.y, data.accel.z);
    M5_LOGV("gx:%f  gy:%f  gz:%f", data.gyro.x , data.gyro.y , data.gyro.z );
    M5_LOGV("mx:%f  my:%f  mz:%f", data.mag.x  , data.mag.y  , data.mag.z  );
*/
    ++frame_count;
  }
  else
  {
    M5.update();

    // Calibration is initiated when a button or screen is clicked.
    if (M5.BtnA.wasClicked() ){
        M5_LOGW("M5.BtnA.wasClicked");
        point2Send = 20;
    }
    if (M5.BtnPWR.wasClicked() || M5.Touch.getDetail().wasClicked())
    {
      startCalibration();
    }
  }

  int32_t sec = millis() / 1000;
  if (prev_sec != sec)
  {
      // ESP32 internal timer
      auto t = time(nullptr);
      {
          auto tm = gmtime(&t);    // for UTC.
          M5.Display.setCursor(0,20);
          M5.Log.printf("ESP32 UTC  :%04d/%02d/%02d (%s)  %02d:%02d:%02d\r\n",
                  tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
                  wd[tm->tm_wday],
                  tm->tm_hour, tm->tm_min, tm->tm_sec);
      }
      prev_sec = sec;
      M5_LOGI("sec:%d  frame:%d", sec, frame_count);
      frame_count = 0;

      if (calib_countdown)
      {
          updateCalibration(calib_countdown - 1);
      }
    //iflxSensor.clearFields();
  
    // Store measured value into point
    // Report RSSI of currently connected network
    // [ 13140][W][main.cpp:424] loop(): InfluxDB flush failed: {"code":"unprocessable entity","message":"failure writing points to database: partial write: points beyond retention policy dropped=1"}
      //iflxSensor.setTime(time(nullptr));
      iflxSensor.addField("rssi", WiFi.RSSI());
      // Check WiFi connection and reconnect if needed
      if (wifiMulti.run() != WL_CONNECTED) {
          M5_LOGE("Wifi connection lost");
      }
      else { 
          // Write point
          if (!client.writePoint(iflxSensor)) {
              M5_LOGE("InfluxDB write failed: %s",
                      client.getLastErrorMessage().c_str());
          }
          // Clear fields for next usage. Tags remain the same.
          iflxSensor.clearFields();
          M5_LOGI("Flushing data into InfluxDB");
          if (!client.flushBuffer()) {
              M5_LOGW("InfluxDB flush failed: %s",
                      client.getLastErrorMessage().c_str());
              M5_LOGE("Full buffer: %d",
                      client.isBufferFull());
              // ? "Yes" : "No");
          }

      }
      if ((sec & 7) == 0)
      { // prevent WDT.
          vTaskDelay(1);
      }
  }
}

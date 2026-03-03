/**
 * main.cpp — ESP32 WiFiMulti + Parallel Sensor → InfluxDB v2 (PlatformIO)
 * ─────────────────────────────────────────────────────────────────────────
 * Core 0 │ taskWiFiMonitor  — periodic WiFi health-check & reconnect
 * Core 1 │ taskSensorUpdate — sensor sampling + InfluxDB line-protocol POST
 *
 * All user settings live in include/config.h
 * Library:  tobiasschuerg/ESP8266 Influxdb  (works on ESP32)
 */

#include <Arduino.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h> // only needed for INFLUXDB_V2_CLOUD_URL constant
#include <Wire.h>
#include <OneBitDisplay.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <Wire.h>
// Displays sensor values on the built-in 72x40 OLED
//
#include "config.h"

ONE_BIT_DISPLAY obd;
// Need to explicitly set the I2C pin numbers on this C3 board
#define SDA_PIN 5
#define SCL_PIN 6
#define LED_PIN 8
#define BOOTSEL_PIN 9
#define COM 0xFF

// ── Logging ────────────────────────────────────────────────────────────────
#define LOG(tag, fmt, ...)                                                     \
  Serial.printf("[%7lu][%-7s] " fmt "\n", millis(), tag, ##__VA_ARGS__)

// ── Shared sensor data ─────────────────────────────────────────────────────
struct SensorData {
  int distance_mm = 0;
  uint32_t lastUpdateMs = 0;
  bool valid = false;
};

static SensorData g_sensor;
static SemaphoreHandle_t g_sensorMutex = nullptr;
static WiFiMulti g_wifiMulti;

// ── InfluxDB v2 client (constructed once, reused across writes) ────────────
static InfluxDBClient
    g_influx(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN
#if INFLUXDB_VERIFY_TLS
             ,
             InfluxDbCloud2CACert // bundle from InfluxDbCloud.h
#endif
    );

// ══════════════════════════════════════════════════════════════════════════════
// Sensor abstraction — replace stub with your real library
// ══════════════════════════════════════════════════════════════════════════════

static bool readSensor(int &dist) {
  // ── Replace with your real sensor call ────────────────────────────────
  //
  //  DHT:
  //    temp = dht.readTemperature();
  //    hum  = dht.readHumidity();
  //    return !isnan(temp) && !isnan(hum);
  //
  //  BME280:
  //    temp = bme.readTemperature();
  //    hum  = bme.readHumidity();
  //    return true;
  //
  // ─────────────────────────────────────────────────────────────────────
  //int dist;
  unsigned long timeout = millis() + 20;
  // Flush any existing data
  while (Serial1.available() > 0) {
    Serial1.read();
  }
  Serial1.write(COM);
  delay(8); // give sensor time to respond
  while (Serial1.available() < 1) {
    if (millis() > timeout) {
      return false;
    }
  }
  // M5.Log.printf("First Wait: %d\n", 500 - (timeout - millis()));

  // Flush until we find the 0xFF header
  timeout = millis() + 20;
  while (Serial1.available()) {
    if (Serial1.peek() == 0xFF)
      break;
    if (millis() > timeout) {
      return false;
    }
    Serial1.read(); // discard non-header bytes
  }

  // Wait for full 4-byte frame
  timeout = millis() + 20;
  while (Serial1.available() < 4) {
    if (millis() > timeout) {
      // M5.Log.printf("3 Timeout ");
      return false;
    }
  }
  byte buf[4];
  Serial1.readBytes(buf, 4);

  // Validate header
  if (buf[0] != 0xFF)
    return false;

  // Validate checksum
  byte checksum = (buf[0] + buf[1] + buf[2]) & 0xFF;
  if (checksum != buf[3])
    return false;

  // Calculate distance
  dist = (buf[1] << 8) | buf[2];
  // Displays Distance
  obd.setCursor(0, 0);
  obd.fillScreen(0);
  obd.printf("D: %d ", dist);

  obd.display();
  return true;
}

// ══════════════════════════════════════════════════════════════════════════════
// InfluxDB v2 publish
// ══════════════════════════════════════════════════════════════════════════════

/**
 * @brief  Write one sensor snapshot to InfluxDB v2 using line protocol.
 *
 *  Line protocol written:
 *    environment,device=esp32-node-1 depth=552
 *
 * @param  d  Local copy of sensor data (called outside the mutex).
 */
static void publishData(const SensorData &d) {
  if (WiFi.status() != WL_CONNECTED) {
    LOG("Influx", "Skip write — WiFi not connected");
    return;
  }

  // Build a Point (measurement name set in config.h)
  Point pt(INFLUXDB_MEASUREMENT);

  // Tags — indexed, low-cardinality metadata
  pt.addTag("device", INFLUXDB_DEVICE_TAG);

  // Fields — the actual measurements
  pt.addField("depth", d.distance_mm);

  // Timestamp: use server time (default) or supply your own NTP epoch:
  //   pt.setTime(time(nullptr));   // requires NTP sync first

  LOG("Influx", "Writing → %s", g_influx.pointToLineProtocol(pt).c_str());

  if (!g_influx.writePoint(pt)) {
    LOG("Influx", "Write FAILED: %s", g_influx.getLastErrorMessage().c_str());
  } else {
    LOG("Influx", "Write OK  (HTTP %d)", g_influx.getLastStatusCode());
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// FreeRTOS Tasks
// ══════════════════════════════════════════════════════════════════════════════

static void taskWiFiMonitor(void * /*pvParams*/) {
  LOG("WiFi", "Monitor task started (Core %d)", xPortGetCoreID());

  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      LOG("WiFi", "OK  ssid=%-20s  rssi=%ddBm  ip=%s", WiFi.SSID().c_str(),
          WiFi.RSSI(), WiFi.localIP().toString().c_str());
    } else {
      LOG("WiFi", "Disconnected — reconnecting…");

      const uint32_t deadline = millis() + WIFI_CONNECT_TIMEOUT_MS;
      bool ok = false;

      while (millis() < deadline) {
        if (g_wifiMulti.run(WIFI_CONNECT_TIMEOUT_MS) == WL_CONNECTED) {
          ok = true;
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
      }

      if (ok) {
        LOG("WiFi", "Reconnected → ssid=%s  ip=%s", WiFi.SSID().c_str(),
            WiFi.localIP().toString().c_str());
      } else {
        LOG("WiFi", "Reconnect timed out — retry in %us",
            WIFI_CHECK_INTERVAL_MS / 1000);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(WIFI_CHECK_INTERVAL_MS));
  }
}

static void taskSensorUpdate(void * /*pvParams*/) {
  LOG("Sensor", "Update task started (Core %d)", xPortGetCoreID());

  for (;;) {
    float t = 0.0f, h = 0.0f;
    int d=0;

    if (readSensor(d)) {
      if (xSemaphoreTake(g_sensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_sensor.distance_mm = d;
        g_sensor.lastUpdateMs = millis();
        g_sensor.valid = true;

        const SensorData snapshot = g_sensor; // copy inside lock
        xSemaphoreGive(g_sensorMutex);

        publishData(snapshot); // network I/O outside the mutex
      } else {
        LOG("Sensor", "Mutex timeout — skipping");
      }
    } else {
      LOG("Sensor", "Read failed");
    }

    vTaskDelay(pdMS_TO_TICKS(SENSOR_UPDATE_INTERVAL_MS));
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// Arduino entry points
// ══════════════════════════════════════════════════════════════════════════════

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

  digitalWrite(LED_PIN, true); // LED off
  pinMode(BOOTSEL_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  delay(400);
  Serial.println("\n╔══════════════════════════════════════════╗");
  Serial.println("║  ESP32 WiFiMulti + InfluxDB v2  (PIO)  ║");
  Serial.println("╚══════════════════════════════════════════╝");

  Serial1.begin(115200, SERIAL_8N1, RX, TX);
  // Register WiFi networks
  for (const auto &n : WIFI_NETWORKS) {
    g_wifiMulti.addAP(n.ssid, n.pass);
    LOG("Setup", "Registered SSID: %s", n.ssid);
  }

  // Initial connect (blocking)
  LOG("Setup", "Connecting…");
  const uint32_t t0 = millis();
  while (g_wifiMulti.run(WIFI_CONNECT_TIMEOUT_MS) != WL_CONNECTED) {
    if (millis() - t0 > WIFI_CONNECT_TIMEOUT_MS) {
      LOG("Setup", "Initial connect failed — continuing offline");
      break;
    }
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    LOG("Setup", "Connected → ssid=%s  ip=%s", WiFi.SSID().c_str(),
        WiFi.localIP().toString().c_str());
  }

  // ── InfluxDB: verify connectivity & server version ─────────────────────
  // setInsecure() skips cert verification for self-signed / plain HTTP.
  // Remove this line (and keep INFLUXDB_VERIFY_TLS 1) for InfluxDB Cloud.
#if !INFLUXDB_VERIFY_TLS
  g_influx.setInsecure();
#endif

  if (WiFi.status() == WL_CONNECTED) {
    if (g_influx.validateConnection()) {
      LOG("Influx", "Connected → server: %s", g_influx.getServerUrl().c_str());
    } else {
      LOG("Influx", "Validation FAILED: %s",
          g_influx.getLastErrorMessage().c_str());
      // Non-fatal: sensor task will keep retrying on each write.
    }
  }

  // Shared mutex
  g_sensorMutex = xSemaphoreCreateMutex();
  configASSERT(g_sensorMutex != nullptr);

  // Spawn tasks
  BaseType_t rc;

  rc =
      xTaskCreatePinnedToCore(taskWiFiMonitor, "WiFiMonitor", TASK_STACK_WIFI,
                              nullptr, TASK_PRIO_WIFI, nullptr, TASK_CORE_WIFI);
  configASSERT(rc == pdPASS);

  rc = xTaskCreatePinnedToCore(taskSensorUpdate, "SensorUpdate",
                               TASK_STACK_SENSOR, nullptr, TASK_PRIO_SENSOR,
                               nullptr, TASK_CORE_SENSOR);
  configASSERT(rc == pdPASS);

  LOG("Setup", "Tasks launched — idle loop running");
}

void loop() {
  // All real work is in the FreeRTOS tasks.
  // Add OTA, serial CLI, or watchdog feed here if needed.
  vTaskDelay(pdMS_TO_TICKS(5000));

  if (xSemaphoreTake(g_sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (g_sensor.valid) {
      LOG("Loop", "Depth=%d%%  age=%ums",
          g_sensor.distance_mm, millis() - g_sensor.lastUpdateMs);
    }
    xSemaphoreGive(g_sensorMutex);
  }
}

/**
 * main.cpp — ESP32 WiFiMulti + Parallel Sensor → InfluxDB v2 (PlatformIO)
 * ─────────────────────────────────────────────────────────────────────────
 * Core 0 │ taskWiFiMonitor  — periodic WiFi health-check & reconnect
 * Core 0 │ taskLed          — blinks LED to reflect system state
 * Core 1 │ taskSensorUpdate — sensor sampling + InfluxDB line-protocol POST
 *
 * LED patterns (all defined in ledPatterns[]):
 *   BOOT        — rapid flutter while setup() runs
 *   CONNECTING  — slow single blink: waiting for WiFi
 *   CONNECTED   — brief heartbeat every 2 s: WiFi up, publishing OK
 *   PUBLISH_ERR — double-blink: InfluxDB write failed
 *   SENSOR_ERR  — triple-blink: sensor read failed
 *   WIFI_ERR    — SOS-like fast triple-blink: WiFi lost and not reconnected
 *
 * All user settings live in include/config.h
 * Library:  tobiasschuerg/ESP8266 Influxdb  (works on ESP32)
 */

#include <Arduino.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h> // only needed for INFLUXDB_V2_CLOUD_URL constant
#include <OneBitDisplay.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <Wire.h>
#include <atomic>
//  Displays sensor values on the built-in 72x40 OLED
//
#include "config.h"
#include "secrets.h"

ONE_BIT_DISPLAY obd;
// ── Logging ────────────────────────────────────────────────────────────────
// esp_log is task-safe and timestamp-aware — output is never dropped or
// interleaved when called from multiple FreeRTOS tasks simultaneously.
// Verbosity is controlled by CORE_DEBUG_LEVEL in platformio.ini.
#include "esp_log.h"
#define LOG(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
//  Serial.printf("[%7lu][%-7s] " fmt "\n", millis(), tag, ##__VA_ARGS__)

// ══════════════════════════════════════════════════════════════════════════════
// LED pattern engine
// ══════════════════════════════════════════════════════════════════════════════

/** System states — set by any task, consumed by taskLed. */
enum class LedState : uint8_t {
  BOOT,        ///< setup() in progress
  CONNECTING,  ///< waiting for initial / reconnect WiFi
  CONNECTED,   ///< WiFi up + last publish OK  (normal heartbeat)
  PUBLISH_ERR, ///< InfluxDB write failed
  SENSOR_ERR,  ///< sensor read returned false
  WIFI_ERR,    ///< WiFi lost, reconnect timed out
};

/** One blink step: on/off duration in ms. {0,0} terminates a sequence. */
struct BlinkStep {
  uint16_t onMs;
  uint16_t offMs;
};

/** Full pattern: repeated sequence + idle gap after the last step. */
struct LedPattern {
  LedState state;
  const BlinkStep steps[8]; // max 7 steps + terminator {0,0}
  uint16_t gapMs;           ///< pause after the whole sequence before repeating
};

// ── Led Pattern table
// ─────────────────────────────────────────────────────────────
static const LedPattern LED_PATTERNS[] = {
    {
        LedState::BOOT,
        {{50, 50}, {50, 50}, {50, 50}, {50, 50}, {0, 0}},
        0 // no gap — flutters continuously
    },
    {LedState::CONNECTING,
     {{200, 1000}, {0, 0}}, // slow single blink every 1.2 s
     0},
    {
        LedState::CONNECTED,
        {{30, 100}, {30, 0}, {0, 0}}, // quick double-tap heartbeat
        2000                          // then 2 s dark
    },
    {LedState::PUBLISH_ERR,
     {{100, 100}, {100, 0}, {0, 0}}, // double-blink
     800},
    {LedState::SENSOR_ERR,
     {{100, 100}, {100, 100}, {100, 0}, {0, 0}}, // triple-blink
     800},
    {LedState::WIFI_ERR,
     // Fast triple-blink (SOS-style)
     {{80, 80}, {80, 80}, {80, 0}, {0, 0}},
     400},
};

/** Shared state: written by WiFi/sensor tasks, read by taskLed. */
static std::atomic<LedState> g_ledState{LedState::BOOT};

/** Find the pattern entry for a given state (falls back to BOOT). */
static const LedPattern &findPattern(LedState s) {
  for (const auto &p : LED_PATTERNS) {
    if (p.state == s)
      return p;
  }
  return LED_PATTERNS[0];
}

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
  //
  //  BME280:
  //    temp = bme.readTemperature();
  //    hum  = bme.readHumidity();
  //    return true;
  //
  // ─────────────────────────────────────────────────────────────────────
  // int dist;
  unsigned long timeout;
  // Flush any existing data
  while (Serial1.available() > 0) {
    Serial1.read();
  }
  Serial1.write(COM);
  delay(8); // give sensor time to respond
  timeout = millis() + 50;
  while (Serial1.available() < 1) {
    if (millis() > timeout) {
      LOG("readSensor()", "1 Timeout ");
      return false;
    }
  }
  // M5.Log.printf("First Wait: %d\n", 500 - (timeout - millis()));
  LOG("readSensor()", "First Wait: %d\n", 20 - (timeout - millis()));

  // Flush until we find the 0xFF header
  timeout = millis() + 100;
  while (Serial1.available()) {
    if (Serial1.peek() == 0xFF)
      break;
    if (millis() > timeout) {
      LOG("readSensor()", "2 Timeout ");
      return false;
    }
    Serial1.read(); // discard non-header bytes
  }

  // Wait for full 4-byte frame
  timeout = millis() + 50;
  while (Serial1.available() < 4) {
    if (millis() > timeout) {
      LOG("readSensor()", "3 Timeout ");
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
  if (checksum != buf[3]) {
    LOG("readSensor()", "CheckSum Error");
    return false;
  }

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
    g_ledState.store(LedState::PUBLISH_ERR);
  } else {
    LOG("Influx", "Write OK  (HTTP %d)", g_influx.getLastStatusCode());
    g_ledState.store(LedState::CONNECTED);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// FreeRTOS Tasks
// ══════════════════════════════════════════════════════════════════════════════

/**
 * @brief  LED blink task — Core 0.
 *         Reads g_ledState and plays the matching pattern on LED_PIN.
 *         Never blocks other tasks — uses vTaskDelay for all timing.
 */
static void taskLed(void * /*pvParams*/) {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, !LED_ON); // start off
  LOG("LED", "Task started (Core %d)", xPortGetCoreID());

  for (;;) {
    const LedPattern &pat = findPattern(g_ledState.load());

    // Play each blink step
    for (uint8_t i = 0; pat.steps[i].onMs != 0; ++i) {
      digitalWrite(LED_PIN, LED_ON);
      vTaskDelay(pdMS_TO_TICKS(pat.steps[i].onMs));
      digitalWrite(LED_PIN, !LED_ON);
      if (pat.steps[i].offMs > 0)
        vTaskDelay(pdMS_TO_TICKS(pat.steps[i].offMs));
    }

    // Gap after the full sequence
    if (pat.gapMs > 0)
      vTaskDelay(pdMS_TO_TICKS(pat.gapMs));
  }
}

static void taskWiFiMonitor(void * /*pvParams*/) {
  LOG("WiFi", "Monitor task started (Core %d)", xPortGetCoreID());

  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      LOG("WiFi", "OK  ssid=%-20s  rssi=%ddBm  ip=%s", WiFi.SSID().c_str(),
          WiFi.RSSI(), WiFi.localIP().toString().c_str());
    } else {
      LOG("WiFi", "Disconnected — reconnecting…");
      g_ledState.store(LedState::CONNECTING);

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
        g_ledState.store(LedState::CONNECTED);
      } else {
        LOG("WiFi", "Reconnect timed out — retry in %us",
            WIFI_CHECK_INTERVAL_MS / 1000);
        g_ledState.store(LedState::WIFI_ERR);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(WIFI_CHECK_INTERVAL_MS));
  }
}

static void taskSensorUpdate(void * /*pvParams*/) {
  LOG("Sensor", "Update task started (Core %d)", xPortGetCoreID());

  for (;;) {
    float t = 0.0f, h = 0.0f;
    int d = 0;

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
      g_ledState.store(LedState::SENSOR_ERR);
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
  // pinMode(LED_PIN, OUTPUT);
  pinMode(20, INPUT); // RX IN?

  // digitalWrite(LED_PIN, true); // LED off
  pinMode(BOOTSEL_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  delay(400);
  ESP_LOGI("Setup", "ESP32 WiFiMulti + InfluxDB v2  (PIO)");

  Serial1.begin(115200, SERIAL_8N1, RX, TX);
  // Register WiFi networks
  for (const auto &n : WIFI_NETWORKS) {
    g_wifiMulti.addAP(n.ssid, n.pass);
    LOG("Setup", "Registered SSID: %s", n.ssid);
  }

  // LED task is spawned first so the BOOT flutter is visible immediately
  BaseType_t rc =
      xTaskCreatePinnedToCore(taskLed, "LED", TASK_STACK_LED, nullptr,
                              TASK_PRIO_LED, nullptr, TASK_CORE_LED);
  configASSERT(rc == pdPASS);

  // Initial connect (blocking)
  g_ledState.store(LedState::CONNECTING);
  LOG("Setup", "Connecting…");
  const uint32_t t0 = millis();
  while (g_wifiMulti.run(WIFI_CONNECT_TIMEOUT_MS) != WL_CONNECTED) {
    if (millis() - t0 > WIFI_CONNECT_TIMEOUT_MS) {
      LOG("Setup", "Initial connect failed — continuing offline");
      g_ledState.store(LedState::WIFI_ERR);
      break;
    }
    delay(500);
    // Serial.print('.');
  }
  // Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    LOG("Setup", "Connected → ssid=%s  ip=%s", WiFi.SSID().c_str(),
        WiFi.localIP().toString().c_str());
    g_ledState.store(LedState::CONNECTED);
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
  // BaseType_t rc;

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
      LOG("Loop", "Depth=%d mm  age=%ums", g_sensor.distance_mm,
          millis() - g_sensor.lastUpdateMs);
    }
    xSemaphoreGive(g_sensorMutex);
  }
}

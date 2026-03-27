#pragma once

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  config.h — all user-facing settings in one place                       ║
// ╚══════════════════════════════════════════════════════════════════════════╝

// ── WiFi credentials ─────────────────────────────────────────────────────────
// Add / remove rows freely; WiFiMulti picks the strongest available AP.
/*
 * moved to secrets.h
struct WifiCredential {
  const char *ssid;
  const char *pass;
};

inline constexpr WifiCredential WIFI_NETWORKS[] = {
    {"*****", "****"},
    {"*****", "****"},
};
*/
// ── Timing
// ────────────────────────────────────────────────────────────────────
static constexpr uint32_t WIFI_CHECK_INTERVAL_MS =
    10000; ///< WiFi watchdog period
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS =
    15000; ///< Max time to (re)connect
static constexpr uint32_t SENSOR_UPDATE_INTERVAL_MS =
    5000; ///< Sensor polling period

// ── FreeRTOS task settings
// ────────────────────────────────────────────────────
static constexpr uint32_t TASK_STACK_WIFI = 4096;
static constexpr uint32_t TASK_STACK_SENSOR =
    10240; ///< InfluxDB TLS+HTTP needs ~8 KB
static constexpr UBaseType_t TASK_PRIO_WIFI = 1;
static constexpr UBaseType_t TASK_PRIO_SENSOR =
    2; ///< higher → sensor never starves
static constexpr BaseType_t TASK_CORE_WIFI = 0;
static constexpr BaseType_t TASK_CORE_SENSOR = 1;

// ── FreeRTOS task settings (LED) ─────────────────────────────────────────────
static constexpr uint32_t TASK_STACK_LED = 2048;
static constexpr UBaseType_t TASK_PRIO_LED = 1; // same as WiFi watchdog
static constexpr BaseType_t TASK_CORE_LED = 0;  // share Core 0 with WiFi

// ── Hardware
// ────────────────────────────────────────────────────────────────── Uncomment
// / adjust for your wiring #define SENSOR_PIN       4     // e.g. DHT data pin
// #define SENSOR_TYPE      DHT22
// Built-in LED on most ESP32 DevKit boards is GPIO 2.
// Change to match your board or an external LED pin.
#define LED_ON LOW // HIGH // flip to LOW if your LED is active-low
// Need to explicitly set the I2C pin numbers on this C3 board
#define SDA_PIN 5
#define SCL_PIN 6
#define LED_PIN 8
#define BOOTSEL_PIN 9
#define COM 0x55

// ── InfluxDB v2
// ─────────────────────────────────────────────────────────────── Cloud:
// INFLUXDB_URL = "https://us-east-1-1.aws.cloud2.influxdata.com" Self-hosted:
// INFLUXDB_URL = "http://192.168.1.100:8086"

// Organisation name (shown in InfluxDB UI → left sidebar)
// #define INFLUXDB_ORG ""

// Bucket to write into
// #define INFLUXDB_BUCKET ""

// API token  (InfluxDB UI → Data → API Tokens → Generate)
// ⚠  Keep this secret — never commit to version control.
//    Better practice: move to a secrets.h that is .gitignore'd.

//#define INFLUXDB_TOKEN                                                         \

// Measurement name (table name in InfluxDB)
#define INFLUXDB_MEASUREMENT "sonar"

// Tag added to every point — useful to distinguish multiple ESP32 nodes
#define INFLUXDB_DEVICE_TAG "esp32-node-1"

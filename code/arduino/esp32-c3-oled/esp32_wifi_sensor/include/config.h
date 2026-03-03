#pragma once

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  config.h — all user-facing settings in one place                       ║
// ╚══════════════════════════════════════════════════════════════════════════╝

// ── WiFi credentials ─────────────────────────────────────────────────────────
// Add / remove rows freely; WiFiMulti picks the strongest available AP.
struct WifiCredential {
  const char *ssid;
  const char *pass;
};

inline constexpr WifiCredential WIFI_NETWORKS[] = {
    {"****", "****"},
    {"****", "****"},
};

// ── Timing
// ────────────────────────────────────────────────────────────────────
static constexpr uint32_t WIFI_CHECK_INTERVAL_MS =
    10000; ///< WiFi watchdog period
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS =
    15000; ///< Max time to (re)connect
static constexpr uint32_t SENSOR_UPDATE_INTERVAL_MS =
    2000; ///< Sensor polling period

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

// ── Hardware
// ────────────────────────────────────────────────────────────────── Uncomment
// / adjust for your wiring #define SENSOR_PIN       4     // e.g. DHT data pin
// #define SENSOR_TYPE      DHT22

// ── InfluxDB v2
// ─────────────────────────────────────────────────────────────── Cloud:
// INFLUXDB_URL = "https://us-east-1-1.aws.cloud2.influxdata.com" Self-hosted:
// INFLUXDB_URL = "http://192.168.1.100:8086"
#define INFLUXDB_URL "http://192.168.1.100:8086"

// Organisation name (shown in InfluxDB UI → left sidebar)
#define INFLUXDB_ORG "*****"

// Bucket to write into
#define INFLUXDB_BUCKET "*****"

// API token  (InfluxDB UI → Data → API Tokens → Generate)
// ⚠  Keep this secret — never commit to version control.
//    Better practice: move to a secrets.h that is .gitignore'd.

#define INFLUXDB_TOKEN   "*****"                                                      \

// Measurement name (table name in InfluxDB)
#define INFLUXDB_MEASUREMENT "sonar"

// Tag added to every point — useful to distinguish multiple ESP32 nodes
#define INFLUXDB_DEVICE_TAG "esp32-node-1"

// Set to 1 to verify the server TLS certificate (recommended for Cloud).
// Set to 0 for self-hosted HTTP or when using a self-signed cert.
#define INFLUXDB_VERIFY_TLS 0

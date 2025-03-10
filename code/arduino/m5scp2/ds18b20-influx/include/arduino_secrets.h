#define NUM_SSID 2
const char ssids[][16] = // 16 is the length of the longest string + 1 ( for the '\0' at the end )
{
        "Vodafone-05C222",
        "ssid"
};

const char pass[][16] =
{
        "*****",
        "*****"
};

// kane584
#define INFLUXDB_URL "http://193.136.137.6:8086"
#define INFLUXDB_TOKEN """
#define INFLUXDB_ORG "7ffb1a7998038e38"
#define INFLUXDB_BUCKET "ardu-rasp"

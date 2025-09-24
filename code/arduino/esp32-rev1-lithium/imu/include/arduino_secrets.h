#define NUM_SSID 3
const char ssids[][16] = // 16 is the length of the longest string + 1 ( for the '\0' at the end )
{
        "Vodafone-05C222",
        "Vodafone-C850D2",
        "Labuino",
};

const char pass[][16] =
{
        "*****",
        "*****",
        "Arduino25",
};

// Kane
#define INFLUXDB_URL "http://193.136.137.6:8086"
#define INFLUXDB_TOKEN "******"
#define INFLUXDB_ORG "7ffb1a7998038e38"
#define INFLUXDB_BUCKET "ardu-rasp"

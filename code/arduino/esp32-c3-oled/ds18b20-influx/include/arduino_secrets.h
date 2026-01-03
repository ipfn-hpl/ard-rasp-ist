#define NUM_SSID 3
const char ssids[][20] = // 10 is the length of the longest string + 1 ( for the '\0' at the end )
{
    "Labuino",
    "IPFN_HPL",
    "Vodafone-437583",
};

const char pass[][20] = // 20 is the length of the longest string + 1 ( for the '\0' at the end )
{
    "Arduino25",
    "******",
    "******",
};
/*
#define INFLUXDB_URL "http://epics.ipfn.tecnico.ulisboa.pt:8086"
#define INFLUXDB_TOKEN "*************"
#define INFLUXDB_ORG "67c974ed52348dd6"
#define INFLUXDB_BUCKET "ardu-rasp"
*/
#define INFLUXDB_URL "http://ard-rasp5-ist.local:8086"

#define INFLUXDB_TOKEN "**********"
#define INFLUXDB_ORG "99ec3c8cc9cefdaf"
#define INFLUXDB_BUCKET "ardu-rasp"


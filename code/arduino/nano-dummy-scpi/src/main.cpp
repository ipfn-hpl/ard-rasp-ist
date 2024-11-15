/*
 *
 *
 */
#include <Arduino.h>

// Include the libraries we need
//#include <OneWire.h>
//#include <DallasTemperature.h>

#include "Vrekrer_scpi_parser.h"

SCPI_Parser my_instrument;

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 12

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)

// arrays to hold device addresses
//DeviceAddress insideThermometer, outsideThermometer;

uint8_t dsCount = 2;

// Assign address manually. The addresses below will need to be changed
// to valid device addresses on your bus. Device address can be retrieved
// by using either oneWire.search(deviceAddress) or individually via
// sensors.getAddress(deviceAddress, index)
// DeviceAddress insideThermometer = { 0x28, 0x1D, 0x39, 0x31, 0x2, 0x0, 0x0, 0xF0 };
// DeviceAddress outsideThermometer   = { 0x28, 0x3F, 0x1C, 0x31, 0x2, 0x0, 0x0, 0x2 };

const int ledPin = LED_BUILTIN;  // the number of the LED pin
const unsigned int samplingInterval = 500; // how often to run the main loop (in ms)

uint8_t analogPins[4] = {A0, A1, A2, A3};

/* timer variables */
unsigned long currentMillis;        // store the current value from millis()
unsigned long previousMillis;       // for comparison with currentMillis
int ledState = LOW;  // ledState used to set the LED

void Identify(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(F("IPFN,SCPI Thermo,#00," VREKRER_SCPI_VERSION));  
  //interface.println(F("Vrekrer,SCPI Dimmer,#00," VREKRER_SCPI_VERSION));
  //*IDN? Suggested return string should be in the following format:
  // "<vendor>,<model>,<serial number>,<firmware>"
}

void QueryAnalog_Input(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  //DC<index>?
  //Queries the Analog Input logic state of DC[index] inout
  //Return values are in 
  //Examples:

  //Get the numeric suffix/index (if any) from the commands
  String header = String(commands.Last());
  header.toUpperCase();
  int suffix = -1;
  sscanf(header.c_str(),"%*[DC]%u", &suffix);

  //If the suffix is valid, print the pin's logic state to the interface
  if ( (suffix >= 0) && (suffix < 4) ) {
      uint8_t ival = analogRead(analogPins[suffix]);
      float val = ival * 5.0 / 1024.0;

    interface.println(String(val, 3));
    }
}

void QueryDS18_Temp(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  //DIn<index>?
  //Queries the logic state of DS[index] sensor
  //Return values are in 
  //Examples:
  // DIn0?     (Queries the state of DOut[0] pin)
  // DI5?      (Queries the state of DOut[5] pin)

  //Get the numeric suffix/index (if any) from the commands
  String header = String(commands.Last());
  header.toUpperCase();
  int suffix = -1;
  sscanf(header.c_str(),"%*[DS]%u", &suffix);

      //If the suffix is valid, print the pin's logic state to the interface
  if ( (suffix >= 0) && (suffix < dsCount) ) {
	  float tempC =  20 + suffix * 10 + random(1000)/500.0;
	  //millis()/1000.0;
	  /*
	  if(tempC == DEVICE_DISCONNECTED_C) 
	  {
		  Serial.println("Error: Could not read temperature data");
		  return;
	  }
	  */
	  //Serial.print("Temp C: ");
	  // Serial.println(tempC);
	  interface.println(String(tempC, 4));
  }
}

void DoNothing(SCPI_C commands, SCPI_P parameters, Stream& interface) {
}

void DoSpe(SCPI_C commands, Stream& interface) {
}



void setup(void)
{
    // start serial port
  pinMode(ledPin, OUTPUT);
  // start serial port
  Serial.begin(57600);
  Serial.println("Dallas Temperature IC Control Library Demo");

  // Start up the library
  //sensors.begin();

  // locate devices on the bus
  Serial.print("Locating devices...");
  Serial.print("Found ");
  //dsCount = 0;
  //sensors.getDeviceCount();
  Serial.print(dsCount, DEC);
  Serial.println(" devices.");

  // report parasite power requirements

  // Search for devices on the bus and assign based on an index. Ideally,
  // you would do this to initially discover addresses on the bus and then
  // use those addresses and manually assign them (see above) once you know
  // the devices on your bus (and assuming they don't change).
  //
  // method 1: by index
  //if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0");
  //if (!sensors.getAddress(outsideThermometer, 1)) Serial.println("Unable to find address for Device 1");

  /*
  for (uint8_t i=0; i < dsCount; i++){
    if (!sensors.getAddress(ds18Sensors[i], i)) { 
      Serial.print("Unable to find address for ds18Sensor ");
      Serial.println(i,DEC);
      }
      else{
        Serial.print("Device ");
        Serial.print(i,DEC);
        Serial.print(" Address: ");
        printAddress(ds18Sensors[i]);
        sensors.setResolution(ds18Sensors[i], TEMPERATURE_PRECISION);
        Serial.print(", Resolution: ");
        Serial.print(sensors.getResolution(ds18Sensors[i]), DEC);
        Serial.println();
      }
  } 
  */
  // method 2: search()
  // search() looks for the next device. Returns 1 if a new address has been
  // returned. A zero might mean that the bus is shorted, there are no devices,
  // or you have already retrieved all of them. It might be a good idea to
  // check the CRC to make sure you didn't get garbage. The order is
  // deterministic. You will always get the same devices in the same order
  //
  // Must be called before search()
  //oneWire.reset_search();
  // assigns the first address found to insideThermometer
  //if (!oneWire.search(insideThermometer)) Serial.println("Unable to find address for insideThermometer");
  // assigns the seconds address found to outsideThermometer
  //if (!oneWire.search(outsideThermometer)) Serial.println("Unable to find address for outsideThermometer");

  /* / show the addresses we found on the bus
  Serial.print("Device 0 Address: ");
  printAddress(insideThermometer);
  Serial.println();

  Serial.print("Device 1 Address: ");
  printAddress(outsideThermometer);
  Serial.println();

  // set the resolution to 9 bit per device
  sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);
  sensors.setResolution(outsideThermometer, TEMPERATURE_PRECISION);

  Serial.print("Device 0 Resolution: ");
  Serial.print(sensors.getResolution(insideThermometer), DEC);
  Serial.println();

  Serial.print("Device 1 Resolution: ");
  Serial.print(sensors.getResolution(outsideThermometer), DEC);
  Serial.println();
  */
    /*
  To fix hash crashes, the hashing magic numbers can be changed before 
  registering commands.
  Use prime numbers up to the SCPI_HASH_TYPE size.
  */
  my_instrument.hash_magic_number = 37; //Default value = 37
  my_instrument.hash_magic_offset = 7;  //Default value = 7

  /*
  Timeout time can be changed even during program execution
  See Error_Handling example for further details.
  */
  my_instrument.timeout = 10; //value in miliseconds. Default value = 10
  my_instrument.RegisterCommand(F("*IDN?"), &Identify);
  /*
  Vrekrer_scpi_parser library.
Configuration options example.
  */
  my_instrument.SetCommandTreeBase(F("STATus"));
    my_instrument.RegisterCommand(F(":OPERation?"), &DoNothing);
    my_instrument.RegisterCommand(F(":QUEStionable?"), &DoNothing);
    my_instrument.RegisterCommand(F(":PRESet"), &DoNothing);  
  my_instrument.SetCommandTreeBase(F("SYSTem"));
    my_instrument.RegisterCommand(F(":ERRor?"), &DoNothing);
    my_instrument.RegisterCommand(F(":ERRor:NEXT?"), &DoNothing);
    my_instrument.RegisterCommand(F(":VERSion?"), &DoNothing);
  my_instrument.SetCommandTreeBase(F("MEASure:SCALar:VOLTage"));
  my_instrument.RegisterCommand(F(":DC#?"), &QueryAnalog_Input);
  my_instrument.SetCommandTreeBase(F(""));
  my_instrument.SetCommandTreeBase(F("MEASure:SCALar:TEMPerature"));
  //Use "#" at the end of a token to accept numeric suffixes.
  my_instrument.RegisterCommand(F("DS#?"), &QueryDS18_Temp);
}


/*
   Main function, calls the temperatures in a loop.
*/
void loop(void)
{
    currentMillis = millis();
    my_instrument.ProcessInput(Serial, "\n");

    if (currentMillis - previousMillis > samplingInterval) {
        previousMillis += samplingInterval;
    // call sensors.requestTemperatures() to issue a global temperature
    // request to all devices on the bus
    //Serial.print("Requesting temperatures...");
    //sensors.requestTemperatures();

    // print the device information
    //printData(insideThermometer);
    //printData(outsideThermometer);
  }
}

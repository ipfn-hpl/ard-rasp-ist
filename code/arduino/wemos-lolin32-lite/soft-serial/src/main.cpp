// On ESP8266:
// At 80MHz runs up 57600bps, and at 160MHz CPU frequency up to 115200bps with
// only negligible errors. Connect pin 13 to 15. For verification and as a
// example for how to use SW serial on the USB to PC connection, which allows
// the use of HW Serial on GPIO13 and GPIO15 instead, #define SWAPSERIAL below.
// Notice how the bitrates are also swapped then between RX/TX and
// GPIO13/GPIO15. Builtin debug output etc. must be stopped on HW Serial in this
// case, as it would interfere with the external communication on GPIO13/GPIO15.
//
// https://www.hackster.io/iyawat-kongmalai/roverc-with-m5atom-396c24
// https://mischianti.org/esp32-wemos-lolin32-lite-high-resolution-pinout-and-specs/
// socat -u -x /dev/ttyUSB0,raw -
//
#include <Arduino.h>
// #include <SoftwareSerial.h>

#ifndef D5
#if defined(ESP32)
#define D8 (5)
#define D5 (18)
#define D7 (23)
#define D6 (19)
#define RX (3)
#define TX (1)
#endif
#endif

#define MYPORT_TX 16          // GPIO 16
#define MYPORT_RX 17          // GPIO 17
#define SOFT_BAUD_RATE 115200 // 230400
// #define SOFT_BAUD_RATE  9600  // 115200
/*
 *115200
230400				    Ok.
460800				    Ok.
500000				    Ok.
576000				    Ok.
921600				    Ok.
*/
auto &usbSerial = Serial;
// EspSoftwareSerial::UART softSerial;

uint64_t time_in_us(struct timeval *tv) {
  return (uint64_t)tv->tv_sec * 1000000L + (uint64_t)tv->tv_usec;
}

// int16_t a[3];
char a[4];
#define PERIOD (1000000.0 * 11.1)
#define AMP 1500

bool ledStatus = false;

void setup() {
  // #ifndef SWAPSERIAL
  pinMode(LED_BUILTIN, OUTPUT);

  usbSerial.begin(115200);
  Serial2.begin(921600, SERIAL_8O2, MYPORT_RX, MYPORT_TX);
  // Important: the buffer size optimizations here, in particular the isrBufSize
  // (11) that is only sufficiently large to hold a single word (up to start - 8
  // data - parity - stop), are on the basis that any char written to the
  // loopback EspSoftwareSerial adapter gets read before another write is
  // performed. Block writes with a size greater than 1 would usually fail. Do
  // not copy this into your own project without reading the documentation.
  // Serial2.begin(115200, SERIAL_8N1, 32, 33); // Grove
  // .begin(38400, SWSERIAL_8N1, MYPORT_RX, MYPORT_TX, false);
  // invert true: uses invert line level logic
  // The octet buffer capacity (bufCapacity) is 95 (93 characters net plus two
  // tolerance). The signal edge detection buffer capacity (isrBufCapacity) is
  // 11,
  // testSerial.begin(BAUD_RATE, EspSoftwareSerial::SWSERIAL_8N1, 32, 33, false,
  // 95, 11); softSerial.begin(SOFT_BAUD_RATE, EspSoftwareSerial::SWSERIAL_8S1,
  // MYPORT_RX, MYPORT_TX, false); //, 95, 11);
  // 8Bit/Odd, Parity,Two stop bit
  /*
  softSerial.begin(SOFT_BAUD_RATE, EspSoftwareSerial::SWSERIAL_8O2, MYPORT_RX,
                   MYPORT_TX, false); //, 95, 11);
  if (!softSerial) { // If the object did not initialize, then its configuration
                     // is invalid
    Serial.println("Invalid EspSoftwareSerial pin configuration, check config");
  }
  // testSerial.begin(BAUD_RATE, EspSoftwareSerial::SWSERIAL_8N1, D7, D8, false,
  // 95, 11); #ifdef HALFDUPLEX
  softSerial.enableIntTx(false);
*/
  usbSerial.println(PSTR("\nSoftware serial test started"));
  /*
      for (char ch = ' '; ch <= 'z'; ch++) {
          softSerial.write(ch);
      }
      */
  // softSerial.println();
  // softSerial.println(PSTR("\nSoftware serial test started"));
}

void serialLoop() {

  static int64_t next_usec = 0;
  static int64_t marte_next_usec = 0;
  char *buffer = (char *)&a[0];
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  int64_t time_us = time_in_us(&tv_now);
  if (time_us > next_usec) { // && point2Save < MAX_POINTS) {
    next_usec = time_us + (10000 * 1000);
    usbSerial.print(time_us);
    usbSerial.println(PSTR(", serial Loop"));
    digitalWrite(LED_BUILTIN, ledStatus);
    ledStatus = not ledStatus;
    // a[0]++;
    // a[1]++;
    // a[1]++;
  }
  if (time_us > marte_next_usec) {
    // echo -ne "\xFE\xFF" >/
    a[0] = 0x55;
    a[1] = 0x55;
    a[2] = 0x55;
    a[3] = 0x55;
    // a[1]++;
    // a[1]++;
    // for (int i = 0; i < 3; i++) {
    //  a[i] = AMP * sin(time_us / PERIOD);
    //}
    marte_next_usec = time_us + (100 * 1000);
    Serial2.write(buffer, 4);
  }
}

void loop() {

  uint8_t byte;
  serialLoop();
  /*
 while (softSerial.available() > 0) {
   int val = softSerial.read();
   // softSerial.read(&byte, 1);
   int par = (softSerial.readParity())
                 ? 1
                 : 0; // result of readParity() always applies to the preceding
                      // read() or peek() call

   usbSerial.printf("Byte: 0x%X, Par:%d\n", val, par);
   // The yield() function in Arduino is used primarily in ESP8266 and ESP32
   // environments to allow the microcontroller to perform background tasks,
   // such as managing WiFi connections, while waiting in a loop. It helps
   // prevent the watchdog timer from resetting the device by allowing other
   // processes to run during long operations.

   yield();
 }
 while (usbSerial.available() > 0) {
   // softSerial.write(usbSerial.read());
   // softSerial.write(
   usbSerial.read();
   yield();
 }
 */
}

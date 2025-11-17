/**
 * @file GPS.ino
 * @author SeanKwok (shaoxiang@m5stack.com)
 * @brief M5AtomS3 Atomic GPS Base Test
 * @version 0.1
 * @date 2023-12-13
 *
 *
 * @Hardwares: M5AtomS3 + Atomic GPS Base
 * @Platform Version: Arduino M5Stack Board Manager v2.0.9
 * @Dependent Library:
 * M5GFX: https://github.com/m5stack/M5GFX
 * M5Unified: https://github.com/m5stack/M5Unified
 * M5AtomS3: https://github.com/m5stack/M5AtomS3
 * TinyGPSPlus: https://github.com/mikalhart/TinyGPSPlus
 *
 * https://github.com/m5stack/M5AtomS3/tree/main/examples/AtomicBase/AtomicGPS/GPS
 * TC found.
GPS data received: check wiring
GPS data received: check wiring
**** ***** ********** *********** **** ********** ******** **** ****** ******
***** ***   ******** ****** ***   1     0         0 3    3.2   38.737282
-9.138523   781  ********** 18:34:41 **** 0.00   ****** ***** *** 1583     23.34
NNE   407   1         1 3    3.2   38.737282  -9.138525   783  11/05/2025
18:34:42 917  0.00   8.70   0.00  N     1583     23.34  NNE   814   3         1
3    3.2   38.737282  -9.138527   786  11/05/2025 18:34:43 921  0.00   8.70 0.00
N     1583     23.34  NNE   1217  5         1 3    3.2   38.737282  -9.138528
784  11/05/2025 18:34:44 918  0.00   8.70   0.00  N     1583     23.34  NNE 1622
7         1 3    3.2   38.737282  -9.138528   783  11/05/2025 18:34:45 917 0.00
8.70   0.00  N     1583     23.34  NNE   2031  9         1 3    3.2   38.737282
-9.138530   792  11/05/2025 18:34:46 926  0.00   8.70   0.00  N 1583     23.34
NNE   2432  11        1 3    3.2   38.737278  -9.138532   784  11/05/2025
18:34:47 918  0.00   8.70   0.00  N     1583     23.34  NNE   2839  13        1
3    3.2   38.737278  -9.138532   788  11/05/2025 18:34:48 922  0.00   8.70 0.00
N     1583     23.34  NNE   3247  15        1 3    3.2   38.737274  -9.138532
794  11/05/2025 18:34:49 928  0.00   8.70   0.00  N     1583     23.34  NNE 3653
17        1 3    3.2   38.737274  -9.138532   795  11/05/2025 18:34:50 929 0.00
8.70   0.00  N     1583     23.34  NNE   4056  19        1 3    3.2   38.737270
-9.138534   791  11/05/2025 18:34:51 925  0.00   8.70   0.00  N 1583     23.34
NNE   4460  21        1 3    3.2   38.737267  -9.138534   789  11/05/2025
18:34:52 923  0.00   8.70   0.00  N     1583     23.34  NNE   4867  23        1
3    3.2   38.737267  -9.138534   794  11/05/2025 18:34:53 928  0.00   8.70 0.00
N     1583     23.34  NNE   5272  25        1 3    3.2   38.737263  -9.138535
793  11/05/2025 18:34:54 927  0.00   8.70   0.00  N     1583     23.34  NNE 5673
27        1 3    3.2   38.737259  -9.138536   785  11/05/2025 18:34:55 919 0.00
8.70   0.00  N     1583     23.34  NNE   6082  29        1 3    3.2   38.737259
-9.138536   793  11/05/2025 18:34:56 927  0.00   8.70   0.00  N 1583     23.34
NNE   6486  31        1 3    3.2   38.737255  -9.138536   791  11/05/2025
18:34:57 925  0.00   8.70   0.00  N     1583     23.34  NNE   6892  33        1
3    3.2   38.737255  -9.138538   794  11/05/2025 18:34:58 928  0.00   8.70 0.00
N     1583     23.34  NNE   7298  35        1 3    3.2   38.737255  -9.138540
795  11/05/2025 18:34:59 929  0.00   8.70   0.00  N     1583     23.34  NNE 7703
37        1 3    3.2   38.737255  -9.138543   796  11/05/2025 18:35:00 930 0.00
8.70   0.00  N     1583     23.34  NNE   8110  39        1 3    3.2   38.737255
-9.138545   800  11/05/2025 18:35:01 934  0.00   8.70   0.00  N 1583     23.34
NNE   8514  41        1 3    3.2   38.737255  -9.138550   799  11/05/2025
18:35:02 933  0.00   8.70   0.00  N     1583     23.34  NNE   8922  43        1
3    3.2   38.737255  -9.138552   804  11/05/2025 18:35:03 938  0.00   8.70 0.00
N     1583     23.34  NNE   9326  45        1 3    3.2   38.737255  -9.138555
801  11/05/2025 18:35:04 935  0.00   8.70   0.00  N     1583     23.34  NNE 9734
47        1 3    3.2   38.737259  -9.138556   808  11/05/2025 18:35:05 942 0.00
8.70   0.00  N     1583     23.34  NNE   10137 49        1
 */

// #include "M5AtomS3.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <TinyGPSPlus.h>

// The TinyGPSPlus object
TinyGPSPlus gps;

// This custom version of delay() ensures that the gps object
// is being "fed".
static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (Serial2.available())
      gps.encode(Serial2.read());
  } while (millis() - start < ms);
}

static void printFloat(float val, bool valid, int len, int prec) {
  if (!valid) {
    while (len-- > 1)
      Serial.print('*');
    Serial.print(' ');
  } else {
    Serial.print(val, prec);
    int vi = abs((int)val);
    int flen = prec + (val < 0.0 ? 2 : 1); // . and -
    flen += vi >= 1000 ? 4 : vi >= 100 ? 3 : vi >= 10 ? 2 : 1;
    for (int i = flen; i < len; ++i)
      Serial.print(' ');
  }
  smartDelay(0);
}

static void printInt(unsigned long val, bool valid, int len) {
  char sz[32] = "*****************";
  if (valid)
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i = strlen(sz); i < len; ++i)
    sz[i] = ' ';
  if (len > 0)
    sz[len - 1] = ' ';
  Serial.print(sz);
  smartDelay(0);
}

static void printDateTime(TinyGPSDate &d, TinyGPSTime &t) {
  if (!d.isValid()) {
    Serial.print(F("********** "));
  } else {
    char sz[32];
    sprintf(sz, "%02d/%02d/%02d ", d.month(), d.day(), d.year());
    Serial.print(sz);
  }

  if (!t.isValid()) {
    Serial.print(F("******** "));
  } else {
    char sz[32];
    sprintf(sz, "%02d:%02d:%02d ", t.hour(), t.minute(), t.second());
    Serial.print(sz);
  }

  printInt(d.age(), d.isValid(), 5);
  smartDelay(0);
}

static void printStr(const char *str, int len) {
  int slen = strlen(str);
  for (int i = 0; i < len; ++i)
    Serial.print(i < slen ? str[i] : ' ');
  smartDelay(0);
}

void setup() {
  delay(2000);
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  // Serial.begin(115200);
  // delay(1000);
  // AtomS3.begin(cfg);

  M5.Display.setTextColor(GREEN);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::FreeSerifBold24pt7b);
  M5.Display.setTextSize(1);
  M5.Display.drawString("GPS", M5.Display.width() / 2, M5.Display.height() / 2);

  M5.Log.println("RTC found.");
  // Serial.println(F("GPS data received: check wiring"));
  //  Serial2.begin(4800, SERIAL_8N1, 5, -1);
  //  https://github.com/m5stack/M5-ProductExampleCodes/blob/master/AtomBase/AtomicRS232/Arduino/AtomicRS232/AtomicRS232.ino
  //    2363][E][esp32-hal-uart.c:285] uartSetPins(): UART2 set pins GPS data
  //    received: check wiring

  // Serial2.begin(4800, SERIAL_8N1, 22, 19);
  Serial2.begin(4800, SERIAL_8N1, 5, 6);
  Serial.println(F("Serial 2 connected"));
}

void loop() {
  static const double LONDON_LAT = 51.508131, LONDON_LON = -0.128002;

  printInt(gps.satellites.value(), gps.satellites.isValid(), 5);
  printFloat(gps.hdop.hdop(), gps.hdop.isValid(), 6, 1);
  printFloat(gps.location.lat(), gps.location.isValid(), 11, 6);
  printFloat(gps.location.lng(), gps.location.isValid(), 12, 6);
  printInt(gps.location.age(), gps.location.isValid(), 5);
  printDateTime(gps.date, gps.time);
  printFloat(gps.altitude.meters(), gps.altitude.isValid(), 7, 2);
  printFloat(gps.course.deg(), gps.course.isValid(), 7, 2);
  printFloat(gps.speed.kmph(), gps.speed.isValid(), 6, 2);
  printStr(gps.course.isValid() ? TinyGPSPlus::cardinal(gps.course.deg())
                                : "*** ",
           6);

  unsigned long distanceKmToLondon =
      (unsigned long)TinyGPSPlus::distanceBetween(
          gps.location.lat(), gps.location.lng(), LONDON_LAT, LONDON_LON) /
      1000;
  printInt(distanceKmToLondon, gps.location.isValid(), 9);

  double courseToLondon = TinyGPSPlus::courseTo(
      gps.location.lat(), gps.location.lng(), LONDON_LAT, LONDON_LON);

  printFloat(courseToLondon, gps.location.isValid(), 7, 2);

  const char *cardinalToLondon = TinyGPSPlus::cardinal(courseToLondon);

  printStr(gps.location.isValid() ? cardinalToLondon : "*** ", 6);

  printInt(gps.charsProcessed(), true, 6);
  printInt(gps.sentencesWithFix(), true, 10);
  printInt(gps.failedChecksum(), true, 9);
  Serial.println();

  smartDelay(1000);

  if (millis() > 5000 && gps.charsProcessed() < 10)
    M5.Log.println("No GPS data received: check wiring");
}

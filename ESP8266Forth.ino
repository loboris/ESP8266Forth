/*
 * Yaffa Forth for the 32 bit ESP8266
 * Currently implemented on a NodeMCU Amica module from electrodragon.com
 *
 * ESP8266Forth incorporates Yaffa by Stuart Wood which has been ported
 * to 32 bits.
 *
 * This code allows interaction with the Forth interpreter via the USB Serial
 * interface or a network connection and allows code to be compiled and run
 * from files on an attached SD memory card. See IODirector.h for details.
 *
 * Connections between the NodeMCU Amica, SD card and LCD are as follows:
 * NodeMCU Card          SD Card        Optional AF 1.8" LCD
 * =========================================================
 *     3.3                 VCC                  VCC
 *     Gnd                 GND                  Gnd
 *     D1 (GPIO5)          CS
 *     D7 (MOSI)           SI                   MOSI
 *     D6 (MISO)           SO
 *     D5 (CLK)            SLK                  SCK
 *     D2 (GPIO4)                              TFT_CS
 *     D8 (GPIO15)                              LITE
 *     D4 (GPIO2)                               D/C
 *
 * A connection must also be made between the RST and D0
 * pins on the Amica to allow hardware reset via "restart" to work.
 *
 * HAS_LCD must be defined to use the LCD and the modified Adafruit
 * library from https://github.com/nzmichaelh/Adafruit-ST7735-Library
 * must be installed.
 *
 * Concept and Porting by: Craig A. Lindley
 * Version: 0.54
 * Last Update: 01/25/2015
 *
 * Copyright (c) 2015, 2016 Craig A. Lindley
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// #define HAS_LCD // Comment this out to remove LCD functionality


#define IS_ROBOT // Comment this out to remove robot functionality

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SD.h>
#include <SPI.h>

#ifdef HAS_LCD
#include "Adafruit_GFX.h"
#include "Adafruit_ST7735.h"
#endif

#include "IODirector.h"
#include "Yaffa.h"

// ***************************************************************
// Start of user configuration items
// ***************************************************************
const int SERIAL_BAUDRATE = 115200;

// WiFi login credentials
const char *WIFI_SSID = "CraigNet";
const char *WIFI_PASS = "craigandheather";

// Hardware Configuration
const int GPIO_SD_CS =  5; // SD card chip select line
const int GPIO_RESET = 16; // Hardware reset line. See note above.

#ifdef HAS_LCD
// Connections between NodeMCU module and Adafruit 1.8" LCD display
const int GPIO_TFT_CS   =  4;
const int GPIO_TFT_DC   =  2;
const int GPIO_TFT_LITE = 15;
#endif

#ifdef IS_ROBOT
// Connections between NodeMCU module and an L293D motor controller chip
const int GPIO_1A   =  4;
const int GPIO_2A   =  0;
const int GPIO_3A   = 15;
const int GPIO_4A   =  2;
#endif

// ***************************************************************
// End of user configuration items
// ***************************************************************

#ifdef HAS_LCD
// Create LCD driver instance
Adafruit_ST7735 lcd = Adafruit_ST7735(GPIO_TFT_CS, GPIO_TFT_DC);
#endif

// Create an I/O director for channeling I/O into and out of Forth
IODirector ioDirector;

// ***************************************************************
// Program Setup Function
// ***************************************************************

void setup(void) {

#ifdef IS_ROBOT
  // Set all four motor chip control lines to outputs
  pinMode(GPIO_1A, OUTPUT);
  pinMode(GPIO_2A, OUTPUT);
  pinMode(GPIO_3A, OUTPUT);
  pinMode(GPIO_4A, OUTPUT);

  // Setting them all low stops the robot
  digitalWrite(GPIO_1A, LOW);
  digitalWrite(GPIO_2A, LOW);
  digitalWrite(GPIO_3A, LOW);
  digitalWrite(GPIO_4A, LOW);
#endif

  // Initialize Serial interface
  delay(1000);
  Serial.begin(SERIAL_BAUDRATE);
  delay(1000);

#ifdef HAS_LCD
  // Turn off the LCD backlight
  pinMode(GPIO_TFT_LITE, OUTPUT);
  digitalWrite(GPIO_TFT_LITE, LOW);

  // Initialize the LCD (blacktab) display
  lcd.initR(INITR_BLACKTAB);

  // Set display orientation
  lcd.setRotation(3);
#endif

  // Initialize WiFi whether it is used or not
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  print_P(PSTR("\n\nConnecting to "));
  Serial.println(WIFI_SSID);
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 20) {
    delay(500);
    Serial.print(".");
  }
  if (i == 21) {
    print_P(PSTR("Could not connect to"));
    Serial.println(WIFI_SSID);
    while (true) {
      delay(500);
    }
  }
  print_P(PSTR("\nConnected to "));
  Serial.println(WIFI_SSID);

  // Start the Telnet server
  server.begin();
  server.setNoDelay(true);

  print_P(PSTR("\nESP8266 32 bit Forth for the NodeMCU Amica\n"));
  print_P(PSTR("Incorporates Yaffa by Stuart Wood\n"));
  print_P(PSTR("Copyright (C) 2015, 2016 Craig A. Lindley\n"));

  // See if SD card is present and working
  if (! SD.begin(GPIO_SD_CS)) {
    print_P(PSTR("\nSD memory card failed or is missing\n"));
    // We're done
    while (true) {
      delay(500);
    }
  }
  print_P(PSTR("\nSD memory card online\n"));

  // See if the autorun file exists
  // If so, load it at boot time
  if (SD.exists((char *) "autorun.frt")) {
    ioDirector.injectAutoRunFile();
  }

  // Initialize Yaffa
  yaffaInit();
}

// ***************************************************************
// Program Loop Function
// ***************************************************************

void loop() {
  // Run Yaffa interactively
  yaffaRun();
}


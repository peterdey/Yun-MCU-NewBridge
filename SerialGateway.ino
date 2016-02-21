  /*
 * Copyright (C) 2013 Henrik Ekblad <henrik.ekblad@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 * 
 * DESCRIPTION
 * The ArduinoGateway prints data received from sensors on the serial link. 
 * The gateway accepts input on seral which will be sent out on radio network.
 *
 * The GW code is designed for Arduino Nano 328p / 16MHz
 *
 * Wire connections (OPTIONAL):
 * - Inclusion button should be connected between digital pin 3 and GND
 * - RX/TX/ERR leds need to be connected between +5V (anode) and digital ping 6/5/4 with resistor 270-330R in a series
 *
 * LEDs (OPTIONAL):
 * - RX (green) - blink fast on radio message recieved. In inclusion mode will blink fast only on presentation recieved
 * - TX (yellow) - blink fast on radio message transmitted. In inclusion mode will blink slowly
 * - ERR (red) - fast blink on error during transmission error or recieve crc error  
 */

#include <SPI.h>  
#include <MySensor.h>  
#include <MyGateway.h>  
#include <stdarg.h>

#define INCLUSION_MODE_TIME 1 // Number of minutes inclusion mode is enabled
#define INCLUSION_MODE_PIN 3 // Digital pin used for inclusion mode button


MyGateway gw(DEFAULT_CE_PIN, DEFAULT_CS_PIN, INCLUSION_MODE_TIME, INCLUSION_MODE_PIN,  6, 5, 4);

char inputString[MAX_RECEIVE_LENGTH] = "";    // A string to hold incoming commands from serial/ethernet interface
int inputPos = 0;
boolean commandComplete = false;  // whether the string is complete

boolean consoleStatus = 0;
static const char CTRL_C = 3;

void setup()  
{ 
  bridgeBegin();
  gw.begin(RF24_PA_LEVEL_GW, RF24_CHANNEL, RF24_DATARATE, messageCallback);
}

void bridgeBegin() {
  delay(2500); //The bare minimum needed to be able to reboot both linino and leonardo.
               // if reboot fails try increasing this number
               // The more you run on linux the higher this number should be
  Serial1.begin(250000); // Set the baud.
  // Wait for U-boot to finish startup.  Consume all bytes until we are done.
  do {
    while (Serial1.available() > 0) {
      Serial1.read();
    }
    delay(1000);
  } while (Serial1.available()>0);

  Serial1.write((uint8_t *)"\xff\0\0\x05XXXXX\x0d\xaf", 11);
  Serial1.print("\n");
  delay(500);
  Serial1.write(CTRL_C);
  Serial1.print("\n");
  delay(500);
  Serial1.print("\x03");
  Serial1.print("\n");
  delay(500);
  Serial1.print("\003");
  Serial1.print("\n");
  delay(250);
  Serial1.print("\n");
  delay(500);
  Serial1.print("/mnt/sda1/newbridge/newbridge.py -q --spy\n");
  while (Serial1.available() > 0) {
    Serial1.read();
  }
}

void loop()  
{ 
  /*
  // Say Hello to the Console when it connects
  if (Console.connected() != consoleStatus) {
    consoleStatus = Console.connected();
    if (Console.connected()) {
      Console.println("0;255;3;0;9;Console connected.");
    } 
  }
  */
  
  gw.processRadioMessage();
  if (commandComplete) {
    // A command wass issued from serial interface
    // We will now try to send it to the actuator
    gw.parseAndSend(inputString);
    commandComplete = false;  
    inputPos = 0;
  }
  
  /*
  // Allow command input either by Console or Serial -- but not both.
  if (Console) {
    consoleEvent();
  } else if (Serial) {
  */
  if (Serial1) {
    // Arduino Leonardo and Yún don't trigger the serialEvent.  Call it ourselves.
    serialEvent();
  }
}

// This will be called when message received
void messageCallback(char *writeBuffer) {
  /*
  if (Console) {
      Console.print(writeBuffer);
  }
  */
  Serial1.print(writeBuffer);
}

// Check for Serial data
void serialEvent() {
  while (Serial1.available()) {
    // get the new byte:
    char inChar = (char)Serial1.read();
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inputPos<MAX_RECEIVE_LENGTH-1 && !commandComplete) { 
      if (inChar == '\n') {
        inputString[inputPos] = 0;
        commandComplete = true;
      } else {
        // add it to the inputString:
        inputString[inputPos] = inChar;
        inputPos++;
      }
    } else {
       // Incoming message too long. Throw away 
        inputPos = 0;
    }
  }
}

/*
// Check for Console data
void consoleEvent() {
  while (Console.available()) {
    // get the new byte:
    char inChar = (char)Console.read(); 
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inputPos<MAX_RECEIVE_LENGTH-1 && !commandComplete) { 
      if (inChar == '\n') {
        inputString[inputPos] = 0;
        commandComplete = true;
      } else {
        // add it to the inputString:
        inputString[inputPos] = inChar;
        inputPos++;
      }
    } else {
       // Incoming message too long. Throw away 
        inputPos = 0;
    }
  }
}
*/
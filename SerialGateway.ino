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
#include "NewBridge.h"

#define INCLUSION_MODE_TIME 1 // Number of minutes inclusion mode is enabled
#define INCLUSION_MODE_PIN 3 // Digital pin used for inclusion mode button


MyGateway gw(DEFAULT_CE_PIN, DEFAULT_CS_PIN, INCLUSION_MODE_TIME, INCLUSION_MODE_PIN,  6, 5, 4);

char inputString[MAX_RECEIVE_LENGTH] = "";    // A string to hold incoming commands from serial/ethernet interface
int inputPos = 0;
boolean commandComplete = false;  // whether the string is complete

unsigned long lastInvalidMessage = 0;
unsigned int invalidMessages = 0;
boolean consoleStatus = 0;

void setup()  
{ 
  Bridge.begin();
  gw.begin(RF24_PA_LEVEL_GW, RF24_CHANNEL, RF24_DATARATE, messageCallback);
}

void loop() { 
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
    
    // Send if it's a valid message; assume the Bridge has failed.  Restart the bridge.
    if (gw.isValidMessage(inputString))
      gw.parseAndSend(inputString);
    else {
      if ((lastInvalidMessage+1000) < millis()) {
        invalidMessages = 0;
      }
      
      //char tmp[MAX_RECEIVE_LENGTH];
      //sprintf(tmp, "0;255;3;0;9;%fs Invalid message [%d/-%fs]: %s\n", float(millis())/1000.0, invalidMessages, float(millis()-lastInvalidMessage)/1000.0, inputString);
      Bridge.print("0;255;3;0;9;");
      Bridge.print(float(millis())/1000.0);
      Bridge.print("s Invalid message [");
      Bridge.print(invalidMessages);
      Bridge.print("/-");
      Bridge.print(float(millis()-lastInvalidMessage)/1000.0);
      Bridge.print("s] (");
      Bridge.print(strlen(inputString));
      Bridge.print(" chars): ");
      
      if (strlen(inputString) <= 10) {
          for(inputPos = 0; inputString[inputPos]; inputPos++)
              Bridge.print((int)inputString[inputPos], HEX);
          Bridge.print(" @@ ");
      }
      
      Bridge.print(inputString);
      
      Bridge.print(F("\n"));

      invalidMessages++;
      if ((lastInvalidMessage+1000) > millis()) {
        if (invalidMessages > 2) {
          Bridge.flush();
          Bridge.end();
          delay(10000);
          Bridge.begin();
        }
      } else {
        lastInvalidMessage = millis();
      }
    }
    
    commandComplete = false;  
    inputPos = 0;
  }
  
    // Arduino Leonardo and Yún don't trigger the serialEvent.  Call it ourselves.
    serialEvent();
}

// This will be called when message received
void messageCallback(char *writeBuffer) {
  Bridge.print(writeBuffer);
}

// Check for Serial data
void serialEvent() {
  while (Bridge.available() > 0) {
    // get the new byte:
    char inChar = (char)Bridge.read(); 
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inputPos<MAX_RECEIVE_LENGTH-1 && !commandComplete) { 
      if (inChar == '\n' || inChar == '\r') {
        inputString[inputPos] = 0;
        if (inputPos > 0)
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

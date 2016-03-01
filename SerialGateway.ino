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

unsigned long lastInvalidMessage = 0;
unsigned int invalidMessages = 0;
boolean consoleStatus = 0;
static const char CTRL_C = 3;

void setup()  
{ 
  bridgeBegin();
  gw.begin(RF24_PA_LEVEL_GW, RF24_CHANNEL, RF24_DATARATE, messageCallback);
}

void purgeSerialInput() {
  do {
    while (Serial1.available() > 0) {
      Serial1.read();
    }
    delay(1000);
  } while (Serial1.available()>0);
}

int strcmp2(char *s, char *t) {
  int i, j;
  	
  for (i=0, j=0; s[i] || t[i]; s[i]==t[i] ? i++ : *s++)
    j++;
  		
  return (i > 0) && (i == j);
}

int waitForShell() {
  char input[64] = "";
  int i = 0;
  int rounds = 0;
  
  while ( 1 ) {
    rounds++;
    Serial1.print(F("\n"));
    delay(250);
    while (Serial1.available() > 0) {
      input[i] = Serial1.read();
      
      if (input[i] == '\n') {
        input[i] = 0;
        inputPos = 0;
      }
      
      input[i+1] = 0;
      if (strcmp("root@Micromark:~# ", input) || strcmp("root@Micromark:/# ", input)) {
          return rounds;
      }
      
      i++;
      if (i == 63)
        i=0;
    }
    delay(500);
  }
}

void bridgeBegin() {
  int rounds = 0;
  delay(2500); //The bare minimum needed to be able to reboot both linino and leonardo.
               // if reboot fails try increasing this number
               // The more you run on linux the higher this number should be
  Serial1.begin(250000); // Set the baud.
  // Wait for U-boot to finish startup.  Consume all bytes until we are done.
  purgeSerialInput();
  
  // Kill the Arduino bridge in case it's running
  Serial1.write((uint8_t *)"\xff\0\0\x05XXXXX\x0d\xaf", 11);
  Serial1.print("\n");
  delay(500);
  Serial1.print(CTRL_C);
  delay(250);
  Serial1.print(F("\n"));
  delay(250);
  Serial1.print(F("\n"));
  delay(250);
  
  purgeSerialInput();
  rounds += waitForShell();
  
  // Start up our own bridge
  Serial1.print("/mnt/sda1/newbridge/newbridge.py -q -l /mnt/sda1/newbridge/log.log\n");
  delay(1000);
  purgeSerialInput();
  Serial1.print("0;255;3;0;9;Shell rounds: ");
  Serial1.println(rounds);
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
    
    // Send if it's a valid message; assume the Bridge has failed.  Restart the bridge.
    if (gw.isValidMessage(inputString))
      gw.parseAndSend(inputString);
    else {
      if ((lastInvalidMessage+1000) < millis()) {
        invalidMessages = 0;
      }
      
      Serial1.print("0;255;3;0;9;");
      Serial1.print(float(millis())/1000.0);
      Serial1.print("s Invalid message [");
      Serial1.print(invalidMessages);
      Serial1.print("/-");
      Serial1.print(float(millis()-lastInvalidMessage)/1000.0);
      Serial1.print("s]: ");
      Serial1.print(inputString);
      Serial1.print(F("\n"));
      
      invalidMessages++;
      if ((lastInvalidMessage+1000) > millis()) {
        if (invalidMessages > 2) {
          Serial1.flush();
          Serial1.end();
          delay(10000);
          bridgeBegin();
        }
      } else {
        lastInvalidMessage = millis();
      }
    }
    
    commandComplete = false;  
    inputPos = 0;
  }
  
  if (Serial1) {
    // Arduino Leonardo and Yún don't trigger the serialEvent.  Call it ourselves.
    serialEvent();
  }
}

// This will be called when message received
void messageCallback(char *writeBuffer) {
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

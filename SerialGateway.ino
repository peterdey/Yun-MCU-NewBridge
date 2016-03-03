/**
 * The MySensors Arduino library handles the wireless radio link and protocol
 * between your home built sensors/actuators and HA controller of choice.
 * The sensors forms a self healing radio network with optional repeaters. Each
 * repeater and gateway builds a routing tables in EEPROM which keeps track of the
 * network topology allowing messages to be routed to nodes.
 *
 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2016 Peter Dey
 * Copyright (C) 2013-2015 Sensnology AB
 * Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors
 *
 * Documentation: http://www.mysensors.org
 * Support Forum: http://forum.mysensors.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 *******************************
 *
 * DESCRIPTION
 * The ArduinoGateway prints data received from sensors on the serial link. 
 * The gateway accepts input on seral which will be sent out on radio network.
 *
 * The GW code is designed for Arduino Nano 328p / 16MHz
 *
 * Wire connections (OPTIONAL):
 * - Inclusion button should be connected between digital pin 3 and GND  
 * - RX/TX/ERR leds need to be connected between +5V (anode) and digital pin 6/5/4 with resistor 270-330R in a series
 *
 * LEDs (OPTIONAL):
 * - To use the feature, uncomment WITH_LEDS_BLINKING in MyConfig.h
 * - RX (green) - blink fast on radio message recieved. In inclusion mode will blink fast only on presentation recieved
 * - TX (yellow) - blink fast on radio message transmitted. In inclusion mode will blink slowly
 * - ERR (red) - fast blink on error during transmission error or recieve crc error 
 * 
 */

#define NO_PORTB_PINCHANGES

#include <MySigningNone.h>
#include <MyTransportNRF24.h>
#include <MyHwATMega328.h>
#include <MySigningAtsha204Soft.h>

#include <SPI.h>  
#include <MySensor.h>  
#include <MyParserSerial.h>  
#include <MyTransport.h>
#include <stdarg.h>
#include "NewBridge.h"

#define MAX_RECEIVE_LENGTH  100 // Max buffersize needed for messages coming from controller
#define MAX_SEND_LENGTH     120 // Max buffersize needed for messages destined for controller
#define INCLUSION_MODE_TIME 1   // Number of minutes inclusion mode is enabled
#define INCLUSION_MODE_PIN  3   // Digital pin used for inclusion mode button
#define RADIO_ERROR_LED_PIN 4   // Error led pin
#define RADIO_RX_LED_PIN    6   // Receive led pin
#define RADIO_TX_LED_PIN    5   // the PCB, on board LED

// NRFRF24L01 radio driver (set low transmit power by default) 
MyTransportNRF24 transport(RF24_CE_PIN, RF24_CS_PIN, RF24_PA_LEVEL_GW);

// Message signing driver (signer needed if MY_SIGNING_FEATURE is turned on in MyConfig.h)
//MySigningNone signer;
//MySigningAtsha204Soft signer;
//MySigningAtsha204 signer;

// Hardware profile 
MyHwATMega328 hw;
MyParserSerial parser;

void incomingMessage(const MyMessage &message);
void serial(const char *fmt, ... );

// Construct MySensors library (signer needed if MY_SIGNING_FEATURE is turned on in MyConfig.h)
// To use LEDs blinking, uncomment WITH_LEDS_BLINKING in MyConfig.h
MySensor gw(transport, hw /*, signer*/, RADIO_RX_LED_PIN, RADIO_TX_LED_PIN, RADIO_ERROR_LED_PIN);

char inputString[MAX_RECEIVE_LENGTH] = "";    // A string to hold incoming commands from serial/ethernet interface
int inputPos = 0;
boolean commandComplete = false;  // whether the string is complete

unsigned long lastInvalidMessage = 0;
unsigned int invalidMessages = 0;
boolean consoleStatus = 0;

void setup()  
{ 
  Bridge.begin();
  gw.begin(incomingMessage, 0, true, 0);

  // Send startup log message on serial
  serial(PSTR("0;255;%d;0;%d;Gateway startup complete (build %s %s)\n"),  C_INTERNAL, I_GATEWAY_READY, __DATE__, __TIME__);
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
  
  gw.process();

  if (commandComplete) {
    // A command wass issued from serial interface
    // We will now try to send it to the remote node
    parseAndSend(gw, inputString);
    commandComplete = false;  
    inputPos = 0;
  }
  
    // Arduino Leonardo and Yún don't trigger the serialEvent.  Call it ourselves.
    serialEvent();
}

// Check for Serial data
void serialEvent() {
  while (Bridge.available()) {
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

void incomingMessage(const MyMessage &message) {
  uint8_t command = mGetCommand(message);
  char convBuf[MAX_PAYLOAD*2+1];
    
  // Pass along the message from sensors to serial line
  serial(PSTR("%d;%d;%d;%d;%d;%s\n"),message.sender, message.sensor, command, mGetAck(message), message.type, message.getString(convBuf));
} 

void parseAndSend(MySensor &gw, char *commandBuffer) {
    boolean ok;
    MyMessage &msg = gw.getLastMessage();
    
    if (parser.parse(msg, commandBuffer)) {
        uint8_t command = mGetCommand(msg);
        
        if (msg.destination==GATEWAY_ADDRESS && command==C_INTERNAL) {
            // Handle messages directed to gateway
            if (msg.type == I_VERSION) {
                // Request for version
                serial(PSTR("0;255;%d;0;%d;%s (build %s %s)\n"), C_INTERNAL, I_VERSION, LIBRARY_VERSION, __DATE__, __TIME__);
            }
        } else {
            gw.txBlink(1);
            ok = gw.sendRoute(msg);
            if (!ok) {
                gw.errBlink(1);
            }
        }
    } else {
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
}

void serial(const char *fmt, ... ) {
    char serialBuffer[MAX_SEND_LENGTH]; // Buffer for building string when sending data to vera
    
    va_list args;
    va_start (args, fmt );
    vsnprintf_P(serialBuffer, MAX_SEND_LENGTH, fmt, args);
    va_end (args);
    Bridge.print(serialBuffer);
}

/*

  Copyright (c) 2016 Peter Dey. All right reserved.
  
*/

#include "NewBridge.h"

// Default constructor
NewBridgeClass::NewBridgeClass() :
    inBuffer(NULL)
{
    // Empty
}

NewBridgeClass::~NewBridgeClass() {
    end();
}

size_t NewBridgeClass::write(uint8_t c) {
    Serial1.write(c);
}

size_t NewBridgeClass::write(const uint8_t *buff, size_t size) {
    return Serial1.write(buff, size);
}

void NewBridgeClass::flush() {
    Serial1.flush();
}

bool NewBridgeClass::connected() {
    uint8_t tmp = 'a';
    //bridge.transfer(&tmp, 1, &tmp, 1);
    return tmp == 1;
}

int NewBridgeClass::available() {
    return Serial1.available();
}

int NewBridgeClass::read() {
    return Serial1.read();
}

int NewBridgeClass::peek() {
    return Serial1.peek();
}

void NewBridgeClass::purgeSerialInput() {
    do {
        while (Serial1.available() > 0) {
            Serial1.read();
        }
        delay(1000);
    } while (Serial1.available()>0);
}

/********
Temporary functions that should eventually be removed

int __strcmp2(char *s, char *t) {
    int i, j;
  	
    for (i=0, j=0; s[i] || t[i]; s[i]==t[i] ? i++ : *s++)
        j++;
    
    return (i > 0) && (i == j);
}

int __waitForShell() {
    char input[64] = "";
    int i = 0;
    int rounds = 0;

    while ( rounds++ < 2 ) {
        Serial1.print(F("\n"));
        delay(250);
        while (Serial1.available() > 0) {
            input[i] = Serial1.read();

            if (input[i] == '\n') {
                input[i] = 0;
                i = 0;
            }

            input[i+1] = 0;
            if (strcmp("root@Micromark:~# ", input) || strcmp("root@Micromark:/# ", input)) {
                delay(250);
                if (Serial1.available() == 0) {
                    continue;
                }
            }

            i++;
            if (i == 63)
                i=0;
        }
        delay(500);
    }
}
******/

int NewBridgeClass::waitForShell() {
    char input[BUFFER_SIZE] = "";
    int i = 0;
    int rounds = 0;

    while ( rounds++ < 2 ) {
        Serial1.print(F("\n"));
        delay(50);
        while (Serial1.available() > 0) {
            input[i++] = (char)Serial1.read();
            input[i] = 0;
            
            if (input[i-1] == '\n') {
                input[i-1] = 0;
                i = 0;
            }
            
            if (i == BUFFER_SIZE)
                i=0;
            
            if (!strcmp("root@Micromark:~# ", input) || !strcmp("root@Micromark:/# ", input)) {
                continue;
            }
            
        }
    }
    return rounds;
}

void NewBridgeClass::begin() {
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
    delay(50);
    Serial1.print(F("\n"));
    delay(50);

    purgeSerialInput();
    rounds += waitForShell();

    // Start up our own bridge
    Serial1.print("/mnt/sda1/newbridge/newbridge.py -q -l /mnt/sda1/newbridge/log.log\n");
    delay(1000);
    purgeSerialInput();
    Serial1.print("0;255;3;0;9;Shell rounds: ");
    Serial1.println(rounds);
}

void NewBridgeClass::end() {
    Serial1.end();
}

NewBridgeClass Bridge;

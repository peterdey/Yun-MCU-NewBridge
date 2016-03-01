/*

  Copyright (c) 2016 Peter Dey. All right reserved.
  
*/

#ifndef NEWBRIDGE_H_
#define NEWBRIDGE_H_

#include <Arduino.h>

class NewBridgeClass : public Stream {
    public:
        // Default constructor uses global Bridge instance
        NewBridgeClass();
        ~NewBridgeClass();

        void begin();
        void end();

        bool connected();

        // Stream methods
        // (read from console)
        int available();
        int read();
        int peek();
        // (write to console socket)
        size_t write(uint8_t);
        size_t write(const uint8_t *buffer, size_t size);
        void flush();

        operator bool () {
            return connected();
        }

    private:
        static const char CTRL_C = 3;

        bool started;
        static const int BUFFER_SIZE = 64;
        uint8_t *inBuffer;

        void purgeSerialInput();
        int waitForShell();
};

extern NewBridgeClass Bridge;

#endif

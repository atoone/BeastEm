#pragma once
#include <stdint.h>
#include <vector>
#include "debug.hpp"

class I2cDevice {
    public:
        virtual bool    atAddress(uint8_t adddress) = 0;
        virtual void    start() = 0;
        virtual uint8_t readNext() = 0;
        virtual void    write(uint8_t value) = 0;
        virtual void    stop() = 0;
};

class I2c {

    enum State { RESET, IDLE, ADDRESS, READ_READY, WRITE_READY, READ, WRITE };

    private:

        State state = RESET;

        uint64_t busState;
        uint64_t busMask;

        uint64_t clockMask, dataMask;

        uint8_t counter;
        uint8_t address;
        uint8_t ioByte;

        bool sendAck;

        uint64_t outputState;

        std::vector<I2cDevice*> devices;

        I2cDevice *currentDevice = NULL;
        uint64_t  clockDbg, dataDbg;

        bool debug = false;
        int  debugLength;
        
        Debug log = Debug(DEBUG_DISABLE);
        void debugSignal();

    public:
        I2c(uint64_t clockMask, uint64_t dataMask);

        uint64_t tick(uint64_t* busState, uint64_t time_ps);

        void addDevice(I2cDevice *device);

        I2cDevice * deviceForAddress(uint8_t address);

        void setDebug(bool debug);
};


#pragma once

#include <ctime>
#include <SDL.h>
#include "i2c.hpp"
#include "digit.hpp"

class I2cRTC: public I2cDevice {

    public:
        I2cRTC(uint8_t address, uint64_t intMask);

        void tick(uint64_t* busState, uint64_t clock_time_ps );

        virtual bool    atAddress(uint8_t adddress);
        virtual void    start();
        virtual uint8_t readNext();
        virtual void    write(uint8_t value);
        virtual void    stop();
    private:
        uint8_t address;
        uint16_t byteCount;
        uint8_t currentAddress;
        uint64_t intMask;

        tm clock = {0};
        uint64_t startTime = 0;
        int weekOffset = 0;
        bool setTime;

        bool squareWave = true;
        uint64_t squareWaveTime = 0;
        
        uint8_t bcd(uint8_t value);
        uint8_t readBcd(uint8_t value);

        static const int MAX_MEM = 0x60;
        static const uint64_t PICOSECONDS_IN_SECOND = (1000000000000ULL);
        static const uint64_t PICOSECONDS_IN_64Hz   = PICOSECONDS_IN_SECOND / 128;
        static const uint64_t PICOSECONDS_IN_32kHz  = PICOSECONDS_IN_SECOND / 65536;
        static const uint64_t PICOSECONDS_IN_8kHz   = PICOSECONDS_IN_SECOND / 16384;
        static const uint64_t PICOSECONDS_IN_4kHz   = PICOSECONDS_IN_SECOND / 8192;
        static const uint64_t PICOSECONDS_IN_1Hz    = PICOSECONDS_IN_SECOND / 2;

        uint8_t mem[MAX_MEM] = {0};

        static const int OSC_EN  = 0x80;
        static const int FLAG_12HR = 0x40;
        static const int FLAG_PM   = 0x20;

        static const int FLAG_OSC  = 0x20;  // In REG_WKDAY

        static const int FLAG_SQWEN = 0x40;  // in REG_CONTROL
        static const int FLAG_CRSTRIM = 0x04;
        static const int MASK_SQWFS   = 0x03;  // 00=32.767kHz  01=8.192kHz  10=4.096kHz  11=1Hz

        static const int REG_SEC = 0;
        static const int REG_MIN = 1;
        static const int REG_HR  = 2;
        static const int REG_WKDAY = 3;
        static const int REG_DATE = 4;
        static const int REG_MONTH = 5;
        static const int REG_YEAR  = 6;
        static const int REG_CONTROL  = 7;

        static const int MASK_SEC = 0x7F;
        static const int MASK_MIN = 0x7F;
        static const int MASK_12HR = 0x1F;
        static const int MASK_24HR = 0x3F;
        static const int MASK_DATE = 0x3F;
        static const int MASK_MONTH = 0x1F;

        const int MONTH_DAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
};
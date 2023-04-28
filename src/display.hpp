#pragma once

#include <SDL.h>
#include "i2c.hpp"
#include "digit.hpp"

class I2cDisplay: public I2cDevice {

    public:
        I2cDisplay(uint8_t address);

        void addDigit(Digit *digit);
        
        virtual bool    atAddress(uint8_t adddress);
        virtual void    start();
        virtual uint8_t readNext();
        virtual void    write(uint8_t value);
        virtual void    stop();
    private:
        uint8_t address;
        uint16_t byteCount;
        uint8_t currentAddress;

        bool commandUnlocked = false;
        uint8_t currentPage = 0;
        uint8_t interruptMask = 0;

        std::vector<Digit*> digits;

        static const int PAGE_0_SIZE = 72;
        static const int PAGE_1_SIZE = 192;
        static const int PAGE_2_SIZE = 192;
        static const int PAGE_3_SIZE = 18;

        uint8_t page_0[PAGE_0_SIZE];
        uint8_t page_1[PAGE_1_SIZE];
        uint8_t page_2[PAGE_2_SIZE];

        uint8_t page_3[PAGE_3_SIZE];

        void writeLEDControl(uint8_t ioByte);
        void writePWM(uint8_t ioByte);
        void writeBreath(uint8_t ioByte);
        void writeFunction(uint8_t ioByte);
};


#include "display.hpp"
#include <iostream>


void I2cDisplay::addDigit(Digit *digit) {
    this->digits.push_back(digit);
}

bool I2cDisplay::atAddress(uint8_t address) {
    return this->address == address;
}

I2cDisplay::I2cDisplay(uint8_t address) {
    this->address = address;
}


void I2cDisplay::start() {
    byteCount = 0;
}

void I2cDisplay::stop() {
    // std::cout << "Display Completed " << byteCount-1 << " bytes" << std::endl;
}

void I2cDisplay::writeLEDControl(uint8_t ioByte) {
    // Page 0
    if( currentAddress >= PAGE_0_SIZE ) {
        return;
    }
    if( currentAddress < 0x18 ) {
        if( page_0[currentAddress] != ioByte ) {
            page_0[currentAddress] = ioByte;

            int digit = currentAddress >> 1;
            uint16_t segmentBits = page_0[currentAddress | 0x01];
            segmentBits = (segmentBits << 8) | page_0[currentAddress & 0x1E];
            digits[digit]->setSegments(segmentBits);
        }
    }

    
}

void I2cDisplay::writePWM(uint8_t ioByte) {
    // Page 1
    if( currentAddress >= PAGE_1_SIZE ) {
        return;
    }
    if( page_1[currentAddress] != ioByte ) {
        int digit = currentAddress >> 4;
        digits[digit]->setBrightness(currentAddress & 0x0F, ioByte > 127 ? 255: ioByte*2);
    }
    page_1[currentAddress] = ioByte;
}

void I2cDisplay::writeBreath(uint8_t ioByte) {
    // Page 2
    if( currentAddress >= PAGE_2_SIZE ) {
        return;
    }
    page_2[currentAddress] = ioByte;
    // Breath control not implemented...
}

void I2cDisplay::writeFunction(uint8_t ioByte) {
    // Page 3
    if( currentAddress >= PAGE_3_SIZE ) {
        return;
    }
    page_3[currentAddress] = ioByte;
}

void I2cDisplay::write(uint8_t ioByte) {
    if( byteCount == 0 ) {
        // std::cout << "Display write from address " << 0+ioByte << std::endl;
        currentAddress = ioByte;
    }
    else {
        if( currentAddress == 0xFE ) {
            if( ioByte == 0xC5 ) {
                // std::cout << "Display command unlocked "<< std::endl;
                commandUnlocked = true;
            }
        }
        else if( currentAddress == 0xFD ) {
            if( commandUnlocked ) {
                currentPage = ioByte & 0x03;
                // std::cout << "Display current page = " << 0+currentPage << std::endl;
            }
            commandUnlocked = 0;
        }
        else if( currentAddress == 0xF0 ) {
                interruptMask = ioByte;
        }
        else {
            switch(currentPage) {
                case 0:
                    writeLEDControl(ioByte);
                    break;
                case 1:
                    writePWM(ioByte);
                    break;
                case 2:
                    writeBreath(ioByte);
                    break;
                case 3:
                    writeFunction(ioByte);
                    break;
            }
        }
        currentAddress++;
    }
    byteCount++;
}

uint8_t I2cDisplay::readNext() {
    uint8_t result = 0;
    currentAddress++;
    return result;
}
#include "rtc.hpp"

#include <iostream>

I2cRTC::I2cRTC(uint8_t address, uint64_t intMask) {
    this->address = address;
    this->intMask = intMask;
    for( int i=0; i<SRAM_LENGTH; i++ ) {
        mem[SRAM_ADDR+i] = rand();
    }
}

void I2cRTC::tick(uint64_t* busState, uint64_t clock_time_ps) {
    if( setTime ) {
        setTime = false;
        startTime = clock_time_ps;
    }
    if( mem[REG_SEC] & OSC_EN ) {
        while( clock_time_ps-startTime > PICOSECONDS_IN_SECOND ) {
            startTime += PICOSECONDS_IN_SECOND;

            if( ++clock.tm_sec > 59 ) {
                clock.tm_sec = 0;
                if( ++clock.tm_min > 59 ) {
                    clock.tm_min = 0;
                    if( ++clock.tm_hour > 23 ) {
                        clock.tm_hour = 0;
                        clock.tm_wday = (clock.tm_wday+1) % 7;
                        clock.tm_yday++;
                        clock.tm_mday++;
                        int leap = ((clock.tm_mon == 1) && (clock.tm_year % 4) == 0) ? 1: 0;
                        if(clock.tm_mday > MONTH_DAYS[clock.tm_mon]+leap) {
                            clock.tm_mday = 1;
                            if(++clock.tm_mon > 11) {
                                clock.tm_mon = 0;
                                clock.tm_year++;
                                clock.tm_yday = 0;
                            }
                        }
                    }
                }
            }
        }
    }
    if( mem[REG_CONTROL] & FLAG_SQWEN ) {
        uint64_t tickTime = 0;

        if( mem[REG_CONTROL] & FLAG_CRSTRIM ) {
            tickTime = PICOSECONDS_IN_64Hz;
        }
        else {
            switch( mem[REG_CONTROL] & MASK_SQWFS ) {
                case 0 : tickTime = PICOSECONDS_IN_32kHz; break;
                case 1 : tickTime = PICOSECONDS_IN_8kHz;  break;
                case 2 : tickTime = PICOSECONDS_IN_4kHz;  break;
                case 3 : tickTime = PICOSECONDS_IN_1Hz; break;
            }
        }
        if( (clock_time_ps - squareWaveTime) > tickTime ) {
            squareWaveTime = clock_time_ps;
            squareWave = !squareWave;
            *busState &= ~intMask;
            if( squareWave ) {
                *busState |= intMask;
            }
            //std::cout << "Tick " << (*busState & intMask) << " from " << squareWave << std::endl;
        }
    }
}

bool I2cRTC::atAddress(uint8_t address) {
    return this->address == address;
}

void I2cRTC::start() { 
    byteCount = 0;
}

void I2cRTC::stop() {
}

uint8_t I2cRTC::bcd(uint8_t value) {
    return ((value / 10) << 4) | (value % 10);
}

uint8_t I2cRTC::readBcd(uint8_t value) {
    return (value & 0x0F) + (value >> 4)*10;
}

uint8_t I2cRTC::readNext() {
    uint8_t result = mem[currentAddress];
    switch( currentAddress ) {
        case REG_SEC:
            result = bcd(clock.tm_sec) | (mem[REG_SEC] & OSC_EN);
            //std::cout << "Sec reg " << 0+result << " from " << 
            //    clock.tm_sec << " -> " << 0+bcd(clock.tm_sec) << " | " << 0+mem[REG_SEC] << std::endl;
            break;
        case REG_MIN:
            result = bcd(clock.tm_min);
            break;
        case REG_HR:
            if( mem[REG_HR ] & FLAG_12HR ) {
                if( clock.tm_hour >= 12 ) {
                    result = bcd(clock.tm_hour-12) | FLAG_12HR | FLAG_PM;
                }
                else {
                    result = bcd(clock.tm_hour) | FLAG_12HR;
                }
            }
            else {
                result = bcd(clock.tm_hour);
            }
            break;
        case REG_WKDAY:
            result = (((clock.tm_wday + weekOffset + 7) % 7)+1) | 
                    ((mem[REG_SEC & OSC_EN])? FLAG_OSC : 0);
            break;
        case REG_DATE:
            result = bcd(clock.tm_mday);
            break;
        case REG_MONTH:
            result = bcd(clock.tm_mon+1);
            break;
        case REG_YEAR:
            result = bcd(clock.tm_year % 100);
            break;
        default:
            // std::cout << "Reading mem " << 0+currentAddress << " = " << 0+mem[currentAddress] << std::endl;
            result = mem[currentAddress];
    }
    // std::cout << "RTC: Read " << 0+currentAddress << " -> " << 0+result << std::endl;
    currentAddress++;
    return result;
}

void I2cRTC::write(uint8_t ioByte) {
    if( byteCount == 0 ) {
        currentAddress = ioByte;
    }
    else {
        // std::cout << "RTC: Write " << 0+currentAddress << " <- " << 0+ioByte << std::endl;
        mem[currentAddress] = ioByte;
        int wkDay;

        switch( currentAddress ) {
            case REG_SEC:
                clock.tm_sec = readBcd(ioByte & MASK_SEC);
                mktime(&clock);
                setTime = true;
                break;
            case REG_MIN:
                clock.tm_min = readBcd(ioByte & MASK_MIN);
                mktime(&clock);
                break;
            case REG_HR:
                if( ioByte & FLAG_12HR ) {
                    if( ioByte & FLAG_PM ) {
                        clock.tm_hour = readBcd(ioByte & MASK_12HR ) + 12;
                    }
                    else {
                        clock.tm_hour = readBcd(ioByte & MASK_12HR);
                    }
                }
                else {
                    clock.tm_hour = readBcd(ioByte & MASK_24HR);
                }
                mktime(&clock);  
                break; 
            case REG_WKDAY:
                mktime(&clock);
                weekOffset = (((ioByte & 0x07)-1) - clock.tm_wday);
                break;
            case REG_DATE:
                wkDay = ((clock.tm_wday + weekOffset + 7) % 7);
                clock.tm_mday = readBcd(ioByte & MASK_DATE);
                mktime(&clock);
                weekOffset = wkDay - clock.tm_wday;
                break;
            case REG_MONTH:
                wkDay = ((clock.tm_wday + weekOffset + 7) % 7);
                clock.tm_mon = readBcd((ioByte & MASK_MONTH)-1);    
                mktime(&clock);
                weekOffset = wkDay - clock.tm_wday;
                break;
            case REG_YEAR:
                wkDay = ((clock.tm_wday + weekOffset + 7) % 7);
                clock.tm_year = 100+readBcd(ioByte);
                mktime(&clock);
                weekOffset = wkDay - clock.tm_wday;
                break;  
            //default:
                // std::cout << "Writing mem " << 0+currentAddress << " = " << 0+mem[currentAddress] << std::endl;
        }
        currentAddress++;
    }
    byteCount++;
}



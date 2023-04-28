#include "i2c.hpp"
#include <iostream>

I2c::I2c(uint64_t clockMask, uint64_t dataMask) {
    busMask = clockMask | dataMask;
    this->clockMask = clockMask;
    this->dataMask = dataMask;

    debugLength = 0;
}

void I2c::setDebug(bool debug) {
    this->debug = debug;
    log.debug_state = debug ? DEBUG_ENABLE : DEBUG_DISABLE;
}

uint64_t I2c::tick(uint64_t *busState, uint64_t clock_time_ps) {
    if( state == RESET ) {
        this->busState = *busState;
        this->state = IDLE;
        this->outputState = clockMask | dataMask;
        log << "I2C Reset" << std::endl;
        return clock_time_ps;
    }

    *busState &= ((~busMask) | outputState);

    if( (this->busState & busMask) == (*busState & busMask) ) {
        return clock_time_ps;
    }

    // std::cout << " Bus state changed!!" << 0+Z80PIO_GET_PB(*busState) << "Output is " << 0+Z80PIO_GET_PB(outputState) << std::endl;

    uint64_t data = *busState & this->dataMask;
    uint64_t clock = *busState & this->clockMask;

    bool clocked = clock != (this->busState & this->clockMask);
    bool dataChange = data != (this->busState & this->dataMask);

    this->busState = *busState;

    dataDbg = (dataDbg << 1) | ((data != 0) ? 1: 0);
    clockDbg = (clockDbg << 1) | ((clock != 0) ? 1: 0);
    debugLength++;

    if( sendAck ) {
        if( clocked )  {
            if( clock == 0 ) { // Change output on low..
                if( counter == 0 ) {
                    outputState &= ~dataMask; // Data low
                    counter++;
                    return clock_time_ps;
                }
                else {
                    // Second low - release data line and fall through to next state..
                    outputState |= dataMask; // Float
                    counter = 0;
                    sendAck = false;
                } 
            }
        } 
        else {
            return clock_time_ps;
        }
    }

    if( dataChange && !clocked && (clock != 0) ) {
        if( data != 0 ) {
            // Stop condition
            log << "Stopped at " << 0+counter << " with " << 0+ioByte << std::endl;
            this->debugSignal();
            outputState = clockMask | dataMask;
            state = IDLE;

            if( currentDevice ) {
                currentDevice->stop();
                currentDevice = NULL;
            }
        }
        else {
            // Start condition
            state = ADDRESS;
            counter = 0;
            address = 0;
        }

        return clock_time_ps;
    }
    
    switch(state) {
        case IDLE:
            break;
        case ADDRESS:
            if( clocked && (clock != 0) ) {
                if( counter < 7 ) {
                    address = (address << 1) | (data ? 1: 0);
                    counter++;
                }
                else {
                    currentDevice = deviceForAddress(address);
                    if( currentDevice ) {
                        if( data != 0 ) {
                            state = READ;
                        }
                        else {
                            state = WRITE;
                        }
                        counter = 0;
                        sendAck = true;
                        currentDevice->start();
                    }
                    else {
                        log << "--- Unknown address. Idle" << std::endl;
                        state = IDLE;
                    }
                }
            }
            break;
        case READ_READY:
        case WRITE_READY:
            if( dataChange ) {
                if( clock ) {
                    // Stop condition
                    log << "Idle" << std::endl;
                    state = IDLE;
                }
                else if( data == 0 ) {
                    // High to low
                    if( state == READ_READY ) {
                        
                    }
                    else {
                        state = WRITE;
                        ioByte = 0;
                    }
                    counter = 0;
                }
            }
            break;
        case READ:
            if( clocked ) {
                if( clock == 0 ) {
                    if( counter == 0 ) {
                        ioByte = currentDevice->readNext();
                        log << "Read byte " << 0+ioByte << std::endl;
                    }
                    if( ioByte & 0x80 ) {
                        outputState |= dataMask;
                    }
                    else {
                        outputState &= ~dataMask;
                    }
                    ioByte = ioByte << 1;

                    counter++;
                    if( counter == 9 ) {
                        counter = 0;
                        outputState |= dataMask;
                        //state = READ_READY;
                    }
                }
            }
            break;
        case WRITE:
            if( clocked && (clock != 0)) {
                ioByte = (ioByte << 1) | ((data != 0)? 1: 0);
                counter++;

                if( counter == 8 ) {
                    //std::cout << "Write byte " << 0+ioByte << std::endl;
                    currentDevice->write(ioByte);
                    counter = 0;
                    state = WRITE;
                    sendAck = true;
                }
            }
            break;
    }

    return clock_time_ps;
}

void I2c::addDevice(I2cDevice *device) {
    devices.push_back(device);
}

I2cDevice * I2c::deviceForAddress(uint8_t address) {
    
    for(auto & device: devices) {
        if( device->atAddress(address) ) {
            log << "--- I2C address "<< 0+address << std::endl;
            return device;
        }
    }

    log << "--- Unknown address "<< 0+address << std::endl;
    return NULL;
}

void I2c::debugSignal() {
    if( !debug ) return;

    if( debugLength > 64 ) {
        debugLength = 64;
    }
    std::cout << std::endl << "Data  :";

    for( int i=debugLength; i>=0; i--) {
        std::cout << ((((dataDbg >> i) & 0x01) != 0) ? "-" : "_");
    }

    std::cout << std::endl << "Clock :";
    for( int i=debugLength; i>=0; i--) {
        std::cout << ((((clockDbg >> i) & 0x01) != 0) ? "-" : "_");
    }

    std::cout << std::endl;

    dataDbg = 0;
    clockDbg = 0;
    debugLength = 0;
}


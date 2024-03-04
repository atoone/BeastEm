#include "binaryFile.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

const char filePathSeparator =
#ifdef _WIN32
                            '\\';
#else
                            '/';
#endif

BinaryFile::BinaryFile(std::string filename, uint32_t address, int page, bool cpuAddress) :
            filename(filename),
            address(address),
            page(page),
            cpuAddress(cpuAddress) {

    shortname = filename;
    std::size_t pos = filename.find_last_of(filePathSeparator);

    if( pos != std::string::npos && pos < (filename.size()-2)) {
        shortname = filename.substr(pos+1);
    }
}

bool BinaryFile::isCPUAddress() {
    return cpuAddress;
}

std::string BinaryFile::getFilename() {
    return filename;
}

std::string BinaryFile::getShortname() {
    return shortname;
}

uint32_t BinaryFile::getAddress() {
    return address;
}

int BinaryFile::getPage() {
    return page;
}

size_t BinaryFile::load(uint8_t *rom, uint8_t *ram, bool pagingEnabled, uint8_t pageMap[4]) {
    uint32_t loadAddress = address;
    std::stringstream ss;
    
    if( page >= 0 ) {
        loadAddress = (page << 14) | (address & 0x3FFF);
        ss << "0x" << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << page;
        ss << ":0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << (address & 0x3FFF);
    }
    else if( cpuAddress ) {
        ss << "CPU 0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << (address & 0x0FFFF);
    }
    else {
        ss << "0x" << std::uppercase << std::setfill('0') << std::setw(5) << std::hex << address;
    }

    std::string destStr = ss.str();

    bool isRom = loadAddress < ROM_SIZE;
    unsigned int space = isRom ? (ROM_SIZE-loadAddress) : (RAM_SIZE+ROM_SIZE-loadAddress);

    std::ifstream ifs(filename, std::ios::binary|std::ios::ate);
    std::ifstream::pos_type pos = ifs.tellg();

    unsigned int length = pos;

    if( length == 0 ) {
        std::cout << "Binary file does not exist: " << filename << std::endl;
        return 0;
    }
    if( cpuAddress && ((address & 0x0FFFF) + length) > 0x0FFFF ) {
        std::cout << "Binary file exceeds space starting at " << destStr << " with " << length << " bytes" << std::endl;
        return 0;
    }
    else if( length > space) {
        std::cout << "Binary file is too big for " << (isRom? "ROM": "RAM") << " starting at " << destStr << ". Actual size: " << (length/1024) << "K" << std::endl;
        return 0;
    }

    ifs.seekg(0, std::ios::beg);
    if( cpuAddress ) {
        int cpuBank = (address >> 14) & 0x03;
        loadAddress = address;
        unsigned int remaining = length;

        while( remaining > 0 ) {
            uint8_t loadPage = pagingEnabled ? pageMap[cpuBank] : 0;

            loadAddress =  (loadPage << 14) | (loadAddress & 0x3FFF);
            isRom = loadAddress < ROM_SIZE;
            unsigned int loadLength = std::min(remaining, 0x4000-(loadAddress & 0x3FFFF));

            std::cout << "Loading " << loadLength << " bytes to page " << loadPage << std::endl;
            if( isRom ) {
                ifs.read((char *)rom+loadAddress, loadLength);
            }
            else {
                ifs.read((char *)ram+(loadAddress-RAM_SIZE), loadLength);
            }
            cpuBank++;
            loadAddress += loadLength;
            remaining -= loadLength;
        }
        std::cout << "Read file '" << filename << "' (" << (length/1024) << "K) to " << destStr << std::endl;
    }
    else if( isRom ) {
        ifs.read((char *)rom+loadAddress, length);
        std::cout << "Read file '" << filename << "' (" << (length/1024) << "K) to ROM starting at " << destStr << std::endl;
    }
    else {
        ifs.read((char *)ram+(loadAddress-RAM_SIZE), length);
        std::cout << "Read file '" << filename << "' (" << (length/1024) << "K) to RAM starting at " << destStr << std::endl;
    }
    ifs.close();

    
    return length;
}
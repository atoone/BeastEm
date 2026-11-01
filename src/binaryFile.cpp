#include "binaryFile.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>

const char filePathSeparator =
#ifdef _WIN32
                            '\\';
#else
                            '/';
#endif

BinaryFile::BinaryFile(std::string filename, uint32_t address, bool watch, BINARY_DEST destination, uint8_t page) :
            filename(filename),
            address(address),
            watch(watch),
            page(page),
            destination(destination) {

    shortname = filename;
    std::size_t pos = filename.find_last_of(filePathSeparator);

    if( pos != std::string::npos && pos < (filename.size()-2)) {
        shortname = filename.substr(pos+1);
    }
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

uint8_t BinaryFile::getPage() {
    return page;
}

BinaryFile::BINARY_DEST BinaryFile::getDestination() {
    return destination;
}

bool BinaryFile::isWatched() {
    return watch;
}

bool BinaryFile::isUpdated() {
    if( watch ) {
        // The file can briefly vanish while an editor or assembler rewrites it;
        // treat that as unchanged and check again next time.
        std::error_code ec;
        auto writeTime = std::filesystem::last_write_time(filename, ec);
        return !ec && lastRead < writeTime;
    }
    return false;
}

void BinaryFile::toggleWatch() {
    watch = !watch;
}

size_t BinaryFile::load(uint8_t *rom, uint8_t *ram, bool pagingEnabled, uint8_t pageMap[4], uint8_t *videoRam) {
    uint32_t loadAddress = address;
    std::stringstream ss;
    std::string destName;

    switch( destination ) {
        case PAGE_OFFSET: {
            loadAddress = (page << 14) | (address & 0x3FFF);
            ss << "0x" << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << (int)page;
            ss << ":0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << (address & 0x3FFF);
            break;
        }
        case LOGICAL: 
            ss << "CPU 0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << (address & 0x0FFFF);
            break;
        case PHYSICAL: 
            ss << "0x" << std::uppercase << std::setfill('0') << std::setw(5) << std::hex << address;
            break;
        case VIDEO_RAM:
            ss << "0x" << std::uppercase << std::setfill('0') << std::setw(5) << std::hex << address;
            break;
    }

    // Work out ROM/RAM and available space from the final load address, which
    // for PAGE_OFFSET differs from the address given on the command line.
    bool isRom = loadAddress < ROM_SIZE;
    unsigned int space;
    if( destination == VIDEO_RAM ) {
        destName = "Video RAM";
        space = (loadAddress < VIDEO_RAM_SIZE) ? (VIDEO_RAM_SIZE-loadAddress) : 0;
    }
    else if( isRom ) {
        destName = "ROM";
        space = ROM_SIZE-loadAddress;
    }
    else {
        destName = "RAM";
        space = (loadAddress < ROM_SIZE+RAM_SIZE) ? (ROM_SIZE+RAM_SIZE-loadAddress) : 0;
    }

    std::string destStr = ss.str();

    std::ifstream ifs(filename, std::ios::binary|std::ios::ate);
    std::ifstream::pos_type pos = ifs.tellg();

    if( !ifs || pos <= 0 ) {
        std::cout << "Binary file does not exist: " << filename << std::endl;
        return 0;
    }
    unsigned int length = pos;

    if( destination == LOGICAL && ((address & 0x0FFFF) + length) > 0x0FFFF ) {
        std::cout << "Binary file exceeds space starting at " << destStr << " with " << length << " bytes" << std::endl;
        return 0;
    }
    else if( length > space ) {
        std::cout << "Binary file is too big for " << destName << " starting at " << destStr << ". Actual size: " << (length/1024) << "K" << std::endl;
        return 0;
    }

    ifs.seekg(0, std::ios::beg);

    // Record the timestamp before reading, so a write during the read triggers another reload
    std::error_code ec;
    auto writeTime = std::filesystem::last_write_time(filename, ec);
    if( !ec ) {
        lastRead = writeTime;
    }

    if( destination == LOGICAL ) {
        int cpuBank = (address >> 14) & 0x03;
        loadAddress = address;
        unsigned int remaining = length;

        while( remaining > 0 ) {
            uint8_t loadPage = pagingEnabled ? pageMap[cpuBank] : 0;

            loadAddress =  (loadPage << 14) | (loadAddress & 0x3FFF);
            isRom = loadAddress < ROM_SIZE;
            unsigned int loadLength = std::min(remaining, 0x4000-(loadAddress & 0x3FFF));

            std::cout << "Loading " << loadLength << " bytes to page " << (int)loadPage << std::endl;
            if( loadPage >= 0x40 ) {
                // Skip data for unmapped pages
                ifs.seekg(loadLength, std::ios::cur);
            }
            else if( isRom ) {
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
    else if( destination == VIDEO_RAM ) {
        ifs.read((char *)videoRam+loadAddress, length);
        std::cout << "Read file '" << filename << "' (" << (length/1024) << "K) to Video RAM starting at " << destStr << std::endl;
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
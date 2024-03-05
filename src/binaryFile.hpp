#pragma once
#include <string>

class BinaryFile {
    static const int ROM_SIZE = (1<<19);
    static const int RAM_SIZE = (1<<19);
    static const int VIDEO_RAM_SIZE = (1<<20);
    
    public:
        enum BINARY_DEST {PHYSICAL, PAGE_OFFSET, LOGICAL, VIDEO_RAM};


        BinaryFile(std::string filename, uint32_t address, BINARY_DEST destination=BINARY_DEST::PHYSICAL, uint8_t page=0);

        std::string getFilename();
        std::string getShortname();
        uint32_t    getAddress();
        uint8_t     getPage();
        BINARY_DEST getDestination();

        size_t load(uint8_t *rom, uint8_t *ram, bool pagingEnabled, uint8_t pageMap[4], uint8_t *videoRam);
    private:
        std::string filename;
        std::string shortname;
        uint32_t    address;
        uint8_t     page;
        BINARY_DEST destination;


};
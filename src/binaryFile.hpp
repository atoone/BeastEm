#pragma once
#include <string>

class BinaryFile {


    static const int ROM_SIZE = (1<<19);
    static const int RAM_SIZE = (1<<19);
    
    public:
        BinaryFile(std::string filename, uint32_t address, int page=-1, bool cpuAddress=false);

        bool        isCPUAddress();
        std::string getFilename();
        std::string getShortname();
        uint32_t    getAddress();
        int         getPage();

        size_t load(uint8_t *rom, uint8_t *ram, bool pagingEnabled, uint8_t pageMap[4]);
    private:
        std::string filename;
        std::string shortname;
        uint32_t    address;
        int         page;
        bool        cpuAddress;


};

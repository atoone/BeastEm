#pragma once
#include <iostream>
#include <string>
#include <map>
#include <vector>

class Listing {

    public:
        struct Location {
            unsigned int fileNum;
            unsigned int lineNum;
            bool valid;
        };

        struct Source {
            const char* filename;
            unsigned int fileNum;
            int page;
        };

        struct Line {
            std::string text;
            std::string head;
            uint32_t    address;
            uint8_t     bytes[4];
            int         byteCount;
            bool        isData = false;
        };

        void addFile(const char* filename, int page);
        void removeFile(unsigned int fileNum);
        int fileCount();
        
        std::vector<Source> getFiles();

        Location getLocation(uint32_t address);
        std::pair<Line, bool>    getLine(Location location);

    private:
        std::vector<std::vector<Line>> files;
        std::vector<Source> sources;

        std::map<int, Location> lineMap;

        const char* addressRegex = "^[0-9]+(\\++\\s*|\\s+)([0-9a-f]{4})";

        int fromHex(char c);

};
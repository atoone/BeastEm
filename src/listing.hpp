#pragma once
#include <cstdint>
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

        struct Line {
            std::string text;
            std::string head;
            uint32_t    address;
            uint8_t     bytes[4];
            int         byteCount;
            bool        isData = false;
        };

        struct Source {
            std::string shortname;
            std::string filename;
            unsigned int fileNum;
            int page;
            std::vector<Line> lines;
        };

        bool    isValidFile(std::string filename);
        int     addFile(std::string filename, int page);
        void    loadFile(Source &source);

        void    removeFile(unsigned int fileNum);
        size_t  fileCount();
        
        std::vector<Source>& getFiles();

        Location getLocation(uint32_t address);
        std::pair<Line, bool>    getLine(Location location);

    private:
        std::vector<Source> sources;
        std::map<int, Location> lineMap;

        const char* addressRegex = "^[0-9]+(\\++\\s*|\\s+)([0-9a-f]{4})";

        bool isValidListing(std::ifstream& stream);
        int fromHex(char c);

};


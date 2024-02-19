#pragma once
#include <cstdint>
#include <iostream>
#include <string>
#include <map>
#include <vector>

class Listing {

    public:
        struct Location {
            int fileNum;
            int lineNum;
            bool valid;
        };

        void addFile(const char* filename, int page);
        int fileCount();
        
        Location getLocation(uint32_t address);
        std::string getLine(Location location);

    private:
        std::vector<std::vector<std::string>> files;

        std::map<int, Location> lineMap;

        const char* addressRegex = "\\s([0-9a-f]+)";

};

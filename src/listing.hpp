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

        struct Source {
            const char* filename;
            int fileNum;
            int page;
        };

        void addFile(const char* filename, int page);
        void removeFile(int fileNum);
        int fileCount();
        
        std::vector<Source> getFiles();

        Location getLocation(uint32_t address);
        std::string getLine(Location location);

    private:
        std::vector<std::vector<std::string>> files;
        std::vector<Source> sources;

        std::map<int, Location> lineMap;

        const char* addressRegex = "\\s([0-9a-f]{4})";

};


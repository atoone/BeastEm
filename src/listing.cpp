#include "listing.hpp"
#include <iostream>
#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>

static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

void Listing::addFile(const char *filename, int page) {
    std::ifstream myfile(filename);
    if(!myfile) {
        std::cout << "Listing file does not exist: " << filename << std::endl;
        exit(1);
    }
    std::string line;
    std::vector<std::string> lines;

    int fileNum = files.size();

    uint16_t address = 0;
    bool foundAddress = false;
    int lineNum = 0;
    int addressLine = 0;

    while (std::getline(myfile, line)) {
        lineNum++;

        lines.push_back(line);

        ltrim(line);
        std::regex matcher = std::regex(addressRegex, std::regex::icase);
        std::smatch match;
        if(std::regex_search(line, match, matcher)) {
            uint16_t nextAddress = std::stoi(match.str(), nullptr, 16);

            if( foundAddress && (nextAddress != address) ) {
                Location loc = {fileNum, addressLine, true};
                lineMap.emplace((page << 16) | address, loc);
            }
            
            foundAddress = true;
            address = nextAddress;
            addressLine = lineNum;
        }
        else {
            std::cout << "No match on line " << lineNum << std::endl;
        }
    }
    myfile.close();
    
    if( foundAddress ) {
        Location loc = {fileNum, addressLine, true};
        lineMap.emplace(address, loc);
    }
    std::cout << "Parsed listing, file "<< filename <<" for page " << page << " has " << lineNum << " lines." << std::endl;
    files.push_back(lines);
}

int Listing::fileCount() {
    return files.size();
}

Listing::Location Listing::getLocation(uint32_t address) {
    auto search = lineMap.find(address);
    if (search != lineMap.end()) {
        return search->second;
    }

    return Location() = {0,0,false};
}

std::string Listing::getLine(Location location) {
    if( location.valid ) {
        return files[location.fileNum][location.lineNum];
    }
    return "";
}

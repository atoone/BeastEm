#include "listing.hpp"
#include <iostream>
#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>
#include <cctype>

static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

const char filePathSeparator =
#ifdef _WIN32
                            '\\';
#else
                            '/';
#endif

std::vector<Listing::Source> Listing::getFiles() {
    return sources;
}

void Listing::removeFile(unsigned int fileNum) {
    files.erase(files.begin() + fileNum);
    sources.erase(sources.begin() + fileNum);

    for( auto it = lineMap.begin(); it != lineMap.end();) {
        if (it->second.fileNum == fileNum)
            it = lineMap.erase(it);
        else {
            if( it->second.fileNum > fileNum ) {
                it->second.fileNum = it->second.fileNum-1;
            }
            ++it;
        }
    }
}

void Listing::addFile(std::string filename, int page) {
    std::ifstream myfile(filename);
    if(!myfile) {
        std::cout << "Listing file does not exist: " << filename << std::endl;
        exit(1);
    }
    std::string text;
    std::vector<Line> lines;

    unsigned int fileNum = files.size();

    std::size_t pos = filename.find_last_of(filePathSeparator);
    std::string shortname = filename;

    if( pos != std::string::npos && ((pos+2) < filename.length()) ) {
        shortname = filename.substr(pos+1);
    }
    Source source = {shortname, filename, fileNum, page};

    sources.push_back(source);

    uint16_t address = 0;
    bool foundAddress = false;
    unsigned int lineNum = 0;
    unsigned int addressLine = 0;

    while (std::getline(myfile, text)) {
        lineNum++;

        Line line = {};
        line.text = text;

        if( text.length() == 0 ) {
            std::cout << "Ending on line " << lineNum << std::endl;
            break;
        }

        ltrim(text);
        std::regex matcher = std::regex(addressRegex, std::regex::icase);
        std::smatch match;
        if(std::regex_search(text, match, matcher)) {
            uint16_t nextAddress = std::stoi(match.str(2), nullptr, 16);

            if( foundAddress && (nextAddress != address) ) {
                Location loc = {fileNum, addressLine, true};
                lineMap.emplace((page << 16) | address, loc);
            }
            
            foundAddress = true;
            address = nextAddress;
            addressLine = lineNum;

            line.address = address;
            line.head = text.substr(0, match.position(2));

            int byteCount = 0;
            unsigned int bytePos = match.position(2)+match.length(2);

            while(byteCount < 4 && ((bytePos+2) < text.length()) && (text[bytePos] == ' ') && isxdigit(tolower(text[bytePos+1])) && isxdigit(tolower(text[bytePos+2]))) {
                line.bytes[byteCount++] = (fromHex(text[bytePos+1]) << 4) | fromHex(text[bytePos+2]);
                bytePos+=3;
            }
            line.byteCount = byteCount;

            if( byteCount > 0 ) {
                line.isData = true;

                while(bytePos < text.length() ) {
                    if( !isspace(text[bytePos++]) ) {
                        line.isData = false;
                        break;
                    }
                }
            }
        }
        else {
            std::cout << "No match on line " << lineNum << ": " << text << std::endl;
        }

        lines.push_back(line);
    }
    myfile.close();
    
    if( foundAddress ) {
        Location loc = {fileNum, addressLine, true};
        lineMap.emplace(address, loc);
    }
    std::cout << "Parsed listing, file "<< filename <<" for page " << page << " has " << lineNum << " lines." << std::endl;
    files.push_back(lines);
}

int Listing::fromHex(char c) {
    if (c>='0' && c<='9')
        return c-'0';
    if (c>='a' && c<='f')
        return 10+(c-'a');
    if (c>='A' && c<='F')
        return 10+(c-'A');
    return -1; // unexpected char
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

std::pair<Listing::Line, bool> Listing::getLine(Location location) {
    if( location.valid && location.fileNum < files.size() && location.lineNum < files[location.fileNum].size()) {
        return std::pair<Listing::Line, bool>( files[location.fileNum][location.lineNum], true );
    }
    return std::pair<Listing::Line, bool>(Listing::Line {}, false);
}

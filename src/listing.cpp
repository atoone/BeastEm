#include "listing.hpp"
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

std::vector<Listing::Source>& Listing::getFiles() {
    return sources;
}

void Listing::removeFile(unsigned int fileNum) {
    sources.erase(sources.begin() + fileNum);

    for( auto source: sources ) {
        if( source.fileNum > fileNum ) {
            source.fileNum = source.fileNum-1;
        }
    }
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

int Listing::addFile(std::string filename, int page, bool watch) {
    std::ifstream myfile(filename);
    if(!myfile) {
        std::cout << "Listing file does not exist: " << filename << std::endl;
        exit(1);
    }
    bool isValid = isValidListing(myfile);
    myfile.close();

    if(!isValid) return -1;

    unsigned int fileNum = sources.size();

    std::size_t pos = filename.find_last_of(filePathSeparator);
    std::string shortname = filename;

    if( pos != std::string::npos && ((pos+2) < filename.length()) ) {
        shortname = filename.substr(pos+1);
    }
    Source source = {shortname, filename, fileNum, page, {}, watch};

    sources.push_back(source);
    return sources.size()-1;
}

bool Listing::isValidFile(std::string filename) {
    std::ifstream myfile(filename);
    if(!myfile) {
        return false;
    }
    bool isValid = isValidListing(myfile);
    myfile.close();
    return isValid;
}

bool Listing::isValidListing(std::ifstream& stream) {
    char buffer[4096];
    stream.read(buffer, sizeof(buffer));
    int length = stream.gcount();

    stream.seekg(0, std::ios::beg);

    if( length < 5 ) {
        return false;
    }
    int lineLength = 0;

    for( int i=0; i<length; i++ ) {
        if(buffer[i] == '\0') return false;

        if(buffer[i] == '\n' || buffer[i] == '\r' ) {
            if( lineLength > 300 ) {
                return false;
            }
            lineLength = 0;
        }
        else {
            lineLength++;
        }
    }

    if( lineLength > 300 ) {
        return false;
    }

    return true;
}

void Listing::loadFile(Source &source) {

    source.lines.clear();
    std::cout << "Clearing old lines " << source.fileNum << std::endl;
    for( auto it = lineMap.begin(); it != lineMap.end();) {
        if (it->second.fileNum == source.fileNum) {
            it = lineMap.erase(it);
        }
        else {
            ++it;
        }
    }
    std::cout << "Lines cleared" << std::endl;

    uint32_t address = 0;
    bool foundAddress = false;
    unsigned int lineNum = 0;
    unsigned int addressLine = 0;

    std::string text;

    source.lastRead = std::filesystem::last_write_time(source.filename);
    std::ifstream myfile(source.filename);

    if( !isValidListing(myfile) ) {
        std::cout << "File '" << source.filename << "' does not appear to be a valid listing. Did you mean to load a binary file with '-f'?" << std::endl;
        return;
    }

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
            uint32_t nextAddress = std::stoi(match.str(2), nullptr, 16);

            if( foundAddress && (nextAddress != address) ) {
                Location loc = {source.fileNum, addressLine, true};
                lineMap.emplace((source.page << 16) | address, loc);
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

        source.lines.push_back(line);
    }
    myfile.close();
    
    if( foundAddress ) {
        Location loc = {source.fileNum, addressLine, true};
        lineMap.emplace((source.page << 16) | address, loc);
    }
    std::cout << "Parsed listing, file "<< source.filename <<" for page " << source.page << " has " << lineNum << " lines." << std::endl;
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

size_t Listing::fileCount() {
    return sources.size();
}

Listing::Location Listing::getLocation(uint32_t address) {
    auto search = lineMap.find(address);
    if (search != lineMap.end()) {
        return search->second;
    }

    return Location() = {0,0,false};
}

std::pair<Listing::Line, bool> Listing::getLine(Location location) {
    if( location.valid && location.fileNum < sources.size() && location.lineNum < sources[location.fileNum].lines.size()) {
        return std::pair<Listing::Line, bool>( sources[location.fileNum].lines[location.lineNum], true );
    }
    return std::pair<Listing::Line, bool>(Listing::Line {}, false);
}

bool Listing::isWatched(Source &file) {
    return file.watch;
}

bool Listing::isUpdated(Source &file) {
    if( file.watch ) {
        return file.lastRead < std::filesystem::last_write_time(file.filename);
    }
    return false;
}

void Listing::toggleWatch(Source &file) {
    file.watch = !file.watch;
}

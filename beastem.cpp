#include <iostream>
#include <sstream>
#include <fstream>
#include <SDL.h>
#include <regex>
#define CHIPS_IMPL
#include "src/z80.h"
#include "src/z80pio.h"
#include "src/uart16c550.h"
#include "src/beast.hpp"
#include "src/i2c.hpp"
#include "src/display.hpp"
#include "src/rtc.hpp"
#include "src/listing.hpp"

/* Using Floooh Chips Z80 cycle stepped emulation from :
 *  https://github.com/floooh/chips/blob/master/chips/z80.h
 */
const int WIDTH = 800, HEIGHT = 600;

const int ONE_KILOHERTZ = 1000; // 1 KHz
const int DEFAULT_SPEED = 8000;

const int ROM_SIZE = (1<<19);
const int RAM_SIZE = (1<<19);

void readBinary(int offset, char *filename, Beast &beast) {
    std::ostringstream ss;
    ss << std::hex << offset;
    std::string destStr = ss.str();

    bool isRom = offset < ROM_SIZE;
    int space = isRom ? (ROM_SIZE-offset) : (RAM_SIZE+ROM_SIZE-offset);

    std::ifstream ifs(filename, std::ios::binary|std::ios::ate);
    std::ifstream::pos_type pos = ifs.tellg();

    int length = pos;
    if( length > space) {
        std::cout << "Binary file is too big for " << (isRom? "ROM": "RAM") << " starting at 0x" << destStr << ". Actual size: " << (length/1024) << "K" << std::endl;
        exit(1);
    }

    ifs.seekg(0, std::ios::beg);
    if( isRom ) {
        ifs.read((char *)beast.getRom()+offset, length);
    }
    else {
        ifs.read((char *)beast.getRam()+(offset-RAM_SIZE), length);
    }
    ifs.close();

    std::cout << "Read file '" << filename << "' (" << (length/1024) << "K) to " << (isRom? "ROM": "RAM") << " starting at 0x" << destStr << std::endl;
}

bool isHexNum(char* value) {
    std::regex matcher = std::regex("[0-9a-f]+", std::regex::icase);
    std::smatch match;
    std::string str(value);
    return std::regex_search(str, match, matcher);
}

bool isNum(char* value) {
    std::regex matcher = std::regex("[0-9]+", std::regex::icase);
    std::smatch match;
    std::string str(value);
    return std::regex_search(str, match, matcher);
}


void printHelp() {
    std::cout << "Usage: beastem <options>" << std::endl;
    std::cout << "Options are:" << std::endl;
    std::cout << "   -f [offset] <binary filename>  : Read binary file into memory at address <offset> (hex)" << std::endl;
    std::cout << "   -l [page] <listing file>       : Read assembly listing file for code in page <page> (hex)" << std::endl;
    std::cout << "   -a <audio-device-num>          : Override the default audio device selection" << std::endl;
    std::cout << "   -v <Audio volume>              : Value 0 to 10 (default 5)" << std::endl;
    std::cout << "   -k <CPU speed>                 : Integer KHz (default 8000)" << std::endl;
    std::cout << "   -b <breakpoint>                : stop at address (hex)" << std::endl;
}

struct BIN_FILE {
    char *filename;
    int  address;
}; 
int main( int argc, char *argv[] ) {

    int targetSpeed = DEFAULT_SPEED;
    int audioDevice = -1;
    int volume = 5;
    uint64_t breakpoint = Beast::NO_BREAKPOINT;
    Listing listing;

    std::vector<BIN_FILE> binaries;

    int index = 1;
    while( index < argc ) {
        if( strcmp(argv[index], "-f") == 0 ) {
            if( index+1 >= argc ) {
                std::cout << "Read binary file: missing arguments. Expected (optional) address offset and filename " << (index+2) << " >= " << argc << std::endl;
                printHelp();
                exit(1);
            }

            char *first = argv[++index];
            if(isHexNum(first)) {
                if( index+1 < argc ) {
                    int offset = std::stoi(first, nullptr, 16);
                    char *filename = argv[++index];
                    binaries.push_back(BIN_FILE{filename, offset});
                }
                else {
                    std::cout << "Read binary file: missing arguments. Expected address offset and filename but just had offset '" << first << "'" << std::endl;
                    printHelp();
                    exit(1);
                }
            }
            else {
                binaries.push_back(BIN_FILE{first, 0});
            }
                
        }
        else if( strcmp(argv[index], "-l") == 0 ) {
            if( index+1 >= argc ) {
                std::cout << "Read listing: missing arguments. Expected (optional) page and filename " << std::endl;
                printHelp();
                exit(1);
            }

            char *first = argv[++index];
            if(isHexNum(first)) {
                if( index+1 < argc ) {
                    int page = std::stoi(first, nullptr, 16);
                    char *filename = argv[++index];
                    listing.addFile(filename, page);
                }
                else {
                    std::cout << "Read listing: missing arguments. Expected page and filename but just had page '" << first << "'" << std::endl;
                    printHelp();
                    exit(1);
                }
            }
            else {
                listing.addFile(first, 0);
            }
        }
        else if( strcmp(argv[index], "-v") == 0 ) {
            if( index+1 >= argc || !isNum(argv[++index]) ) {
                std::cout << "Volume: missing argument. Expected digit 0-10 " << std::endl;
                printHelp();
                exit(1);
            }
            volume = std::stoi(argv[index], nullptr, 10);
        }
        else if( strcmp(argv[index], "-k") == 0 ) {
            if( index+1 >= argc || !isNum(argv[++index]) ) {
                std::cout << "CPU Speed: missing argument. Expected kilohertz speed, eg. 8000" << std::endl;
                printHelp();
                exit(1);
            }
            targetSpeed = std::stoi(argv[index], nullptr, 10);
        }
        else if( strcmp(argv[index], "-b") == 0 ) {
            if( index+1 >= argc || !isHexNum(argv[++index]) ) {
                std::cout << "Breakpoint: missing argument. Expected breakpoint address in hex." << std::endl;
                printHelp();
                exit(1);
            }
            breakpoint = std::stoi(argv[index], nullptr, 16);
        }
        else if( strcmp(argv[index], "-h") == 0 ) {
            printHelp();
            exit(1);
        }
        else {
            std::cout << "** Unknown option: " << argv[index] << std::endl;
            printHelp();
            exit(1);
        }
        index++;
    }

    SDL_Init( SDL_INIT_EVERYTHING );

    SDL_Window *window = SDL_CreateWindow("Feersum MicroBeast Emulator (Alpha) v0.1", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);

    if( NULL == window ) {
        std::cout << "Could not create window: " << SDL_GetError() << std::endl;
        return 1;
    }


    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if( !renderer) {
        std::cout << "Could not create renderer: " << SDL_GetError() << std::endl;
        return 1;
    }

    Beast beast = Beast(renderer, WIDTH, HEIGHT, listing);
    
    for(auto bf: binaries) {
        readBinary(bf.address, bf.filename, beast);
    }

    beast.init(targetSpeed*ONE_KILOHERTZ, breakpoint, audioDevice, volume);



    beast.mainLoop();

    SDL_DestroyWindow( window );
    SDL_Quit();

    return EXIT_SUCCESS;
}
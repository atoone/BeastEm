#include <iostream>
#include <sstream>
#include <fstream>
#include <SDL.h>
#include <SDL_net.h>
#include <regex>
#define CHIPS_IMPL
#include "src/z80.h"
#include "src/z80pio.h"
#include "src/uart16c550.h"
#include "src/beast.hpp"
#include "src/binaryFile.hpp"
#include "src/videobeast.hpp"
#include "src/i2c.hpp"
#include "src/display.hpp"
#include "src/rtc.hpp"
#include "src/listing.hpp"
#include "nfd.h"

/* Using Floooh Chips Z80 cycle stepped emulation from :
 *  https://github.com/floooh/chips/blob/master/chips/z80.h
 */
const int WIDTH = 800, HEIGHT = 610;

const int ONE_KILOHERTZ = 1000; // 1 KHz
const int DEFAULT_SPEED = 8000;

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

bool isFloatNum(char* value) {
    std::regex matcher = std::regex("[0-9\\.]+", std::regex::icase);
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
    std::cout << "   -s <audio-sample-rate>         : Override the default audio sample rate (22050)" << std::endl;
    std::cout << "   -v <Audio volume>              : Value 0 to 10 (default 5)" << std::endl;
    std::cout << "   -k <CPU speed>                 : Integer KHz (default 8000)" << std::endl;
    std::cout << "   -b <breakpoint>                : Stop at address (hex)" << std::endl;
    std::cout << "   -z <zoom-level>                : Zoom the user interface by the given value" << std::endl;
    std::cout << "   -d <filename> | -d2 <filename> : Start VideoBeast with the given file in video ram" << std::endl;              
}

int main( int argc, char *argv[] ) {

    int targetSpeed = DEFAULT_SPEED;
    int audioDevice = -1;
    int sampleRate = Beast::AUDIO_FREQ;
    int volume = 4;
    float zoom = 1.0;
    
    uint64_t breakpoint = Beast::NOT_SET;
    Listing listing;
    VideoBeast *videoBeast = nullptr;

    std::vector<BinaryFile> binaries;

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
                    unsigned int offset = std::stoi(first, nullptr, 16);
                    binaries.push_back(BinaryFile(argv[++index], offset));
                }
                else {
                    std::cout << "Read binary file: missing arguments. Expected address offset and filename but just had offset '" << first << "'" << std::endl;
                    printHelp();
                    exit(1);
                }
            }
            else {
                binaries.push_back(BinaryFile(first, 0));
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
                    std::string *filename = new std::string( argv[++index] );
                    listing.addFile(*filename, page);
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
        else if( strcmp(argv[index], "-d") == 0 || strcmp(argv[index], "-d2") == 0) {
            if( index+1 >= argc ) {
                std::cout << "Display: missing argument. Expected intitial data for VideoBeast" << std::endl;
                printHelp();
                exit(1);
            }
            float zoom = strcmp(argv[index], "-d") == 0 ? 1.0 : 2.0;

            videoBeast = new VideoBeast(zoom);
            binaries.push_back(BinaryFile(argv[++index], 0, BinaryFile::VIDEO_RAM));
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
        else if( strcmp(argv[index], "-a") == 0 ) {
            if( index+1 >= argc || !isNum(argv[++index]) ) {
                std::cout << "Audio Device: expected integer device ID" << std::endl;
                printHelp();
                exit(1);
            }
            audioDevice = std::stoi(argv[index], nullptr, 10);
        }
        else if( strcmp(argv[index], "-s") == 0 ) {
            if( index+1 >= argc || !isNum(argv[++index]) ) {
                std::cout << "Sample Rate: expected integer audio sample rate" << std::endl;
                printHelp();
                exit(1);
            }
            sampleRate = std::stoi(argv[index], nullptr, 10);
        }
        else if( strcmp(argv[index], "-z") == 0 ) {
            if( index+1 >= argc || !isFloatNum(argv[++index]) ) {
                std::cout << "Zoom: expected numeric scale value" << std::endl;
                printHelp();
                exit(1);
            }
            zoom = std::stof(argv[index], nullptr);
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

    NFD_Init();
    SDL_Init( SDL_INIT_EVERYTHING );

    SDL_Window *window = SDL_CreateWindow("Feersum MicroBeast Emulator v1.1 rc1", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH*zoom, HEIGHT*zoom, SDL_WINDOW_ALLOW_HIGHDPI);

    if( NULL == window ) {
        std::cout << "Could not create window: " << SDL_GetError() << std::endl;
        return 1;
    }

    if (SDLNet_Init() == -1) {
        std::cout << "SDLNet_Init error: " << SDLNet_GetError() << std::endl;
    }

    if( (binaries.size() == 0 || (binaries.size() == 1 && binaries[0].getDestination() == BinaryFile::VIDEO_RAM)) && listing.fileCount() == 0 ) {
        std::cout << "No file or listing arguments, loading firmware" << std::endl;
        listing.addFile("firmware.lst", 0);
        listing.addFile("monitor.lst", 35);
        binaries.push_back(BinaryFile("flash_v1.5.bin", 0));
    }

    Beast beast = Beast(window, WIDTH, HEIGHT, zoom, listing, binaries);
 
    beast.init(targetSpeed*ONE_KILOHERTZ, breakpoint, audioDevice, volume, sampleRate, videoBeast);

    beast.mainLoop();

    SDL_DestroyWindow( window );
    SDL_Quit();

    return EXIT_SUCCESS;
}
#pragma once
#include <set>
#include <vector>
#include "SDL.h"
#include "SDL_ttf.h"
#include "SDL2_gfxPrimitives.h"

#include "z80.h"
#include "z80pio.h"
#include "digit.hpp"
#include "i2c.hpp"
#include "display.hpp"
#include "gui.hpp"
#include "rtc.hpp"
#include "uart16c550.h"
#include "listing.hpp"
#include "binaryFile.hpp"
#include "instructions.hpp"
#include "videobeast.hpp"
#include "debugmanager.hpp"
#include "pagemap.hpp"

#define BEAST_IO_MASK (Z80_M1|Z80_IORQ|Z80_A7|Z80_A6|Z80_A5|Z80_A4)

#define BEAST_IO_SEL_PINS (Z80_IORQ)

#define PIO_SEL_MASK  (BEAST_IO_MASK)
#define PIO_SEL_PINS  (BEAST_IO_SEL_PINS|Z80_A4)

class Beast {

    enum Modifier {NONE, CTRL, SHIFT, CTRL_SHIFT, SHIFT_SWAP};

    enum Mode {RUN, STEP, OUT, OVER, TAKE, DEBUG, FILES, BREAKPOINTS, WATCHPOINTS, QUIT};

    enum StopReason {STOP_NONE, STOP_STEP, STOP_BREAKPOINT, STOP_WATCHPOINT, STOP_ESCAPE};

    enum Selection {SEL_PC, SEL_A, SEL_HL, SEL_BC, SEL_DE, SEL_FLAGS, SEL_SP, SEL_IX, SEL_IY,
        SEL_PAGING, SEL_PAGE0, SEL_PAGE1, SEL_PAGE2, SEL_PAGE3,
        SEL_A2, SEL_HL2, SEL_BC2, SEL_DE2,
        SEL_MEM0, SEL_VIEWPAGE0, SEL_VIDEOVIEW0, SEL_VIEWADDR0, SEL_MEM1, SEL_VIEWPAGE1, SEL_VIDEOVIEW1, SEL_VIEWADDR1, SEL_MEM2, SEL_VIEWPAGE2, SEL_VIDEOVIEW2, SEL_VIEWADDR2,
        SEL_VOLUME,
        SEL_LISTING,
        SEL_BREAKPOINT,
        SEL_BP_LIST, SEL_BP_ADDRESS,
        SEL_WP_LIST, SEL_WP_ADDRESS, SEL_WP_RANGE, SEL_WP_TYPE,
        SEL_END_MARKER };

    enum MemView {MV_PC, MV_SP, MV_HL, MV_BC, MV_DE, MV_IX, MV_IY, MV_Z80, MV_MEM, MV_VIDEO};

    enum VideoView{VV_RAM, VV_REG, VV_PAL1, VV_PAL2, VV_SPR};

    static const int PROMPT_SOURCE_FILE    = 1;
    static const int PROMPT_BINARY_FILE    = 2;
    static const int PROMPT_LISTING        = 3;
    static const int PROMPT_BINARY_CPU     = 4;
    static const int PROMPT_BINARY_PAGE    = 5;
    static const int PROMPT_BINARY_ADDRESS = 6;
    static const int PROMPT_BINARY_PAGE2   = 7; 
    static const int PROMPT_BINARY_VIDEO   = 8;
    static const int PROMPT_WRITE_ADDRESS  = 9; 
    static const int PROMPT_WRITE_LENGTH   = 10;

    static const int ROM_SIZE = 1<<19;
    static const int RAM_SIZE = 1<<19;

    struct BeastKey {
        SDL_KeyCode key;
        int row;
        int col;
        Modifier mod;
    };


    public:
        Beast(SDL_Window *window, int screenWidth, int screenHeight, float zoom, Listing &listing, std::vector<BinaryFile> files);
        ~Beast();

        void init(uint64_t targetSpeedHz, uint64_t breakpoint, int audioDevice, int volume, int sampleRate, VideoBeast *videoBeast);
        void reset();
        void mainLoop();
        uint64_t run(bool run, uint64_t tickCount);

        uint8_t *getRom();
        uint8_t *getRam();

        void keyDown(SDL_Keycode keyCode);
        void keyUp(SDL_Keycode keyCode);
        void onDraw();

        uint8_t readKeyboard(uint16_t port);

        Digit* getDigit(int index);
        
        void loadSamples(Sint16 *stream, int length);

        static const int AUDIO_FREQ = 16000;
        static const int AUDIO_BUFFER_SIZE = 16384;
        static const int BYTES_PER_SAMPLE = 2;

        static const uint64_t ONE_SECOND_PS = UINT64_C(1000000000000);
        static const uint64_t UART_CLOCK_HZ = UINT64_C(1843200);

        static const uint64_t NOT_SET = 0xFFFFFFFFULL;

    private:
        SDL_Window    *window;
        SDL_Renderer  *sdlRenderer;
        SDL_Texture   *keyboardTexture;
        SDL_Texture   *pcbTexture;
        uint32_t      windowId;

        uint8_t       rom[ROM_SIZE]; // 512K rom
        uint8_t       ram[RAM_SIZE]; // 512K ram
        uint8_t*      videoRam = {0};

        uint8_t                 memoryPage[4];
        Listing                &listing;
        std::vector<BinaryFile> binaryFiles;
        GUI                     gui;

        const char* PCB_IMAGE="layout_2d.png";
        const char* DEFAULT_VIDEO_FILE="videobeast.dat";

        TTF_Font *font, *smallFont, *midFont, *indicatorFont;
        int screenWidth, screenHeight;
        float zoom = 1.0f;

        Mode    mode = DEBUG;
        int     selection = 0;
        int     fileActionIndex = -1;
        int     breakpointSelection = 0;
        bool    breakpointEditMode = false;
        int     watchpointSelection = 0;
        bool    watchpointEditMode = false;
        int     watchpointEditField = 0;  // 0=address, 1=range, 2=type
        bool    watchpointAddMode = false;  // true when adding new, false when editing existing
        uint32_t watchpointEditAddress = 0;
        uint16_t watchpointEditRange = 1;
        bool     watchpointEditOnRead = false;
        bool     watchpointEditOnWrite = true;
        bool     watchpointEditIsPhysical = false;

        z80_t    cpu;
        z80pio_t pio;
        uart_t     uart;
        Instructions *instr;

        I2c      *i2c;
        I2cDisplay *display1;
        I2cDisplay *display2;
        I2cRTC     *rtc;

        VideoBeast *videoBeast;
        uint64_t   nextVideoBeastTickPs;

        DebugManager *debugManager;

        // Stop reason tracking for debug display
        StopReason stopReason = STOP_NONE;
        uint16_t   watchpointTriggerAddress = 0;  // Address of instruction that caused WP trigger
        int        watchpointTriggerIndex = -1;   // Which WP (0-7) was triggered
        int        breakpointTriggerIndex = -1;   // Which BP (0-7) was triggered
        uint16_t   currentInstructionPC = 0;      // PC at start of current instruction (for accurate WP trigger address)

        uint64_t pins;
        uint8_t portB;
        uint64_t clock_cycle_ps;
        uint64_t clock_time_ps  = 0;
        uint64_t targetSpeedHz;

        bool     romOperation = false;
        uint8_t  romSequence = 0;
        uint8_t  romOperationMask = 0x80;
        uint64_t romCompletePs = 0;

        const uint64_t ROM_BYTE_WRITE_PS = 20 * 1000000ULL; 
        const uint64_t ROM_CHIP_ERASE_PS = 100000 * 1000000ULL;
        const uint64_t ROM_SECTOR_ERASE_PS = 25000 * 1000000ULL;


        bool       pagingEnabled = false;
        uint8_t    readMem(uint16_t address);
        uint8_t    readPage(int page, uint16_t address);
        void       writeMem(int page, uint16_t address, uint8_t data);

        MemView    memView[3] = {MV_PC, MV_SP, MV_HL};
        uint16_t   memAddress[3] = {0};
        uint16_t   memPageAddress[3] = {0};
        uint8_t    memViewPage[3] = {0};
        uint32_t   memVideoAddress[3][5] = {0};
        VideoView  memVideoView[3] = {VV_RAM, VV_RAM, VV_RAM};

        uint32_t   memoryEditAddress = 0;
        uint32_t   memoryEditAddressMask = 0;
        int        memoryEditPage = 0;
        int        memoryEditView = 0;

        uint64_t              listAddress = NOT_SET;
        std::vector<uint16_t> decodedAddresses;         // Addresses decoded on screen

        static const int FRAME_RATE = 50;

        int16_t     audioBuffer[AUDIO_BUFFER_SIZE] = {0};
        int16_t     audioLastSample = 0;
        int         audioRead = 0;
        int         audioWrite= 0;
        int         audioAvailable = 0;
        uint64_t    audioSampleRatePs;
        int         volume;
        const char* audioFilename = "audio.raw";
        FILE*       audioFile = nullptr;

        std::string *listingPath = nullptr;

        int         writeDataLength = 0;
        int         writeDataAddress = 0;

        SDL_Renderer* createRenderer(SDL_Window *window);
        void          initVideoBeast();
        float         checkZoomFactor(int screenWidth, int screenHeight, float zoom);
        SDL_Texture*  loadTexture(SDL_Renderer *renderer, const char* filename);
        void          setupAudio(int audioDevice, int sampleRate, int volume);

        void redrawScreen();
        void drawBeast();
        void drawKeys();
        void drawKey(int col, int row, int offsetX, int offsetY, bool pressed);
        int  drawMemoryLayout(int index, int topRow, int id, SDL_Color textColor, SDL_Color bright);

        void displayMem(int x, int y, SDL_Color textColor, uint16_t markAddress, int page);
        void displayVideoMem(int x, int y, SDL_Color textColor, VideoView view, uint32_t markAddress);

        std::string nameFor(MemView view);
        uint32_t  addressFor(int view);
        MemView   nextView(MemView view, int dir);
        VideoView nextVideoView(VideoView view, int dir);

        void updateSelection(int direction, int maxSelection);
        void itemSelect(int direction);
        void startMemoryEdit(int view);
        void updateMemoryEdit(int delta, bool startEdit);
        uint32_t getAddressMask(int view);
        uint32_t getVideoAddress(int index, VideoView view);
        void     setVideoAddress(int index, VideoView view, uint32_t value);
        uint8_t  readVideoMemory(VideoView view, uint32_t address);
        void     writeVideoMemory(VideoView view, uint32_t memoryEditAddress, uint8_t value );
        bool itemEdit();
        void editComplete();

        void fileMenu(SDL_Event windowEvent);
        void debugMenu(SDL_Event windowEvent);
        void breakpointsMenu(SDL_Event windowEvent);
        void drawBreakpoints();
        void watchpointsMenu(SDL_Event windowEvent);
        void drawWatchpoints();
        PageMap pageMap;

        void filePrompt(unsigned int index);
        void sourceFilePrompt();
        void binaryFilePrompt(int promptId);
        void checkWatchedFiles();

        uint8_t loadBinaryPage = 0;

        void onFile();
        void onDebug();
        void promptComplete();
        void reportLoad(size_t bytes);
        void updatePrompt();

        void writeDataPrompt();

        void drawListing(int page, uint16_t address, SDL_Color textColor, SDL_Color highColor, SDL_Color disassColor);
        
        const static int DISPLAY_CHARS = 24;
        const static int DISPLAY_WIDTH = DISPLAY_CHARS * Digit::DIGIT_WIDTH;

        std::vector<Digit> display;

        const int KEY_WIDTH = 64;
        const int KEY_HEIGHT = 64;

        const int KEY_COLS = 12;
        const int KEY_ROWS = 4;

        const int KEYBOARD_WIDTH = KEY_WIDTH*(KEY_COLS+0.5);
        const int KEYBOARD_HEIGHT= KEY_HEIGHT*KEY_ROWS;

        const char* BEAST_FONT="Roboto-Medium.ttf";
        const int FONT_SIZE = 28;
        const int SMALL_FONT_SIZE = 14;
        const int MID_FONT_SIZE = 14;

        // Indicator colors for breakpoints and watchpoints (RGB + Alpha)
        static constexpr uint8_t BP_COLOR_R = 220;
        static constexpr uint8_t BP_COLOR_G = 50;
        static constexpr uint8_t BP_COLOR_B = 50;
        static constexpr uint8_t BP_ALPHA_ENABLED = 255;
        static constexpr uint8_t BP_ALPHA_DISABLED = 100;
        static constexpr uint8_t WP_COLOR_R = 0;
        static constexpr uint8_t WP_COLOR_G = 160;
        static constexpr uint8_t WP_COLOR_B = 160;

        const int MAX_KEYS = 48;
        const char* KEY_CAPS[48] = {"Up", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "Del",
            "Down", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", ":",
            "Shift", "A", "S", "D", "F", "G", "H", "J", "K", "L", ".", "Enter",
            "@","Ctrl", "Z", "X", "C", "V", "B", "N", "M", "Space", "Left", "Right"};

        const char* KEY_CAPS_SHIFT[48] = {"", "!", "\"", "#", "$", "%", "^", "&", "*", "(", ")", "",
            "", "", "", "", "", "", "", "", "", "", "", ";",
            "", "", "", "", "", "", "", "", "", "", ",", "",
            "@","", "", "", "", "", "", "", "", "", "", ""};

        const char* KEY_CAPS_CTRL[48] = {"", "|", "'", "", "", "", "{", "}", "`", "[", "]", "",
            "", "", "", "", "", "", "", "", "+", "=", "-", "",
            "", "", "", "", "", "", "", "<", "@", ">", "_", "",
            "@","", "", "", "", "", "\\", "?", "/", "", "", ""};

        const int KEY_INDENTS[4] = {0, KEY_WIDTH/6, KEY_WIDTH/5, -2*KEY_WIDTH/3};

        bool changed = true;

        static const int KEY_MAP_LENGTH = 59;

        const BeastKey KEY_MAP[KEY_MAP_LENGTH] = {
            BeastKey{SDLK_UP, 0, 0, NONE},
            BeastKey{SDLK_1, 0, 1, NONE},
            BeastKey{SDLK_2, 0, 2, NONE},
            BeastKey{SDLK_3, 0, 3, NONE},
            BeastKey{SDLK_4, 0, 4, NONE},
            BeastKey{SDLK_5, 0, 5, NONE},
            BeastKey{SDLK_6, 0, 6, NONE},
            BeastKey{SDLK_7, 0, 7, NONE},
            BeastKey{SDLK_8, 0, 8, NONE},
            BeastKey{SDLK_9, 0, 9, NONE},
            BeastKey{SDLK_0, 0, 10, NONE},
            BeastKey{SDLK_BACKSPACE, 0, 11, NONE},

            BeastKey{SDLK_DOWN, 1, 0, NONE},
            BeastKey{SDLK_q, 1, 1, NONE},
            BeastKey{SDLK_w, 1, 2, NONE},
            BeastKey{SDLK_e, 1, 3, NONE},
            BeastKey{SDLK_r, 1, 4, NONE},
            BeastKey{SDLK_t, 1, 5, NONE},
            BeastKey{SDLK_y, 1, 6, NONE},
            BeastKey{SDLK_u, 1, 7, NONE},
            BeastKey{SDLK_i, 1, 8, NONE},
            BeastKey{SDLK_o, 1, 9, NONE},
            BeastKey{SDLK_p, 1, 10, NONE},
            BeastKey{SDLK_SEMICOLON, 1, 11, SHIFT_SWAP},
  
            BeastKey{SDLK_LCTRL, 3, 1, NONE},
            BeastKey{SDLK_a, 2, 1, NONE},
            BeastKey{SDLK_s, 2, 2, NONE},
            BeastKey{SDLK_d, 2, 3, NONE},
            BeastKey{SDLK_f, 2, 4, NONE},
            BeastKey{SDLK_g, 2, 5, NONE},
            BeastKey{SDLK_h, 2, 6, NONE},
            BeastKey{SDLK_j, 2, 7, NONE},
            BeastKey{SDLK_k, 2, 8, NONE},
            BeastKey{SDLK_l, 2, 9, NONE},
            BeastKey{SDLK_PERIOD, 2, 10, NONE},
            BeastKey{SDLK_RETURN, 2, 11, NONE},

            BeastKey{SDLK_LSHIFT, 2, 0, NONE},
            BeastKey{SDLK_RSHIFT, 2, 0, NONE},
            BeastKey{SDLK_z, 3, 2, NONE},
            BeastKey{SDLK_x, 3, 3, NONE},
            BeastKey{SDLK_c, 3, 4, NONE},
            BeastKey{SDLK_v, 3, 5, NONE},
            BeastKey{SDLK_b, 3, 6, NONE},
            BeastKey{SDLK_n, 3, 7, NONE},
            BeastKey{SDLK_m, 3, 8, NONE},
            BeastKey{SDLK_SPACE, 3, 9, NONE},
            BeastKey{SDLK_LEFT, 3, 10, NONE},
            BeastKey{SDLK_RIGHT, 3, 11, NONE},

            BeastKey{SDLK_COMMA, 2, 10, SHIFT},
            BeastKey{SDLK_BACKSLASH, 3, 6, CTRL},
            //BeastKey{SDLK_QUESTION, 3, 7, CTRL},
            BeastKey{SDLK_SLASH, 3, 8, CTRL},
            //BeastKey{SDLK_LESS, 2, 7, CTRL},
            //BeastKey{SDLK_AT, 2, 8, CTRL},
            //BeastKey{SDLK_GREATER, 2, 9, CTRL},
            //BeastKey{SDLK_PLUS, 1, 8, CTRL},
            BeastKey{SDLK_EQUALS, 1, 9, CTRL},
            BeastKey{SDLK_MINUS, 1, 10, CTRL}
        };

        std::set<int> keySet = {}; 

        const int KEY_SHIFT = 24;
        const int KEY_CTRL = 37;


};


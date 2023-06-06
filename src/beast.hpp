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
#include "rtc.hpp"
#include "uart16c550.h"
#include "listing.hpp"
#include "instructions.hpp"

#define BEAST_IO_MASK (Z80_M1|Z80_IORQ|Z80_A7|Z80_A6|Z80_A5|Z80_A4)

#define BEAST_IO_SEL_PINS (Z80_IORQ)

#define PIO_SEL_MASK  (BEAST_IO_MASK)
#define PIO_SEL_PINS  (BEAST_IO_SEL_PINS|Z80_A4)

class Beast {

    enum Modifier {NONE, CTRL, SHIFT, CTRL_SHIFT};

    enum Mode {RUN, STEP, OUT, OVER, TAKE, DEBUG, QUIT};

    enum Selection {SEL_PC, SEL_A, SEL_HL, SEL_BC, SEL_DE, SEL_FLAGS, SEL_SP, SEL_IX, SEL_IY, 
        SEL_PAGING, SEL_PAGE0, SEL_PAGE1, SEL_PAGE2, SEL_PAGE3, 
        SEL_A2, SEL_HL2, SEL_BC2, SEL_DE2, 
        SEL_MEM0, SEL_VIEWPAGE0, SEL_MEM1, SEL_VIEWPAGE1, SEL_MEM2, SEL_VIEWPAGE2, 
        SEL_BREAKPOINT,
        SEL_END_MARKER };

    enum MemView {MV_PC, MV_SP, MV_HL, MV_BC, MV_DE, MV_IX, MV_IY, MV_Z80, MV_MEM};

    struct BeastKey {
        SDL_KeyCode key;
        int row;
        int col;
        Modifier mod;
    };


    public:
        Beast(SDL_Renderer *sdlRenderer, int screenWidth, int screenHeight, float zoom, Listing &listing);
        ~Beast();

        void init(uint64_t targetSpeedHz, uint64_t breakpoint, int audioDevice, int volume, int sampleRate);
        void mainLoop();
        uint64_t run(bool run, uint64_t tickCount);

        uint8_t *getRom();
        uint8_t *getRam();

        void keyDown(SDL_Keycode keyCode);
        void keyUp(SDL_Keycode keyCode);
        void onDraw();

        void onDebug();

        uint8_t readKeyboard(uint16_t port);

        Digit* getDigit(int index);
        
        void loadSamples(Sint16 *stream, int length);

        static const int AUDIO_FREQ = 22050;
        static const int AUDIO_BUFFER_SIZE = 4096;

        static const uint64_t NO_BREAKPOINT = 0xFFFFFFFFULL;
    private:
        SDL_Renderer  *sdlRenderer;
        SDL_Texture   *keyboardTexture;
        TTF_Font *font, *smallFont, *monoFont;
        int screenWidth, screenHeight;
        float zoom = 1.0f;

        Mode    mode = DEBUG;
        int     selection;
        z80_t    cpu;
        z80pio_t pio;
        uart_t     uart;
        Instructions *instr;

        I2c      *i2c;
        I2cDisplay *display1;
        I2cDisplay *display2;
        I2cRTC     *rtc;

        uint64_t pins;
        uint8_t portB;
        uint64_t clock_cycle_ps;
        uint64_t clock_time_ps  = 0;
        uint64_t targetSpeedHz;
        uint64_t breakpoint = 0xF20D;
        uint64_t lastBreakpoint = 0;

        uint8_t rom[(1<<19)]; // 512K rom
        uint8_t ram[(1<<19)]; // 512K ram

        uint8_t memoryPage[4];
        bool    pagingEnabled = false;
        uint8_t readMem(uint16_t address);
        uint8_t readPage(int page, uint16_t address);

        MemView memView[3] = {MV_PC, MV_SP, MV_HL};
        uint16_t memAddress[3] = {0};
        uint16_t memPageAddress[3] = {0};
        uint8_t  memViewPage[3] = {0};

        bool       editMode = false;
        uint16_t   editValue, editOldValue;
        int        editIndex;
        int        editDigits;
        int        editX, editY, editOffset;

        const int COL1 = 50;
        const int COL2 = 190;
        const int COL3 = 330;
        const int COL4 = 470;

        const int ROW1 = 56;
        const int ROW2 = ROW1+16;
        const int ROW3 = ROW2+16;
        const int ROW4 = ROW3+16;
        const int ROW5 = ROW4+16;

        const int ROW7 = ROW5+32;
        const int ROW8 = ROW7+16;

        const int ROW11 = ROW8+40;
        const int ROW12 = ROW11+16;

        const int ROW15 = ROW12+40;
        const int ROW16 = ROW15+16;

        const int ROW19 = ROW16+44;
        const int ROW20 = ROW19+16;

        const int ROW22 = ROW20+28;
        const int END_ROW = ROW22+(13*14);

        Listing &listing;
        Listing::Location currentLoc = {0,0, false};
        std::vector<uint16_t> decodedAddresses;         // Addresses decoded on screen

        static const int FRAME_RATE = 50;

        int16_t     audioBuffer[AUDIO_BUFFER_SIZE] = {0};
        int         audioRead = 0;
        int         audioWrite= 1;
        int         audioAvailable = 0;
        uint64_t    audioSampleRatePs;
        int         volume;
        const char* audioFilename = "audio.raw";
        FILE*       audioFile = nullptr;

        void redrawScreen();
        void drawBeast();
        void drawKeys();
        void drawKey(int col, int row, int offsetX, int offsetY, bool pressed);
        void displayMem(int x, int y, SDL_Color textColor, uint16_t markAddress, int page);
        std::string nameFor(MemView view);
        uint16_t addressFor(int view);
        MemView nextView(MemView view, int dir);
        void updateSelection(int direction, int maxSelection);
        void itemSelect(int direction);
        void startEdit(uint16_t value, int x, int y, int offset, int digits);
        bool itemEdit();
        void editComplete();
        void displayEdit();

        void drawListing(uint16_t address, SDL_Color textColor, SDL_Color highColor);
        template<typename... Args> void print(int x, int y, SDL_Color color, const char *fmt, Args... args);
        template<typename... Args> void print(int x, int y, SDL_Color color, int highlight, SDL_Color background, const char *fmt, Args... args);
        void printb(int x, int y, SDL_Color color, int highlight, SDL_Color background, char* buffer);
        
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
        const char* MONO_FONT = "RobotoMono-VariableFont_wght.ttf";
        const int FONT_SIZE = 28;
        const int SMALL_FONT_SIZE = 14;
        const int MONO_SIZE = 14;

        const int MAX_KEYS = 48;
        const char* KEY_CAPS[48] = {"Up", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "Del",
            "Down", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", ":",
            "Ctrl", "A", "S", "D", "F", "G", "H", "J", "K", "L", ".", "Enter",
            "@","Shift", "Z", "X", "C", "V", "B", "N", "M", "Space", "Left", "Right"};

        const int KEY_INDENTS[4] = {0, KEY_WIDTH/6, KEY_WIDTH/5, -2*KEY_WIDTH/3};

        bool changed = true;

        static const int KEY_MAP_LENGTH = 58;

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
            BeastKey{SDLK_COLON, 1, 11, NONE},
  
            BeastKey{SDLK_LCTRL, 2, 0, NONE},
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

            BeastKey{SDLK_LSHIFT, 3, 1, NONE},
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

            BeastKey{SDLK_SEMICOLON, 1, 11, SHIFT},
            BeastKey{SDLK_COMMA, 2, 10, SHIFT},
            BeastKey{SDLK_BACKSLASH, 3, 5, CTRL},
            BeastKey{SDLK_QUESTION, 3, 6, CTRL},
            BeastKey{SDLK_SLASH, 3, 7, CTRL},
            BeastKey{SDLK_LESS, 2, 7, CTRL},
            BeastKey{SDLK_AT, 2, 8, CTRL},
            BeastKey{SDLK_GREATER, 2, 9, CTRL},
            BeastKey{SDLK_PLUS, 1, 8, CTRL},
            BeastKey{SDLK_EQUALS, 1, 9, CTRL},
            BeastKey{SDLK_MINUS, 1, 10, CTRL}
        };

        std::set<int> keySet = {}; 

        const int KEY_SHIFT = 36;
        const int KEY_CTRL = 24;


};


#pragma once
#include <set>
#include <vector>
#include "SDL.h"

class VideoBeast {

    static const uint8_t IDLE = 255;
    
    static const int MAX_LAYERS = 6;
    static const int MAX_LAYER_TIMES = MAX_LAYERS + 3;

    static const int VIDEO_MODES = 2;
    static const int VIDEO_RAM_LENGTH = 1024*1024;

    static const int REGISTERS_LENGTH = 256;
    static const int PALETTE_LENGTH   = 256;

    static const int MAX_LINE_WIDTH   = 1024;

    static const uint64_t RENDER_CLOCK_PS = 14814ULL;

    static const int SET_UNLOCKED     = 0xF3;

    static const int REG_MODE           = 0xFF;
    static const int REG_LOCKED         = 0xFE;
    static const int REG_BACKGROUND_H   = 0xFD;
    static const int REG_BACKGROUND_L   = 0xFC;
    static const int REG_CURRENT_LINE_H = 0xFB;
    static const int REG_CURRENT_LINE_L = 0xFA;
    static const int REG_PAGE_0         = 0xF9;
    static const int REG_PAGE_1         = 0xF8;
    static const int REG_PAGE_2         = 0xF7;
    static const int REG_PAGE_3         = 0xF6;
    static const int REG_LOWER_REG      = 0xF5;

    static const int REG_MULT_X_L       = 0xE0;
    static const int REG_MULT_X_H       = 0xE1;
    static const int REG_MULT_Y_L       = 0xE2;
    static const int REG_MULT_Y_H       = 0xE3;

    static const int REG_PRODUCT_B0     = 0xE4;
    static const int REG_PRODUCT_B1     = 0xE5;
    static const int REG_PRODUCT_B2     = 0xE6;
    static const int REG_PRODUCT_B3     = 0xE7;

    static const int LAYER_TYPE_NONE    = 0;
    static const int LAYER_TYPE_TEXT    = 1;
    static const int LAYER_TYPE_SPRITE  = 2;
    static const int LAYER_TYPE_TILE    = 3;
    static const int LAYER_TYPE_8BPP    = 4;
    static const int LAYER_TYPE_4BPP    = 5;

    // Layer definition offsets
    static const int REG_OFF_LAYER_TYPE   = 0;
    static const int REG_OFF_LAYER_TOP    = 1;
    static const int REG_OFF_LAYER_BOTTOM = 2;
    static const int REG_OFF_LAYER_LEFT   = 3;
    static const int REG_OFF_LAYER_RIGHT  = 4;
    static const int REG_OFF_LAYER_X_L    = 5;
    static const int REG_OFF_LAYER_XY     = 6;
    static const int REG_OFF_LAYER_Y_L    = 7;

    static const int REG_OFF_TEXT_MAP     = 8;  // Character map base / 16K
    static const int REG_OFF_TEXT_FONT    = 9;  // Font data base / 2K
    static const int REG_OFF_TEXT_PALETTE = 10; // [3:0] - Palette index, [4] - 0: Normal palette. 1: Sinclair palette
    static const int REG_OFF_TEXT_BITMAP  = 11; // 1bpp Bitmap base / 16K   (0 disables)

    static const int REG_OFF_TILE_MAP     = 8;  // Tile map base / 16K
    static const int REG_OFF_TILE_GRAPHIC = 9;  // Graphics base / 32K

    static const int REG_OFF_BITMAP_BASE    = 8;  // Bitmap base / 16K
    static const int REG_OFF_BITMAP_PALETTE = 10; // [3:0] - Palette index

    struct VideoMode {
        int pixelWidth, pixelHeight;
        
        int totalWidth, totalHeight;
        
        uint64_t pixel_clock_ps;
    };

    public:
        VideoBeast(char* initialMemFile, float zoom);
        ~VideoBeast();

        void     init(uint64_t clock_time_ps);

        uint64_t tick(uint64_t clock_time_ps);

        void     write(uint16_t addr, uint8_t data, uint64_t clock_time_ps);
        uint8_t  read(uint16_t addr, uint64_t clock_time_ps);

        void handleEvent(SDL_Event windowEvent);
    
    private:
        uint8_t mem[VIDEO_RAM_LENGTH];

        uint8_t registers[REGISTERS_LENGTH];

        uint32_t palette1[PALETTE_LENGTH];
        uint32_t palette2[PALETTE_LENGTH];

        uint16_t paletteReg1[PALETTE_LENGTH];
        uint16_t paletteReg2[PALETTE_LENGTH];

        // Screen modes
        int mode = 0;
        int nextMode = 0;

        bool isDoubled = false;

        uint64_t next_action_time_ps;
        uint64_t next_line_time_ps;
        uint64_t next_multiply_available_ps;

        uint16_t currentLine = 0;
        uint16_t displayLine = 0;
        uint8_t  currentLayer = IDLE;
        bool drawNextLine = false;

        uint64_t debugFromNs = 0;

        uint64_t layer_times_ps[MAX_LAYER_TIMES];
        int      layer_time_index;
        bool     debug_layers = false;

        float    layer_time_alpha = 0.7;
        
        int layer_col_r[MAX_LAYER_TIMES];
        int layer_col_g[MAX_LAYER_TIMES];
        int layer_col_b[MAX_LAYER_TIMES];

        const VideoMode VIDEO_MODE[VIDEO_MODES] = {
            VideoMode{ 640, 480, 800, 525, 40000ULL }, // 25Mhz pixel clock
            VideoMode{ 848, 480, 1088, 517, 29767ULL}  // 33.594Mhz pixel clock
        };

        SDL_Window *window;
        SDL_Surface *surface;
        SDL_PixelFormat *pixel_format;
        float requestedZoom = 1.0;
        float zoom = 2.0;
        uint32_t windowID;

        uint32_t line_buffer[MAX_LINE_WIDTH];

        uint32_t background;

        // Read a file into graphics ram
        void readMem(char* filename);

        void tickNextFrame();

        void createWindow();
        void updateMode();
        void checkWindow(int width, int height);
        void clearWindow();

        void loadPalette(const char *filename, uint32_t *palette, uint16_t *paletteReg);
        void loadRegisters(const char *filename);

        // Get a colour value for the window surface from a packed RGB palette entry
        uint32_t getColour(uint16_t packedRGB);

        uint64_t drawTextLayer(int layberBase);
        uint64_t drawTileLayer(int layberBase);
        uint64_t drawBppBitmap(int layberBase);
        uint64_t draw4ppBitmap(int layberBase);

        // Sinclair address mode
        uint32_t getSinclairAddress(uint16_t addr);
};

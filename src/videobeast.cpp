#include "videobeast.hpp"
#include <iostream>
#include <fstream>
#include <algorithm> 

VideoBeast::VideoBeast(char *initialMemFile, float zoom) {
    readMem(initialMemFile);
    requestedZoom = zoom;
}

VideoBeast::~VideoBeast() {
}

int VideoBeast::init(uint64_t clock_time_ps, int guiWidth) {
    if( surface == nullptr ) {
        createWindow();
    }

    next_action_time_ps = clock_time_ps;
    next_line_time_ps = clock_time_ps + VIDEO_MODE[mode].totalWidth * VIDEO_MODE[mode].pixel_clock_ps;

    displayLine = VIDEO_MODE[mode].totalHeight-1;
    currentLine = 0;
    currentLayer = IDLE;

    pixel_format = SDL_AllocFormat(SDL_PIXELFORMAT_RGB555);

    loadRegisters("video_registers.mem");
    loadPalette("palette_1.mem", palette1, paletteReg1);
    loadPalette("palette_2.mem", palette2, paletteReg2);

    background = getColour((registers[REG_BACKGROUND_H] << 8) + registers[REG_BACKGROUND_L]);
    clearWindow();

    SDL_DisplayMode display;

    int displayIndex = SDL_GetWindowDisplayIndex(window);
    if( SDL_GetDesktopDisplayMode(displayIndex, &display) == 0) {
        int space = (display.w - guiWidth - (VIDEO_MODE[mode].pixelWidth * zoom))/3;
        if( space < 0 ) space = 0;
        SDL_SetWindowPosition(window, display.w - (VIDEO_MODE[mode].pixelWidth * zoom) - space, SDL_WINDOWPOS_CENTERED);

        return space;
    }
    return -1;
}

uint32_t VideoBeast::getColour(uint16_t packedRGB) {
    uint8_t r,g,b;
    SDL_GetRGB(packedRGB, pixel_format, &r, &g, &b);
    return SDL_MapRGB(surface->format, r, g, b);
}

void VideoBeast::unpackRGB(uint16_t packedRGB, uint8_t *r, uint8_t *g, uint8_t *b) {
    SDL_GetRGB(packedRGB, pixel_format, r, g, b);
}

uint64_t VideoBeast::drawBppBitmap(int layerBase) {
    int scrollY = ((registers[layerBase + REG_OFF_LAYER_XY] & 0xF0) << 4) + registers[layerBase + REG_OFF_LAYER_Y_L];
    int scrollX = ((registers[layerBase + REG_OFF_LAYER_XY] & 0x0F) << 8) + registers[layerBase + REG_OFF_LAYER_X_L];

    int row = ((currentLine - 8*registers[layerBase + REG_OFF_LAYER_TOP]) + scrollY ) & 0x1FF;

    int start = registers[layerBase + REG_OFF_LAYER_LEFT]*8;
    int end = registers[layerBase + REG_OFF_LAYER_RIGHT]*8;

    int baseAddress = (registers[layerBase + REG_OFF_BITMAP_BASE] << 14) + 512*row;

    for( int x=start; x<end; x++ ) {
        int address = baseAddress + ((scrollX++) & 0x1FF);
        line_buffer[x] = palette2[mem[address]];
    }
    return (end-start) * RENDER_CLOCK_PS / 2;
}

uint64_t VideoBeast::draw4ppBitmap(int layerBase) {
    int scrollY = ((registers[layerBase + REG_OFF_LAYER_XY] & 0xF0) << 4) + registers[layerBase + REG_OFF_LAYER_Y_L];
    int scrollX = ((registers[layerBase + REG_OFF_LAYER_XY] & 0x0F) << 8) + registers[layerBase + REG_OFF_LAYER_X_L];

    int row = ((currentLine - 8*registers[layerBase + REG_OFF_LAYER_TOP]) + scrollY ) & 0x1FF;

    int start = registers[layerBase + REG_OFF_LAYER_LEFT]*8;
    int end = registers[layerBase + REG_OFF_LAYER_RIGHT]*8;

    int baseAddress = (registers[layerBase + REG_OFF_BITMAP_BASE] << 14) + 512*row;
    int paletteIndex = (registers[layerBase + REG_OFF_BITMAP_PALETTE] & 0x0F) << 4;

    for( int x=start; x<end; x++ ) {
        int address = baseAddress + ((scrollX >> 1) & 0x1FF);
        if( (scrollX & 0x01) == 0) {
            line_buffer[x] = palette1[paletteIndex + (mem[address] >> 4)];
        }
        else {
            line_buffer[x] = palette1[paletteIndex + (mem[address] & 0x0F)];
        }
        scrollX++;
    }
    return (end-start) * RENDER_CLOCK_PS / 4;
}


uint64_t VideoBeast::drawTextLayer(int layerBase) {
    int scrollY = ((registers[layerBase + REG_OFF_LAYER_XY] & 0xF0) << 4) + registers[layerBase + REG_OFF_LAYER_Y_L];
    int scrollX = ((registers[layerBase + REG_OFF_LAYER_XY] & 0x0F) << 8) + registers[layerBase + REG_OFF_LAYER_X_L];

    int row = ((currentLine - 8*registers[layerBase + REG_OFF_LAYER_TOP]) + scrollY ) & 0x1FF;

    int start = registers[layerBase + REG_OFF_LAYER_LEFT]*8;
    int end = registers[layerBase + REG_OFF_LAYER_RIGHT]*8;

    int mapAddress = (registers[layerBase + REG_OFF_TEXT_MAP] << 14)  + ((row & 0x1F8) << 5); // leftmost column of current row
    int fontAddress = (registers[layerBase + REG_OFF_TEXT_FONT] << 11);

    int paletteIndex = (registers[layerBase + REG_OFF_TEXT_PALETTE] & 0x0F) << 4;

    int discard = (scrollX & 0x07);

    for( int x=start; x<end; ) {
        int address = mapAddress + ((scrollX >> 2) & 0xFE);

        int glyph = mem[address];
        int attributes = mem[address+1];

        uint32_t foreground = palette1[paletteIndex + (attributes >> 4)];
        bool transparentFG  = (paletteReg1[paletteIndex + (attributes >> 4)] & 0x8000) != 0;

        uint32_t background = palette1[paletteIndex + (attributes & 0x0F)];
        bool transparentBG  = (paletteReg1[paletteIndex + (attributes & 0x0F)] & 0x8000) != 0;

        uint8_t pixels = mem[fontAddress + (8*glyph) + (row& 0x7)]; // TODO: 1bpp graphics..

        pixels <<= discard;
        for( int count = 8-discard; count--> 0 && x < end; ) {
            if( (pixels & 0x80) != 0 ) {
                if( !transparentFG ) {
                    line_buffer[x] = foreground;
                }
            }
            else if( !transparentBG ) {
                line_buffer[x] = background;
            }
            pixels <<= 1;
            x++;
        }
        discard = 0;
        scrollX += 8;
    }
    return (end-start) * RENDER_CLOCK_PS / 4;
}

uint64_t VideoBeast::drawTileLayer(int layerBase) {
    int scrollY = ((registers[layerBase + REG_OFF_LAYER_XY] & 0xF0) << 4) + registers[layerBase + REG_OFF_LAYER_Y_L];
    int scrollX = ((registers[layerBase + REG_OFF_LAYER_XY] & 0x0F) << 8) + registers[layerBase + REG_OFF_LAYER_X_L];

    int row = ((currentLine - 8*registers[layerBase + REG_OFF_LAYER_TOP]) + scrollY ) & 0x1FF;

    int start = registers[layerBase + REG_OFF_LAYER_LEFT]*8;
    int end = registers[layerBase + REG_OFF_LAYER_RIGHT]*8;

    int mapAddress = (registers[layerBase + REG_OFF_TILE_MAP] << 14)  + ((row & 0x1F8) << 5); // leftmost column of current row
    int tileAddress = (registers[layerBase + REG_OFF_TILE_GRAPHIC] << 15);

    int discard = (scrollX & 0x07);

    for( int x=start; x<end; ) {
        int address = mapAddress + ((scrollX >> 2) & 0xFE);

        int tile = ((mem[address+1] & 0x03) << 8) + mem[address];
        int paletteIndex = mem[address+1] & 0xF0;

        if( (mem[address+1] & 0x08) == 0 ) {
            int tileBase = tileAddress + (32*tile) + (4 * (row & 0x07));
            uint32_t pixels = ((mem[tileBase] << 24) + (mem[tileBase+1] << 16) + (mem[tileBase+2] << 8) + (mem[tileBase+3])) << (discard*4);

            for( int count = 8-discard; count--> 0 && x < end; ) {
                int colourIdx = paletteIndex + ((pixels >> 28) & 0x0F);
                if( (paletteReg1[colourIdx] & 0x08000) == 0) {
                    line_buffer[x] = palette1[colourIdx];
                }
                pixels <<= 4;
                x++;
            }
        }
        else {
            x += 8-discard;
        }
        discard = 0;
        scrollX += 8;
    }

    return (end-start) * RENDER_CLOCK_PS / 2;
}

void VideoBeast::loadPalette(const char *filename, uint32_t *palette, uint16_t *paletteReg) {
    std::ifstream myfile(filename);
    if(!myfile) {
        std::cout << "Palette file does not exist: " << filename << std::endl;
        exit(1);
    }
    std::string line;
    int idx = 0;

    while (std::getline(myfile, line)) {
        if( idx >= PALETTE_LENGTH ) {
            std::cerr << "WARNING: Palette length is exceeded in file " << filename << std::endl;
            break;
        }
        uint16_t colour555 = std::stoi(line, nullptr, 16);
        paletteReg[idx] = colour555;
        palette[idx++] = getColour(colour555);
    }
    std::cout << "Read " << idx << " entries for palette from file " << filename << std::endl;
    
    myfile.close();
}

void VideoBeast::loadRegisters(const char *filename) {
    std::ifstream myfile(filename);
    if(!myfile) {
        std::cout << "Register file does not exist: " << filename << std::endl;
        exit(1);
    }
    std::string line;
    int idx = REGISTERS_LENGTH-1;

    while (std::getline(myfile, line)) {
        if( line.length() == 0 || line[0] == '-' ) {
            continue;
        }
        if( idx < 0 ) {
            std::cerr << "WARNING: Register length is exceeded in file " << filename << std::endl;
            break;
        }
        uint8_t value = std::stoi(line, nullptr, 16);
        registers[idx--] = value;
    }
    std::cout << "Read " << (REGISTERS_LENGTH-idx-1) << " entries for registers from file " << filename << std::endl;
    
    myfile.close();
}

void VideoBeast::handleEvent(SDL_Event windowEvent)
{
    if( SDL_KEYDOWN == windowEvent.type && windowEvent.window.windowID == windowID ) {
        switch( windowEvent.key.keysym.sym ) {
            case SDLK_d : 
                debug_layers  = !debug_layers;
                break;
            case SDLK_b :
                layer_time_alpha = 1.0-layer_time_alpha;
                break;
        }
    }
}

void VideoBeast::tickNextFrame() {
    drawNextLine = true;
    displayLine = 0;
    currentLine = 0;
    SDL_UpdateWindowSurface(window);
    isDoubled = (registers[REG_MODE] & 0x08) != 0;

    if( mode != (registers[REG_MODE] & 0x7) ) {
        mode = registers[REG_MODE] & 0x7;
        if( mode >= VIDEO_MODES ) {
            std::cout << "Unsupported video mode " << mode << std::endl;
        } 
        else {
            updateMode();
        }
    }
}

uint64_t VideoBeast::tick(uint64_t clock_time_ps) {
    if( clock_time_ps >= next_line_time_ps ) {
        next_line_time_ps += VIDEO_MODE[mode].totalWidth * VIDEO_MODE[mode].pixel_clock_ps;      

        if( ++displayLine >= VIDEO_MODE[mode].totalHeight ) {
            tickNextFrame();
        }
        else if( !isDoubled || ((displayLine & 0x01) == 0)) {
            drawNextLine = true;
            currentLine++;
        }

        if (debug_layers && displayLine <= VIDEO_MODE[mode].pixelHeight ) {
            int screenWidth = VIDEO_MODE[mode].pixelWidth;
            if( isDoubled ) screenWidth /= 2;

            float scale = ((float)screenWidth) / (VIDEO_MODE[mode].totalWidth * VIDEO_MODE[mode].pixel_clock_ps);

            int current = 0;

            for( int i=0; i<screenWidth; i++ ) {
                int r = (int)((line_buffer[i] & 0x0FF) * layer_time_alpha);
                int g = (int)(((line_buffer[i] >> 8) & 0x0FF) * layer_time_alpha);
                int b = (int)(((line_buffer[i] >> 16) & 0x0FF) * layer_time_alpha);

                if( current < MAX_LAYER_TIMES ) {
                    if( layer_times_ps[current] < (i/scale) ) {
                        r = std::min(r + (int)((1.0-layer_time_alpha) * 255), 255);
                        g = std::min(g + (int)((1.0-layer_time_alpha) * 255), 255);
                        b = std::min(b + (int)((1.0-layer_time_alpha) * 255), 255);
                        current++;
                    }
                    else {
                        r = std::min(r + (int)((1.0-layer_time_alpha) * layer_col_r[current]), 255);
                        g = std::min(g + (int)((1.0-layer_time_alpha) * layer_col_g[current]), 255);
                        b = std::min(b + (int)((1.0-layer_time_alpha) * layer_col_b[current]), 255);
                    }
                }

                line_buffer[i] = r + (g<<8) + (b<<16);
            }
        }

        if( displayLine > 0 && displayLine <= VIDEO_MODE[mode].pixelHeight ) {
            int step = (int)(isDoubled ? zoom * 2.0 : zoom);
                
            int dest = (displayLine-1)*surface->pitch * zoom;
            int line = (displayLine-1);

            // TODO: This is a bit nasty - lots of assumptions about bytes per pixel..
            // ;VIDEO_MODE[mode].pixelWidth
            if( debugFromNs != 0 ) {
                std::cout << "Draw line " << (displayLine-1) << " current line " << currentLine << " time " << (clock_time_ps/1000 - debugFromNs) << std::endl;
            }
            for(int row=0; row<zoom; row++) {
                if( line++ < surface->h ) {
                    for(int i=0; i<surface->w; i++) {
                        *(Uint32 *)((Uint8 *)surface->pixels+dest) = line_buffer[i/step];
                        dest += surface->format->BytesPerPixel;
                    }
                }
            }
        }
    }

    if( currentLayer == IDLE ) {
        if( drawNextLine ) {
            if( debugFromNs != 0 ) {
                std::cout << "Clear line " << (displayLine-1) << " current line " << currentLine << " time " << (clock_time_ps/1000 - debugFromNs) << std::endl;
            }
            for( int i=0; i<MAX_LINE_WIDTH; i++ ) {
                line_buffer[i] = background;
            }
            if( debug_layers ) {
                layer_time_index = 0;
                for( int i=0; i<MAX_LAYER_TIMES; i++ ) {
                    layer_times_ps[i] = 0;
                }
                layer_times_ps[0] =  MAX_LINE_WIDTH*RENDER_CLOCK_PS/4;
                layer_col_r[0] = 196;
                layer_col_g[0] = 196;
                layer_col_b[0] = 196;
            }
            currentLayer = 0;
            next_action_time_ps = clock_time_ps + MAX_LINE_WIDTH*RENDER_CLOCK_PS/4;
            drawNextLine = false;
        }
        else {
            next_action_time_ps = next_line_time_ps;
        }
    }
    else if( next_action_time_ps <= clock_time_ps ) {
        next_action_time_ps = clock_time_ps + RENDER_CLOCK_PS*3;

        int debug_colour = 1;

        int layer_base = 0x80 + (16 * currentLayer);
        if( currentLine >= 8* registers[layer_base + REG_OFF_LAYER_TOP] &&
            currentLine <  8*(registers[layer_base + REG_OFF_LAYER_BOTTOM]+1) ) {
            
            debug_colour = 1+registers[ layer_base + REG_OFF_LAYER_TYPE ];

            switch( registers[ layer_base + REG_OFF_LAYER_TYPE ]) {
                case LAYER_TYPE_NONE : 
                    break;
                case LAYER_TYPE_TEXT :
                    next_action_time_ps = clock_time_ps + drawTextLayer(layer_base);
                    break;
                case LAYER_TYPE_TILE : 
                    next_action_time_ps = clock_time_ps + drawTileLayer(layer_base);
                    break;
                case LAYER_TYPE_8BPP :
                    next_action_time_ps = clock_time_ps + drawBppBitmap(layer_base);
                    break;
                case LAYER_TYPE_4BPP : 
                    next_action_time_ps = clock_time_ps + draw4ppBitmap(layer_base);
                    break;
            }
        }

        if( debug_layers ) {
            if( ++layer_time_index < MAX_LAYER_TIMES ) {
                layer_times_ps[layer_time_index] = layer_times_ps[layer_time_index-1] + next_action_time_ps - clock_time_ps;
                layer_col_r[layer_time_index] = (debug_colour & 0x01) ? 255 : 0;
                layer_col_g[layer_time_index] = (debug_colour & 0x02) ? 255 : 0;
                layer_col_b[layer_time_index] = (debug_colour & 0x04) ? 255 : 0;
            }
        }
        currentLayer++;

        if( currentLayer == MAX_LAYERS) {
            currentLayer = IDLE;
        }
    }

    if( next_action_time_ps <= clock_time_ps || next_line_time_ps <= clock_time_ps ) {
        std::cout << "VideoBeast clock sync error" << std::endl;
        next_line_time_ps = clock_time_ps + VIDEO_MODE[mode].totalWidth * VIDEO_MODE[mode].pixel_clock_ps;
        next_action_time_ps = next_line_time_ps;

        return next_line_time_ps;
    }

    if( next_action_time_ps > next_line_time_ps ) {
        std::cout << "Videobeast scan time exceeded on line " << currentLine << std::endl;
    }

    if( next_line_time_ps <= clock_time_ps ) {
        std::cout << "Line time sync error" << std::endl;
    }

    return std::min( next_action_time_ps, next_line_time_ps );
}

void VideoBeast::write(uint16_t addr, uint8_t data, uint64_t clock_time_ps) {
    if( (addr & 0x3FFE) == 0x3FFE ) {
        // Top two registers, always visible
        registers[addr & 0xFF] = data;
    }
    else if( (addr & 0x3F00) == 0x3F00 && registers[REG_LOCKED] == SET_UNLOCKED ) {
        // Register access
        if( (addr & 0xFF) >= 0x80 ) {
            registers[addr & 0xFF] = data;

            if ( (addr & 0x0FF) == REG_MULT_X_L ||
                 (addr & 0x0FF) == REG_MULT_X_H ||
                 (addr & 0x0FF) == REG_MULT_Y_L ||
                 (addr & 0x0FF) == REG_MULT_Y_H) {
                next_multiply_available_ps = clock_time_ps + 12 * RENDER_CLOCK_PS;
            }
        }
        else if( (registers[REG_LOWER_REG] & 0x10) == 0 ) {
            // Palettes
            int idx = ((registers[REG_LOWER_REG] & 0x03) << 6) | ((addr & 0x7F) >> 1);

            if( (registers[REG_LOWER_REG] & 0x04) == 0 ) {
                // Palette 1
                uint16_t val = paletteReg1[idx];

                if ((addr & 0x01) == 0) {
                    paletteReg1[idx] = (val & 0xFF00) | data;
                }
                else {
                    paletteReg1[idx] = (val & 0x0FF) | (data <<8);
                }
                palette1[idx] = getColour(paletteReg1[idx]);
            }
            else {
                // Palette 2
                uint16_t val = paletteReg2[idx];

                if ((addr & 0x01) == 0) {
                    paletteReg2[idx] = (val & 0xFF00) | data;
                }
                else {
                    paletteReg2[idx] = (val & 0x0FF) | (data <<8);
                }
                palette2[idx] = getColour(paletteReg2[idx]);
            }
        }
        else {
            // Sprites
            // TODO
        }
    }
    else {
        // Ram access
        switch( registers[REG_MODE] >> 5) {
            case 0 : 
                mem[ (registers[REG_PAGE_0] << 12) + (addr & 0x3FFF) ] = data;
                break;
            case 1 :
                if ((addr & 0x2000) == 0) { 
                    mem[ (registers[REG_PAGE_0] << 12 ) + (addr & 0x1FFF)] = data;
                }
                else {
                    mem[ (registers[REG_PAGE_1] << 12 ) + (addr & 0x1FFF)] = data;
                }
                break;
            case 2 :
                switch ((addr >> 12) & 0x03 ) {
                    case 0 : mem[ (registers[REG_PAGE_0] << 11 ) + (addr & 0x0FFF)] = data; break;
                    case 1 : mem[ (registers[REG_PAGE_1] << 11 ) + (addr & 0x0FFF)] = data; break;
                    case 2 : mem[ (registers[REG_PAGE_2] << 11 ) + (addr & 0x0FFF)] = data; break;
                    case 3 : mem[ (registers[REG_PAGE_3] << 11 ) + (addr & 0x0FFF)] = data; break;
                    default:
                        std::cout << "VideoBeast 4K low page write error";
                }
                
            case 3 :
                switch ((addr >> 12) & 0x03 ) {
                    case 0 : mem[ 0x80000 | ((registers[REG_PAGE_0] << 11 ) + (addr & 0x0FFF))] = data; break;
                    case 1 : mem[ 0x80000 | ((registers[REG_PAGE_1] << 11 ) + (addr & 0x0FFF))] = data; break;
                    case 2 : mem[ 0x80000 | ((registers[REG_PAGE_2] << 11 ) + (addr & 0x0FFF))] = data; break;
                    case 3 : mem[ 0x80000 | ((registers[REG_PAGE_3] << 11 ) + (addr & 0x0FFF))] = data; break;
                    default:
                        std::cout << "VideoBeast 4K high page write error";
                }
            case 4:
                mem[getSinclairAddress(addr)] = data;
                break;
            default:
                std::cout << "Videobeast unknown page map mode " << (registers[REG_MODE] >> 5) << std::endl;

        } 
    }
}

uint8_t VideoBeast::read(uint16_t addr, uint64_t clock_time_ps) {
    if( (addr & 0x3FFE) == 0x3FFE ) {
        // Top two registers, always visible
        return registers[addr & 0xFF];
    }
    else if( (addr & 0x3F00) == 0x3F00 && registers[REG_LOCKED] == SET_UNLOCKED ) {
        // Register access
        if( (addr & 0xFF) >= 0x80 ) {
            registers[REG_CURRENT_LINE_L] = currentLine & 0x0FF;
            registers[REG_CURRENT_LINE_H] = currentLine >> 8;

            if( next_multiply_available_ps != 0 && next_multiply_available_ps <= clock_time_ps ) {
                int32_t mult_x = ((registers[REG_MULT_X_L] | (registers[REG_MULT_X_H] << 8)) << 16) >> 16;
                int32_t mult_y = ((registers[REG_MULT_Y_L] | (registers[REG_MULT_Y_H] << 8)) << 16) >> 16;

                int32_t product = mult_x * mult_y;
                registers[REG_PRODUCT_B0] = product & 0x0FF;
                registers[REG_PRODUCT_B1] = (product >>  8) & 0x0FF;
                registers[REG_PRODUCT_B2] = (product >> 16) & 0x0FF;
                registers[REG_PRODUCT_B3] = (product >> 24) & 0x0FF;

                next_multiply_available_ps = 0;
            }
            return registers[addr & 0xFF];
        }
        if( (registers[REG_LOWER_REG] & 0x10) == 0 ) {
            // Palettes
            if( (registers[REG_LOWER_REG] & 0x04) == 0 ) {
                // Palette 1
                uint16_t val = paletteReg1[ ((registers[REG_LOWER_REG] & 0x03) << 6) | ((addr & 0x7F) >> 1)];

                return ((addr & 0x01) == 0) ? val & 0x0FF : val >> 8;
            }
            else {
                // Palette 2
                uint16_t val = paletteReg2[ ((registers[REG_LOWER_REG] & 0x03) << 6) | ((addr & 0x7F) >> 1)];

                return ((addr & 0x01) == 0) ? val & 0x0FF : val >> 8;
            }
        }
        else {
            // Sprites
            return 0;
        }
    }
    else {
        // Ram access
        switch( registers[REG_MODE] >> 5) {
            case 0 : 
                return mem[ (registers[REG_PAGE_0] << 12) + (addr & 0x3FFF) ];
            case 1 :
                return ((addr & 0x2000) == 0) ? 
                    mem[ (registers[REG_PAGE_0] << 12 ) + (addr & 0x1FFF)] :
                    mem[ (registers[REG_PAGE_1] << 12 ) + (addr & 0x1FFF)];
            case 2 :
                switch ((addr >> 12) & 0x03 ) {
                    case 0 : return mem[ (registers[REG_PAGE_0] << 11 ) + (addr & 0x0FFF)];
                    case 1 : return mem[ (registers[REG_PAGE_1] << 11 ) + (addr & 0x0FFF)];
                    case 2 : return mem[ (registers[REG_PAGE_2] << 11 ) + (addr & 0x0FFF)];
                    case 3 : return mem[ (registers[REG_PAGE_3] << 11 ) + (addr & 0x0FFF)];
                    default:
                        std::cout << "VideoBeast 4K low page error";
                        return 0;
                }
                
            case 3 :
                switch ((addr >> 12) & 0x03 ) {
                    case 0 : return mem[ 0x80000 | ((registers[REG_PAGE_0] << 11 ) + (addr & 0x0FFF))];
                    case 1 : return mem[ 0x80000 | ((registers[REG_PAGE_1] << 11 ) + (addr & 0x0FFF))];
                    case 2 : return mem[ 0x80000 | ((registers[REG_PAGE_2] << 11 ) + (addr & 0x0FFF))];
                    case 3 : return mem[ 0x80000 | ((registers[REG_PAGE_3] << 11 ) + (addr & 0x0FFF))];
                    default:
                        std::cout << "VideoBeast 4K high page error";
                        return 0;
                }
            case 4:
                return mem[getSinclairAddress(addr)];
            default:
                std::cout << "Videobeast unknown page map mode " << (registers[REG_MODE] >> 5) << std::endl;

        } 
    }

    return 0;
}

uint8_t VideoBeast::readRam(uint32_t address) {
    return mem[ address & (VIDEO_RAM_LENGTH-1) ];
}

uint8_t VideoBeast::readRegister(uint32_t address) {
    return registers[address & (REGISTERS_LENGTH-1)];
}

uint8_t VideoBeast::readPalette(int palette, uint32_t address) {
    uint16_t value = 0;

    if( palette == 1 ) {
        value = paletteReg1[(address >> 1) & (PALETTE_LENGTH-1)];
    }
    else {
        value = paletteReg2[(address >> 1) & (PALETTE_LENGTH-1)];
    }

    return (address & 1) ? value >> 8 : value;
}

uint8_t VideoBeast::readSprite(uint32_t address) {
    return 0; // TODO: Return sprite data
}

void VideoBeast::writeRam(uint32_t address, uint8_t value)  {
    mem[ address & (VIDEO_RAM_LENGTH-1) ] = value;
}

void VideoBeast::writeRegister(uint8_t address, uint8_t value) {
    registers[address] = value;
}

void VideoBeast::writePalette(int palette, uint16_t address, uint8_t value) {
    uint16_t mask = 0x0FF00;
    uint16_t update= value & 0x0FF;

    uint16_t index = (address >> 1) & (PALETTE_LENGTH-1);

    if( address & 1) {
        mask = 0x0FF;
        update <<=8;
    }

    if( palette == 1 ) {
        paletteReg1[index] = (paletteReg1[index] & mask) | update;
    }
    else {
        paletteReg2[index] = (paletteReg2[index] & mask) | update;
    }
}

void VideoBeast::writeSprite(uint16_t address, uint8_t value) {
    // TODO: Update sprite data
}

uint32_t VideoBeast::getSinclairAddress(uint16_t addr) {
    addr = addr & 0x3FFF;
    if( addr < 0x1800 ) {
        // Spectrum bitmap
        return ((registers[REG_PAGE_1] & 0x3F) << 14) + 
               ((registers[REG_PAGE_2] & 0x04) << 11) +
               ((addr & 0x1800)) +                         // Y7:6   -> Bits 14-13
               ((addr & 0xE0  ) << 4) +                    // Y5:3   -> Bits 12-10
               ((addr & 0x700 ) >> 1) +                    // Y2:0   -> Bits 9-7
               ((registers[REG_PAGE_2] & 0x03) << 5) +     //        -> Bits 6-5
               ((addr & 0x1F  ));                          // X4:0   -> Bits 0-4          
    } 
    else if( addr < 0x1B00 ) {
        // Spectrum attributes
        return ((registers[REG_PAGE_0] & 0x3F) << 14) + 
               ((registers[REG_PAGE_2] & 0x04) << 10) +
               ((addr & 0x3E0 ) << 2 ) +                   // Y4:0   -> Bits 11-7
               ((registers[REG_PAGE_2] & 0x03) << 5) +     //        -> Bits 6-5
               ((addr & 0x1F  ));                          // X4:0   -> Bits 0-4     
    }
    else {
        // General ram
        return ((registers[REG_PAGE_3] & 0x3F) << 14) + addr;
    }
}

void VideoBeast::readMem(char* filename) {
    std::ifstream ifs(filename, std::ios::binary|std::ios::ate);
    std::ifstream::pos_type pos = ifs.tellg();

    int length = pos;
    if( length > VIDEO_RAM_LENGTH) {
        std::cout << "Binary file is too big for VideoBeast (maximum 1Mb). Actual size: " << (length/1024) << "K" << std::endl;
        exit(1);
    }
    if( length == 0 ) {
        std::cout << "VideoBeast binary file does not exist: " << filename << std::endl;
        exit(1);
    }

    ifs.seekg(0, std::ios::beg);
    ifs.read((char *)mem, length);
    ifs.close();

    std::cout << "Read VideoBeast file '" << filename << "' (" << (length/1024) << "K)" << std::endl;
}

void VideoBeast::createWindow() {
    int width = VIDEO_MODE[mode].pixelWidth * requestedZoom;
    int height = VIDEO_MODE[mode].pixelHeight * requestedZoom;

    window = SDL_CreateWindow("Feersum VideoBeast Emulator (Beta) v1.0", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_ALLOW_HIGHDPI);

    if( NULL == window ) {
        std::cout << "Could not create window: " << SDL_GetError() << std::endl;
        exit(1);
    }

    windowID = SDL_GetWindowID( window );
    checkWindow(width, height);
}

void VideoBeast::updateMode() {
    int width = VIDEO_MODE[mode].pixelWidth * requestedZoom;
    int height = VIDEO_MODE[mode].pixelHeight * requestedZoom;

    SDL_SetWindowSize(window, width, height);

    checkWindow(width, height);
}

void VideoBeast::checkWindow(int width, int height) {
    surface = SDL_GetWindowSurface(window);
    zoom = requestedZoom;

    if(surface->w != width) {
        float widthScale = (float)surface->w / (float) (width);
        float heightScale = (float)surface->h / (float) (height);

        if(widthScale != heightScale) {
            std::cerr << "WARNING: width scale != height scale" << std::endl;
        }

        zoom *= widthScale;
    }
}

void VideoBeast::clearWindow() {
    uint32_t dest = 0;
    
    for( int y=0; y < surface->h; y++ ) {
        for(int x=0; x<surface->w; x++) {
            *(Uint32 *)((uint8_t*)surface->pixels+dest) = background;
            dest += surface->format->BytesPerPixel;
        }
    }
    SDL_UpdateWindowSurface(window);
}

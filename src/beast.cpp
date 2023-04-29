#include "beast.hpp"
#include <iostream>
#include <cstring>
#include <stdio.h>
#include <iomanip>
#include "z80.h"
#include "z80pio.h"
#include "listing.hpp"

Beast::Beast(SDL_Renderer *renderer, int screenWidth, int screenHeight, float zoom, Listing &listing) 
    : rom {}, ram {}, memoryPage {0}, listing(listing) {

    this->sdlRenderer = renderer;
    this->screenWidth = screenWidth;
    this->screenHeight = screenHeight;
    this->zoom = zoom;

    instr = new Instructions();

    i2c = new I2c(Z80PIO_PB6, Z80PIO_PB7);
    display1 = new I2cDisplay(0x50);
    display2 = new I2cDisplay(0x53);
    rtc = new I2cRTC(0x6f, Z80PIO_PB5);

    i2c->addDevice(display1);
    i2c->addDevice(display2);
    i2c->addDevice(rtc);
    
    TTF_Init();
    font = TTF_OpenFont(BEAST_FONT, FONT_SIZE*zoom);

    if( !font) {
        std::cout << "Couldn't load font "<< BEAST_FONT << std::endl;
        exit(1);
    }
    smallFont = TTF_OpenFont(BEAST_FONT, SMALL_FONT_SIZE*zoom);
    if( !smallFont) {
        std::cout << "Couldn't load font "<< BEAST_FONT << std::endl;
        exit(1);
    }

    monoFont = TTF_OpenFont(MONO_FONT, MONO_SIZE*zoom);

    keyboardTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, KEYBOARD_WIDTH*zoom, KEYBOARD_HEIGHT*zoom);
    
    drawKeys();
    for( int i=0; i<DISPLAY_CHARS; i++) {
        display.push_back(Digit(renderer, zoom));
    }
}

void audio_callback(void *_beast, Uint8 *_stream, int _length) {
    Sint16 *stream = (Sint16*) _stream;
    int length = _length / 2;
    Beast* beast = (Beast*) _beast;

    beast->loadSamples(stream, length);
}

void Beast::init(uint64_t targetSpeedHz, uint64_t breakpoint, int audioDevice, int volume, int sampleRate) {
    pins = z80_init(&cpu);

    z80pio_init(&pio);

    this->targetSpeedHz = targetSpeedHz;
    clock_cycle_ps = UINT64_C(1000000000000) / targetSpeedHz;
    float speed = targetSpeedHz / 1000000.0f;

    std::cout << "Clock cycle time ps = " << clock_cycle_ps << ", speed = " << std::setprecision(2) << std::fixed << speed << "MHz" << std::endl;
    clock_time_ps  = 0;

    this->breakpoint = breakpoint;

    portB = 0xFF;

    for( int i=0; i<12; i++ ) {
        display1->addDigit(getDigit(i));
        display2->addDigit(getDigit(i+12));
    }

    uart_init(&uart, UINT64_C(1843200), clock_time_ps);

    if( sampleRate > 0 ) {
        audioSampleRatePs = UINT64_C(1000000000000) / sampleRate;
        this->volume = volume;
        SDL_AudioSpec desiredSpec;

        desiredSpec.freq = sampleRate;
        desiredSpec.format = AUDIO_S16SYS;
        desiredSpec.channels = 1;
        desiredSpec.samples = AUDIO_BUFFER_SIZE/4;
        desiredSpec.callback = audio_callback;
        desiredSpec.userdata = this;

        SDL_AudioSpec obtainedSpec;

        /* Handling audio devices */
        int num_audio_dev = SDL_GetNumAudioDevices(false);
        for (int i = 0; i < num_audio_dev; ++i) {
            SDL_Log("Audio device %d: %s", i, SDL_GetAudioDeviceName(i, 0));
        }

        // you might want to look for errors here  SDL_GetAudioDeviceName(6, 0)
        SDL_AudioDeviceID id = SDL_OpenAudioDevice((audioDevice >= 0) ? SDL_GetAudioDeviceName(audioDevice, 0): NULL, 0, &desiredSpec, &obtainedSpec, 0);

        // start play audio
        SDL_PauseAudioDevice(id, 0);
    }
    else {
        audioSampleRatePs = 0;
    }
}

void Beast::loadSamples(Sint16 *stream, int length) {
    int index = 0;
    while(index < length && ((audioRead+1)%AUDIO_BUFFER_SIZE) != audioWrite) {
        audioRead = (audioRead+1)%AUDIO_BUFFER_SIZE;
        stream[index++] = audioBuffer[audioRead];
    }

    if( index > 0 && audioFile ) {
        fwrite(stream, 2, index, audioFile);
    }
    while(index < length) {
        stream[index++] = audioBuffer[audioRead];
    }
}

Beast::~Beast() {
    SDL_CloseAudio();
    if( audioFile ) {
        fclose(audioFile);
        audioFile = nullptr;
    }
}

uint8_t *Beast::getRom() {
    return rom;
}

uint8_t *Beast::getRam() {
    return ram;
}

Digit *Beast::getDigit(int index) {
    return &display[index];
}

void Beast::drawKeys() {

    SDL_SetRenderTarget(sdlRenderer, keyboardTexture);

    SDL_SetRenderDrawColor(sdlRenderer, 0xE0, 0xE0, 0xF0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(sdlRenderer);

    for(int col=0; col<KEY_COLS; col++) {
        for(int row=0; row<KEY_ROWS; row++) {
            if( col+row*KEY_COLS >= MAX_KEYS ) continue;
            drawKey(col,row, 0, 0, false);
        }
    }
    // Render to window again...
    SDL_SetRenderDrawColor(sdlRenderer, 0x0, 0x0, 0x0, SDL_ALPHA_OPAQUE);
    SDL_SetRenderTarget(sdlRenderer, NULL);
}

void Beast::drawKey(int col, int row, int offsetX, int offsetY, bool pressed) {
    if( row == 3 && col == 0 ) return;
    int x1 = (offsetX + col*KEY_WIDTH + KEY_INDENTS[row])*zoom;
    int y1 = (offsetY + row*KEY_HEIGHT)*zoom;
    int rad = 5*zoom;
    SDL_Color keyColour = {0,0,0};
    
    if( pressed ) {
        roundedBoxRGBA(sdlRenderer, x1+5*zoom, y1+5*zoom, x1+(KEY_WIDTH-5)*zoom, y1+(KEY_HEIGHT-5)*zoom, rad, 0xA0, 0xA0, 0xA0, 0xFF);
    }

    bool isSmall = strlen(KEY_CAPS[col+row*KEY_COLS]) > 1;

    SDL_Surface *textSurface = TTF_RenderText_Blended(isSmall? smallFont: font, KEY_CAPS[col+row*KEY_COLS], keyColour);
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(sdlRenderer, textSurface);

    SDL_Rect textRect;

    textRect.x = (offsetX + (col+0.5)*KEY_WIDTH  + KEY_INDENTS[row])*zoom - textSurface->w * 0.5;
    textRect.y = (offsetY + (row+0.5)*KEY_HEIGHT)*zoom - textSurface->h *0.5;
    textRect.w = textSurface->w;
    textRect.h = textSurface->h;

    SDL_RenderCopy(sdlRenderer, textTexture, NULL, &textRect);
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);

    roundedRectangleRGBA(sdlRenderer, x1+5*zoom, y1+5*zoom, x1+(KEY_WIDTH-5)*zoom, y1+(KEY_HEIGHT-5)*zoom, rad, 0,0,0 , 0xFF);
}

void Beast::mainLoop() {
    run(false, 0); // One tick to get going...
    while( mode != QUIT ) {
        if( mode == RUN ) {
            uint64_t start_time = SDL_GetPerformanceCounter();

            uint64_t tick_count = run(true, 0);
            
            uint64_t end_time = SDL_GetPerformanceCounter();
            double duration = ((double)(end_time-start_time))/SDL_GetPerformanceFrequency();
            double mhz = ((double)tick_count)/1000000/duration;

            std::cout << "Speed " << mhz << " Mhz" << std::endl;
            while( !z80_opdone(&cpu)) {
                run(false, 0);
            }
        }
        else if( mode == STEP ) {
            do {
                run(false, 0);
            }
            while( !z80_opdone(&cpu));

            mode = DEBUG;
        }
        else if( mode == OUT ) {
            instr->resetStack();
            bool isOut = false;
            uint64_t tickCount = 0;
            do {
                if( z80_opdone(&cpu)) {
                    isOut = instr->isOut(readMem(cpu.pc-1), readMem(cpu.pc));
                }
                tickCount = run(false, tickCount);
            }
            while( !z80_opdone(&cpu) || (!isOut && mode == OUT) );
            mode = DEBUG;
        }
        else if( mode == OVER ) {
            while( !z80_opdone(&cpu) ) {
                run(false, 0);
            }
            if( instr->isJumpOrReturn(readMem(cpu.pc-1), readMem(cpu.pc)) ) {
                // Unconditional jump/return.. always step..
                std::cout << "Unconditional jump, stepping" << std::endl;
                mode = STEP;
                continue;
            }
            else {
                int length = instr->instructionLength(readMem(cpu.pc-1), readMem(cpu.pc));
                if( length < 0 ) {
                    std::cout << "Instruction length unknown, stepping";
                    mode = STEP;
                    continue;
                }
                uint16_t breakPoint = cpu.pc+length;
                uint64_t tickCount = 0;
                do {
                    tickCount = run(false, tickCount);
                }
                while( !z80_opdone(&cpu) || (cpu.pc != breakPoint && mode == OVER) );
            }
            mode = DEBUG;
        }
        else if( mode == TAKE) {
            uint16_t branchAddress = cpu.pc-1;
            bool isTaken = false;
            uint64_t tickCount = 0;
            do {
                if( z80_opdone(&cpu) && (cpu.pc-1 == branchAddress)) {
                    isTaken = instr->isTaken(readMem(cpu.pc-1), readMem(cpu.pc), cpu.f);
                }
                tickCount = run(false, tickCount);
            }
            while( !z80_opdone(&cpu) || (!isTaken && mode == TAKE) );
            mode = DEBUG;
        }

        if( mode == DEBUG ) {
            drawBeast();
            onDebug();
            SDL_Event windowEvent;

            while( SDL_PollEvent(&windowEvent ) == 0 ) {
                SDL_Delay(25);
            };
            
            if( SDL_QUIT == windowEvent.type ) {
                mode = QUIT;
            }

            if( SDL_KEYDOWN == windowEvent.type ) {
                switch( windowEvent.key.keysym.sym ) {
                    case SDLK_PAGEUP  : if( selection > 0 ) selection--; break;
                    case SDLK_PAGEDOWN: if( selection < 16 ) selection++; break;

                    case SDLK_q    : mode = QUIT;   break;
                    case SDLK_r    : mode = RUN;    break;
                    case SDLK_s    : mode = STEP;   break;
                    case SDLK_u    : mode = OUT;    break;
                    case SDLK_o    : mode = OVER;   break;
                    case SDLK_t    : 
                        if( instr->isConditional(readMem(cpu.pc-1), readMem(cpu.pc))) {
                            mode = TAKE;
                        }
                        else {
                            mode = STEP;
                        }
                        break;
                    case SDLK_a     :
                        if( audioFile ) {
                            fclose(audioFile);
                            audioFile = nullptr;
                        }
                        else {
                            audioFile = fopen(audioFilename, "ab");
                        }
                        break;
                }
            }
        }
    }
}

uint64_t Beast::run(bool run, uint64_t tickCount) {
    SDL_Event windowEvent;

    uint64_t startTime = SDL_GetTicks();
    uint64_t startClockPs = clock_time_ps;
    uint64_t lastAudioSample = clock_time_ps;
    uint8_t lastAudio = 0;

    do {
        clock_time_ps += clock_cycle_ps;

        pins = z80_tick(&cpu, pins) & Z80_PIN_MASK;

        pins |= Z80_IEIO;

        if ((pins & PIO_SEL_MASK) == PIO_SEL_PINS) {
            pins |= Z80PIO_CE;
        }
        if (pins & Z80_A0) { pins |= Z80PIO_BASEL; }
        if (pins & Z80_A1) { pins |= Z80PIO_CDSEL; }

        Z80PIO_SET_PAB(pins, 0xFF, portB); /// Set uart_int, i2c_clk, i2c_data

        pins = z80pio_tick(&pio, pins);
        i2c->tick(&pins, clock_time_ps);
        rtc->tick(&pins, clock_time_ps);

        pins = (pins & ~Z80_INT) | ((pins & Z80PIO_INT) ? Z80_INT : 0);

        portB = Z80PIO_GET_PB(pins);
        portB &= ~0x10; // Clear the UART int pin...

        uart_tick(&uart, clock_time_ps);

        if (pins & Z80_MREQ) {
            const uint16_t addr = Z80_GET_ADDR(pins);
            uint16_t mappedAddr = addr & 0x3FFF;
            bool isRam = false;
            
            if( pagingEnabled ) {
                int page = memoryPage[(addr >> 14) & 0x03];
                isRam = (page & 0xE0) == 0x20;
                mappedAddr |= (page & 0x1F) << 14;
            }
            if (pins & Z80_RD) {
                uint8_t data = isRam ? ram[mappedAddr] : rom[mappedAddr];
                Z80_SET_DATA(pins, data);
            }
            else if (pins & Z80_WR) {
                uint8_t data = Z80_GET_DATA(pins);
                if( isRam ) {
                    ram[mappedAddr] = data;
                }
            }
        }
        else if (pins & Z80_IORQ) {
            const uint16_t port = Z80_GET_ADDR(pins);
            if (pins & Z80_RD) {
                // handle IO input request at port
                //...
                if( (port & 0xF0) == 0x00) {
                    Z80_SET_DATA(pins, readKeyboard(port));
                }
                else if( (port & 0xF0) == 0x20) {
                    Z80_SET_DATA(pins, uart_read(&uart, port & 0x07));
                }
            }
            else if (pins & Z80_WR) {
                // handle IO output request at port
                //...
                if( (port & 0x0F0) == 0x70 ) {
                    // Memory system.
                    if( (port & 0x04) == 0) {
                        uint8_t page = memoryPage[port & 0x03] = Z80_GET_DATA(pins);
                    }
                    else {
                        pagingEnabled = (pins & Z80_D0) != 0;
                    }
                }
                else if( (port & 0xF0) == 0x20) {
                    uart_write(&uart, port & 0x07, Z80_GET_DATA(pins), clock_time_ps);
                }
                else if( (port & 0xF0) == 0x10) {
                    
                }
            }
        }

        uint64_t elapsed = SDL_GetTicks() - startTime;
        if( elapsed < (clock_time_ps - startClockPs)/1000000000ULL ) {
            SDL_Delay(1);
        }

        if( (audioSampleRatePs != 0) && clock_time_ps - lastAudioSample > audioSampleRatePs ) {
            lastAudioSample += audioSampleRatePs;
            int next = (audioWrite+1)%AUDIO_BUFFER_SIZE;
            if( next != audioRead ) {
                audioBuffer[audioWrite] = (uart.modem_control_register & MCR_OUT2) ? 400*volume : -400*volume;
                audioWrite = next;
            }
        }

        if( tickCount % (targetSpeedHz/FRAME_RATE) == 0 ) { 
            if( SDL_PollEvent(&windowEvent ) != 0 ) {
                if( SDL_QUIT == windowEvent.type ) {
                    mode = QUIT;
                    break;
                }
                if( SDL_KEYDOWN == windowEvent.type ) {
                    if( windowEvent.key.keysym.sym == SDLK_ESCAPE ) {
                        mode = DEBUG;
                        run = false;
                    }
                    else 
                        keyDown(windowEvent.key.keysym.sym);
                }
                if( SDL_KEYUP == windowEvent.type ) {
                    keyUp(windowEvent.key.keysym.sym);
                }
            }
            onDraw();
        }
        tickCount++;
        if( cpu.pc-1 == breakpoint && z80_opdone(&cpu)) {
            mode = DEBUG;
            run = false;
        }
    }
    while( run );

    return tickCount;
}

void Beast::keyDown(SDL_Keycode keyCode) {
    for( int i=0; i<KEY_MAP_LENGTH; i++) {
        if(KEY_MAP[i].key == keyCode) {
            switch(KEY_MAP[i].mod) {
                case NONE: 
                    break;
                case SHIFT:
                    keySet.insert(KEY_SHIFT);
                    keySet.erase(KEY_CTRL);
                    break;
                case CTRL:
                    keySet.erase(KEY_SHIFT);
                    keySet.insert(KEY_CTRL);
                    break;
                case CTRL_SHIFT:
                    keySet.insert(KEY_SHIFT);
                    keySet.insert(KEY_CTRL);
                    break;
            };
            keySet.insert(KEY_MAP[i].row*12 + KEY_MAP[i].col);
            break;
        }
    }
    changed = true;
}

void Beast::keyUp(SDL_Keycode keyCode) {
    for( int i=0; i<KEY_MAP_LENGTH; i++) {
        if(KEY_MAP[i].key == keyCode) {
            if(KEY_MAP[i].mod != NONE) {
                keySet.erase(KEY_SHIFT);
                keySet.erase(KEY_CTRL);
            }

            keySet.erase(KEY_MAP[i].row*12 + KEY_MAP[i].col);
            break;
        }
    }
    changed = true;
}

uint8_t Beast::readKeyboard(uint16_t port) {
    uint8_t result = 0x3F;

    for( int key: keySet ) {
        int row = key / 12;
        int col = key % 12;
        if( col >= 6 ) {
            // Right hand side...
            if( ((port >> (row+12)) & 0x01) == 0 ) {
                result &= ~(0x01 << (col-6));
            }
        } else {
            if( ((port >> (11-row)) & 0x01) == 0 ) {
                result &= ~(0x020 >> (col));
            }
        }
    }
    
    return result;
}

void Beast::onDebug() {
    boxRGBA(sdlRenderer, 40*zoom, 40*zoom, (screenWidth-40)*zoom, (screenHeight-40)*zoom, 0xF0, 0xF0, 0xE0, 0xF0);
    
    SDL_Color textColor = {0, 0x30, 0x30};
    SDL_Color highColor = {0x80, 0x30, 0x30};
    SDL_Color bright =    {0xD0, 0xFF, 0xD0};

    int id = selection;

    print(50, 48, textColor, id--?0:2, bright, "PC = 0x%04X", (uint16_t)(cpu.pc-1));
    print(50, 64, textColor, id--?0:2, bright, " A = 0x%02X", cpu.a );
    print(50, 80, textColor, id--?0:2, bright, "HL = 0x%04X", cpu.hl);
    print(50, 96, textColor, id--?0:2, bright, "BC = 0x%04X", cpu.bc);
    print(50, 112, textColor, id--?0:2, bright, "DE = 0x%04X", cpu.de);

    char carry = (cpu.f & Z80_CF) ? 'C' : 'c';
    char neg = (cpu.f & Z80_NF) ? 'N' : 'n';
    char overflow = (cpu.f & Z80_VF) ? 'V' : 'v';
    char zero = (cpu.f & Z80_ZF) ? 'Z' : 'z';
    char sign = (cpu.f & Z80_SF) ? 'S' : 's';

    char xflag = (cpu.f & Z80_XF) ? '1' : '0';
    char hflag = (cpu.f & Z80_HF) ? '1' : '0';
    char yflag = (cpu.f & Z80_YF) ? '1' : '0';

    print(200, 48, textColor, id--?0:5, bright, "Flags = %c%c%c%c%c%c%c%c", sign, zero, yflag, hflag, xflag, overflow, neg, carry);
    print(200, 80, textColor, id--?0:2, bright, "SP = 0x%04X", cpu.sp);
    print(200, 96, textColor, id--?0:2, bright, "IX = 0x%04X", cpu.ix);
    print(200, 112, textColor, id--?0:2, bright, "IY = 0x%04X", cpu.iy);

    if( pagingEnabled ) {
        print(390, 48, textColor, id--?0:-7, bright, "Paging enabled" );
    }
    else {
        print(390, 48, textColor, id--?0:-8, bright, "Paging disabled" );
    }

    print(390, 64, textColor, id--?0:6, bright, "Page 0 0x%02X", memoryPage[0]);
    print(390, 80, textColor, id--?0:6, bright, "Page 1 0x%02X", memoryPage[1]);
    print(390, 96, textColor, id--?0:6, bright, "Page 2 0x%02X", memoryPage[2]);
    print(390, 112, textColor, id--?0:6, bright, "Page 3 0x%02X", memoryPage[3]);

    print(580, 48, textColor, "[R]un");
    print(580, 64, textColor, "[S]tep");
    print(580, 80, textColor, "Step [O]ver");
    print(580, 96, textColor, "Step o[U]t");
    print(580, 112, textColor, "Until branch [T]aken");

    uint16_t address = ((cpu.pc-1) & 0xFFF0)-16;
    print(50, 154, textColor, id--?0:2, bright, "PC");
    displayMem(address, 100, 140, textColor, cpu.pc-1);

    address = (cpu.sp & 0xFFF0)-16;
    print(50, 210, textColor, id--?0:2, bright, "SP");
    displayMem(address, 100, 196, textColor, cpu.sp);

    address = (cpu.hl & 0xFFF0)-16;
    print(50, 266, textColor, id--?0:2, bright, "HL");
    displayMem(address, 100, 252, textColor, cpu.hl);

    std::bitset<8> ioSelectA(pio.port[0].io_select);
    std::bitset<8> portDataA(Z80PIO_GET_PA(pins));

    print(50, 308, textColor, "Port A");
    print(120, 308, textColor, (char*)ioSelectA.to_string('O', 'I').c_str());
    print(120, 324, textColor, (char*)portDataA.to_string().c_str());

    std::bitset<8> ioSelectB(pio.port[1].io_select);
    std::bitset<8> portDataB(Z80PIO_GET_PB(pins));

    print(320, 308, textColor, "Port B");
    print(390, 308, textColor, (char*)ioSelectB.to_string('O', 'I').c_str());
    print(390, 324, textColor, (char*)portDataB.to_string().c_str());

    print(580, 308, textColor, "[A]ppend audio %s", audioFile?"ON":"OFF");
    print(580, 324, textColor, "File \"%s\"", audioFilename);

    int page = pagingEnabled ? memoryPage[((cpu.pc-1) >> 14) & 0x03] : 0;
    currentLoc = listing.getLocation(page << 16 | (cpu.pc-1));

    if( currentLoc.valid ) {
        int startLine = currentLoc.lineNum - 4;
        if( startLine < 0 ) startLine = 0;
        Listing::Location displayLoc = currentLoc;

        for( int i=0; i<12; i++ ) {
            displayLoc.lineNum = i+startLine;
            std::string line = listing.getLine(displayLoc);
            print(50, 350+(14*i), i==3 ? highColor: textColor, "%.86s", const_cast<char*>(line.c_str()));
        }
    }
    else {
        drawListing( cpu.pc-1, textColor, highColor );
    }

    print( 50, 532, textColor, "[L]ist address");
    print( 200, 532, textColor, "[C]urrent address");

    if( breakpoint <= 0xFFFF ) {
        print(390, 532, textColor, "[B]reakpoint = 0x%04X", breakpoint);
    }
    
    print(620, 532, textColor, "[Q]uit");

    SDL_RenderPresent(sdlRenderer);
}

void Beast::drawListing(uint16_t address, SDL_Color textColor, SDL_Color highColor) {
    int length;
    auto f = [this](auto address) { return this->readMem(address); };

    int matchedLine = -1;

    for( int i=0; i<decodedAddresses.size(); i++) {
        if( decodedAddresses[i] == address ) {
            matchedLine = i;
            break;
        }
    }
    if( matchedLine > 7 ) {
        address = decodedAddresses[matchedLine-7];
    }
    else if( matchedLine >= 0 ) {
        address = decodedAddresses[0];
    }

    for( int i=0; i<12; i++ ) {
        if( decodedAddresses.size() > i ) {
            decodedAddresses[i] = address;
        }
        else {
            decodedAddresses.push_back(address);
        }
        std::string line = instr->decode(address, f, &length);
        print(50, 350+(14*i), (address == cpu.pc-1) ? highColor: textColor, "%04X             %s", address, const_cast<char*>(line.c_str()));
        address += length;
    }
}

void Beast::displayMem(uint16_t address, int x, int y, SDL_Color textColor, uint16_t markAddress) {
    char buffer[200]; 
    for( int row=0; row<3; row++ ) {
        int c = snprintf( buffer, sizeof(buffer), "0x%04X ", address);
        if( c < 0 || c >= sizeof(buffer) ) {
            break;
        }
        for( uint16_t i=0; i<16; i++ ) {
            uint8_t data = readMem(address+i);

            int cs = snprintf( buffer+c, sizeof(buffer)-c, 
                (address+i == markAddress) ? ">%02X" : " %02X", data);
            if( cs < 0 || cs+c >= sizeof(buffer) ) {
                return;
            }
            c+=cs;
        }
        int cn =snprintf(buffer+c, sizeof(buffer)-c, "   ");
        if( cn < 0 || cn+c >= sizeof(buffer) ) {
            return;
        }
        c+=cn;
        for( int i=0; i<16; i++ ) {
            uint8_t data = readMem(address+i);
            if( data < 32 || data > 127 ) {
                data = '.';
            }

            buffer[c++] = data;
            buffer[c] = 0;
            if( c+1 >= sizeof(buffer)) {
                return;
            }
        }
        print(x, y+(14*row), textColor, buffer);
        address += 16;
    }
}

uint8_t Beast::readMem(uint16_t address) {
    uint16_t mappedAddr = address & 0x3FFF;
    bool isRam = false;
    
    if( pagingEnabled ) {
        int page = memoryPage[(address >> 14) & 0x03];
        isRam = (page & 0xE0) == 0x20;
        mappedAddr |= (page & 0x1F) << 14;
    }
    return isRam ? ram[mappedAddr] : rom[mappedAddr];
}

template<typename... Args> void Beast::print(int x, int y, SDL_Color color, char *fmt, Args... args) {
    char buffer[200]; 

    int c = snprintf(buffer, sizeof(buffer), fmt, args...);

    if( c > 0 && c<sizeof(buffer)) {
        printb(x,y, color, 0, {0}, buffer);
    }
}

template<typename... Args> void Beast::print(int x, int y, SDL_Color color, int highlight, SDL_Color background, char *fmt, Args... args) {
    char buffer[200]; 

    int c = snprintf(buffer, sizeof(buffer), fmt, args...);

    if( c > 0 && c<sizeof(buffer)) {
        printb(x,y, color, highlight, background, buffer);
    }
}

void Beast::printb(int x, int y, SDL_Color color, int highlight, SDL_Color background, char* buffer) {
    SDL_Surface *textSurface = TTF_RenderText_Blended(monoFont, buffer, color);
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(sdlRenderer, textSurface);{}    if( highlight != 0 ) {  
        int width;
        int height;

        if( highlight > 0 ) {
            buffer[highlight] = (char)0;
            TTF_SizeUTF8(monoFont, buffer, &width, &height);
            boxRGBA(sdlRenderer, x*zoom, (y+1)*zoom, x*zoom+width, (y-2)*zoom+height, background.r, background.g, background.b, 0xFF);
        }
        else if( highlight < 0 ) {
            buffer += strlen(buffer)+highlight;
            TTF_SizeUTF8(monoFont, buffer, &width, &height);
            boxRGBA(sdlRenderer, x*zoom+textSurface->w-width, (y+1)*zoom, x*zoom+textSurface->w, (y-2)*zoom+height, background.r, background.g, background.b, 0xFF);
        }
    }

    SDL_Rect textRect;
    textRect.x = x*zoom; 
    textRect.y = y*zoom;
    textRect.w = textSurface->w;
    textRect.h = textSurface->h;

    SDL_RenderCopy(sdlRenderer, textTexture, NULL, &textRect);

    SDL_DestroyTexture(textTexture);
    SDL_FreeSurface(textSurface);
}


void Beast::onDraw() {
    // Do something..
    for( int i=0; i<DISPLAY_CHARS; i++) {
        changed |= display[i].changed;
    }

    if( changed) { 
        drawBeast();
        SDL_RenderPresent(sdlRenderer);
        changed = false;
    }

}

void Beast::drawBeast() {
    int keyboardTop = (screenHeight - KEYBOARD_HEIGHT);
    int displayTop = (keyboardTop-8 - Digit::DIGIT_HEIGHT);

    SDL_SetRenderDrawColor(sdlRenderer, 0x0, 0x0, 0x0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(sdlRenderer);
    SDL_Rect textRect = {0, keyboardTop*zoom, KEYBOARD_WIDTH*zoom, KEYBOARD_HEIGHT*zoom};
    SDL_RenderCopy(sdlRenderer, keyboardTexture, NULL, &textRect);

    for( int i=0; i<DISPLAY_CHARS; i++) {
        display[i].onDraw(sdlRenderer, 4 + i*(Digit::DIGIT_WIDTH+1), displayTop);
    }

    if( keySet.size() > 0 ) {
        for(std::set<int>::iterator it=keySet.begin(); it!=keySet.end(); ++it) {
            int key = *it;
            int row = key / 12;
            int col = key % 12;
            drawKey(col, row, 0, keyboardTop, true);
        }
    }
}

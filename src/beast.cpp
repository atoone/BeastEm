#include "beast.hpp"
#include <iostream>
#include <cstring>
#include <stdio.h>
#include <iomanip>
#include "z80.h"
#include "z80pio.h"
#include "listing.hpp"

Beast::Beast(SDL_Window *window, int screenWidth, int screenHeight, float zoom, Listing &listing) 
    : rom {}, ram {}, memoryPage {0}, listing(listing) {

    windowId = SDL_GetWindowID(window);

    this->screenWidth = screenWidth;
    this->screenHeight = screenHeight;
    this->zoom = createRenderer(window, screenWidth, screenHeight, zoom);

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
    midFont = TTF_OpenFont(BEAST_FONT, MID_FONT_SIZE*zoom);
    if( !midFont) {
        std::cout << "Couldn't load font "<< BEAST_FONT << std::endl;
        exit(1);
    }

    monoFont = TTF_OpenFont(MONO_FONT, MONO_SIZE*zoom);

    keyboardTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, KEYBOARD_WIDTH*zoom, KEYBOARD_HEIGHT*zoom);
    
    drawKeys();
    for( int i=0; i<DISPLAY_CHARS; i++) {
        display.push_back(Digit(sdlRenderer, zoom));
    }
}

float Beast::createRenderer(SDL_Window *window, int screenWidth, int screenHeight, float zoom) {
    sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if( !sdlRenderer) {
        std::cout << "Could not create renderer: " << SDL_GetError() << std::endl;
        exit(1);
    }

    int rw = 0, rh = 0;
    SDL_GetRendererOutputSize(sdlRenderer, &rw, &rh);
    if(rw != screenWidth*zoom) {
        float widthScale = (float)rw / (float) (screenWidth*zoom);
        float heightScale = (float)rh / (float) (screenHeight*zoom);

        if(widthScale != heightScale) {
            std::cerr << "WARNING: width scale != height scale" << std::endl;
        }

        zoom *= widthScale;
    }

    return zoom;
}

void audio_callback(void *_beast, Uint8 *_stream, int _length) {
    Sint16 *stream = (Sint16*) _stream;
    int length = _length / 2;
    Beast* beast = (Beast*) _beast;

    beast->loadSamples(stream, length);
}

void Beast::init(uint64_t targetSpeedHz, uint64_t breakpoint, int audioDevice, int volume, int sampleRate, VideoBeast *videoBeast) {
    this->videoBeast = videoBeast;

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
    
    if( videoBeast ) {
        videoBeast->init(clock_time_ps);
        nextVideoBeastTickPs = 0;
    }

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
    if( audioAvailable < length ) return;

    while(index < length && ((audioRead+1)%AUDIO_BUFFER_SIZE) != audioWrite) {
        audioRead = (audioRead+1)%AUDIO_BUFFER_SIZE;
        stream[index++] = audioBuffer[audioRead];
        audioAvailable--;
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

    if( strlen(KEY_CAPS_SHIFT[col+row*KEY_COLS]) > 0 ) {
        textSurface = TTF_RenderText_Blended(midFont, KEY_CAPS_SHIFT[col+row*KEY_COLS], keyColour);
        textTexture = SDL_CreateTextureFromSurface(sdlRenderer, textSurface);

        textRect.x = (offsetX + (col+0.75)*KEY_WIDTH  + KEY_INDENTS[row])*zoom - textSurface->w * 0.5;
        textRect.y = (offsetY + (row+0.3)*KEY_HEIGHT)*zoom - textSurface->h *0.5;
        textRect.w = textSurface->w;
        textRect.h = textSurface->h;

        SDL_RenderCopy(sdlRenderer, textTexture, NULL, &textRect);
        SDL_FreeSurface(textSurface);
        SDL_DestroyTexture(textTexture);
    }

    if( strlen(KEY_CAPS_CTRL[col+row*KEY_COLS]) > 0 ) {
        textSurface = TTF_RenderText_Blended(midFont, KEY_CAPS_CTRL[col+row*KEY_COLS], keyColour);
        textTexture = SDL_CreateTextureFromSurface(sdlRenderer, textSurface);

        textRect.x = (offsetX + (col+0.75)*KEY_WIDTH  + KEY_INDENTS[row])*zoom - textSurface->w * 0.5;
        textRect.y = (offsetY + (row+0.75)*KEY_HEIGHT)*zoom - textSurface->h *0.5;
        textRect.w = textSurface->w;
        textRect.h = textSurface->h;

        SDL_RenderCopy(sdlRenderer, textTexture, NULL, &textRect);
        SDL_FreeSurface(textSurface);
        SDL_DestroyTexture(textTexture);
    }

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
                if( !uart_connected(&uart) ) {
                    uart_connect(&uart, true);
                    break;
                }
            }

            if( windowEvent.window.windowID != windowId && videoBeast) {
                videoBeast->handleEvent(windowEvent);
                continue;
            }

            if( SDL_RENDER_TARGETS_RESET == windowEvent.type ) {
                redrawScreen();
            }

            if( SDL_QUIT == windowEvent.type ) {
                mode = QUIT;
            }

            if( SDL_KEYDOWN == windowEvent.type ) {
                if( editMode ) {
                    int digit = -1;

                    switch( windowEvent.key.keysym.sym ) {
                        case SDLK_ESCAPE: editMode = false; break;
                        case SDLK_RETURN: editComplete(); break;
                        case SDLK_BACKSPACE: 
                            if( editIndex < editDigits-1 ) {
                                editIndex++;
                                editValue = (editValue & ~(0x000F << (editIndex*4))) | (editOldValue & (0x000F << (editIndex*4)));
                            } 
                            break;
                        case SDLK_0 ... SDLK_9: digit = windowEvent.key.keysym.sym - SDLK_0; break;
                        case SDLK_a ... SDLK_f: digit = windowEvent.key.keysym.sym - SDLK_a + 10; break;
                    }
                    if( digit >= 0 ) {
                        editValue = (editValue & ~(0x000F << (editIndex*4))) | (digit << (editIndex*4));
                        editIndex--;
                        if(editIndex < 0) {
                            editComplete();
                        }
                    }
                }
                else {
                    int maxSelection = (breakpoint == NO_BREAKPOINT) ? static_cast<int>(SEL_BREAKPOINT) : static_cast<int>(SEL_END_MARKER);

                    switch( windowEvent.key.keysym.sym ) {
                        case SDLK_PAGEUP  : updateSelection(-1, maxSelection); break;
                        case SDLK_PAGEDOWN: updateSelection(1, maxSelection); break;
                        case SDLK_RIGHT : itemSelect(1); break;
                        case SDLK_LEFT:   itemSelect(-1); break;
                        case SDLK_UP  :
                            if( itemEdit() ) {
                                editValue = (editValue-1) & (0x0FFFF >> ((4-editDigits)*4));
                                editComplete();
                            };
                            break;
                        case SDLK_DOWN  :
                            if( itemEdit() ) {
                                editValue = (editValue+1) & (0x0FFFF >> ((4-editDigits)*4));
                                editComplete();
                            };
                            break;
                        case SDLK_RETURN: itemEdit(); break;

                        case SDLK_b    : 
                            if( breakpoint != NO_BREAKPOINT ) {
                                lastBreakpoint = breakpoint;
                                breakpoint = NO_BREAKPOINT;
                                selection = SEL_PC;
                            }
                            else {
                                selection = SEL_BREAKPOINT;
                                editMode = true;
                                editOldValue = editValue = lastBreakpoint;
                                editDigits = 4;
                                editIndex = 3;
                                editY = END_ROW;
                                editX = 390;
                                editOffset = 18;
                                breakpoint = lastBreakpoint;
                            }
                            break;
                        case SDLK_q    : mode = QUIT;   break;
                        case SDLK_r    : mode = RUN;    break;
                        case SDLK_s    : mode = STEP;   break;
                        case SDLK_u    : mode = OUT;    break;
                        case SDLK_o    : mode = OVER;   break;
                        case SDLK_d    : uart_connect(&uart, false); break;
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
}

void Beast::updateSelection(int direction, int maxSelection) {
    selection += direction;
    if( selection < 0 ) selection = maxSelection-1;
    if( selection >= maxSelection ) selection = 0;
    if( selection == SEL_VIEWPAGE0 && memView[0] != MV_MEM ) selection += direction;
    if( selection == SEL_VIEWPAGE1 && memView[1] != MV_MEM ) selection += direction;
    if( selection == SEL_VIEWPAGE2 && memView[2] != MV_MEM ) selection += direction;
}

uint64_t Beast::run(bool run, uint64_t tickCount) {
    SDL_Event windowEvent;

    uint64_t startTime = SDL_GetTicks();
    uint64_t startClockPs = clock_time_ps;
    uint64_t lastAudioSample = clock_time_ps;

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
            uint32_t mappedAddr = addr & 0x3FFF;
            bool isRam = false;
            bool isVb  = false;

            if( pagingEnabled ) {
                int page = memoryPage[(addr >> 14) & 0x03];
                isRam = (page & 0xE0) == 0x20;
                isVb  = (page & 0xE0) == 0x40;
                mappedAddr |= (page & 0x1F) << 14;
            }
            if (pins & Z80_RD) {
                if( isRam ) {
                    uint8_t data = ram[mappedAddr];
                    Z80_SET_DATA(pins, data);
                }
                else if( videoBeast && isVb ) {
                    uint8_t data = videoBeast->read(mappedAddr, clock_time_ps);
                    Z80_SET_DATA(pins, data);
                }
                else if( romOperation ) {
                    if( clock_time_ps >= romCompletePs ) {
                        romSequence = 0;
                        romOperation = false;
                    }
                    else {
                        uint8_t data = rom[mappedAddr] ^ romOperationMask;
                        romOperationMask ^= 0x40;
                        Z80_SET_DATA(pins, data);
                    }
                }
                else {
                    uint8_t data = rom[mappedAddr];
                    Z80_SET_DATA(pins, data);
                }
            }
            else if (pins & Z80_WR) {
                uint8_t data = Z80_GET_DATA(pins);
                if( isRam ) {
                    ram[mappedAddr] = data;
                }
                else if( videoBeast && isVb ) {
                    videoBeast->write(mappedAddr, data, clock_time_ps);
                }
                else {
                    if( romSequence == 3 && clock_time_ps >= romCompletePs ) {
                        romSequence = 0;
                        romOperation = false;
                    }

                    switch( romSequence ) {
                        case 0: if( mappedAddr == 0x5555 && data == 0xaa ) {
                                romSequence = 1;
                            }
                            else {
                                romSequence = 0;
                            }
                            break;
                        case 1: if( mappedAddr == 0x2AAA && data == 0x55 ) {
                                romSequence = 2;
                            }
                            else {
                                romSequence = 0;
                            }
                            break;
                        case 2: if( mappedAddr == 0x5555 && ((data & 0xF0) != 0)) {
                                romSequence = data;
                            }
                            else {
                                romSequence = 0;
                            }
                            break;
                        case 3:
                            break;
                        case 0xA0: 
                            rom[mappedAddr] = data;
                            romOperation = true;
                            romCompletePs = clock_time_ps + ROM_BYTE_WRITE_PS;
                            romSequence = 3;
                            break;
                        case 0x80:
                            if( mappedAddr == 0x5555 && data == 0xaa ) {
                                romSequence = 0x81;
                            }
                            else {
                                romSequence = 0;
                            }
                            break;
                        case 0x81: 
                            if( mappedAddr == 0x2AAA && data == 0x55 ) {
                                romSequence = 0x82;
                            }
                            else {
                                romSequence = 0;
                            }
                            break;
                        case 0x82: 
                            if( mappedAddr == 0x5555 && data == 0x10 ) { // Chip erase
                                std::cout << "Erasing chip " << std::endl;
                                for( int i=1<<19; i>0; ) {
                                    rom[--i] = 0xFF;
                                }
                                romOperation = true;
                                romCompletePs = clock_time_ps + ROM_CHIP_ERASE_PS;
                                romSequence  = 3;
                            }
                            else if (data == 0x30) { // Sector erase
                                uint32_t sectorAddress = mappedAddr & ~0x0FFFULL;
                                std::cout << "Erasing sector " << (sectorAddress >> 12) << std::endl;
                                for( int i=0; i< 0x1000; i++) {
                                    rom[sectorAddress+i] = 0xFF;
                                }
                                romOperation = true;
                                romCompletePs = clock_time_ps + ROM_SECTOR_ERASE_PS;
                                romSequence = 3;
                            }
                            else {
                                romSequence = 0;
                            }
                            break;
                        default:
                            romSequence = 0;
                    }
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
                        memoryPage[port & 0x03] = Z80_GET_DATA(pins);
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

        if( videoBeast && (nextVideoBeastTickPs <= clock_time_ps) ) {
            nextVideoBeastTickPs = videoBeast->tick(clock_time_ps);
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
                audioAvailable++;
            }
        }

        if( tickCount % (targetSpeedHz/FRAME_RATE) == 0 ) { 
            if( SDL_PollEvent(&windowEvent ) != 0 ) {
                if( windowEvent.window.windowID != windowId && videoBeast) {
                    videoBeast->handleEvent(windowEvent);
                }
                else if( SDL_QUIT == windowEvent.type ) {
                    mode = QUIT;
                    break;
                }
                else if( SDL_KEYDOWN == windowEvent.type ) {
                    if( windowEvent.key.keysym.sym == SDLK_ESCAPE ) {
                        mode = DEBUG;
                        run = false;
                    }
                    else 
                        keyDown(windowEvent.key.keysym.sym);
                }
                else if( SDL_KEYUP == windowEvent.type ) {
                    keyUp(windowEvent.key.keysym.sym);
                }
                else if( SDL_RENDER_TARGETS_RESET == windowEvent.type ) {
                    redrawScreen();
                }
            }
            onDraw();
        }
        tickCount++;
        if( (uint64_t)(cpu.pc-1) == breakpoint && z80_opdone(&cpu)) {
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
    }\
    return result;
}

void Beast::startEdit(uint16_t value, int x, int y, int offset, int digits) {
    editMode = true;
    editValue = value;
    editOldValue = value;
    editX = x;
    editY = y;
    editOffset = offset;
    editDigits = digits;
    editIndex = digits-1;
}

bool Beast::itemEdit() {
    switch( selection ) {
        case SEL_PC: startEdit( cpu.pc-1, COL1, ROW1,  8, 4); break;
        case SEL_A : startEdit( cpu.a, COL1, ROW2, 8, 2); break;
        case SEL_HL: startEdit( cpu.hl, COL1, ROW3, 8, 4); break;
        case SEL_BC: startEdit( cpu.bc, COL1, ROW4, 8, 4); break;
        case SEL_DE: startEdit( cpu.de, COL1, ROW5, 8, 4); break;

        case SEL_SP: startEdit( cpu.sp, COL2, ROW3, 8, 4); break;
        case SEL_IX: startEdit( cpu.ix, COL2, ROW4, 8, 4); break;
        case SEL_IY: startEdit( cpu.iy, COL2, ROW5, 8, 4); break;

        case SEL_PAGING: pagingEnabled = !pagingEnabled; break;
        case SEL_PAGE0 : startEdit( memoryPage[0], COL3, ROW2, 10, 2); break;
        case SEL_PAGE1 : startEdit( memoryPage[1], COL3, ROW3, 10, 2);  break;
        case SEL_PAGE2 : startEdit( memoryPage[2], COL3, ROW4, 10, 2); break;
        case SEL_PAGE3 : startEdit( memoryPage[3], COL3, ROW5, 10, 2); break;

        case SEL_MEM0 : 
            if( memView[0] == MV_Z80) startEdit( memAddress[0], 104, ROW8, 3, 4); 
            if( memView[0] == MV_MEM) startEdit( memPageAddress[0], 104, ROW8, 3, 4); 
            break;
        case SEL_MEM1 : 
            if( memView[1] == MV_Z80) startEdit( memAddress[1], 104, ROW12, 3, 4); 
            if( memView[1] == MV_MEM) startEdit( memPageAddress[1], 104, ROW12, 3, 4); 
            break;
        case SEL_MEM2 : 
            if( memView[2] == MV_Z80) startEdit( memAddress[2], 104, ROW16, 3, 4); 
            if( memView[2] == MV_MEM) startEdit( memPageAddress[2], 104, ROW16, 3, 4);
            break;

        case SEL_VIEWPAGE0 : startEdit( memViewPage[0], COL1, ROW8+14, 1, 2); break;
        case SEL_VIEWPAGE1 : startEdit( memViewPage[1], COL1, ROW12+14, 1, 2); break;
        case SEL_VIEWPAGE2 : startEdit( memViewPage[2], COL1, ROW16+14, 1, 2); break;

        case SEL_A2: startEdit( cpu.af2 & 0xFF, COL4, ROW2, 9, 2); break;
        case SEL_HL2: startEdit( cpu.hl2, COL4, ROW3, 9, 4); break;
        case SEL_BC2: startEdit( cpu.bc2, COL4, ROW4, 9, 4);  break;
        case SEL_DE2: startEdit( cpu.de2, COL4, ROW5, 9, 4); break;

        case SEL_BREAKPOINT:  startEdit( breakpoint, 390, END_ROW, 18, 4); break;
    }

    return editMode;
}

void Beast::editComplete() {
    switch( selection ) {
        case SEL_PC: pins = z80_prefetch(&cpu, editValue); run(false, 0);  break;
        case SEL_A: cpu.a = editValue; break;
        case SEL_HL: cpu.hl = editValue; break;
        case SEL_BC: cpu.bc = editValue; break;
        case SEL_DE: cpu.de = editValue; break;
        
        case SEL_SP: cpu.sp = editValue; break;
        case SEL_IX: cpu.ix = editValue; break;
        case SEL_IY: cpu.iy = editValue; break;

        case SEL_PAGE0: memoryPage[0] = editValue; break;
        case SEL_PAGE1: memoryPage[1] = editValue; break;
        case SEL_PAGE2: memoryPage[2] = editValue; break;
        case SEL_PAGE3: memoryPage[3] = editValue; break;

        case SEL_MEM0: 
            if( memView[0] == MV_Z80 ) memAddress[0] = editValue;
            if( memView[0] == MV_MEM ) memPageAddress[0] = editValue & 0x3FFF;
            break;
        case SEL_MEM1: 
            if( memView[1] == MV_Z80 ) memAddress[1] = editValue;
            if( memView[1] == MV_MEM ) memPageAddress[1] = editValue & 0x3FFF;
            break;
        case SEL_MEM2: 
            if( memView[2] == MV_Z80 ) memAddress[2] = editValue;
            if( memView[2] == MV_MEM ) memPageAddress[2] = editValue & 0x3FFF;
            break;

        case SEL_VIEWPAGE0 : memViewPage[0] = editValue; break;
        case SEL_VIEWPAGE1 : memViewPage[1] = editValue; break;
        case SEL_VIEWPAGE2 : memViewPage[2] = editValue; break;

        case SEL_A2: cpu.af2 = ((cpu.af2 & 0xFF00) | (editValue & 0x0FF)); break;
        case SEL_HL2: cpu.hl2 = editValue; break;
        case SEL_BC2: cpu.bc2 = editValue; break;
        case SEL_DE2: cpu.de2 = editValue; break;

        case SEL_BREAKPOINT: breakpoint = editValue; break;
    }
    editMode = false;
}

void Beast::onDebug() {
    boxRGBA(sdlRenderer, 32*zoom, 32*zoom, (screenWidth-24)*zoom, (screenHeight-24)*zoom, 0xF0, 0xF0, 0xE0, 0xE8);
    
    SDL_Color textColor = {0, 0x30, 0x30};
    SDL_Color highColor = {0xA0, 0x30, 0x30};
    SDL_Color bright =    {0xD0, 0xFF, 0xD0};
    SDL_Color menuColor = {0x30, 0x30, 0xA0};

    print(COL1, 34, menuColor, "[R]un");
    print(COL2, 34, menuColor, "[S]tep");
    print(COL3, 34, menuColor, "Step [O]ver");
    print(COL4, 34, menuColor, "Step o[U]t");
    print(640, 34, menuColor, "Until [T]aken");

    int id = selection;

    print(COL1, ROW1, textColor, id--?0:2, bright, "PC = 0x%04X", (uint16_t)(cpu.pc-1));
    print(COL1, ROW2, textColor, id--?0:2, bright, " A = 0x%02X", cpu.a );
    print(COL1, ROW3, textColor, id--?0:2, bright, "HL = 0x%04X", cpu.hl);
    print(COL1, ROW4, textColor, id--?0:2, bright, "BC = 0x%04X", cpu.bc);
    print(COL1, ROW5, textColor, id--?0:2, bright, "DE = 0x%04X", cpu.de);

    char carry = (cpu.f & Z80_CF) ? 'C' : 'c';
    char neg = (cpu.f & Z80_NF) ? 'N' : 'n';
    char overflow = (cpu.f & Z80_VF) ? 'V' : 'v';
    char zero = (cpu.f & Z80_ZF) ? 'Z' : 'z';
    char sign = (cpu.f & Z80_SF) ? 'S' : 's';

    char xflag = (cpu.f & Z80_XF) ? '1' : '0';
    char hflag = (cpu.f & Z80_HF) ? '1' : '0';
    char yflag = (cpu.f & Z80_YF) ? '1' : '0';

    print(COL2, ROW1, textColor, id--?0:5, bright, "Flags %c%c%c%c%c%c%c%c", sign, zero, yflag, hflag, xflag, overflow, neg, carry);
    print(COL2, ROW3, textColor, id--?0:2, bright, "SP = 0x%04X", cpu.sp);
    print(COL2, ROW4, textColor, id--?0:2, bright, "IX = 0x%04X", cpu.ix);
    print(COL2, ROW5, textColor, id--?0:2, bright, "IY = 0x%04X", cpu.iy);

    if( pagingEnabled ) {
        print(COL3, ROW1, textColor, id--?0:-2, bright, "Paging ON" );
    }
    else {
        print(COL3, ROW1, textColor, id--?0:-3, bright, "Paging OFF" );
    }

    print(COL3, ROW2, textColor, id--?0:6, bright, "Page 0 0x%02X", memoryPage[0]);
    print(COL3, ROW3, textColor, id--?0:6, bright, "Page 1 0x%02X", memoryPage[1]);
    print(COL3, ROW4, textColor, id--?0:6, bright, "Page 2 0x%02X", memoryPage[2]);
    print(COL3, ROW5, textColor, id--?0:6, bright, "Page 3 0x%02X", memoryPage[3]);

    print(COL4, ROW2, textColor, id--?0:3, bright, " A' = 0x%02X", cpu.af2 & 0x0FF );
    print(COL4, ROW3, textColor, id--?0:3, bright, "HL' = 0x%04X", cpu.hl2);
    print(COL4, ROW4, textColor, id--?0:3, bright, "BC' = 0x%04X", cpu.bc2);
    print(COL4, ROW5, textColor, id--?0:3, bright, "DE' = 0x%04X", cpu.de2);

    print(640, ROW1, textColor, "%s", (cpu.iff1 == cpu.iff2) ? (cpu.iff1 ? "EI" : "DI") : (cpu.iff1 ? "??" : "NMI"));
    print(640, ROW2, textColor, "IM%01X", cpu.im);
    print(640, ROW3, textColor, "I   = 0x%02X", cpu.i);
    print(640, ROW4, textColor, "R   = 0x%02X", cpu.r);

    print(COL1, ROW8, textColor, id--?0:4, bright, "%s", nameFor(memView[0]).c_str());
    displayMem(104, ROW7, textColor, addressFor(0), memView[0] == MV_MEM ? memViewPage[0] : -1);
    if( memView[0] == MV_MEM ) {
        print(COL1, ROW8+14, textColor, id--?0:2, bright, "%02X", memViewPage[0]);
    }
    else id--;

    print(COL1, ROW12, textColor, id--?0:4, bright, "%s", nameFor(memView[1]).c_str());
    displayMem(104, ROW11, textColor, addressFor(1), memView[1] == MV_MEM ? memViewPage[1] : -1);
    if( memView[1] == MV_MEM ) {
        print(COL1, ROW12+14, textColor, id--?0:2, bright, "%02X", memViewPage[1]);
    }
    else id--;

    print(COL1, ROW16, textColor, id--?0:4, bright, "%s", nameFor(memView[2]).c_str());
    displayMem(104, ROW15, textColor, addressFor(2), memView[2] == MV_MEM ? memViewPage[2] : -1);
    if( memView[2] == MV_MEM ) {
        print(COL1, ROW16+14, textColor, id--?0:2, bright, "%02X", memViewPage[2]);
    }
    else id--;

    std::bitset<8> ioSelectA(pio.port[0].io_select);
    std::bitset<8> portDataA(Z80PIO_GET_PA(pins));

    print(COL1, ROW19, textColor, "Port A");
    print(120, ROW19, textColor, (char*)ioSelectA.to_string('O', 'I').c_str());
    print(120, ROW20, textColor, (char*)portDataA.to_string().c_str());

    std::bitset<8> ioSelectB(pio.port[1].io_select);
    std::bitset<8> portDataB(Z80PIO_GET_PB(pins));

    print(220, ROW19, textColor, "Port B");
    print(290, ROW19, textColor, (char*)ioSelectB.to_string('O', 'I').c_str());
    print(290, ROW20, textColor, (char*)portDataB.to_string().c_str());

    print(430, ROW19, textColor, "[A]ppend audio %s", audioFile?"ON":"OFF");
    print(430, ROW20, textColor, "File \"%s\"", audioFilename);

    print(620, ROW19, textColor, "TTY :%d", uart_port(&uart));
    if( uart_connected(&uart)) {
        print(620, ROW20, textColor, "Connected [D]rop");
    }
    else {
        print(620, ROW20, textColor, "Disconnected");
    }

    int page = pagingEnabled ? memoryPage[((cpu.pc-1) >> 14) & 0x03] : 0;
    currentLoc = listing.getLocation(page << 16 | (cpu.pc-1));

    if( currentLoc.valid ) {
        int startLine = currentLoc.lineNum - 4;
        if( startLine < 0 ) startLine = 0;
        Listing::Location displayLoc = currentLoc;

        for( int i=0; i<12; i++ ) {
            displayLoc.lineNum = i+startLine;
            std::string line = listing.getLine(displayLoc);
            print(COL1, ROW22+(14*i), i==3 ? highColor: textColor, "%.86s", const_cast<char*>(line.c_str()));
        }
    }
    else {
        drawListing( cpu.pc-1, textColor, highColor );
    }

    print( COL1, END_ROW, menuColor, "[L]ist address");
    print( 200, END_ROW, menuColor, "[C]urrent address");

    if( breakpoint != NO_BREAKPOINT ) {
        print(390, END_ROW, menuColor, id--?0:-4, bright, "[B]reakpoint = 0x%04X", breakpoint);
    }
    else {
        print(390, END_ROW, menuColor, "[B]reakpoint");
    }
    
    print(620, END_ROW, menuColor, "[Q]uit");

    if( editMode ) {
        displayEdit();
    }

    SDL_RenderPresent(sdlRenderer);
}

std::string Beast::nameFor(MemView view) {
    switch(view) {
        case MV_PC : return "PC";
        case MV_SP : return "SP";
        case MV_HL : return "HL";
        case MV_BC : return "BC";
        case MV_DE : return "DE";
        case MV_IX : return "IX";
        case MV_IY : return "IY";
        case MV_Z80: return "Z80";
        case MV_MEM: return "PAGE";
        default:
            return "???";
    }
}

uint16_t Beast::addressFor(int view) {
switch(memView[view]) {
        case MV_PC : return cpu.pc-1;
        case MV_SP : return cpu.sp;
        case MV_HL : return cpu.hl;
        case MV_BC : return cpu.bc;
        case MV_DE : return cpu.de;
        case MV_IX : return cpu.ix;
        case MV_IY : return cpu.iy;
        case MV_Z80: return memAddress[view];
        case MV_MEM: return memPageAddress[view];
        default:
            return 0;
    }
}

Beast::MemView Beast::nextView(MemView view, int dir) {
    switch(view) {
        case MV_PC : return dir == 1 ? MV_SP : MV_MEM;
        case MV_SP : return dir == 1 ? MV_HL : MV_PC;
        case MV_HL : return dir == 1 ? MV_BC : MV_SP;
        case MV_BC : return dir == 1 ? MV_DE : MV_HL;
        case MV_DE : return dir == 1 ? MV_IX : MV_BC;
        case MV_IX : return dir == 1 ? MV_IY : MV_DE;
        case MV_IY : return dir == 1 ? MV_Z80 : MV_IX;
        case MV_Z80 : return dir == 1 ? MV_MEM : MV_IY;
        case MV_MEM : return dir == 1 ? MV_PC : MV_Z80;
        default:
            return MV_PC;
    }
}

void Beast::itemSelect(int direction) {
    switch(selection) {
        case SEL_MEM0 : memView[0] = nextView(memView[0], direction); break;
        case SEL_MEM1 : memView[1] = nextView(memView[1], direction); break;
        case SEL_MEM2 : memView[2] = nextView(memView[2], direction); break; 
    }
}

void Beast::drawListing(uint16_t address, SDL_Color textColor, SDL_Color highColor) {
    int length;
    auto f = [this](uint16_t address) { return this->readMem(address); };

    int matchedLine = -1;

    for( size_t i=0; i<decodedAddresses.size(); i++) {
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

    for( size_t i=0; i<12; i++ ) {
        if( decodedAddresses.size() > i ) {
            decodedAddresses[i] = address;
        }
        else {
            decodedAddresses.push_back(address);
        }
        std::string line = instr->decode(address, f, &length);
        print(COL1, ROW22+(14*i), (address == cpu.pc-1) ? highColor: textColor, "%04X             %s", address, const_cast<char*>(line.c_str()));
        address += length;
    }
}

void Beast::displayMem(int x, int y, SDL_Color textColor, uint16_t markAddress, int page) {
    uint16_t address = (markAddress & 0xFFF0)-16;

    uint16_t addressMask = page < 0 ? 0xFFFF : 0x3FFF;

    const int BUFFER_SIZE = 200;
    char buffer[BUFFER_SIZE]; 
    for( int row=0; row<3; row++ ) {
        int c = snprintf( buffer, BUFFER_SIZE, "0x%04X ", address & addressMask);
        if( c < 0 || c >= BUFFER_SIZE ) {
            break;
        }
        for( uint16_t i=0; i<16; i++ ) {
            uint8_t data = page < 0 ? readMem(address+i): readPage(page, address+i);

            int cs = snprintf( buffer+c, BUFFER_SIZE-c, 
                (address+i == markAddress) ? ">%02X" : " %02X", data);
            if( cs < 0 || cs+c >= BUFFER_SIZE ) {
                return;
            }
            c+=cs;
        }
        int cn =snprintf(buffer+c, BUFFER_SIZE-c, "   ");
        if( cn < 0 || cn+c >= BUFFER_SIZE ) {
            return;
        }
        c+=cn;
        for( int i=0; i<16; i++ ) {
            uint8_t data = page < 0 ? readMem(address+i): readPage(page, address+i);
            if( data < 32 || data > 127 ) {
                data = '.';
            }

            buffer[c++] = data;
            buffer[c] = 0;
            if( c+1 >= BUFFER_SIZE) {
                return;
            }
        }
        print(x, y+2+(14*row), textColor, buffer);
        address += 16;
    }
}

uint8_t Beast::readMem(uint16_t address) {
    uint32_t mappedAddr = address & 0x3FFF;
    bool isRam = false;
    
    if( pagingEnabled ) {
        int page = memoryPage[(address >> 14) & 0x03];
        isRam = (page & 0xE0) == 0x20;
        mappedAddr |= (page & 0x1F) << 14;
    }
    return isRam ? ram[mappedAddr] : rom[mappedAddr];
}

uint8_t Beast::readPage(int page, uint16_t address) {
    bool isRam = page > 0x1F;
    uint32_t mappedAddr = (address & 0x3FFF) | (page & 0x1F) << 14;
    return isRam ? ram[mappedAddr] : rom[mappedAddr];
}

template<typename... Args> void Beast::print(int x, int y, SDL_Color color, const char *fmt, Args... args) {
    char buffer[200]; 

    int c = snprintf(buffer, sizeof(buffer), fmt, args...);

    if( c > 0 && c<(int)sizeof(buffer)) {
        printb(x,y, color, 0, {0}, buffer);
    }
}

template<typename... Args> void Beast::print(int x, int y, SDL_Color color, int highlight, SDL_Color background, const char *fmt, Args... args) {
    char buffer[200]; 

    int c = snprintf(buffer, sizeof(buffer), fmt, args...);

    if( c > 0 && c<(int)sizeof(buffer)) {
        printb(x,y, color, highlight, background, buffer);
    }
}

void Beast::displayEdit() {
    char buffer[2] = {'0', 0};
    int width;
    int height;

    SDL_Color background = {0xFF, 0xFF, 0xFF};

    TTF_SizeUTF8(monoFont, buffer, &width, &height);
    boxRGBA(sdlRenderer, editX*zoom+(width*(editOffset-1)), (editY+1)*zoom, editX*zoom+(width*(editOffset+editDigits-1)), (editY-2)*zoom+height, background.r, background.g, background.b, 0xFF);

    SDL_Rect textRect;

    SDL_Color normal = {0x00, 0x00, 0x00};
    SDL_Color edited = {0xF0, 0x40, 0x40};

    for( int i=editDigits-1; i>=0; i--) {
        snprintf(buffer, sizeof(buffer), "%01X", (editValue >> (i*4)) & 0x0F);

        SDL_Color color = i == editIndex ? edited: normal;

        SDL_Surface *textSurface = TTF_RenderText_Blended(monoFont, buffer, color);
        SDL_Texture *textTexture = SDL_CreateTextureFromSurface(sdlRenderer, textSurface);

        textRect.x = editX*zoom+(width*(editOffset+(editDigits-i)-2)); 
        textRect.y = editY*zoom;
        textRect.w = textSurface->w;
        textRect.h = textSurface->h;

        SDL_RenderCopy(sdlRenderer, textTexture, NULL, &textRect);

        SDL_DestroyTexture(textTexture);
        SDL_FreeSurface(textSurface);
    }
}

void Beast::printb(int x, int y, SDL_Color color, int highlight, SDL_Color background, char* buffer) {
    SDL_Surface *textSurface = TTF_RenderText_Blended(monoFont, buffer, color);
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(sdlRenderer, textSurface);

    if( highlight != 0 ) {  
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
    SDL_Rect textRect = {0, (int)(keyboardTop*zoom), (int)(KEYBOARD_WIDTH*zoom), (int)(KEYBOARD_HEIGHT*zoom)};
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

void Beast::redrawScreen() {
    for( int i=0; i<DISPLAY_CHARS; i++) {
        display[i].changed = true;
    }
    drawKeys();
    drawBeast();
}

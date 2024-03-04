#include "beast.hpp"
#include <iostream>
#include <cstring>
#include <stdio.h>
#include <iomanip>
#include <algorithm>
#include "z80.h"
#include "z80pio.h"
#include "listing.hpp"
#include "SDL_image.h"
#include "nfd.h"

Beast::Beast(SDL_Window *window, int screenWidth, int screenHeight, float zoom, Listing &listing, std::vector<BinaryFile> files) 
    : rom {}, ram {}, memoryPage {0}, listing(listing), binaryFiles(files), gui(createRenderer(window), screenWidth, screenHeight) {

    windowId = SDL_GetWindowID(window);

    this->window = window;
    this->screenWidth = screenWidth;
    this->screenHeight = screenHeight;
    this->zoom = checkZoomFactor(screenWidth, screenHeight, zoom);

    TTF_Init();
    gui.init(this->zoom);

    instr = new Instructions();

    i2c = new I2c(Z80PIO_PB6, Z80PIO_PB7);
    display1 = new I2cDisplay(0x50);
    display2 = new I2cDisplay(0x53);
    rtc = new I2cRTC(0x6f, Z80PIO_PB5);

    i2c->addDevice(display1);
    i2c->addDevice(display2);
    i2c->addDevice(rtc);
    
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

    gui.startPrompt(0, "Loading...");
    gui.drawPrompt(true);
    gui.endPrompt(true);

    keyboardTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, KEYBOARD_WIDTH*zoom, KEYBOARD_HEIGHT*zoom);
    pcbTexture = loadTexture(sdlRenderer, PCB_IMAGE);

    drawKeys();
    for( int i=0; i<DISPLAY_CHARS; i++) {
        display.push_back(Digit(sdlRenderer, zoom));
    }
}

SDL_Texture* Beast::loadTexture(SDL_Renderer *sdlRenderer, const char* filename) {
    SDL_Texture *texture = NULL;
    texture = IMG_LoadTexture(sdlRenderer, filename);

    if (texture == NULL) {
        std::cout << "Failed to load texture, '" << filename << "' " << SDL_GetError() << std::endl;
    }

    return texture;
}

SDL_Renderer* Beast::createRenderer(SDL_Window *window) {
    sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if( !sdlRenderer) {
        std::cout << "Could not create renderer: " << SDL_GetError() << std::endl;
        exit(1);
    }
    return sdlRenderer;
}

float Beast::checkZoomFactor(int screenWidth, int screenHeight, float zoom) {
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

    uart_init(&uart, UART_CLOCK_HZ, clock_time_ps);
    
    if( videoBeast ) {
        int leftBorder = videoBeast->init(clock_time_ps, screenWidth*zoom);
        nextVideoBeastTickPs = 0;

        if( leftBorder > 0 ) SDL_SetWindowPosition(window, leftBorder, SDL_WINDOWPOS_CENTERED);
        SDL_RaiseWindow(window);
    }

    for(auto &bf: binaryFiles) {
        bf.load(rom, ram, pagingEnabled, memoryPage);
    }

    for(auto &source: listing.getFiles()) {
        listing.loadFile(source);
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

void Beast::reset() {
    z80_reset(&cpu);
    uart_reset(&uart, UART_CLOCK_HZ);
    pagingEnabled = false;
    for( int i=0; i<4; i++ ) {
        memoryPage[i] = 0;
    }
}

void Beast::loadSamples(Sint16 *stream, int length) {
    int index = 0;
    //if( audioAvailable < length ) return;

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

        if( (mode == DEBUG) || (mode == FILES) ) {
            drawBeast();
            
            if( mode == DEBUG ) {
                onDebug();
            }
            else {
                onFile();
            }

            gui.drawPrompt(false);
            gui.drawEdit();

            SDL_RenderPresent(sdlRenderer);

            if( gui.endPrompt(false) && gui.isPromptOK() ) {
                promptComplete();
            }

            SDL_Event windowEvent;

            while( SDL_PollEvent(&windowEvent ) == 0 ) {
                SDL_Delay(25);
                if( !uart_connected(&uart) ) {
                    uart_connect(&uart, true);
                    break;
                }
            }

            if( SDL_RENDER_TARGETS_RESET == windowEvent.type ) {
                redrawScreen();
            }

            if( SDL_QUIT == windowEvent.type ) {
                mode = QUIT;
            }

            if( windowEvent.window.windowID != windowId && videoBeast) {
                videoBeast->handleEvent(windowEvent);
                continue;
            }

            if (windowEvent.window.event == SDL_WINDOWEVENT_CLOSE) {
                mode = QUIT;
            }

            if( SDL_KEYDOWN == windowEvent.type ) {
                if( gui.isPrompt() ) {
                    gui.handleKey(windowEvent.key.keysym.sym);
                    if( gui.promptChanged() ) {
                        updatePrompt();
                    }
                }
                else if( gui.isEditing() ) {
                    if( gui.handleKey(windowEvent.key.keysym.sym) ) {
                        if( gui.isEditOK() ) {
                            editComplete();
                        }
                    }
                    else {
                        switch( windowEvent.key.keysym.sym ) {
                            case SDLK_UP       : updateMemoryEdit(-16, false); break;
                            case SDLK_DOWN     : updateMemoryEdit(16, false); break;
                            case SDLK_LEFT     : updateMemoryEdit(-1, false); break;
                            case SDLK_RIGHT    : updateMemoryEdit(1, false); break;
                        }
                    }
                }
                else if( mode == DEBUG ) {
                    debugMenu(windowEvent);
                }
                else {
                    fileMenu(windowEvent);
                }
            }
        }
    }
}

void Beast::debugMenu(SDL_Event windowEvent) {
    int maxSelection = (breakpoint == NOT_SET) ? static_cast<int>(SEL_BREAKPOINT) : static_cast<int>(SEL_END_MARKER);

    switch( windowEvent.key.keysym.sym ) {
        case SDLK_UP       : updateSelection(-1, maxSelection); break;
        case SDLK_DOWN     : updateSelection(1, maxSelection); break;
        case SDLK_LEFT  :
            if( itemEdit() ) {
                gui.editDelta(-1);
                editComplete();
            }
            else
                itemSelect(-1);
            break;
        case SDLK_RIGHT  :
            if( itemEdit() ) {
                gui.editDelta(1);
                editComplete();
            }
            else
                itemSelect(1);
            break;
        case SDLK_RETURN: 
            if( SEL_MEM0 == selection || SEL_VIDEOVIEW0 == selection ) startMemoryEdit(0);
            else if( SEL_MEM1 == selection || SEL_VIDEOVIEW1 == selection ) startMemoryEdit(1);
            else if( SEL_MEM2 == selection || SEL_VIDEOVIEW2 == selection ) startMemoryEdit(2);
            else itemEdit(); 
            break;
        case SDLK_SPACE : 
            if( SEL_MEM0 <= selection && selection <= SEL_VIEWADDR0 ) startMemoryEdit(0);
            if( SEL_MEM1 <= selection && selection <= SEL_VIEWADDR1 ) startMemoryEdit(1);
            if( SEL_MEM2 <= selection && selection <= SEL_VIEWADDR2 ) startMemoryEdit(2);
            break;
        case SDLK_b    : 
            if( breakpoint != NOT_SET ) {
                lastBreakpoint = breakpoint;
                breakpoint = NOT_SET;
                selection = SEL_PC;
            }
            else {
                selection = SEL_BREAKPOINT;
                gui.startEdit( lastBreakpoint, 440, GUI::END_ROW, 16, 4);
                breakpoint = lastBreakpoint;
            }
            break;
        case SDLK_q    : mode = QUIT;   break;
        case SDLK_r    : mode = RUN;    break;
        case SDLK_e    : reset();                // Fall through into step
        case SDLK_s    : mode = STEP;   break;
        case SDLK_u    : mode = OUT;    break;
        case SDLK_o    : mode = OVER;   break;
        case SDLK_f    : mode = FILES;   selection = 0; break;
        case SDLK_l    : if( listAddress == NOT_SET ) listAddress = (cpu.pc-1) & 0x0FFFF; else listAddress = NOT_SET; break;

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

void Beast::fileMenu(SDL_Event windowEvent) {
    unsigned int maxSelection = listing.fileCount() + binaryFiles.size();

    switch( windowEvent.key.keysym.sym ) {
        case SDLK_UP  : updateSelection(-1, maxSelection); break;
        case SDLK_DOWN: updateSelection(1, maxSelection); break;
        case SDLK_b    : mode = DEBUG;   selection = 0; break;
        case SDLK_RETURN: filePrompt(selection); break;
        case SDLK_1 ... SDLK_9: {
            unsigned int index = windowEvent.key.keysym.sym - SDLK_1;
            if( index < listing.fileCount() + binaryFiles.size() ) filePrompt(index); 
            break;
        } 
        case SDLK_s    : sourceFilePrompt(); break;
        case SDLK_l    : binaryFilePrompt(PROMPT_BINARY_CPU); break;
        case SDLK_p    : binaryFilePrompt(PROMPT_BINARY_PAGE); break;
        case SDLK_a    : binaryFilePrompt(PROMPT_BINARY_ADDRESS); break; 
    }
}

void Beast::filePrompt(unsigned int index) {
    if( index < listing.fileCount() ) {
        gui.startPrompt(PROMPT_SOURCE_FILE, "Select action for %s", listing.getFiles()[index].shortname.c_str());
        gui.promptChoice({"Refresh", "Delete"});
        fileActionIndex = index;
    }
    else if(index-listing.fileCount() < binaryFiles.size()) {
        gui.startPrompt(PROMPT_BINARY_FILE, "Select action for %s", binaryFiles[index-listing.fileCount()].getShortname().c_str());
        gui.promptChoice({"Reload", "Forget"});
        fileActionIndex = index-listing.fileCount();
    }
}

void Beast::sourceFilePrompt() {
    nfdchar_t  *path;
    nfdresult_t result = NFD_OpenDialog(&path, NULL, 0, NULL);
    
    SDL_RaiseWindow(window);

    if (result == NFD_OKAY) {
        listingPath = new std::string(path);
        NFD_FreePath(path);
        gui.startPrompt(PROMPT_LISTING, "Load listing to page 0x00");
        gui.promptValue(0, 24, 2);
    }   
}

void Beast::binaryFilePrompt(int promptId) {
    nfdchar_t  *path;
    nfdresult_t result = NFD_OpenDialog(&path, NULL, 0, NULL);
    
    SDL_RaiseWindow(window);

    if (result == NFD_OKAY) {
        listingPath = new std::string(path);
        NFD_FreePath(path);

        switch( promptId ) {
            case PROMPT_BINARY_ADDRESS :
                gui.startPrompt(PROMPT_BINARY_ADDRESS, "Load file to physical address 0x00000");
                gui.promptValue(0, 32, 5);
                break;
            case PROMPT_BINARY_PAGE :
                gui.startPrompt(PROMPT_BINARY_PAGE, "Load file to page 0x00");
                gui.promptValue(0, 21, 2);
                break;
            case PROMPT_BINARY_CPU :
                gui.startPrompt(PROMPT_BINARY_CPU, "Load file to CPU address 0x0000");
                gui.promptValue(0, 28, 4);
                break;
        }
    }
}

void Beast::promptComplete() {
    switch( gui.getPromptId() ) {
        case PROMPT_SOURCE_FILE    : {
            Listing::Source& source = listing.getFiles()[fileActionIndex];
            
            if( gui.getEditValue()==0 ) {
                gui.startPrompt(0, "Refreshing ...");
                gui.drawPrompt(true);
                listing.loadFile(source); 
                gui.endPrompt(true);
            }
            else {
                listing.removeFile(fileActionIndex); selection = std::max(0, fileActionIndex-1); 
            }
            break;
        }
        case PROMPT_BINARY_FILE : {
            if( gui.getEditValue()==0 ) {
                gui.startPrompt(0, "Reloading ...");
                gui.drawPrompt(true);
                binaryFiles[fileActionIndex].load(rom, ram, pagingEnabled, memoryPage);
                gui.endPrompt(true);
            }
            else {
                binaryFiles.erase(binaryFiles.begin() + fileActionIndex);
            }
            break;
        }
        case PROMPT_LISTING : {
            gui.startPrompt(0, "Loading ...");
            gui.drawPrompt(true);
            listing.addFile( *listingPath, gui.getEditValue()); 
            gui.endPrompt(true);
            break;
        }
        case PROMPT_BINARY_ADDRESS: {
            BinaryFile binary = BinaryFile(*listingPath, gui.getEditValue());
            reportLoad( binary.load(rom, ram, pagingEnabled, memoryPage) );
            binaryFiles.push_back(binary);
            break;
        }
        case PROMPT_BINARY_CPU: {
            BinaryFile binary = BinaryFile(*listingPath, gui.getEditValue(), -1, true);
            reportLoad( binary.load(rom, ram, pagingEnabled, memoryPage) );
            binaryFiles.push_back(binary);
            break;
        }
        case PROMPT_BINARY_PAGE:
            loadBinaryPage = gui.getEditValue();
            gui.startPrompt(PROMPT_BINARY_PAGE2, "Address in page 0x%02X: 0x0000", loadBinaryPage);
            gui.promptValue(0, 28, 4);
            break;  
        case PROMPT_BINARY_PAGE2: {
            BinaryFile binary = BinaryFile(*listingPath, gui.getEditValue(), loadBinaryPage);
            reportLoad( binary.load(rom, ram, pagingEnabled, memoryPage) );
            binaryFiles.push_back(binary);
            break;
        }
    }
}

void Beast::reportLoad(size_t bytes) {
    if( bytes > 0 ) {
        gui.startPrompt(0, "Loaded %d bytes OK", bytes);
    }
    else {
        gui.startPrompt(0, "Could not load file");
    }
    gui.promptYesNo();
}

void Beast::updatePrompt() {
    switch( gui.getPromptId() ) {
        case PROMPT_LISTING        : gui.updatePrompt("Load listing to page 0x%02X", gui.getEditValue()); break;
        case PROMPT_BINARY_ADDRESS : gui.updatePrompt("Load file to physical address 0x%05X", gui.getEditValue()); break;
        case PROMPT_BINARY_PAGE    : gui.updatePrompt("Load file to page 0x%02X", gui.getEditValue()); break;
        case PROMPT_BINARY_PAGE2   : gui.updatePrompt("Address in page 0x%02X: 0x%04X", loadBinaryPage, gui.getEditValue()); break;
        case PROMPT_BINARY_CPU     : gui.updatePrompt("Load file to CPU address 0x%04X", gui.getEditValue()); break;
    }
}

void Beast::updateSelection(int direction, int maxSelection) {
    selection += direction;
    bool skip;

    do {    
        skip = false;

        if( selection < 0 ) selection = maxSelection-1;
        if( selection >= maxSelection ) selection = 0;

        if( selection == SEL_VIEWPAGE0 &&  memView[0] != MV_MEM ) skip = true;
        if( selection == SEL_VIDEOVIEW0 && memView[0] != MV_VIDEO ) skip = true;
        if( selection == SEL_VIEWADDR0 && memView[0] != MV_MEM && memView[0] != MV_Z80 && memView[0] != MV_VIDEO ) skip = true;

        if( selection == SEL_VIEWPAGE1 &&  memView[1] != MV_MEM ) skip = true;
        if( selection == SEL_VIDEOVIEW1 && memView[1] != MV_VIDEO ) skip = true;
        if( selection == SEL_VIEWADDR1 && memView[1] != MV_MEM && memView[1] != MV_Z80 && memView[1] != MV_VIDEO) skip = true;

        if( selection == SEL_VIEWPAGE2 &&  memView[2] != MV_MEM ) skip = true;
        if( selection == SEL_VIDEOVIEW2 && memView[2] != MV_VIDEO ) skip = true;
        if( selection == SEL_VIEWADDR2 && memView[2] != MV_MEM && memView[2] != MV_Z80 && memView[2] != MV_VIDEO) skip = true;

        if( selection == SEL_LISTING && listAddress == NOT_SET ) skip = true;

        if( skip ) selection += direction;
    } while( skip );


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

void Beast::startMemoryEdit(int view) {
    if( gui.isContinuousEdit() ) {
        gui.endEdit(false);
        return;
    }
    memoryEditAddress = addressFor(view);
    memoryEditPage = memView[view] == MV_MEM ? memViewPage[view] : -1;
    memoryEditAddressMask = getAddressMask(view);
    memoryEditView = view;

    updateMemoryEdit(0, true);
}

void Beast::updateMemoryEdit(int delta, bool startEdit) {
    if( !startEdit && !gui.isContinuousEdit() ) return;

    memoryEditAddress += delta;
    if( memoryEditPage > 0 && memoryEditPage < 0x40 ) {
        if( (memoryEditAddress & 0xFF00) == 0xFF00 ) {
            memoryEditPage--;
        }
        if( (memoryEditAddress & 0x4000) == 0x4000) {
            memoryEditPage++;
        }
    }
    memoryEditAddress &= memoryEditAddressMask;

    uint8_t data = 0;

    if( memView[memoryEditView] == MV_VIDEO ) {
        data = readVideoMemory(memVideoView[memoryEditView], memoryEditAddress);
    } 
    else {
        data = memoryEditPage < 0 ? readMem(memoryEditAddress): readPage(memoryEditPage, memoryEditAddress);
    }
    int offset = 10 + 3*(memoryEditAddress & 0x0F);

    gui.startEdit(data, GUI::COL_MEM, GUI::ROW8 + 4*GUI::MEM_ROW_HEIGHT*memoryEditView, offset, 2, true);
}

bool Beast::itemEdit() {
    if( gui.isContinuousEdit() ) return false;

    switch( selection ) {
        case SEL_PC: gui.startEdit( cpu.pc-1, GUI::COL1, GUI::ROW1,  8, 4); break;
        case SEL_A : gui.startEdit( cpu.a, GUI::COL1, GUI::ROW2, 8, 2); break;
        case SEL_HL: gui.startEdit( cpu.hl, GUI::COL1, GUI::ROW3, 8, 4); break;
        case SEL_BC: gui.startEdit( cpu.bc, GUI::COL1, GUI::ROW4, 8, 4); break;
        case SEL_DE: gui.startEdit( cpu.de, GUI::COL1, GUI::ROW5, 8, 4); break;

        case SEL_SP: gui.startEdit( cpu.sp, GUI::COL2, GUI::ROW3, 8, 4); break;
        case SEL_IX: gui.startEdit( cpu.ix, GUI::COL2, GUI::ROW4, 8, 4); break;
        case SEL_IY: gui.startEdit( cpu.iy, GUI::COL2, GUI::ROW5, 8, 4); break;

        case SEL_PAGING: pagingEnabled = !pagingEnabled; break;
        case SEL_PAGE0 : gui.startEdit( memoryPage[0], GUI::COL3, GUI::ROW2, 10, 2); break;
        case SEL_PAGE1 : gui.startEdit( memoryPage[1], GUI::COL3, GUI::ROW3, 10, 2); break;
        case SEL_PAGE2 : gui.startEdit( memoryPage[2], GUI::COL3, GUI::ROW4, 10, 2); break;
        case SEL_PAGE3 : gui.startEdit( memoryPage[3], GUI::COL3, GUI::ROW5, 10, 2); break;

        case SEL_VIEWADDR0 : 
            if( memView[0] == MV_Z80) gui.startEdit( memAddress[0], GUI::COL1, GUI::ROW8, 3, 4); 
            if( memView[0] == MV_MEM) gui.startEdit( memPageAddress[0], GUI::COL1, GUI::ROW9, 3, 4); 
            if( memView[0] == MV_VIDEO) gui.startEdit( getVideoAddress(0, memVideoView[0]), GUI::COL1, GUI::ROW9, 3, 5); 
            break;
        case SEL_VIEWADDR1 : 
            if( memView[1] == MV_Z80) gui.startEdit( memAddress[1], GUI::COL1, GUI::ROW12, 3, 4); 
            if( memView[1] == MV_MEM) gui.startEdit( memPageAddress[1], GUI::COL1, GUI::ROW13, 3, 4); 
            if( memView[1] == MV_VIDEO) gui.startEdit( getVideoAddress(1, memVideoView[1]), GUI::COL1, GUI::ROW13, 3, 5); 
            break;
        case SEL_VIEWADDR2 : 
            if( memView[2] == MV_Z80) gui.startEdit( memAddress[2], GUI::COL1, GUI::ROW16, 3, 4); 
            if( memView[2] == MV_MEM) gui.startEdit( memPageAddress[2], GUI::COL1, GUI::ROW17, 3, 4);
            if( memView[2] == MV_VIDEO) gui.startEdit( getVideoAddress(2, memVideoView[2]), GUI::COL1, GUI::ROW17, 3, 5); 
            break;

        case SEL_VIEWPAGE0 : gui.startEdit( memViewPage[0], GUI::COL1, GUI::ROW8, 1, 2);  break;
        case SEL_VIEWPAGE1 : gui.startEdit( memViewPage[1], GUI::COL1, GUI::ROW12, 1, 2); break;
        case SEL_VIEWPAGE2 : gui.startEdit( memViewPage[2], GUI::COL1, GUI::ROW16, 1, 2); break;

        case SEL_A2: gui.startEdit( cpu.af2 & 0xFF, GUI::COL4, GUI::ROW2, 9, 2); break;
        case SEL_HL2: gui.startEdit( cpu.hl2, GUI::COL4, GUI::ROW3, 9, 4); break;
        case SEL_BC2: gui.startEdit( cpu.bc2, GUI::COL4, GUI::ROW4, 9, 4);  break;
        case SEL_DE2: gui.startEdit( cpu.de2, GUI::COL4, GUI::ROW5, 9, 4); break;

        case SEL_LISTING: gui.startEdit( listAddress, GUI::COL1, GUI::END_ROW, 10, 4); break;
        case SEL_BREAKPOINT:  gui.startEdit( breakpoint, 440, GUI::END_ROW, 16, 4); break;
    }

    return gui.isEditing();
}

void Beast::editComplete() {
    uint32_t editValue = gui.getEditValue();

    if( gui.isContinuousEdit() ) {
        if( memView[memoryEditView] != MV_VIDEO ) {
            writeMem(memoryEditPage, memoryEditAddress, editValue );
        }
        else {
            writeVideoMemory(memVideoView[memoryEditView], memoryEditAddress, editValue );
        }
        updateMemoryEdit(1, false);
        return;
    }
    else switch( selection ) {
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

        case SEL_VIEWADDR0: 
            if( memView[0] == MV_Z80 ) memAddress[0] = editValue;
            if( memView[0] == MV_MEM ) memPageAddress[0] = editValue & 0x3FFF;
            if( memView[0] == MV_VIDEO ) setVideoAddress(0, memVideoView[0], getAddressMask(0) & editValue);
            break;
        case SEL_VIEWADDR1: 
            if( memView[1] == MV_Z80 ) memAddress[1] = editValue;
            if( memView[1] == MV_MEM ) memPageAddress[1] = editValue & 0x3FFF;
            if( memView[1] == MV_VIDEO ) setVideoAddress(1, memVideoView[1], getAddressMask(1) & editValue);
            break;
        case SEL_VIEWADDR2: 
            if( memView[2] == MV_Z80 ) memAddress[2] = editValue;
            if( memView[2] == MV_MEM ) memPageAddress[2] = editValue & 0x3FFF;
            if( memView[2] == MV_VIDEO ) setVideoAddress(2, memVideoView[2], getAddressMask(2) & editValue);
            break;

        case SEL_VIEWPAGE0 : memViewPage[0] = editValue <= 0x40 ? editValue : 0x40; break;
        case SEL_VIEWPAGE1 : memViewPage[1] = editValue <= 0x40 ? editValue : 0x40; break;
        case SEL_VIEWPAGE2 : memViewPage[2] = editValue <= 0x40 ? editValue : 0x40; break;

        case SEL_A2: cpu.af2 = ((cpu.af2 & 0xFF00) | (editValue & 0x0FF)); break;
        case SEL_HL2: cpu.hl2 = editValue; break;
        case SEL_BC2: cpu.bc2 = editValue; break;
        case SEL_DE2: cpu.de2 = editValue; break;

        case SEL_LISTING: listAddress = editValue; break;
        case SEL_BREAKPOINT: breakpoint = editValue; break;
    }
    gui.endEdit(false);
}

void Beast::onFile() {
    boxRGBA(sdlRenderer, 32*zoom, 32*zoom, (screenWidth-24)*zoom, (screenHeight-24)*zoom, 0xF0, 0xF0, 0xE0, 0xE8);
    
    SDL_Color textColor = {0, 0x30, 0x30};
    SDL_Color bright =    {0xD0, 0xFF, 0xD0};
    SDL_Color menuColor = {0x30, 0x30, 0xA0};


    gui.print(GUI::COL1, 34, menuColor, "Add [S]ource");
    gui.print(GUI::COL2, 34, menuColor, "[L]oad to CPU");
    gui.print(GUI::COL3, 34, menuColor, "Load to [P]age");
    gui.print(GUI::COL4, 34, menuColor, "Load to [A]ddress");

    int row = GUI::ROW2;    
    int index = 1;
    int id = selection;

    if( listing.fileCount() > 0 ) {
        gui.print( GUI::COL3, row, textColor, "Source Files" );

        row += 2*GUI::ROW_HEIGHT;

        for(auto &source: listing.getFiles()) { 
            gui.print( GUI::COL1, row, textColor, id--?0:4, bright, "[%2d] Page 0x%02X   %s", index++, source.page, source.filename.c_str());
            row += GUI::ROW_HEIGHT;
        }

        row += GUI::ROW_HEIGHT;
    }

    if( binaryFiles.size() > 0 ) {
        gui.print( GUI::COL3, row, textColor, "Binary Files" ); 
        
        row += 2*GUI::ROW_HEIGHT;

        for(auto &file: binaryFiles) {

            if( file.isCPUAddress() ) {
                gui.print( GUI::COL1, row, textColor, id--?0:4, bright, "[%2d] Z80           0x%04X  %.60s", index++, file.getAddress() & 0x0FFFF, file.getFilename().c_str());
            }
            else if( file.getPage() >= 0 ) {
                gui.print( GUI::COL1, row, textColor, id--?0:4, bright, "[%2d] Logical 0x%02X:0x%04X  %.60s", index++, file.getPage(), file.getAddress() & 0x03FFF, file.getFilename().c_str());
            }
            else {
                gui.print( GUI::COL1, row, textColor, id--?0:4, bright, "[%2d] Physical     0x%05X  %.60s", index++, file.getAddress() & 0x0FFFFF, file.getFilename().c_str());
            }
            row += GUI::ROW_HEIGHT;
        }
    }

    gui.print(640, GUI::END_ROW, menuColor, "[B]ack");
}

void Beast::onDebug() {
    boxRGBA(sdlRenderer, 32*zoom, 32*zoom, (screenWidth-24)*zoom, (screenHeight-24)*zoom, 0xF0, 0xF0, 0xE0, 0xE8);
    
    SDL_Color textColor = {0, 0x30, 0x30};
    SDL_Color disassColor={0x60, 0, 0x60};
    SDL_Color highColor = {0xA0, 0x30, 0x30};
    SDL_Color bright =    {0xD0, 0xFF, 0xD0};
    SDL_Color menuColor = {0x30, 0x30, 0xA0};

    gui.print(GUI::COL1, 34, menuColor, "[R]un");
    gui.print(GUI::COL2, 34, menuColor, "[S]tep");
    gui.print(GUI::COL3, 34, menuColor, "Step [O]ver");
    gui.print(GUI::COL4, 34, menuColor, "Step o[U]t");
    gui.print(640, 34, menuColor, "Until [T]aken");

    int id = selection;

    gui.print(GUI::COL1, GUI::ROW1, textColor, id--?0:2, bright, "PC = 0x%04X", (uint16_t)(cpu.pc-1));
    gui.print(GUI::COL1, GUI::ROW2, textColor, id--?0:2, bright, " A = 0x%02X", cpu.a );
    gui.print(GUI::COL1, GUI::ROW3, textColor, id--?0:2, bright, "HL = 0x%04X", cpu.hl);
    gui.print(GUI::COL1, GUI::ROW4, textColor, id--?0:2, bright, "BC = 0x%04X", cpu.bc);
    gui.print(GUI::COL1, GUI::ROW5, textColor, id--?0:2, bright, "DE = 0x%04X", cpu.de);

    char carry = (cpu.f & Z80_CF) ? 'C' : 'c';
    char neg = (cpu.f & Z80_NF) ? 'N' : 'n';
    char overflow = (cpu.f & Z80_VF) ? 'V' : 'v';
    char zero = (cpu.f & Z80_ZF) ? 'Z' : 'z';
    char sign = (cpu.f & Z80_SF) ? 'S' : 's';

    char xflag = (cpu.f & Z80_XF) ? '1' : '0';
    char hflag = (cpu.f & Z80_HF) ? '1' : '0';
    char yflag = (cpu.f & Z80_YF) ? '1' : '0';

    gui.print(GUI::COL2, GUI::ROW1, textColor, id--?0:5, bright, "Flags %c%c%c%c%c%c%c%c", sign, zero, yflag, hflag, xflag, overflow, neg, carry);
    gui.print(GUI::COL2, GUI::ROW3, textColor, id--?0:2, bright, "SP = 0x%04X", cpu.sp);
    gui.print(GUI::COL2, GUI::ROW4, textColor, id--?0:2, bright, "IX = 0x%04X", cpu.ix);
    gui.print(GUI::COL2, GUI::ROW5, textColor, id--?0:2, bright, "IY = 0x%04X", cpu.iy);

    if( pagingEnabled ) {
        gui.print(GUI::COL3, GUI::ROW1, textColor, id--?0:-2, bright, "Paging ON" );
    }
    else {
        gui.print(GUI::COL3, GUI::ROW1, textColor, id--?0:-3, bright, "Paging OFF" );
    }

    gui.print(GUI::COL3, GUI::ROW2, textColor, id--?0:6, bright, "Page 0 0x%02X", memoryPage[0]);
    gui.print(GUI::COL3, GUI::ROW3, textColor, id--?0:6, bright, "Page 1 0x%02X", memoryPage[1]);
    gui.print(GUI::COL3, GUI::ROW4, textColor, id--?0:6, bright, "Page 2 0x%02X", memoryPage[2]);
    gui.print(GUI::COL3, GUI::ROW5, textColor, id--?0:6, bright, "Page 3 0x%02X", memoryPage[3]);

    gui.print(GUI::COL4, GUI::ROW2, textColor, id--?0:3, bright, " A' = 0x%02X", cpu.af2 & 0x0FF );
    gui.print(GUI::COL4, GUI::ROW3, textColor, id--?0:3, bright, "HL' = 0x%04X", cpu.hl2);
    gui.print(GUI::COL4, GUI::ROW4, textColor, id--?0:3, bright, "BC' = 0x%04X", cpu.bc2);
    gui.print(GUI::COL4, GUI::ROW5, textColor, id--?0:3, bright, "DE' = 0x%04X", cpu.de2);

    gui.print(640, GUI::ROW1, textColor, "%s", (cpu.iff1 == cpu.iff2) ? (cpu.iff1 ? "EI" : "DI") : (cpu.iff1 ? "??" : "NMI"));
    gui.print(640, GUI::ROW2, textColor, "IM%01X", cpu.im);
    gui.print(640, GUI::ROW3, textColor, "I   = 0x%02X", cpu.i);
    gui.print(640, GUI::ROW4, textColor, "R   = 0x%02X", cpu.r);

    id = drawMemoryLayout(0, GUI::ROW7, id, textColor, bright);
    id = drawMemoryLayout(1, GUI::ROW11, id, textColor, bright);
    id = drawMemoryLayout(2, GUI::ROW15, id, textColor, bright);

    std::bitset<8> ioSelectA(pio.port[0].io_select);
    std::bitset<8> portDataA(Z80PIO_GET_PA(pins));

    gui.print(GUI::COL1, GUI::ROW19, textColor, "Port A");
    gui.print(120, GUI::ROW19, textColor, (char*)ioSelectA.to_string('O', 'I').c_str());
    gui.print(120, GUI::ROW20, textColor, (char*)portDataA.to_string().c_str());

    std::bitset<8> ioSelectB(pio.port[1].io_select);
    std::bitset<8> portDataB(Z80PIO_GET_PB(pins));

    gui.print(220, GUI::ROW19, textColor, "Port B");
    gui.print(290, GUI::ROW19, textColor, (char*)ioSelectB.to_string('O', 'I').c_str());
    gui.print(290, GUI::ROW20, textColor, (char*)portDataB.to_string().c_str());

    gui.print(430, GUI::ROW19, menuColor, "[A]ppend audio %s", audioFile?"ON":"OFF");
    gui.print(430, GUI::ROW20, textColor, "File \"%s\"", audioFilename);

    gui.print(620, GUI::ROW19, textColor, "TTY :%d", uart_port(&uart));
    if( uart_connected(&uart)) {
        gui.print(620, GUI::ROW20, menuColor, "Connected [D]rop");
    }
    else {
        gui.print(620, GUI::ROW20, textColor, "Disconnected");
    }

    uint16_t address = listAddress == NOT_SET ? cpu.pc-1 : listAddress;

    int page = pagingEnabled ? memoryPage[((address) >> 14) & 0x03] : 0;
    drawListing( page, address, textColor, highColor, disassColor );

    if( listAddress == NOT_SET ) {
        gui.print( GUI::COL1, GUI::END_ROW, menuColor, "[L]ist address");
        id--;
    }
    else {
        gui.print( GUI::COL1, GUI::END_ROW, menuColor, id--?0:-4, bright, "[L]ist 0x%04X", listAddress);
    }

    gui.print( GUI::COL2, GUI::END_ROW, menuColor, "R[E]set");
    gui.print( GUI::COL3, GUI::END_ROW, menuColor, "[F]iles");

    if( breakpoint == NOT_SET ) {
        gui.print(440, GUI::END_ROW, menuColor, "[B]reakpoint");
    }
    else {
        gui.print(440, GUI::END_ROW, menuColor, id--?0:-4, bright, "[B]reakpoint 0x%04X", breakpoint);
       
    }
    
    gui.print(640, GUI::END_ROW, menuColor, "[Q]uit");
}

int Beast::drawMemoryLayout(int view, int topRow, int id, SDL_Color textColor, SDL_Color bright) {
    gui.print(GUI::COL1, topRow, textColor, id--?0:5, bright, "%s", nameFor(memView[view]).c_str());
    
    uint32_t address = memView[view] != MV_VIDEO ? addressFor(view): getVideoAddress(view, memVideoView[view]);
    int page = memView[view] == MV_MEM ? memViewPage[view] : -1;
    
    if( gui.isContinuousEdit() && (view==memoryEditView) ) {
        page = memoryEditPage;
        address = memoryEditAddress;
    }

    if( memView[view] != MV_VIDEO ) {
        displayMem(GUI::COL_MEM, topRow, textColor, address, page);
    }  

    if( memView[view] == MV_MEM ) {
        gui.print(GUI::COL1, topRow+GUI::MEM_ROW_HEIGHT, textColor, id--?0:2, bright, "%02X", page);
        id--;
    }
    else if( memView[view] == MV_VIDEO ) {
        displayVideoMem(GUI::COL_MEM,topRow, textColor, memVideoView[view], address);
        id--;

        switch( memVideoView[view] ) {
            case VV_RAM     : gui.print(GUI::COL1, topRow+GUI::MEM_ROW_HEIGHT, textColor, id--?0:3, bright, "RAM");    break;
            case VV_REG     : gui.print(GUI::COL1, topRow+GUI::MEM_ROW_HEIGHT, textColor, id--?0:4, bright, "REGS");   break;
            case VV_PAL1    : gui.print(GUI::COL1, topRow+GUI::MEM_ROW_HEIGHT, textColor, id--?0:5, bright, "PAL 1");    break;
            case VV_PAL2    : gui.print(GUI::COL1, topRow+GUI::MEM_ROW_HEIGHT, textColor, id--?0:5, bright, "PAL 2");    break;
            case VV_SPR     : gui.print(GUI::COL1, topRow+GUI::MEM_ROW_HEIGHT, textColor, id--?0:6, bright, "SPRITE"); break;
            default:
                gui.print(GUI::COL1, topRow+GUI::MEM_ROW_HEIGHT, textColor, id--?0:4, bright, "????");
        }
    }
    else id-=2;

    if( memView[view] == MV_MEM ) {
        gui.print(GUI::COL1, topRow + GUI::MEM_ROW_HEIGHT*2, textColor, id--?0:6, bright, "0x%04X", address);
    }
    else if( memView[view] == MV_Z80 ) {
        gui.print(GUI::COL1, topRow + GUI::MEM_ROW_HEIGHT, textColor, id--?0:6, bright, "0x%04X", address);
    }
    else if( memView[view] == MV_VIDEO ) {
        gui.print(GUI::COL1, topRow + GUI::MEM_ROW_HEIGHT*2, textColor, id--?0:7, bright, "0x%05X", address);
    }
    else id--;

    return id;
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
        case MV_VIDEO: return "VIDEO";
        default:
            return "???";
    }
}

uint32_t Beast::addressFor(int view) {
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
        case MV_VIDEO:
            return getVideoAddress(view, memVideoView[view]);
        default:
            return 0;
    }
}

uint32_t Beast::getAddressMask(int view) {
    if( memView[view] == MV_MEM ) {
        return 0x3FFF;
    }
    else if( memView[view] == MV_VIDEO ) {
        switch(memVideoView[view]) {
            case VV_RAM : return VideoBeast::VIDEO_RAM_LENGTH-1;
            case VV_REG : return VideoBeast::REGISTERS_LENGTH-1;
            case VV_PAL1: 
            case VV_PAL2: return VideoBeast::PALETTE_LENGTH*2-1;
            case VV_SPR : return VideoBeast::SPRITE_LENGTH*VideoBeast::SPRITE_BYTES-1;
        }
    }
    
    return 0x0FFFF;
}

Beast::MemView Beast::nextView(MemView view, int dir) {
    switch(view) {
        case MV_PC : return dir == 1 ? MV_SP : (videoBeast ? MV_VIDEO : MV_MEM);
        case MV_SP : return dir == 1 ? MV_HL : MV_PC;
        case MV_HL : return dir == 1 ? MV_BC : MV_SP;
        case MV_BC : return dir == 1 ? MV_DE : MV_HL;
        case MV_DE : return dir == 1 ? MV_IX : MV_BC;
        case MV_IX : return dir == 1 ? MV_IY : MV_DE;
        case MV_IY : return dir == 1 ? MV_Z80 : MV_IX;
        case MV_Z80 : return dir == 1 ? MV_MEM : MV_IY;
        case MV_MEM : return dir == 1 ? (videoBeast ? MV_VIDEO: MV_PC) : MV_Z80;
        case MV_VIDEO: return dir == 1 ? MV_PC : MV_MEM;
        default:
            return MV_PC;
    }
}

Beast::VideoView Beast::nextVideoView(VideoView view, int dir) {
    switch( view ) {
        case VV_RAM : return dir == 1 ? VV_REG : VV_SPR;
        case VV_REG : return dir == 1 ? VV_PAL1 : VV_RAM;
        case VV_PAL1 : return dir == 1 ? VV_PAL2 : VV_REG;
        case VV_PAL2 : return dir == 1 ? VV_SPR : VV_PAL1;
        case VV_SPR : return dir == 1 ? VV_RAM : VV_PAL2;
        default:
            return VV_RAM;        
    }

}

void Beast::itemSelect(int direction) {
    if( gui.isContinuousEdit() ) {
        return;
    }

    switch(selection) {
        case SEL_MEM0 : memView[0] = nextView(memView[0], direction); break;
        case SEL_MEM1 : memView[1] = nextView(memView[1], direction); break;
        case SEL_MEM2 : memView[2] = nextView(memView[2], direction); break; 
        case SEL_VIDEOVIEW0 : memVideoView[0] = nextVideoView(memVideoView[0], direction); break;
        case SEL_VIDEOVIEW1 : memVideoView[1] = nextVideoView(memVideoView[1], direction); break;
        case SEL_VIDEOVIEW2 : memVideoView[2] = nextVideoView(memVideoView[2], direction); break;
    }
}

void Beast::drawListing(int page, uint16_t address, SDL_Color textColor, SDL_Color highColor, SDL_Color disassColor) {
    Listing::Location currentLoc = listing.getLocation(page << 16 | address);

    int matchedLine = -1;

    if( currentLoc.valid ) {
        currentLoc.lineNum = currentLoc.lineNum < 4 ? 0 : currentLoc.lineNum - 4;

        std::pair<Listing::Line, bool> line = listing.getLine(currentLoc);
        if( line.second ) {
            address = line.first.address;
        }
    }
    else {
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
    }

    int length;
    auto f = [this](uint16_t address) { return this->readMem(address); };
    
    for( size_t i=0; i<12; i++ ) {
        std::pair<Listing::Line, bool> line;

        if( currentLoc.valid ) { 
            line = listing.getLine(currentLoc);

            while( line.second && line.first.address < address ) {
                currentLoc.lineNum++;
                line = listing.getLine(currentLoc);
            } 

            bool valid = line.second;

            if( line.second && line.first.address == address ) {
                for( int i=0; i<line.first.byteCount; i++ ) {
                    if( readMem(address+i) != line.first.bytes[i] ) {
                        valid = false;
                        break;
                    }
                }
                currentLoc.lineNum++;
            }

            if( valid && line.first.address == address ) {
                gui.print(GUI::COL1, GUI::ROW22+(14*i), i==3 ? highColor: textColor, "%.86s", const_cast<char*>(line.first.text.c_str()));
                address += line.first.byteCount;
                continue;
            }
            if( line.second && line.first.address == address && line.first.isData ) {
                std::string byteString;
                for( int j=4; j-->0; ) {
                    if( j < line.first.byteCount ) {
                        char buffer[4];
                        int c = snprintf( buffer, 4, "%02X ", readMem(address+j));
                        if( c>0 && c<4 )
                            byteString.insert( 0, buffer, c );
                    }
                    else {
                        byteString.insert(0, "   ");
                    }
                }
                gui.print(GUI::COL1, GUI::ROW22+(14*i), (address == cpu.pc-1) ? highColor: disassColor, "----   %04X %s", address, const_cast<char*>(byteString.c_str()));
                address += line.first.byteCount;
                continue;
            }
        }

        if( decodedAddresses.size() > i ) {
            decodedAddresses[i] = address;
        }
        else {
            decodedAddresses.push_back(address);
        }
        std::string decoded = instr->decode(address, f, &length);
        decoded.insert(0, "                ");

        for( int j=4; j-->0; ) {
            if( j < length) {
                char buffer[4];
                int c = snprintf( buffer, 4, "%02X ", readMem(address+j));
                if( c>0 && c<4 )
                    decoded.insert( 0, buffer, c );
            }
            else {
                decoded.insert(0, "   ");
            }
        }

        gui.print(GUI::COL1, GUI::ROW22+(14*i), (address == cpu.pc-1) ? highColor: disassColor, "----   %04X %s", address, const_cast<char*>(decoded.c_str()));
        address += length;
    }
}
uint8_t Beast::readVideoMemory(VideoView view, uint32_t address) {
    switch( view ) {
        case VV_RAM: return videoBeast->readRam(address);
        case VV_REG: return videoBeast->readRegister(address); 
        case VV_PAL1: return videoBeast->readPalette(1, address);  
        case VV_PAL2: return videoBeast->readPalette(2, address);
        case VV_SPR: return videoBeast->readSprite(address);
    }

    return 0;
}

void Beast::writeVideoMemory(VideoView view, uint32_t address, uint8_t value ) {
    switch( view ) {
        case VV_RAM: videoBeast->writeRam(address, value);          break;
        case VV_REG: videoBeast->writeRegister(address, value);     break; 
        case VV_PAL1: videoBeast->writePalette(1, address, value);  break; 
        case VV_PAL2: videoBeast->writePalette(2, address, value);  break; 
        case VV_SPR: videoBeast->writeSprite(address, value);       break;
    }
}

void Beast::displayVideoMem(int x, int y, SDL_Color textColor, VideoView view, uint32_t markAddress) {
    
    uint32_t address = (markAddress & 0xFFFF0)-16;
    
    const int BUFFER_SIZE = 200;
    char buffer[BUFFER_SIZE]; 

    for( int row=0; row<3; row++ ) {
        int c=0;
        switch( view ) {
            case VV_RAM : c = snprintf( buffer, BUFFER_SIZE,   "0x%05X ", address & 0x0FFFFF);  break;
            case VV_REG : c = snprintf( buffer, BUFFER_SIZE, "   0x%02X ", address & 0X0FF);  break;
            case VV_PAL1: c = snprintf( buffer, BUFFER_SIZE,  "  0x%03X ", address & 0x1FF);  break;
            case VV_PAL2: c = snprintf( buffer, BUFFER_SIZE,  "  0x%03X ", address & 0x1FF);  break;
            case VV_SPR : c = snprintf( buffer, BUFFER_SIZE,  "  0x%03X ", address & 0x7FF);  break;
        }
        if( c < 0 || c >= BUFFER_SIZE ) {
            break;
        }

        for( uint16_t i=0; i<16; i++ ) {
            uint8_t data = readVideoMemory(view, address+i);

            int cs = snprintf( buffer+c, BUFFER_SIZE-c, (address+i == markAddress) ? ">%02X" : " %02X", data);
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

        if( view != VV_PAL1 && view != VV_PAL2 ) {
            for( int i=0; i<16; i++ ) {
                uint8_t data = readVideoMemory(view, address+i);

                if( data < 32 || data > 127 ) {
                    data = '.';
                }

                buffer[c++] = data;
                buffer[c] = 0;
                if( c+1 >= BUFFER_SIZE) {
                    return;
                }
            }
        }

        gui.print(x, y+(GUI::MEM_ROW_HEIGHT*row), textColor, buffer);

        if( view == VV_PAL1 || view == VV_PAL2 ) {
            for( int i=0; i<8; i++ ) {
                uint16_t packedRGB = readVideoMemory(view, address+i*2) + (readVideoMemory(view, address+i*2+1) << 8);
                uint8_t r, g, b;

                videoBeast->unpackRGB(packedRGB, &r, &g, &b);

                boxRGBA(sdlRenderer, (x+480+(i*GUI::MEM_ROW_HEIGHT))*zoom, (y+(GUI::MEM_ROW_HEIGHT*row)+4)*zoom, (x+478+((i+1)*GUI::MEM_ROW_HEIGHT))*zoom, (y+(GUI::MEM_ROW_HEIGHT*(row+1))+2)*zoom, r, g, b, 0xFF);
                if( packedRGB & 0x8000 ) {
                    uint8_t col = ((1+r+g+b)/3) > 0x80 ? 0 : 0xFF;

                    lineRGBA(sdlRenderer, (x+480+(i*GUI::MEM_ROW_HEIGHT))*zoom, (y+(GUI::MEM_ROW_HEIGHT*row)+4)*zoom, (x+478+((i+1)*GUI::MEM_ROW_HEIGHT))*zoom, (y+(GUI::MEM_ROW_HEIGHT*(row+1))+2)*zoom, col, col, col, 0xFF);
                    lineRGBA(sdlRenderer, (x+480+(i*GUI::MEM_ROW_HEIGHT))*zoom, (y+(GUI::MEM_ROW_HEIGHT*(row+1))+2)*zoom, (x+478+((i+1)*GUI::MEM_ROW_HEIGHT))*zoom, (y+(GUI::MEM_ROW_HEIGHT*row)+4)*zoom, col, col, col, 0xFF);
                }
            }
        }
        address += 16;
    }
}
void Beast::displayMem(int x, int y, SDL_Color textColor, uint16_t markAddress, int page) {
    uint16_t address = (markAddress & 0xFFF0)-16;

    uint16_t addressMask = page < 0 ? 0xFFFF : 0x3FFF;

    const int BUFFER_SIZE = 200;
    char buffer[BUFFER_SIZE]; 
    for( int row=0; row<3; row++ ) {
        int c = snprintf( buffer, BUFFER_SIZE, " 0x%04X ", address & addressMask);
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
        gui.print(x, y+(GUI::MEM_ROW_HEIGHT*row), textColor, buffer);
        address += 16;
    }
}

void Beast::writeMem(int page, uint16_t address, uint8_t data) {
    if( page < 0 ) {
        page = memoryPage[(address >> 14) & 0x03];
    }

    uint32_t mappedAddr = (address & 0x3FFF) | ((page & 0x1F) << 14);

    if( (page & 0xE0) == 0x20 ) {
        ram[mappedAddr] = data;
    }
    else if( (page & 0xE0) == 0x40 ) {
        if( videoBeast ) {
            videoBeast->write(address, data, clock_time_ps);
        }
    }
    else {
        rom[mappedAddr] = data;
    }
}

uint8_t Beast::readMem(uint16_t address) {
    int page = pagingEnabled ? memoryPage[(address >> 14) & 0x03] : 0;
    return readPage(page, address);
}

uint8_t Beast::readPage(int page, uint16_t address) {
    bool isRam = (page & 0xE0) == 0x20;
    if( !isRam && ((page & 0xE0) == 0x40)) {
        // Videobeast
        if( videoBeast ) {
            return videoBeast->read(address, clock_time_ps);
        }
        else {
            return 0;
        }
    }
    uint32_t mappedAddr = (address & 0x3FFF) | (page & 0x1F) << 14;
    return isRam ? ram[mappedAddr] : rom[mappedAddr];
}

uint32_t Beast::getVideoAddress(int index, VideoView view) {
    switch( view ) {
        case VV_RAM : return memVideoAddress[index][0];
        case VV_REG : return memVideoAddress[index][1];
        case VV_PAL1: return memVideoAddress[index][2];
        case VV_PAL2: return memVideoAddress[index][3];
        case VV_SPR : return memVideoAddress[index][4];
    }

    return 0;
}

void Beast::setVideoAddress(int index, VideoView view, uint32_t value) {
    switch( view ) {
        case VV_RAM : memVideoAddress[index][0] = value;  break;
        case VV_REG : memVideoAddress[index][1] = value;  break;
        case VV_PAL1: memVideoAddress[index][2] = value;  break;
        case VV_PAL2: memVideoAddress[index][3] = value;  break;
        case VV_SPR : memVideoAddress[index][4] = value;  break;
    }
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

    SDL_Point size;
    SDL_QueryTexture(pcbTexture, NULL, NULL, &size.x, &size.y);

    int pcbHeight = (int)((screenWidth / (float)size.x) * size.y);

    SDL_SetRenderDrawColor(sdlRenderer, 0x0, 0x0, 0x0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(sdlRenderer);
    SDL_Rect textRect = {0, (int)(keyboardTop*zoom), (int)(KEYBOARD_WIDTH*zoom), (int)(KEYBOARD_HEIGHT*zoom)};
    SDL_RenderCopy(sdlRenderer, keyboardTexture, NULL, &textRect);

    SDL_Rect pcbRect = {0, (int)((displayTop-pcbHeight) * zoom)-4, (int)(screenWidth*zoom), (int)(displayTop * zoom)-4 };
    SDL_RenderCopy(sdlRenderer, pcbTexture, NULL, &pcbRect);

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

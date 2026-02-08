#include "pagemap.hpp"
#include "assets.hpp"
#include "SDL2_gfxPrimitives.h"
#include <cstdarg>
#include <cstdio>
#include <iostream>

PageMap::PageMap() {}

PageMap::~PageMap() {
    close();
}

void PageMap::toggle() {
    if( window ) {
        close();
    }
    else {
        window = SDL_CreateWindow("Page Map",
            savedX, savedY,
            WIDTH, HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);
        if( !window ) {
            std::cerr << "Could not create page map window: " << SDL_GetError() << std::endl;
            return;
        }
        winId = SDL_GetWindowID(window);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if( !renderer ) {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        }
        if( !renderer ) {
            SDL_DestroyWindow(window);
            window = nullptr;
            return;
        }
        std::string fontPath = assetPath("Roboto-Medium.ttf");
        font = TTF_OpenFont(fontPath.c_str(), 13);
        smallFont = TTF_OpenFont(fontPath.c_str(), 11);
        std::string monoPath = assetPath("RobotoMono-VariableFont_wght.ttf");
        monoFont = TTF_OpenFont(monoPath.c_str(), 14);
        fontH = textHeight(font);
        smallFontH = textHeight(smallFont);
    }
}

void PageMap::close() {
    if( window ) {
        SDL_GetWindowPosition(window, &savedX, &savedY);
    }
    if( monoFont ) { TTF_CloseFont(monoFont); monoFont = nullptr; }
    if( smallFont ) { TTF_CloseFont(smallFont); smallFont = nullptr; }
    if( font ) { TTF_CloseFont(font); font = nullptr; }
    if( renderer ) { SDL_DestroyRenderer(renderer); renderer = nullptr; }
    if( window ) { SDL_DestroyWindow(window); window = nullptr; }
    winId = 0;
}

bool PageMap::isOpen() const {
    return window != nullptr;
}

uint32_t PageMap::windowId() const {
    return winId;
}

int PageMap::textHeight(TTF_Font *f) {
    if( !f ) return 0;
    int w, h;
    TTF_SizeUTF8(f, "0", &w, &h);
    return h;
}

void PageMap::print(TTF_Font *f, int x, int y, SDL_Color color, const char* fmt, ...) {
    char buffer[200];
    va_list args;
    va_start(args, fmt);
    int c = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if( c <= 0 || c >= (int)sizeof(buffer) || !f ) return;

    SDL_Surface *surface = TTF_RenderText_Blended(f, buffer, color);
    if( !surface ) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if( texture ) {
        SDL_Rect rect = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, NULL, &rect);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void PageMap::printRotated(TTF_Font *f, int cx, int cy, double angle, SDL_Color color, const char* fmt, ...) {
    char buffer[200];
    va_list args;
    va_start(args, fmt);
    int c = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if( c <= 0 || c >= (int)sizeof(buffer) || !f ) return;

    SDL_Surface *surface = TTF_RenderText_Blended(f, buffer, color);
    if( !surface ) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if( texture ) {
        // Position so the centre of the rotated text is at (cx, cy)
        SDL_Rect dst = {cx - surface->w / 2, cy - surface->h / 2, surface->w, surface->h};
        SDL_RenderCopyEx(renderer, texture, NULL, &dst, angle, NULL, SDL_FLIP_NONE);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void PageMap::draw(const uint8_t memoryPage[4], bool pagingEnabled) {
    if( !window ) return;

    // Clear window background
    SDL_SetRenderDrawColor(renderer, 0xF0, 0xF0, 0xE0, 0xFF);
    SDL_RenderClear(renderer);

    SDL_Color menuColor = {0x30, 0x30, 0xA0, 255};
    SDL_Color addrColor = {0x60, 0x60, 0x60, 255};  // Lighter for addresses
    SDL_Color darkText = {0x20, 0x20, 0x20, 255};
    SDL_Color lightText = {0xFF, 0xFF, 0xFF, 255};
    SDL_Color borderColor = {0x40, 0x40, 0x40, 255};

    // Page purpose colours (from microbeast_memory_map.svg)
    SDL_Color cpmColor     = {73, 147, 243, 255};
    SDL_Color ramDiskColor = {77, 178, 187, 255};
    SDL_Color freeColor    = {188, 236, 241, 255};
    SDL_Color restoreColor = {246, 107, 113, 255};
    SDL_Color romDiskColor = {177, 77, 180, 255};
    SDL_Color bootColor    = {249, 208, 251, 255};

    // Title
    print(font, 20, 8, menuColor, "PAGE MAP");

    // Layout constants - each box is exactly boxHeight pixels tall,
    // occupying rows [y, y+boxHeight-1]. Next box starts at y+boxHeight.
    int boxWidth = 90;
    int boxHeight = 21;
    int topY = 40;
    int colHeight = 32 * boxHeight;  // Total column pixel height

    // Measured font heights for precise vertical centering
    int fh = fontH;    // Page label font height
    int sfh = smallFontH;  // Address font height
    int sfw = 0;  // Address font character width
    { int dummy; TTF_SizeUTF8(smallFont, "0", &sfw, &dummy); }

    // Layout: RAM brackets | RAM addresses | RAM pages ... centre addr | centre | ... ROM addresses | ROM pages | ROM brackets
    // All X positions scaled by 1.3 from original 560px layout to fit 728px window
    int ramBracketX = 88;
    int ramAddrX = 131 - sfw;
    int ramColX = 166;

    int romAddrX = 495 - sfw;
    int romColX = 522;
    int romBracketX = 619;

    // Helper lambda to get page colour
    auto getPageColor = [&](uint8_t pageNum) -> SDL_Color {
        if( pageNum >= 0x35 ) return freeColor;
        if( pageNum >= 0x25 ) return ramDiskColor;
        if( pageNum == 0x24 ) return freeColor;
        if( pageNum >= 0x20 ) return cpmColor;
        if( pageNum >= 0x14 ) return restoreColor;
        if( pageNum >= 0x10 ) return restoreColor;
        if( pageNum >= 0x04 ) return romDiskColor;
        return bootColor;
    };

    auto needsLightText = [&](uint8_t pageNum) -> bool {
        if( pageNum >= 0x38 ) return false;
        if( pageNum >= 0x25 ) return false;
        if( pageNum == 0x24 ) return false;
        if( pageNum >= 0x20 ) return true;
        if( pageNum >= 0x10 ) return true;
        if( pageNum >= 0x04 ) return true;
        return false;
    };

    // --- Draw RAM page fills ---
    print(font, ramColX + 10, topY - 34, menuColor, "PHYSICAL");
    print(font, ramColX + 25, topY - 18, menuColor, "RAM");
    for( int i = 0; i < 32; i++ ) {
        uint8_t pageNum = 0x3F - i;
        int y = topY + i * boxHeight;
        SDL_Color fillColor = getPageColor(pageNum);
        SDL_SetRenderDrawColor(renderer, fillColor.r, fillColor.g, fillColor.b, fillColor.a);
        SDL_Rect r = {ramColX, y, boxWidth, boxHeight};
        SDL_RenderFillRect(renderer, &r);
    }
    // Outer border around entire RAM column
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, 255);
    SDL_Rect ramBorder = {ramColX, topY, boxWidth, colHeight};
    SDL_RenderDrawRect(renderer, &ramBorder);
    // Horizontal dividers between RAM pages
    for( int i = 1; i < 32; i++ ) {
        int y = topY + i * boxHeight;
        SDL_RenderDrawLine(renderer, ramColX, y, ramColX + boxWidth - 1, y);
    }

    // RAM page labels and address labels
    for( int i = 0; i < 32; i++ ) {
        uint8_t pageNum = 0x3F - i;
        int y = topY + i * boxHeight;

        // Page number centred vertically inside the box
        SDL_Color txtColor = needsLightText(pageNum) ? lightText : darkText;
        print(font, ramColX + 30, y + (boxHeight - fh) / 2, txtColor, "#%02X", pageNum);

        // Address label: vertical centre of text == bottom edge of page box
        uint32_t physBase = ((uint32_t)(pageNum & 0x1F) << 14) | 0x80000;
        int bottomEdge = y + boxHeight;
        print(smallFont, ramAddrX, bottomEdge - sfh / 2, addrColor, "%05X", physBase);
    }
    // Top-edge address for topmost RAM page (#3F)
    print(smallFont, ramAddrX, topY - sfh / 2, addrColor, "%05X", (uint32_t)0xFFFFF);

    // --- Draw ROM page fills ---
    print(font, romColX + 10, topY - 34, menuColor, "PHYSICAL");
    print(font, romColX + 25, topY - 18, menuColor, "ROM");
    for( int i = 0; i < 32; i++ ) {
        uint8_t pageNum = 0x1F - i;
        int y = topY + i * boxHeight;
        int yb = y + boxHeight - 1;

        if( pageNum >= 0x10 && pageNum <= 0x13 ) {
            // Shared pages: upper-left triangle = RESTORE red, lower-right = ROM DISK purple
            int x1 = romColX, x2 = romColX + boxWidth - 1;
            filledTrigonRGBA(renderer, x1, y, x2, y, x1, yb,
                             restoreColor.r, restoreColor.g, restoreColor.b, 255);
            filledTrigonRGBA(renderer, x2, y, x1, yb, x2, yb,
                             romDiskColor.r, romDiskColor.g, romDiskColor.b, 255);
        } else {
            SDL_Color fillColor = getPageColor(pageNum);
            SDL_SetRenderDrawColor(renderer, fillColor.r, fillColor.g, fillColor.b, fillColor.a);
            SDL_Rect r = {romColX, y, boxWidth, boxHeight};
            SDL_RenderFillRect(renderer, &r);
        }
    }
    // Outer border around entire ROM column
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, 255);
    SDL_Rect romBorder = {romColX, topY, boxWidth, colHeight};
    SDL_RenderDrawRect(renderer, &romBorder);
    // Horizontal dividers between ROM pages
    for( int i = 1; i < 32; i++ ) {
        int y = topY + i * boxHeight;
        SDL_RenderDrawLine(renderer, romColX, y, romColX + boxWidth - 1, y);
    }

    // ROM page labels and address labels
    for( int i = 0; i < 32; i++ ) {
        uint8_t pageNum = 0x1F - i;
        int y = topY + i * boxHeight;

        // Page number centred vertically inside the box
        SDL_Color txtColor = needsLightText(pageNum) ? lightText : darkText;
        print(font, romColX + 30, y + (boxHeight - fh) / 2, txtColor, "#%02X", pageNum);

        // Address label: vertical centre of text == bottom edge of page box
        uint32_t physBase = (uint32_t)(pageNum & 0x1F) << 14;
        int bottomEdge = y + boxHeight;
        print(smallFont, romAddrX, bottomEdge - sfh / 2, addrColor, "%05X", physBase);
    }
    // Top-edge address for topmost ROM page (#1F)
    print(smallFont, romAddrX, topY - sfh / 2, addrColor, "%05X", (uint32_t)0x7FFFF);

    // --- Purpose brackets / labels ---
    auto drawBracketLeft = [&](int startIdx, int endIdx, int bx, const char* label, SDL_Color bracketColor) {
        int y1 = topY + startIdx * boxHeight;
        int y2 = topY + (endIdx + 1) * boxHeight - 1;
        int midY = (y1 + y2) / 2;
        lineRGBA(renderer, bx, y1, bx, y2, bracketColor.r, bracketColor.g, bracketColor.b, 255);
        lineRGBA(renderer, bx, y1, bx + 4, y1, bracketColor.r, bracketColor.g, bracketColor.b, 255);
        lineRGBA(renderer, bx, y2, bx + 4, y2, bracketColor.r, bracketColor.g, bracketColor.b, 255);
        // Label reads bottom-to-top (-90), just left of bracket
        int labelX = bx - fh / 2 - 4;
        printRotated(font, labelX, midY, -90.0, bracketColor, "%s", label);
    };

    auto drawBracketRight = [&](int startIdx, int endIdx, int bx, const char* label, SDL_Color bracketColor) {
        int y1 = topY + startIdx * boxHeight;
        int y2 = topY + (endIdx + 1) * boxHeight - 1;
        int midY = (y1 + y2) / 2;
        lineRGBA(renderer, bx, y1, bx, y2, bracketColor.r, bracketColor.g, bracketColor.b, 255);
        lineRGBA(renderer, bx, y1, bx - 4, y1, bracketColor.r, bracketColor.g, bracketColor.b, 255);
        lineRGBA(renderer, bx, y2, bx - 4, y2, bracketColor.r, bracketColor.g, bracketColor.b, 255);
        // Label reads top-to-bottom (+90), just right of bracket
        int labelX = bx + fh / 2 + 4;
        printRotated(font, labelX, midY, 90.0, bracketColor, "%s", label);
    };

    // RAM brackets (left side)
    drawBracketLeft(28, 31, ramBracketX, "CP/M", menuColor);
    drawBracketLeft(11, 26, ramBracketX, "RAM DISK", menuColor);

    // ROM brackets (right side) - offset +2 so RESTORE/BOOT don't touch the page boxes
    drawBracketRight(28, 31, romBracketX, "BOOT", menuColor);
    drawBracketRight(0, 15, romBracketX, "RESTORE", restoreColor);
    drawBracketRight(12, 27, romBracketX + 18, "ROM DISK", romDiskColor);

    // --- Centre column: 4 logical CPU banks (same box height as physical pages) ---
    // Gaps: RAM right (256) --88px-- centre left (344) --88px-- ROM left (522)
    int centreBoxWidth = boxWidth;
    int centreColX = 344;
    int centreAddrX = centreColX - 27;  // Address labels left of centre boxes

    // 4 banks at boxHeight each, vertically centred in the column
    int centreTopY = topY + (colHeight - 4 * boxHeight) / 2;

    // VideoBeast colour for pages in 0x40-0x5F range
    SDL_Color videoBeastColor = {0x80, 0x80, 0x80, 255};

    // Bank ordering: high addresses at top to match physical columns
    const int bankOrder[4] = {3, 2, 1, 0};

    // Column title - just above the logical pages
    print(font, centreColX + 13, centreTopY - 34, menuColor, "LOGICAL");
    print(font, centreColX + 25, centreTopY - 18, menuColor, "CPU");

    // Track arrow info for drawing after the boxes
    struct ArrowInfo {
        uint8_t page;
        bool isRam;
        bool isVideoBeast;
        int targetRowIdx;
    } arrows[4];

    for( int row = 0; row < 4; row++ ) {
        int bank = bankOrder[row];
        uint8_t page = pagingEnabled ? memoryPage[bank] : (uint8_t)bank;
        int y = centreTopY + row * boxHeight;

        bool isRam = (page & 0xE0) == 0x20;
        bool isRom = (page & 0xE0) == 0x00;
        bool isVideoBeast = (page & 0xE0) == 0x40;
        bool isSharedRom = isRom && (page & 0x1F) >= 0x10 && (page & 0x1F) <= 0x13;

        // Draw fill
        if( isSharedRom ) {
            int x1 = centreColX, x2 = centreColX + centreBoxWidth - 1;
            int yb = y + boxHeight - 1;
            filledTrigonRGBA(renderer, x1, y, x2, y, x1, yb,
                             restoreColor.r, restoreColor.g, restoreColor.b, 255);
            filledTrigonRGBA(renderer, x2, y, x1, yb, x2, yb,
                             romDiskColor.r, romDiskColor.g, romDiskColor.b, 255);
        } else if( isVideoBeast ) {
            SDL_SetRenderDrawColor(renderer, videoBeastColor.r, videoBeastColor.g, videoBeastColor.b, 255);
            SDL_Rect r = {centreColX, y, centreBoxWidth, boxHeight};
            SDL_RenderFillRect(renderer, &r);
        } else {
            uint8_t colorPage = isRam ? page : (page & 0x1F);
            SDL_Color fillColor = getPageColor(colorPage);
            SDL_SetRenderDrawColor(renderer, fillColor.r, fillColor.g, fillColor.b, fillColor.a);
            SDL_Rect r = {centreColX, y, centreBoxWidth, boxHeight};
            SDL_RenderFillRect(renderer, &r);
        }

        // Text colour
        bool useLightText;
        if( isSharedRom ) useLightText = true;
        else if( isVideoBeast ) useLightText = false;
        else useLightText = needsLightText(isRam ? page : (page & 0x1F));
        SDL_Color txtColor = useLightText ? lightText : darkText;

        // Label: "n: #PP" where n = logical bank number (0-3), PP = physical page
        char label[16];
        snprintf(label, sizeof(label), "%d: #%02X", bank, page);
        int tw, th;
        TTF_SizeUTF8(font, label, &tw, &th);
        print(font, centreColX + (centreBoxWidth - tw) / 2, y + (boxHeight - fh) / 2, txtColor, "%s", label);

        // Address label at bottom edge of this bank (same style as physical pages)
        uint16_t baseAddr = bank * 0x4000;
        int bottomEdge = y + boxHeight;
        print(smallFont, centreAddrX, bottomEdge - sfh / 2, addrColor, "%04X", baseAddr);

        // Store arrow info
        arrows[row].page = page;
        arrows[row].isRam = isRam;
        arrows[row].isVideoBeast = isVideoBeast;
        if( isRam )
            arrows[row].targetRowIdx = 0x3F - page;
        else if( !isVideoBeast )
            arrows[row].targetRowIdx = 0x1F - (page & 0x1F);
        else
            arrows[row].targetRowIdx = 0;
    }

    // Outer border around entire centre column
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, 255);
    SDL_Rect centreBorder = {centreColX, centreTopY, centreBoxWidth, 4 * boxHeight};
    SDL_RenderDrawRect(renderer, &centreBorder);
    // Horizontal dividers between banks
    for( int i = 1; i < 4; i++ ) {
        int y = centreTopY + i * boxHeight;
        SDL_RenderDrawLine(renderer, centreColX, y, centreColX + centreBoxWidth - 1, y);
    }

    // Top-edge address for topmost bank (ceiling of address space)
    print(smallFont, centreAddrX, centreTopY - sfh / 2, addrColor, "%04X", 0xFFFF);

    // Paging status indicator centred below the logical page boxes
    {
        char pagingBuf[16];
        snprintf(pagingBuf, sizeof(pagingBuf), "Paging: %s", pagingEnabled ? "ON" : "OFF");
        int tw, th;
        TTF_SizeUTF8(smallFont, pagingBuf, &tw, &th);
        print(smallFont, centreColX + (centreBoxWidth - tw) / 2, centreTopY + 4 * boxHeight + 6, addrColor, "%s", pagingBuf);
    }

    // --- Draw mapping arrows ---
    int ramLaneCount = 0, romLaneCount = 0;
    int ramLane[4] = {0}, romLane[4] = {0};

    for( int row = 0; row < 4; row++ ) {
        if( arrows[row].isVideoBeast ) continue;
        if( arrows[row].isRam )
            ramLane[row] = ramLaneCount++;
        else
            romLane[row] = romLaneCount++;
    }

    // Arrow routing gaps - constrained to avoid overlapping address labels
    // RAM side: between RAM box right edge and centre address column left edge
    int ramGapLeft = ramColX + boxWidth;
    int ramGapRight = centreAddrX - 2;
    // ROM side: between centre box right edge and ROM address column left edge
    int romGapLeft = centreColX + centreBoxWidth;
    int romGapRight = romAddrX - 2;

    for( int row = 0; row < 4; row++ ) {
        if( arrows[row].isVideoBeast ) continue;

        uint8_t page = arrows[row].page;
        bool isRam = arrows[row].isRam;
        int targetRow = arrows[row].targetRowIdx;

        // Arrow colour: use dark grey for BOOT pages (#00-#03) which are too light otherwise
        uint8_t colorPage = isRam ? page : (page & 0x1F);
        SDL_Color arrowColor;
        if( !isRam && (page & 0x1F) <= 0x03 )
            arrowColor = {0x88, 0x88, 0x88, 255};  // Grey for BOOT
        else
            arrowColor = getPageColor(colorPage);

        // Start at horizontal edge of centre bank box, vertical centre of bank
        int bankY = centreTopY + row * boxHeight + boxHeight / 2;
        int startX = isRam ? centreColX : (centreColX + centreBoxWidth);

        // End at horizontal edge of physical page box, vertical centre of target page
        int targetY = topY + targetRow * boxHeight + boxHeight / 2;
        int endX = isRam ? (ramColX + boxWidth) : romColX;

        // Lane position for vertical routing
        int laneX;
        if( isRam ) {
            int totalLanes = ramLaneCount > 0 ? ramLaneCount : 1;
            int laneSpacing = (ramGapRight - ramGapLeft) / (totalLanes + 1);
            laneX = ramGapRight - (ramLane[row] + 1) * laneSpacing;
        } else {
            int totalLanes = romLaneCount > 0 ? romLaneCount : 1;
            int laneSpacing = (romGapRight - romGapLeft) / (totalLanes + 1);
            laneX = romGapLeft + (romLane[row] + 1) * laneSpacing;
        }

        uint8_t ar = arrowColor.r, ag = arrowColor.g, ab = arrowColor.b;

        // 3-segment routed arrow (drawn 3px thick via +/-1 offset)
        for( int d = -1; d <= 1; d++ ) {
            lineRGBA(renderer, startX, bankY + d, laneX, bankY + d, ar, ag, ab, 255);
            lineRGBA(renderer, laneX + d, bankY, laneX + d, targetY, ar, ag, ab, 255);
            lineRGBA(renderer, laneX, targetY + d, endX, targetY + d, ar, ag, ab, 255);
        }

        // Arrowhead at the physical page end
        int sz = 5;
        if( isRam ) {
            filledTrigonRGBA(renderer, endX, targetY, endX + sz, targetY - sz, endX + sz, targetY + sz,
                             ar, ag, ab, 255);
        } else {
            filledTrigonRGBA(renderer, endX, targetY, endX - sz, targetY - sz, endX - sz, targetY + sz,
                             ar, ag, ab, 255);
        }
    }

    // Legend centred at the bottom - same font/size/colour as main debug screen hints
    {
        const char* legend = "[ESC]: Close";
        int tw, th;
        TTF_SizeUTF8(monoFont, legend, &tw, &th);
        print(monoFont, (WIDTH - tw) / 2, HEIGHT - 24, menuColor, "%s", legend);
    }

    SDL_RenderPresent(renderer);
}

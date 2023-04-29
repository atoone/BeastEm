#include "digit.hpp"
#include <iostream>

Digit::Digit(SDL_Renderer *renderer, float zoom) {
    this->zoom = zoom;
    digitTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, DIGIT_WIDTH*zoom, DIGIT_HEIGHT*zoom);

    createSegments();

    segmentFlags = 0x0FFFF;
    for( int i=0; i<SEGMENTS; i++ ) {
        brightness[i] = 255;
    }
}

void Digit::createSegments() {

    short segWidth = round(DIGIT_WIDTH*zoom/8.0);
    short segLen   = round(DIGIT_WIDTH*zoom-segWidth*1.5);

    short bevel    = round(segWidth/4.0);
    short b2 = bevel*2;
    short b3 = bevel*3;

    // A: Top
    addTo( DISPLAY_SEGMENTS_X[0], 0, {b2, b3, segLen-b3, segLen-b2, segLen-segWidth, segWidth});
    addTo( DISPLAY_SEGMENTS_Y[0], segWidth, {bevel, 0, 0, bevel, b3, b3} );

    // B: Top-right
    addTo( DISPLAY_SEGMENTS_X[1], segLen, {-bevel, -b3, -b3, -bevel, 0, 0});
    addTo( DISPLAY_SEGMENTS_Y[1], segWidth+bevel, {b2, segWidth, segLen-segWidth, segLen-b2, segLen-b3, b3});

    // C: Bottom-right
    addTo( DISPLAY_SEGMENTS_X[2], segLen, {-bevel, -b3, -b3, -bevel, 0, 0});
    addTo( DISPLAY_SEGMENTS_Y[2], segWidth+segLen+bevel, {b2, segWidth, segLen-segWidth, segLen-b2, segLen-b3, b3});

    // D: Bottom
    addTo( DISPLAY_SEGMENTS_X[3], 0, {b2, b3, segLen-b3, segLen-b2, segLen-segWidth, segWidth});
    addTo( DISPLAY_SEGMENTS_Y[3], 2*segLen+segWidth+b2, {-bevel, 0, 0, -bevel, -b3, -b3} );

    // E: Bottom-left
    addTo( DISPLAY_SEGMENTS_X[4], 0, {bevel, b3, b3, bevel, 0, 0});
    addTo( DISPLAY_SEGMENTS_Y[4], segWidth+segLen+bevel, {b2, segWidth, segLen-segWidth, segLen-b2, segLen-b3, b3});

    // F: Top-left
    addTo( DISPLAY_SEGMENTS_X[5], 0, {bevel, b3, b3, bevel, 0, 0});
    addTo( DISPLAY_SEGMENTS_Y[5], segWidth+bevel, {b2, segWidth, segLen-segWidth, segLen-b2, segLen-b3, b3});

    // G1: Centre left
    addTo( DISPLAY_SEGMENTS_X[6], 0, {b2, segWidth, segLen/2-b3, segLen/2-bevel, segLen/2-b3, segWidth});
    addTo( DISPLAY_SEGMENTS_Y[6], segWidth+segLen+bevel, {0, -b2, -b2, 0, b2, b2});

    // G2: Centre right
    addTo( DISPLAY_SEGMENTS_X[7], segLen/2-bevel, {b2, segWidth, segLen/2-b3, segLen/2-bevel, segLen/2-b3, segWidth});
    addTo( DISPLAY_SEGMENTS_Y[7], segWidth+segLen+bevel, {0, -b2, -b2, 0, b2, b2});

    // H: Diag top left
    addTo( DISPLAY_SEGMENTS_X[8], segWidth, {bevel, b2, segLen/2-segWidth*2, segLen/2-segWidth*2, segLen/2-segWidth*2-bevel, bevel});
    addTo( DISPLAY_SEGMENTS_Y[8], segWidth*2, {bevel, bevel, segLen-segWidth*3, segLen-segWidth-b3, segLen-segWidth-b3, segWidth+b2});

    // J: Center top
    addTo( DISPLAY_SEGMENTS_X[9], segLen/2, {-b2, b2, b2, 0, -b2});
    addTo( DISPLAY_SEGMENTS_Y[9], segWidth+b2, {b3, b3, segLen-segWidth-bevel, segLen-b3, segLen-segWidth-bevel});

    // K: Diag top right
    addTo( DISPLAY_SEGMENTS_X[10], segLen-segWidth, {-bevel, -b2, -segLen/2+segWidth*2, -segLen/2+segWidth*2, -segLen/2+segWidth*2+bevel, -bevel});
    addTo( DISPLAY_SEGMENTS_Y[10], segWidth*2, {bevel, bevel, segLen-segWidth*3, segLen-segWidth-b3, segLen-segWidth-b3, segWidth+b2});

    // L: Diag bottom right
    addTo( DISPLAY_SEGMENTS_X[11], segLen-segWidth, {-bevel, -b2, -segLen/2+segWidth*2, -segLen/2+segWidth*2, -segLen/2+segWidth*2+bevel, -bevel});
    addTo( DISPLAY_SEGMENTS_Y[11], 2*segLen+b2, {-bevel, -bevel, -segLen+segWidth*3, -segLen+segWidth+b3, -segLen+segWidth+b3, -segWidth-b2});

    // M: Center bottom
    addTo( DISPLAY_SEGMENTS_X[12], segLen/2, {-b2, 0, b2, b2, -b2,});
    addTo( DISPLAY_SEGMENTS_Y[12], segLen+segWidth, {segWidth+bevel, b3, segWidth+bevel, segLen-b3, segLen-b3});

    // N: Diag bottom left
    addTo( DISPLAY_SEGMENTS_X[13], segWidth, {bevel, b2, segLen/2-segWidth*2, segLen/2-segWidth*2, segLen/2-segWidth*2-bevel, bevel});
    addTo( DISPLAY_SEGMENTS_Y[13], 2*segLen+b2, {-bevel, -bevel, -segLen+segWidth*3, -segLen+segWidth+b3, -segLen+segWidth+b3, -segWidth-b2});

    // DP: 
    addTo( DISPLAY_SEGMENTS_X[14], segLen, {0, bevel, b2, b3, b3, b2, bevel, 0});
    addTo( DISPLAY_SEGMENTS_Y[14], 2*segLen+segWidth+b2, {bevel, 0, 0, bevel, b2, b3, b3, b2});
}

void Digit::addTo(std::vector<short>&v, int offset, std::initializer_list<int> list) {
    for( int val: list) {
        v.push_back((short)(offset + val));
    }
}

void Digit::refresh(SDL_Renderer *renderer) {
    SDL_SetRenderTarget(renderer, digitTexture);
    SDL_SetRenderDrawColor(renderer, 0x0, 0x0, 0x40, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    for(int i=0; i<15; i++) {
        if( (segmentFlags >> i) & 0x01 == 0x01) {
            filledPolygonRGBA(renderer, &DISPLAY_SEGMENTS_X[i][0], &DISPLAY_SEGMENTS_Y[i][0], DISPLAY_SEGMENTS_X[i].size(), 0xF0, 0xF0, 0xFF, brightness[i]);
        }
    }
    SDL_SetRenderTarget(renderer, NULL);
    changed = false;
}



void Digit::onDraw(SDL_Renderer *renderer, int x, int y) {
    if( changed ) {
        refresh(renderer);
    }
    SDL_Rect digitRect;

    digitRect.x = x*zoom;
    digitRect.y = y*zoom;
    digitRect.w = DIGIT_WIDTH*zoom;
    digitRect.h = DIGIT_HEIGHT*zoom;

    SDL_RenderCopy(renderer, digitTexture, NULL, &digitRect);
}

void Digit::setSegments(uint16_t segmentMask) {
    segmentFlags = segmentMask;
    changed = true;
}

void Digit::setBrightness(int segment, uint8_t brightness) {
    if( segment < SEGMENTS ) {
        this->brightness[segment] = brightness;
        changed = true;
    }
}

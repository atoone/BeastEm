#pragma once
#include <set>
#include <vector>
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL2_gfxPrimitives.h>

class Digit {
    private:
        SDL_Texture   *digitTexture;
        float zoom;
        
        const static int SEGMENTS = 15;

        short segmentFlags;
        short brightness[SEGMENTS];

        

        std::vector<short> DISPLAY_SEGMENTS_X[SEGMENTS];
        std::vector<short> DISPLAY_SEGMENTS_Y[SEGMENTS];

        void createSegments();
        void addTo(std::vector<short>&v, int offset, std::initializer_list<int> list);
        
        void refresh(SDL_Renderer *renderer);

        
    public: 
        const static int DIGIT_WIDTH = 32;
        const static int DIGIT_HEIGHT = 64;

        Digit(SDL_Renderer *sdlRenderer, float zoom);

        void onDraw(SDL_Renderer *renderer, int x, int y);

        void setSegments( uint16_t segmentMask );
        void setBrightness( int segment, uint8_t brightness);

        bool changed = true;
};
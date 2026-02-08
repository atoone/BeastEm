#pragma once
#include "SDL.h"
#include "SDL_ttf.h"

class PageMap {
public:
    PageMap();
    ~PageMap();

    void toggle();
    void close();
    bool isOpen() const;
    uint32_t windowId() const;
    void draw(const uint8_t memoryPage[4], bool pagingEnabled);

private:
    SDL_Window   *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    uint32_t      winId = 0;
    TTF_Font     *font = nullptr, *smallFont = nullptr, *monoFont = nullptr;
    int           fontH = 0, smallFontH = 0;
    int           savedX = SDL_WINDOWPOS_UNDEFINED, savedY = SDL_WINDOWPOS_UNDEFINED;
    static const int WIDTH = 728, HEIGHT = 740;

    void print(TTF_Font *f, int x, int y, SDL_Color color, const char* fmt, ...);
    void printRotated(TTF_Font *f, int cx, int cy, double angle, SDL_Color color, const char* fmt, ...);
    int  textHeight(TTF_Font *f);
};

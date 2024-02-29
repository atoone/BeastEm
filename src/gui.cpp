#include "gui.hpp"
#include <iostream>
#include <cstring>
#include <stdio.h>

void GUI::setZoom(float zoom) {
    this->zoom = zoom;
    monoFont = TTF_OpenFont(MONO_FONT, MONO_SIZE*zoom);
    
    if( !monoFont) {
        std::cout << "Couldn't load font "<< MONO_FONT << std::endl;
        exit(1);
    }
}

void GUI::startEdit(uint32_t value, int x, int y, int offset, int digits) {
    editValue = value;
    editOldValue = value;

    editX = x;
    editY = y;
    editOffset = offset;
    editDigits = digits;  
    editIndex = digits-1;
}

void GUI::displayEdit() {
    char buffer[2] = {'0', 0};
    int width;
    int height;

    SDL_Color background = {0xFF, 0xFF, 0xFF};

    TTF_SizeUTF8(monoFont, buffer, &width, &height);
    boxRGBA(sdlRenderer, editX*zoom+(width*(editOffset-1))-1, (editY+1)*zoom, editX*zoom+(width*(editOffset+editDigits-1)), (editY-2)*zoom+height, background.r, background.g, background.b, 0xFF);

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

uint32_t GUI::getEditValue() {
    return editValue;
}

int GUI::getDigits() {
    return editDigits;
}

bool GUI::editDigit(uint8_t digit) {
    editValue = (editValue & ~(0x000F << (editIndex*4))) | (digit << (editIndex*4));
    return --editIndex < 0;
}

void GUI::editDelta(int delta) {
    editValue = (editValue+delta) & (0x0FFFFF >> ((5-editDigits)*4));
}

void GUI::editBackspace() {
    if( editIndex < editDigits ) {
        editIndex++;
        editValue = (editValue & ~(0x000F << (editIndex*4))) | (editOldValue & (0x000F << (editIndex*4)));
    }                  
}

std::pair<int, int> GUI::prompt(char* buffer) {
    char padding[2] = {'0', 0};
    SDL_Color background = {0xF0, 0xF0, 0xFF};
    int width;
    int height;
    int charWidth;
    
    TTF_SizeUTF8(monoFont, padding, &charWidth, &height);
    TTF_SizeUTF8(monoFont, buffer, &width, &height);

    int promptX = (screenWidth*zoom-width)/2;
    int promptY = (screenHeight*zoom+height)/2;
    boxRGBA(sdlRenderer, promptX-2*charWidth, promptY-3*height, promptX+width+2*charWidth, promptY+2*height, background.r, background.g, background.b, 0xFF);

    SDL_Rect textRect;

    SDL_Color color = {0x00, 0x00, 0x00};

    SDL_Surface *textSurface = TTF_RenderText_Blended(monoFont, buffer, color);
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(sdlRenderer, textSurface);

    textRect.x = promptX; 
    textRect.y = (promptY-height);
    textRect.w = textSurface->w;
    textRect.h = textSurface->h;

    SDL_RenderCopy(sdlRenderer, textTexture, NULL, &textRect);

    SDL_DestroyTexture(textTexture);
    SDL_FreeSurface(textSurface);

    return std::make_pair(promptX, promptY);
}


void GUI::printb(int x, int y, SDL_Color color, int highlight, SDL_Color background, char* buffer) {
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
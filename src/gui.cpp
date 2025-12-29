#include "gui.hpp"
#include <iostream>
#include <cstring>
#include <stdio.h>

void GUI::init(float zoom) {
    this->zoom = zoom;
    monoFont = TTF_OpenFont(MONO_FONT, MONO_SIZE*zoom);

    if( !monoFont) {
        std::cout << "Couldn't load font "<< MONO_FONT << std::endl;
        exit(1);
    }

    char padding[2] = {'0', 0};
    TTF_SizeUTF8(monoFont, padding, &charWidth, &charHeight);
}

void GUI::startEdit(uint32_t value, int x, int y, int offset, int digits, bool isContinue, EditType editType) {
    editValue = value;
    editOldValue = value;

    editX = x;
    editY = y;
    editOffset = offset;
    editDigits = digits;  
    editIndex = digits-1;
    this->editType = editType;

    editContinue = isContinue;
    editOK = false;
}

bool GUI::isEditing() {
    return editIndex >= 0;
}

bool GUI::isContinuousEdit() {
    return editContinue;
}

bool GUI::isEditOK() {
    return editOK;
}

void GUI::endEdit(bool isOK) {
    editOK = isOK;
    editContinue = false;
    editIndex = -1;
}

void GUI::drawEdit() {
    if( editIndex < 0 ) return;

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
        if( editType == ET_HEX ) {
            snprintf(buffer, sizeof(buffer), "%01X", (editValue >> (i*4)) & 0x0F);
        }
        else if( editType == ET_BASE_10 ) {
            int divisor = i > 0 ? i*10: 1;
            snprintf(buffer, sizeof(buffer), "%01d", (editValue / divisor) % 10);
        }
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

bool GUI::handleKey(SDL_Keycode key) {
    if( editIndex >= 0 ) {
        switch(key) {
            case SDLK_ESCAPE: endEdit(false); break;
            case SDLK_RETURN: endEdit(true);  break;
            case SDLK_BACKSPACE: editBackspace();  break;
            case SDLK_0 ... SDLK_9: editDigit( key - SDLK_0 ); break;
            case SDLK_a ... SDLK_f: editDigit( key - SDLK_a + 10 ); break;
            default:
                return false;
        }
    }
    if( promptStarted ) {
        if( promptType == PT_CONFIRM ) {
            if( key == SDLK_RETURN || key == SDLK_y ) {
                promptOK = true;
                promptCompleted = true;
            }
            if( key == SDLK_ESCAPE || key == SDLK_n ) {
                promptOK = false;
                promptCompleted = true;
            }
        }
        if( promptType == PT_VALUE ) {
            if( editIndex < 0 ) {
                promptOK = editOK;
                promptCompleted = !editContinue;
                if( !promptCompleted && key == SDLK_BACKSPACE ) {
                    editIndex = editDigits-1;
                }
            } 
            if( !promptCompleted ) {
                if( key == SDLK_RETURN ) {
                    promptOK = true;
                    promptCompleted = true;
                    editContinue = false;
                }
            }
        }
        if( promptType == PT_CHOICE ) {
            switch(key) {
                case SDLK_ESCAPE : promptOK = false; promptCompleted = true; break;
                case SDLK_RETURN : promptOK = true; promptCompleted = true; break;
                case SDLK_UP     :
                case SDLK_LEFT   : if( editValue > 0 ) editValue--; break;
                case SDLK_DOWN   :
                case SDLK_RIGHT  : if( editValue < promptChoices.size()-1 ) editValue++; break;
            }
        }
    }
    return true;
}

uint32_t GUI::getEditValue() {
    return editValue;
}

void GUI::editDigit(uint8_t digit) {
    if( editIndex >= 0 ) {
        if( editType == ET_HEX ) {
            editValue = (editValue & ~(0x000F << (editIndex*4))) | (digit << (editIndex*4));
        }
        else if( editType == ET_BASE_10 && digit < 10 ) {
            int divisor = editIndex > 0 ? 10 * editIndex: 1;
            editValue = editValue - (((editValue / divisor) % 10) * divisor) + (digit * divisor);
        }
        editOK = --editIndex < 0;
    }
}

void GUI::editDelta(int delta) {
    editValue = (editValue+delta) & (0x0FFFFFF >> ((6-editDigits)*4));
}

void GUI::editBackspace() {    if( editIndex < (editDigits-1) ) {
        editIndex++;
        editValue = (editValue & ~(0x000F << (editIndex*4))) | (editOldValue & (0x000F << (editIndex*4)));
    }                  
}

bool GUI::isPrompt() {
    return promptStarted;
}

bool GUI::endPrompt(bool forceClose) {
    if( forceClose || (promptStarted && promptCompleted) ) {
        promptStarted = false;
        promptCompleted = false;

        return true;
    }
    return false;
}

void GUI::drawPrompt(bool immediate) {
    if(!promptStarted || promptCompleted) return;
    SDL_Color background = {0xF0, 0xF0, 0xFF};

    boxRGBA(sdlRenderer, promptX-2*charWidth, promptY-promptHeight/2-charHeight, promptX+promptWidth+2*charWidth, promptY+promptHeight/2+charHeight, background.r, background.g, background.b, 0xFF);

    SDL_Rect textRect;

    SDL_Color color  = {0x00, 0x00, 0x00};
    SDL_Color bright = {0xD0, 0xFF, 0xD0};

    SDL_Surface *textSurface = TTF_RenderText_Blended(monoFont, promptBuffer, color);
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(sdlRenderer, textSurface);

    textRect.x = promptX; 
    textRect.y = (promptY-promptHeight/2);
    textRect.w = textSurface->w;
    textRect.h = textSurface->h;

    SDL_RenderCopy(sdlRenderer, textTexture, NULL, &textRect);

    SDL_DestroyTexture(textTexture);
    SDL_FreeSurface(textSurface);

    if( promptType == PT_CHOICE ) {
        std::string prompt = (editValue > 0 ? "< ": "  ")+promptChoices[editValue]+(editValue < promptChoices.size()-1 ? " >": "  ");
        textSurface = TTF_RenderText_Blended(monoFont, prompt.c_str(), color);
        textTexture = SDL_CreateTextureFromSurface(sdlRenderer, textSurface);
        textRect.x = (screenWidth - textSurface->w)/2;
        textRect.y = promptY+charHeight/2;
        textRect.w = textSurface->w;
        textRect.h = textSurface->h;

        boxRGBA(sdlRenderer, textRect.x, textRect.y, textRect.x+textRect.w, textRect.y+textRect.h, bright.r, bright.g, bright.b, 0xFF);
        SDL_RenderCopy(sdlRenderer, textTexture, NULL, &textRect);

        SDL_DestroyTexture(textTexture);
        SDL_FreeSurface(textSurface);
    }
    if( immediate ) {
        SDL_RenderPresent(sdlRenderer);
    }
}

void GUI::promptYesNo() {
    promptType = PT_CONFIRM;
}

void GUI::promptValue(uint32_t value, int offset, int digits) {
    startEdit(value, promptX, promptY-charHeight/2, offset, digits, true);
    oldPromptValue = value;
    promptType = PT_VALUE;
    promptStarted = true;
    promptCompleted = false;
}

void GUI::promptChoice(std::vector<std::string> choices) {
    promptChoices = choices;
    promptType = PT_CHOICE;
    promptY -= charHeight;
    editValue = 0;
    promptHeight += 2*charHeight;
}

int GUI::getPromptId() {
    return promptId;
}

bool GUI::isPromptOK() {
    return promptOK;
}

bool GUI::promptChanged() {
    return promptType == PT_VALUE && (oldPromptValue != editValue);
}

void GUI::prompt() {
    TTF_SizeUTF8(monoFont, promptBuffer, &promptWidth, &promptHeight);

    promptX = (screenWidth*zoom-promptWidth)/2;
    promptY = (screenHeight*zoom)/2;

    promptType = PT_NONE;
    promptStarted = true;
    promptCompleted = false;
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
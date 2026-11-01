#include "gui.hpp"
#include "assets.hpp"
#include "theme.hpp"
#include <iostream>
#include <cstring>
#include <stdio.h>

void GUI::init(float zoom) {
    this->zoom = zoom;
    std::string uiPath = ::fontPath(Theme::instance().font_ui);
    monoFont = TTF_OpenFont(uiPath.c_str(), MONO_SIZE*zoom);

    if( !monoFont) {
        std::cout << "Couldn't load font "<< uiPath << std::endl;
        exit(1);
    }

    std::string labelPath = ::fontPath(Theme::instance().font_ui_labels);
    labelFont = TTF_OpenFont(labelPath.c_str(), MONO_SIZE*zoom);
    if( !labelFont) {
        std::cout << "Couldn't load font "<< labelPath << std::endl;
        exit(1);
    }

    memAddrFont      = TTF_OpenFont(::fontPath(Theme::instance().font_ui_mem_address).c_str(),  MONO_SIZE*zoom);
    memHexFont       = TTF_OpenFont(::fontPath(Theme::instance().font_ui_mem_hex).c_str(),      MONO_SIZE*zoom);
    memCharsFont     = TTF_OpenFont(::fontPath(Theme::instance().font_ui_mem_chars).c_str(),    MONO_SIZE*zoom);
    keyhintKeyFont   = TTF_OpenFont(::fontPath(Theme::instance().font_ui_keyhint_key).c_str(),  MONO_SIZE*zoom);
    keyhintHintFont  = TTF_OpenFont(::fontPath(Theme::instance().font_ui_keyhint_hint).c_str(), MONO_SIZE*zoom);
    if (!memAddrFont || !memHexFont || !memCharsFont || !keyhintKeyFont || !keyhintHintFont) {
        std::cout << "Couldn't load memory-viewer / keyhint fonts" << std::endl;
        exit(1);
    }

    char padding[2] = {'0', 0};
    TTF_SizeUTF8(monoFont, padding, &charWidth, &charHeight);
}

void GUI::startEdit(uint32_t value, int x, int y, int characterOffset, int digits, bool isContinue, EditType editType) {
    editValue = value;
    editOldValue = value;

    editX = x;
    editY = y;
    editOffset = characterOffset;
    editLength = digits;
    editIndex = digits-1;
    this->editType = editType;
    editAddressDontCare = false;

    editContinue = isContinue;
    editOK = false;
    promptType = PT_NONE;
}

void GUI::startAddressEdit(uint32_t value, bool isPhysical, int x, int y, int characterOffset) {
    if (isPhysical) {
        // Physical: 20-bit address (5 hex digits)
        editValue = value & 0xFFFFF;
    } else {
        // Logical: only 16-bit address matters
        editValue = value & 0xFFFF;
    }
    editOldValue = editValue;

    editX = x;
    editY = y;
    editOffset = characterOffset;
    editLength = 5;  // Always 5 characters for address
    editIndex = 4;   // Start at leftmost position
    editType = ET_ADDRESS;
    editAddressDontCare = !isPhysical;  // Logical = don't care first nibble
    editAddressDontCareOld = editAddressDontCare;  // Save for backspace

    editContinue = false;
    editOK = false;
    promptType = PT_NONE;
}

void GUI::startStringEdit(std::string value, int x, int y, int length, int characterOffset) {
    stringValue = value;

    editX = x;
    editY = y;
    editOffset = characterOffset;
    editLength = length;
    stringIndex = 0;
    editType = ET_STRING;

    isFirstTextEvent = true;

    editContinue = false;
    editOK = false;
    promptType = PT_NONE;
}

bool GUI::isEditing() {
    return promptType != PT_LABEL && (editType != ET_STRING ? editIndex >= 0 : stringIndex < editLength);
}

bool GUI::isContinuousEdit() {
    return editContinue;
}

bool GUI::isEditOK() {
    return editOK;
}

bool GUI::isLogicalAddress() {
    return editAddressDontCare;
}

void GUI::endEdit(bool isOK) {
    editOK = isOK;
    editContinue = false;
    editIndex = -1;
    stringIndex = editLength;
}

void GUI::drawEdit() {
    if( !isEditing() ) return;

    char buffer[2] = {'0', 0};
    int width;
    int height;

    const Theme &theme = Theme::instance();
    SDL_Color background = theme.edit_bg;

    TTF_SizeUTF8(monoFont, buffer, &width, &height);
    boxRGBA(sdlRenderer, editX*zoom+(width*(editOffset-1))-1, (editY+1)*zoom, editX*zoom+(width*(editOffset+editLength-1)), (editY-2)*zoom+height, background.r, background.g, background.b, 0xFF);

    SDL_Rect textRect;

    SDL_Color normal = theme.edit_ink;
    SDL_Color edited = theme.edit_highlight;

    for( int i=editLength-1; i>=0; i--) {
        bool isCurrentIndex = i == editIndex;

        if( editType == ET_ADDRESS ) {
            // Address mode: 5 digits, first is bank nibble or '#' for don't-care
            if( i == 4 ) {
                // First nibble: show '#' if logical, hex digit if physical
                if( editAddressDontCare ) {
                    buffer[0] = '#';
                } else {
                    snprintf(buffer, sizeof(buffer), "%01X", (editValue >> 16) & 0x0F);
                }
            } else {
                // Lower 4 nibbles: always hex from editValue
                snprintf(buffer, sizeof(buffer), "%01X", (editValue >> (i*4)) & 0x0F);
            }
        }
        else if( editType == ET_HEX ) {
            snprintf(buffer, sizeof(buffer), "%01X", (editValue >> (i*4)) & 0x0F);
        }
        else if( editType == ET_BASE_10 ) {
            int divisor = i > 0 ? i*10: 1;
            snprintf(buffer, sizeof(buffer), "%01d", (editValue / divisor) % 10);
        }
        else if( editType == ET_STRING ) {
            if((editLength-1-i)>=(int)stringValue.length()) {
                continue;
            }
            isCurrentIndex = (editLength-1-i) == stringIndex;
            snprintf(buffer, sizeof(buffer), "%c", stringValue[editLength-1-i]);
        }

        SDL_Color color = isCurrentIndex ? edited: normal;

        SDL_Surface *textSurface = TTF_RenderText_Blended(monoFont, buffer, color);
        SDL_Texture *textTexture = SDL_CreateTextureFromSurface(sdlRenderer, textSurface);

        textRect.x = editX*zoom+(width*(editOffset+(editLength-i)-2));
        textRect.y = editY*zoom;
        textRect.w = textSurface->w;
        textRect.h = textSurface->h;

        SDL_RenderCopy(sdlRenderer, textTexture, NULL, &textRect);

        SDL_DestroyTexture(textTexture);
        SDL_FreeSurface(textSurface);
    }
}

void GUI::handleText(char text[]) {
    if( ((editType == ET_STRING && stringIndex < editLength) ||
         (promptType == PT_LABEL && stringIndex < MAX_LABEL_LENGTH))  && !isFirstTextEvent ) {
        // Ignore CTRL-C, CTRL-V and Newline/Carriage Return values
        if( !( SDL_GetModState() & KMOD_CTRL && ( text[0] == 'c' || text[0] == 'C' || text[0] == 'v' || text[0] == 'V' ) ) &&
            !( text[0] == '\n' || text[0] == '\r') ) {

            if( stringIndex < (int)stringValue.length() ) {
                stringValue.resize(stringIndex);
            }

            stringValue += text;

            if( (int)stringValue.length() > editLength ) {
                stringValue.resize(editLength);
            }
            stringIndex = stringValue.length();
            if( stringIndex == editLength ) {
                editOK = true;
            }

            if (promptType == PT_LABEL) {
                lookupSource->lookup(stringValue);
                lookupIndex = 0;
            }
        }
    }
    isFirstTextEvent = false;
}

bool GUI::handleKey(SDL_Keycode key) {
    if( isEditing() ) {
        switch(key) {  
            case SDLK_ESCAPE: endEdit(false); return true;
            case SDLK_RETURN: endEdit(true); return true;
            case SDLK_BACKSPACE: editBackspace(); return true;
        }

        if( editType != ET_STRING ) {
            switch(key) {
                case SDLK_0 ... SDLK_9: editDigit( key - SDLK_0 ); break;
                case SDLK_a ... SDLK_f: editDigit( key - SDLK_a + 10 ); break;
                case SDLK_PERIOD : startPrompt(-1, "Start typing label to search"); promptLabel(); promptLabelToEdit = true; break;
                default:
                    // For ET_ADDRESS mode, accept non-hex chars as "don't care" in first position
                    if( editType == ET_ADDRESS && editIndex == 4 ) {
                        // Any non-hex key in first position = logical address (don't care)
                        editAddressDontCare = true;
                        editValue &= 0xFFFF;  // Clear bank nibble
                        editIndex--;  // Move to next position
                    } else {
                        return false;
                    }
            }
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
                    editIndex = editLength-1;
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
        if( promptType == PT_LABEL ) {
            switch(key) { 
                case SDLK_ESCAPE: 
                    if (promptLabelToEdit) {
                        promptType = PT_NONE;
                        promptStarted = false;
                        editLength = oldEditLength;
                    }
                    else {    
                        promptOK = false; promptCompleted = true;
                    }
                    break;
                case SDLK_RETURN: 
                    if (lookupIndex > 0) {
                        int value = lookupSource->getValue(lookupIndex-1);
                        //int physicalVal = lookupSource->getAdditionalValue(lookupIndex-1);
                        editValue = value;
                    }

                    if (promptLabelToEdit) {
                        promptType = PT_NONE;
                        promptStarted = false;
                        editLength = oldEditLength;
                    }
                    else {    
                        promptOK = lookupIndex > 0; promptCompleted = true; 
                    }
                    break;
                case SDLK_BACKSPACE: editBackspace();  break;
                case SDLK_UP     :
                case SDLK_LEFT   : if( lookupIndex > 1 ) lookupIndex--; break;
                case SDLK_DOWN   :
                case SDLK_RIGHT  : if( lookupIndex < lookupSource->matches() ) lookupIndex++; break;
            }

            
        }
    }
    return true;
}

uint32_t GUI::getEditValue() {
    return editValue;
}

std::string GUI::getStringValue() {
    return stringValue;
}

void GUI::editDigit(uint8_t digit) {
    if( isEditing() ) {
        if( editType == ET_ADDRESS ) {
            if( editIndex == 4 ) {
                // First nibble: hex digit = physical address, store bank nibble
                editAddressDontCare = false;
                editValue = (editValue & 0xFFFF) | ((uint32_t)digit << 16);
            } else {
                // Lower 4 nibbles: standard hex edit
                editValue = (editValue & ~(0x000F << (editIndex*4))) | (digit << (editIndex*4));
            }
        }
        else if( editType == ET_HEX ) {
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
    editValue = (editValue+delta) & (0x0FFFFFF >> ((6-editLength)*4));
}

void GUI::editBackspace() {
    if( editType != ET_STRING && promptType != PT_LABEL ) {
        if( editIndex < (editLength-1) ) {
            editIndex++;
            if( editType == ET_ADDRESS && editIndex == 4 ) {
                // Backing up to first nibble: restore original don't-care state and bank nibble
                editAddressDontCare = editAddressDontCareOld;
                editValue = (editValue & 0xFFFF) | (editOldValue & 0xF0000);
            } else {
                editValue = (editValue & ~(0x000F << (editIndex*4))) | (editOldValue & (0x000F << (editIndex*4)));
            }
        }
    }
    else {
        if( stringIndex > 0 ) {
            stringIndex--;
            stringValue.resize(stringIndex);

            if (promptType == PT_LABEL) {
                lookupSource->lookup(stringValue);
                lookupIndex = 0;
            }
        }
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
    const Theme &theme = Theme::instance();
    SDL_Color background = theme.prompt_bg;

    int promptTop = promptY-zoom*promptHeight/2-charHeight;
    boxRGBA(sdlRenderer, promptX-2*charWidth, promptTop, promptX+promptWidth+2*charWidth, promptY+zoom*promptHeight/2+charHeight, background.r, background.g, background.b, 0xFF);
    rectangleRGBA(sdlRenderer, promptX-2*charWidth, promptTop, promptX+promptWidth+2*charWidth, promptY+zoom*promptHeight/2+charHeight, theme.border.r, theme.border.g, theme.border.b, theme.border.a);

    SDL_Rect textRect;

    SDL_Color color  = theme.edit_ink;
    SDL_Color bright = theme.highlight_bg;

    SDL_Surface *textSurface = TTF_RenderText_Blended(monoFont, promptBuffer, color);
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(sdlRenderer, textSurface);

    textRect.x = promptX; 
    textRect.y = promptTop+charHeight;
    textRect.w = textSurface->w;
    textRect.h = textSurface->h;

    SDL_RenderCopy(sdlRenderer, textTexture, NULL, &textRect);

    SDL_DestroyTexture(textTexture);
    SDL_FreeSurface(textSurface);

    if( promptType == PT_CHOICE ) {
        std::string prompt = (editValue > 0 ? "< ": "  ")+promptChoices[editValue]+(editValue < promptChoices.size()-1 ? " >": "  ");
        textSurface = TTF_RenderText_Blended(monoFont, prompt.c_str(), color);
        textTexture = SDL_CreateTextureFromSurface(sdlRenderer, textSurface);
        textRect.x = (screenWidth*zoom - textSurface->w)/2;
        textRect.y = promptY+charHeight/2;
        textRect.w = textSurface->w;
        textRect.h = textSurface->h;

        boxRGBA(sdlRenderer, textRect.x, textRect.y, textRect.x+textRect.w, textRect.y+textRect.h, bright.r, bright.g, bright.b, 0xFF);
        SDL_RenderCopy(sdlRenderer, textTexture, NULL, &textRect);

        SDL_DestroyTexture(textTexture);
        SDL_FreeSurface(textSurface);
    }
    if( promptType == PT_LABEL ) {
        int maxLen = strlen(promptBuffer)-4;
        char *buffer = new char[maxLen+1];

        if( stringValue.length() > 0 ) {
            std::size_t length = stringValue.copy(buffer, maxLen);
            buffer[length] = '\0';

            printfb(monoFont, (promptX + 3*charWidth)/zoom, (promptTop+3*charHeight)/zoom, color, lookupIndex == 0 ? length: 0, bright, buffer);
        }
        for (size_t i=0; i<lookupSource->matches() && i<LABEL_LIST_LENGTH; i++ ) {
            std::size_t length = lookupSource->getLabel(i).copy(buffer, maxLen);
            buffer[length] = '\0';

            printfb(monoFont, (promptX + charWidth)/zoom,  (promptTop+4.5*charHeight)/zoom+i*MEM_ROW_HEIGHT, color, lookupIndex == i+1 ? length: 0, bright, buffer);
            if( lookupIndex == i+1 ) {
                print((promptX + 2*charWidth)/zoom, (promptTop-1*charHeight)/zoom+promptHeight, color, 0, bright, lookupSource->getDescription1(i).c_str());
                print((promptX + 2*charWidth)/zoom, promptTop/zoom+promptHeight, color, 0, bright, lookupSource->getDescription2(i).c_str());
            }
        }
    }
    if( immediate ) {
        SDL_RenderPresent(sdlRenderer);
    }
}

void GUI::promptYesNo() {
    promptType = PT_CONFIRM;
}

void GUI::promptValue(uint32_t value, int offset, int digits) {
    startEdit(value, promptX/zoom, (promptY-charHeight/2)/zoom, offset, digits, true);
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

void GUI::promptLabel() {
    promptType = PT_LABEL;
    stringValue = "";
    stringIndex = 0;
    isFirstTextEvent = true;
    oldEditLength = editLength;
    editLength = MAX_LABEL_LENGTH;
    promptHeight += 5*ROW_HEIGHT + LABEL_LIST_LENGTH * MEM_ROW_HEIGHT;
    this->lookupSource = lookupSource;
    lookupSource->lookup(stringValue);
    lookupIndex = 0;
    promptLabelToEdit = false;
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

TTF_Font *GUI::getFont() {
    return monoFont;
}

void GUI::prompt() {
    TTF_SizeUTF8(monoFont, promptBuffer, &promptWidth, &promptHeight);

    promptX = (screenWidth*zoom-promptWidth)/2;
    promptY = (screenHeight*zoom)/2;

    promptType = PT_NONE;
    promptStarted = true;
    promptCompleted = false;
}


int GUI::getWidthFor(int characters) {
    return charWidth * characters / zoom;
}

int GUI::printfb(TTF_Font *font, int x, int y, SDL_Color color, int highlight, SDL_Color background, char* buffer) {
    SDL_Surface *textSurface = TTF_RenderText_Blended(font, buffer, color);
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(sdlRenderer, textSurface);
    int effectiveWidth = textSurface->w / zoom;

    SDL_Rect textRect;
    textRect.x = x*zoom;
    textRect.y = y*zoom;
    textRect.w = textSurface->w;
    textRect.h = textSurface->h;

    // Step 1: draw the full text in its normal color
    SDL_RenderCopy(sdlRenderer, textTexture, NULL, &textRect);

    // Step 2/3: for highlighted items, draw the highlight box and re-render the
    // highlighted substring on top using highlight_ink for readable contrast.
    if (highlight != 0) {
        int hWidth, hHeight;
        int boxX;
        char savedChar = 0;
        int savedIdx = -1;
        char *substr;

        if (highlight > 0) {
            savedIdx = highlight;
            savedChar = buffer[highlight];
            buffer[highlight] = 0;
            substr = buffer;
            TTF_SizeUTF8(font, substr, &hWidth, &hHeight);
            boxX = x*zoom;
        } else {
            substr = buffer + strlen(buffer) + highlight;
            TTF_SizeUTF8(font, substr, &hWidth, &hHeight);
            boxX = x*zoom + textSurface->w - hWidth;
        }

        boxRGBA(sdlRenderer, boxX, (y+3)*zoom, boxX + hWidth, y*zoom + textSurface->h,
                background.r, background.g, background.b, 0xFF);

        SDL_Color inkColor = Theme::instance().highlight_ink;
        SDL_Surface *inkSurface = TTF_RenderText_Blended(font, substr, inkColor);
        if (inkSurface) {
            SDL_Texture *inkTexture = SDL_CreateTextureFromSurface(sdlRenderer, inkSurface);
            SDL_Rect inkRect = {boxX, (int)(y*zoom), inkSurface->w, inkSurface->h};
            SDL_RenderCopy(sdlRenderer, inkTexture, NULL, &inkRect);
            SDL_DestroyTexture(inkTexture);
            SDL_FreeSurface(inkSurface);
        }

        if (savedIdx >= 0) buffer[savedIdx] = savedChar;
    }

    SDL_DestroyTexture(textTexture);
    SDL_FreeSurface(textSurface);
    return effectiveWidth;
}

static int sizeInGuiUnits(TTF_Font *font, const char *text, float zoom) {
    int w, h;
    TTF_SizeUTF8(font, text, &w, &h);
    return (int)(w / zoom);
}

int GUI::memAddressWidth(const char *text) { return sizeInGuiUnits(memAddrFont, text, zoom); }
int GUI::memHexWidth(const char *text)     { return sizeInGuiUnits(memHexFont, text, zoom); }
int GUI::memCharsWidth(const char *text)   { return sizeInGuiUnits(memCharsFont, text, zoom); }

int GUI::printKeyHintB(int x, int y, SDL_Color color, int highlight, SDL_Color background, char* buffer) {
    int avg = (color.r + color.g + color.b) / 3;
    SDL_Color hintColor = {(Uint8)avg, (Uint8)avg, (Uint8)avg, color.a};

    char *colon = strchr(buffer, ':');
    if (!colon || colon == buffer) {
        return printfb(keyhintHintFont, x, y, hintColor, highlight, background, buffer);
    }
    int keyLen = (int)(colon - buffer);

    // Measure the key portion with its own font so the hint portion starts at the right x.
    char savedChar = buffer[keyLen];
    buffer[keyLen] = '\0';
    int keyWidthPx, hPx;
    TTF_SizeUTF8(keyhintKeyFont, buffer, &keyWidthPx, &hPx);
    int keyWidth = (int)(keyWidthPx / zoom);

    SDL_Color keyTextColor = Theme::instance().key_ink;
    printfb(keyhintKeyFont, x, y, keyTextColor, keyLen, hintColor, buffer);
    buffer[keyLen] = savedChar;

    int restWidth = printfb(keyhintHintFont, x + keyWidth, y, hintColor, highlight, background, buffer + keyLen);
    return keyWidth + restWidth;
}
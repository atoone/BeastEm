#pragma once

#include "gui.hpp"

const std::string TITLE_STRING = "Feersum MicroBeast Emulator";

class HelpGui {

    public:
        static GUI::Mode helpMenu(SDL_Event windowEvent, GUI::Mode mode) {
            switch (windowEvent.key.keysym.sym) {
                case SDLK_r :
                    return GUI::RUN;
                case SDLK_ESCAPE:
                    return GUI::DEBUG;
            }
            return mode;
        }

        static void drawHelp(SDL_Renderer *sdlRenderer, int screenWidth, int screenHeight, float zoom, GUI *gui) {
            boxRGBA(sdlRenderer, 32 * zoom, 32 * zoom, (screenWidth - 24) * zoom,
                    (screenHeight - 24) * zoom, 0xF0, 0xF0, 0xE0, 0xE8);

            SDL_Color textColor = {0, 0x30, 0x30, 255};
            SDL_Color menuColor = {0x30, 0x30, 0xA0, 255};
            SDL_Color bright = {0xD0, 0xFF, 0xD0, 255};

            drawCentered(GUI::ROW1, TITLE_STRING.c_str(), gui, menuColor, sdlRenderer, screenWidth, zoom);
            
            drawCentered(GUI::ROW3, "Welcome! Press 'R' to Run MicroBeast, or ESCAPE for debug menu.", gui, textColor, sdlRenderer, screenWidth, zoom);

            int row = GUI::ROW5;

            gui->print(GUI::COL1, row, textColor, "Keyboard shortcuts:");
            row += GUI::ROW_HEIGHT*2;

            gui->print(GUI::COL1, row, textColor, "ESCAPE pauses emulation and opens the debug menu. In debug, '?' shows this help");
            row += GUI::ROW_HEIGHT*2;

            int offset = gui->print(GUI::COL1, row, textColor, "Menu items are shown in ");
            offset += gui->print(GUI::COL1+offset, row, menuColor, "[B]lue");
            row += GUI::ROW_HEIGHT;

            gui->print(GUI::COL1, row, textColor, "Press the letter in square brackets to activate the menu item");
            row += GUI::ROW_HEIGHT*2;           

            offset = gui->print(GUI::COL1, row, textColor, "Editable values are ");
            gui->print(GUI::COL1+offset, row, textColor, 11, bright, "highlighted");
            row += GUI::ROW_HEIGHT;

            gui->print(GUI::COL1, row, textColor, "Cursor UP and DOWN to select, LEFT and RIGHT to adjust value");
            row += GUI::ROW_HEIGHT;           

            gui->print(GUI::COL1, row, textColor, "Press ENTER to edit the selected value");
            row += GUI::ROW_HEIGHT*2;

            gui->print(GUI::COL1, row, textColor, "When editing, press ENTER to use the new value, or ESCAPE to cancel");
            row += GUI::ROW_HEIGHT;

            gui->print(GUI::COL1, row, textColor, "Press '.' (period) to lookup a label from source files");
            row += GUI::ROW_HEIGHT;

            gui->print(GUI::COL1, row, textColor, "Prefix lookup with <digit>:  (e.g. '1:' etc.) to search specific source file");
            row += GUI::ROW_HEIGHT*2;

            gui->print(GUI::COL1, row, textColor, "On the main Debug page, use PAGE UP, PAGE DOWN to move the assembly listing");
            row += GUI::ROW_HEIGHT;

            gui->print(GUI::COL1, row, textColor, "Press 'H' before PAGE UP/DOWN to move back through execution history");
            row += GUI::ROW_HEIGHT;

            gui->print(GUI::COL1, row, textColor, "Then press 'L' or 'H' to return listing to current CPU PC address");
            row += GUI::ROW_HEIGHT*2;

            gui->print(GUI::COL1, row, textColor, "Create a breakpoint at the current CPU address by pressing 'C'");
            row += GUI::ROW_HEIGHT;     

            gui->print(GUI::COL1, row, textColor, "Or press 'Y' to create a breakpoint at the current list location");
            row += GUI::ROW_HEIGHT;  

            gui->print(GUI::COL1, GUI::END_ROW, menuColor, "[R]un");
            gui->print(GUI::COL5, GUI::END_ROW, menuColor, "[ESC]:Exit");
        }

        static void drawCentered(int y, const char* text, GUI *gui, const SDL_Color &menuColor, SDL_Renderer *sdlRenderer, int screenWidth, float zoom)
        {
            SDL_Surface *textSurface = TTF_RenderText_Blended(gui->getFont(), text, menuColor);
            SDL_Texture *textTexture = SDL_CreateTextureFromSurface(sdlRenderer, textSurface);
            SDL_Rect textRect;

            textRect.x = (screenWidth - textSurface->w) / 2;
            textRect.y = y * zoom;
            textRect.w = textSurface->w;
            textRect.h = textSurface->h;

            SDL_RenderCopy(sdlRenderer, textTexture, NULL, &textRect);

            SDL_DestroyTexture(textTexture);
            SDL_FreeSurface(textSurface);
        }

    private:

};

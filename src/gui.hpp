#pragma once
#include <utility>
#include <vector>
#include <string>


#include "SDL.h"
#include "SDL_ttf.h"
#include "SDL2_gfxPrimitives.h"

class GUI {

    enum PromptType {PT_NONE, PT_CONFIRM, PT_VALUE, PT_CHOICE};

    public:
        static const int COL1 = 50;
        static const int COL2 = 190;
        static const int COL3 = 330;
        static const int COL4 = 470;
        static const int COL5 = 610;

        static const int COL_MEM = 130;

        static const int ROW_HEIGHT = 16;
        static const int MEM_ROW_HEIGHT = 14;

        static const int ROW1 = 56;
        static const int ROW2 = ROW1+16;
        static const int ROW3 = ROW2+16;
        static const int ROW4 = ROW3+16;
        static const int ROW5 = ROW4+16;

        static const int ROW7 = ROW5+32;
        static const int ROW8 = ROW7+MEM_ROW_HEIGHT;
        static const int ROW9 = ROW8+MEM_ROW_HEIGHT;

        static const int ROW11 = ROW9+2*MEM_ROW_HEIGHT;
        static const int ROW12 = ROW11+MEM_ROW_HEIGHT;
        static const int ROW13 = ROW12+MEM_ROW_HEIGHT;

        static const int ROW15 = ROW13+2*MEM_ROW_HEIGHT;
        static const int ROW16 = ROW15+MEM_ROW_HEIGHT;
        static const int ROW17 = ROW16+MEM_ROW_HEIGHT;

        static const int ROW19 = ROW16+44;
        static const int ROW20 = ROW19+16;

        static const int ROW22 = ROW20+28;
        static const int END_ROW = ROW22+(13*14);

        enum EditType {ET_HEX, ET_BASE_10};

        GUI(SDL_Renderer *sdlRenderer, int screenWidth, int screenHeight):
            sdlRenderer (sdlRenderer),
            screenWidth (screenWidth),
            screenHeight (screenHeight)
            {};
        ~GUI() {};

        void      init(float zoom);

        void      startEdit(uint32_t value, int x, int y, int offset, int digits, bool isContinue = false, EditType editType = ET_HEX);
        bool      isEditing();
        bool      isContinuousEdit();
        bool      isEditOK();
        void      endEdit(bool editOK);
        void      drawEdit();
        bool      handleKey(SDL_Keycode key);
        uint32_t  getEditValue();
        void      editDelta(int delta);

        bool      isPrompt();                        // True if we're currently in a prompt
        bool      endPrompt(bool forceClose);        // True if the user has completed a prompt
        void      drawPrompt(bool immediate);        // Draws the current prompt if one exits
        void      promptYesNo();
        void      promptValue(uint32_t value, int offset, int digits);
        void      promptChoice(std::vector<std::string> choices);
        
        int       getPromptId();
        bool      isPromptOK();
        bool      promptChanged();

        template<typename... Args> void print(int x, int y, SDL_Color color, const char *fmt, Args... args) {
            char buffer[200];
            int c;
            if constexpr (sizeof...(Args) == 0) {
                c = snprintf(buffer, sizeof(buffer), "%s", fmt);
            } else {
                c = snprintf(buffer, sizeof(buffer), fmt, args...);
            }
            if( c > 0 && c<(int)sizeof(buffer)) {
                printb(x,y, color, 0, {0}, buffer);
            }
        }

        template<typename... Args> void print(int x, int y, SDL_Color color, int highlight, SDL_Color background, const char *fmt, Args... args) {
            char buffer[200];
            int c;
            if constexpr (sizeof...(Args) == 0) {
                c = snprintf(buffer, sizeof(buffer), "%s", fmt);
            } else {
                c = snprintf(buffer, sizeof(buffer), fmt, args...);
            }
            if( c > 0 && c<(int)sizeof(buffer)) {
                printb(x,y, color, highlight, background, buffer);
            }
        }

        template<typename... Args> void startPrompt(int id, const char *fmt, Args... args) {
            promptId = id;
            int c;
            if constexpr (sizeof...(Args) == 0) {
                c = snprintf(promptBuffer, sizeof(promptBuffer), "%s", fmt);
            } else {
                c = snprintf(promptBuffer, sizeof(promptBuffer), fmt, args...);
            }
            if( c > 0 && c<(int)sizeof(promptBuffer)) {
                prompt();
            }
        }

         template<typename... Args> void updatePrompt(const char *fmt, Args... args) {
            snprintf(promptBuffer, sizeof(promptBuffer), fmt, args...);
            oldPromptValue = editValue;
        }

    private:
    
        const char* MONO_FONT = "RobotoMono-VariableFont_wght.ttf";
        const int   MONO_SIZE = 14;

        SDL_Renderer  *sdlRenderer;

        int screenWidth, screenHeight;
        int charWidth, charHeight;
        float zoom = 1.0;

        TTF_Font *monoFont;

        uint32_t   editValue, editOldValue;

        int        editX, editY, editOffset;
        int        editDigits;
        int        editIndex = -1;
        bool       editContinue = false;
        bool       editOK = false;
        EditType   editType;

        int        promptId;
        PromptType promptType;
        int        promptX, promptY;
        int        promptWidth, promptHeight;
        char       promptBuffer[200] = {};
        bool       promptStarted = false;
        bool       promptCompleted = false;
        bool       promptOK = false;
        
        std::vector<std::string> promptChoices;

        uint32_t   oldPromptValue;

        void      editBackspace();
        void      editDigit(uint8_t digit);
        void      prompt();
        void      printb(int x, int y, SDL_Color color, int highlight, SDL_Color background, char* buffer);
};
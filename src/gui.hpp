#pragma once
#include <utility>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

#include "SDL.h"
#include "SDL_ttf.h"
#include "SDL2_gfxPrimitives.h"

class Lookup {
    /* The lookup class is used by the GUI to look up matching labels for the user to select a value by name */
    public:
        /* Perform a lookup on the given match string, and remember the results */
        virtual void lookup(std::string match) = 0;

        /* Return the number of matches in the most recent lookup */
        virtual size_t matches() = 0;

        /* Get the label for the n-th match in the most recent lookup */
        virtual std::string getLabel(size_t index) = 0;

        /* Get the numerical value for the n-th match in the most recent lookup */
        virtual int getValue(size_t index) = 0;

        /* Get a description for the n-th match in the most recent lookup */
        virtual std::string getDescription1(size_t index) = 0;

        /* Get a description for the n-th match in the most recent lookup */
        virtual std::string getDescription2(size_t index) = 0;
};


class GUI {

    enum PromptType {PT_NONE, PT_CONFIRM, PT_VALUE, PT_CHOICE, PT_LABEL};

    public:
        enum Mode {RUN, STEP, OUT, OVER, TAKE, DEBUG, FILES, BREAKPOINTS, WATCHPOINTS, TRACELOG, HELP, QUIT};

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

        static const int LABEL_LIST_LENGTH = 12;
        static const int MAX_LABEL_LENGTH  = 12;

        enum EditType {ET_HEX, ET_BASE_10, ET_ADDRESS, ET_STRING};

        GUI(Lookup *lookupSource, SDL_Renderer *sdlRenderer, int screenWidth, int screenHeight):
            sdlRenderer (sdlRenderer),
            screenWidth (screenWidth),
            screenHeight (screenHeight)
            {
                this->lookupSource = lookupSource;
            };
        ~GUI() {};

        void      init(float zoom);

        void      startEdit(uint32_t value, int x, int y, int characterOffset, int digits, bool isContinue = false, EditType editType = ET_HEX);
        void      startAddressEdit(uint32_t value, bool isPhysical, int x, int y, int characterOffset);
        void      startStringEdit(std::string value, int x, int y, int length, int characterOffset=1);
        bool      isEditing();
        bool      isContinuousEdit();
        bool      isEditOK();
        bool      isLogicalAddress();  // For ET_ADDRESS: true if first nibble is "don't care"
        void      endEdit(bool editOK);
        void      drawEdit();
        bool      handleKey(SDL_Keycode key);
        void      handleText(char text[]);
        uint32_t  getEditValue();
        std::string getStringValue();
        void      editDelta(int delta);

        bool      isPrompt();                        // True if we're currently in a prompt
        bool      endPrompt(bool forceClose);        // True if the user has completed a prompt
        void      drawPrompt(bool immediate);        // Draws the current prompt if one exits
        void      promptYesNo();
        void      promptValue(uint32_t value, int offset, int digits);
        void      promptChoice(std::vector<std::string> choices);

        void      promptLabel();

        int       getPromptId();
        bool      isPromptOK();
        bool      promptChanged();

        TTF_Font *getFont();
        
        template<typename... Args> int print(int x, int y, SDL_Color color, const char *fmt, Args... args) {
            char buffer[200];
            int c;
            if constexpr (sizeof...(Args) == 0) {
                c = snprintf(buffer, sizeof(buffer), "%s", fmt);
            } else {
                c = snprintf(buffer, sizeof(buffer), fmt, args...);
            }
            if( c > 0 && c<(int)sizeof(buffer)) {
                return printb(x,y, color, 0, {0}, buffer);
            }
            return 0;
        }

        template<typename... Args> int print(int x, int y, SDL_Color color, int highlight, SDL_Color background, const char *fmt, Args... args) {
            char buffer[200];
            int c;
            if constexpr (sizeof...(Args) == 0) {
                c = snprintf(buffer, sizeof(buffer), "%s", fmt);
            } else {
                c = snprintf(buffer, sizeof(buffer), fmt, args...);
            }
            if( c > 0 && c<(int)sizeof(buffer)) {
                return printb(x,y, color, highlight, background, buffer);
            }
            return 0;
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

        /* Helper function to give string format functionality for generating a std::string */
        template<typename ... Args> static std::string string_format( const std::string& format, Args ... args ) {
            int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
            if( size_s <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
            auto size = static_cast<size_t>( size_s );
            std::unique_ptr<char[]> buf( new char[ size ] );
            std::snprintf( buf.get(), size, format.c_str(), args ... );
            return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
        }

        /* Returns the gui width for a string of the given number of characters */
        int       getWidthFor(int characters);

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
        int        editLength;
        int        editIndex = -1;
        bool       editContinue = false;
        bool       editOK = false;
        EditType   editType;
        bool       editAddressDontCare = false;     // For ET_ADDRESS: first nibble is "don't care" (logical address)
        bool       editAddressDontCareOld = false;  // Original state for backspace restore

        int        promptId;
        PromptType promptType;
        int        promptX, promptY;
        int        promptWidth, promptHeight;
        char       promptBuffer[200] = {};
        bool       promptStarted = false;
        bool       promptCompleted = false;
        bool       promptOK = false;
        
        std::vector<std::string> promptChoices;
        Lookup     *lookupSource;
        size_t     lookupIndex;
        bool       promptLabelToEdit;

        uint32_t   oldPromptValue;
        int        oldEditLength;  // Stores edit length when dropping into label prompt.

        std::string stringValue;
        int         stringIndex = -1;
        bool        isFirstTextEvent = false;

        void      editBackspace();
        void      editDigit(uint8_t digit);
        void      prompt();

        /* Print a buffer at the given location, returning the gui width of the displayed string */
        int       printb(int x, int y, SDL_Color color, int highlight, SDL_Color background, char* buffer);

};
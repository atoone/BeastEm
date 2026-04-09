#pragma once
#include <set>
#include <vector>
#include "SDL.h"

#include "gui.hpp"
#include "debugmanager.hpp"
    
class BreakpointGui {

    enum BEdit {NOEDIT, LOCATION, NAME};

    public:
        BreakpointGui(SDL_Renderer *sdlRenderer, int screenWidth, int screenHeight, float zoom, GUI *gui, DebugManager *debugManager);
        ~BreakpointGui();

        GUI::Mode breakpointsMenu(SDL_Event windowEvent, GUI::Mode mode);
        void breakpointTextEvent();
        void drawBreakpoints();
        GUI::Mode watchpointsMenu(SDL_Event windowEvent, GUI::Mode mode);
        void drawWatchpoints();

        GUI::Mode traceLogMenu(SDL_Event windowEvent, GUI::Mode mode);
        void drawTraceLog();
        void resetMode();

    private:        
        DebugManager    *debugManager;
        GUI             *gui;
        SDL_Renderer    *sdlRenderer;
        
        int screenWidth, screenHeight;
        float zoom = 1.0f;
        
        int     breakpointSelection = 0;
        BEdit   breakpointEditMode = BEdit::NOEDIT;
        int     watchpointSelection = 0;
        bool    watchpointEditMode = false;
        int     watchpointEditField = 0;  // 0=address, 1=range, 2=type
        bool    watchpointAddMode = false;  // true when adding new, false when editing existing
        uint32_t watchpointEditAddress = 0;
        uint16_t watchpointEditRange = 1;
        bool     watchpointEditOnRead = false;
        bool     watchpointEditOnWrite = true;
        bool     watchpointEditIsPhysical = false;

        size_t    logStart;
        
        const int MAX_NAME_LENGTH = 12; // Maximum length for breakpoint names

        const int LOG_LIST_SIZE = 20;
};

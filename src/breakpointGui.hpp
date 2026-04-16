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
        void addBreakpoint(uint32_t address, bool isPhysical);
        void breakpointTextEvent();
        void drawBreakpoints();
        GUI::Mode watchpointsMenu(SDL_Event windowEvent, GUI::Mode mode);
        void drawWatchpoints();

        std::string getTraceString(TraceLog log, Trace trace, int &value);

        GUI::Mode traceLogMenu(SDL_Event windowEvent, GUI::Mode mode);
        void drawTraceLog();
        void resetMode(bool selectLast);
        void setAddresses(uint16_t cpuAddress, uint8_t cpuPage, uint64_t listingAddress);

    private:        
        DebugManager    *debugManager;
        GUI             *gui;
        SDL_Renderer    *sdlRenderer;
        
        int screenWidth, screenHeight;
        float zoom = 1.0f;


        size_t  breakpointSelection = 0;
        BEdit   breakpointEditMode = BEdit::NOEDIT;
        size_t  watchpointSelection = 0;
        bool    watchpointEditMode = false;
        int     watchpointEditField = 0;  // 0=address, 1=range, 2=type
        bool    watchpointAddMode = false;  // true when adding new, false when editing existing
        uint32_t watchpointEditAddress = 0;
        uint16_t watchpointEditRange = 1;
        bool     watchpointEditOnRead = false;
        bool     watchpointEditOnWrite = true;
        bool     watchpointEditIsPhysical = false;

        size_t    logStart;
        size_t    currentLog;
        bool      showRelative = false;
        
        uint16_t  cpuAddress;
        uint8_t   cpuPage;
        uint64_t  listingAddress;

        std::string addSeparator(uint64_t value);

        const int MAX_NAME_LENGTH = 12; // Maximum length for breakpoint names

        const size_t LOG_LIST_SIZE = 20;
};

#include "breakpointGui.hpp"

BreakpointGui::BreakpointGui(SDL_Renderer *sdlRenderer, int screenWidth, int screenHeight, float zoom, GUI *gui, DebugManager *debugManager) {
    this->sdlRenderer = sdlRenderer;
    this->gui = gui;
    this->debugManager = debugManager;

    this->screenWidth = screenWidth;
    this->screenHeight = screenHeight;
    this->zoom = zoom;
}

BreakpointGui::~BreakpointGui() {
}

GUI::Mode BreakpointGui::breakpointsMenu(SDL_Event windowEvent, GUI::Mode mode) {
    int bpCount = debugManager->getBreakpointCount();

    if (breakpointEditMode == BEdit::LOCATION)
    {
        // In edit mode - handle address entry
        if (gui->handleKey(windowEvent.key.keysym.sym))
        {
            if (gui->isEditOK())
            {
                uint32_t address = gui->getEditValue();
                bool isPhysical = !gui->isLogicalAddress();

                // Check if we're editing an existing breakpoint or adding new
                const Breakpoint *existingBp =
                    debugManager->getBreakpoint(breakpointSelection);
                if (existingBp)
                {
                    // Editing existing - remove old and add new with same enabled state
                    debugManager->updateBreakpoint(breakpointSelection, address, isPhysical);
                }
                else
                {
                    // Adding new breakpoint
                    int newIndex = debugManager->addBreakpoint(address, isPhysical);
                    if (newIndex >= 0)
                    {
                        breakpointSelection = newIndex;
                    }
                }
                breakpointEditMode = BEdit::NOEDIT;
            }
            // Check if edit was canceled (e.g., by ESCAPE handled internally by GUI)
            if (!gui->isEditing() && !gui->isPrompt())
            {
                breakpointEditMode = BEdit::NOEDIT;
            }
        }
        return mode;
    }
    else if (breakpointEditMode == BEdit::NAME)
    {
        if (gui->handleKey(windowEvent.key.keysym.sym))
        {
            if (gui->isEditOK())
            {
                debugManager->setBreakpointName(breakpointSelection, gui->getStringValue());
            }

            if (!gui->isEditing())
            {
                breakpointEditMode = BEdit::NOEDIT;
            }
        }
        return mode;
    }

    switch (windowEvent.key.keysym.sym)
    {
    case SDLK_UP:
        if (breakpointSelection > 0)
        {
            breakpointSelection--;
        }
        else
        {
            breakpointSelection = 7; // Wrap to bottom
        }
        break;

    case SDLK_DOWN:
        if (breakpointSelection < 7)
        {
            breakpointSelection++;
        }
        else
        {
            breakpointSelection = 0; // Wrap to top
        }
        break;

    case SDLK_a:
        // Add new breakpoint if list not full
        if (bpCount < 8)
        {
            breakpointEditMode = BEdit::LOCATION;
            // Position edit after " %d   0x" prefix - default to logical (don't care)
            gui->startAddressEdit(0, false, GUI::COL1,
                                 GUI::ROW3 + (bpCount * GUI::ROW_HEIGHT), 9);
            breakpointSelection = bpCount;
        }
        break;

    case SDLK_d:
        // Delete selected breakpoint
        if (breakpointSelection < bpCount)
        {
            debugManager->removeBreakpoint(breakpointSelection);
            bpCount = debugManager->getBreakpointCount();
            // Move selection to next valid item, or previous if deleted last
            if (breakpointSelection >= bpCount && bpCount > 0)
            {
                breakpointSelection = bpCount - 1;
            }
            else if (bpCount == 0)
            {
                breakpointSelection = 0;
            }
        }
        break;

    case SDLK_SPACE:
        // Toggle enable/disable on selected populated slot
        if (breakpointSelection < bpCount)
        {
            const Breakpoint *bp = debugManager->getBreakpoint(breakpointSelection);
            if (bp)
            {
                if (!bp->enabled || bp->isTrace)
                {
                    debugManager->setBreakpointEnabled(breakpointSelection, !bp->enabled);
                    debugManager->setBreakpointIsTrace(breakpointSelection, false);
                }
                else if (bp->enabled && !bp->isTrace)
                {
                    debugManager->setBreakpointIsTrace(breakpointSelection, true);
                }
            }
        }
        break;

    case SDLK_RETURN:
        // Edit selected breakpoint address
        if (breakpointSelection < bpCount)
        {
            const Breakpoint *bp = debugManager->getBreakpoint(breakpointSelection);
            if (bp)
            {
                breakpointEditMode = BEdit::LOCATION;
                // Start address edit with current physical/logical state
                gui->startAddressEdit(
                    bp->address, bp->isPhysical, GUI::COL1,
                    GUI::ROW3 + (breakpointSelection * GUI::ROW_HEIGHT), 9);
            }
        }
        break;
    case SDLK_n:
        if (breakpointSelection < bpCount)
        {
            const Breakpoint *bp = debugManager->getBreakpoint(breakpointSelection);
            if (bp)
            {
                gui->startStringEdit(bp->name, GUI::COL3, GUI::ROW3 + (breakpointSelection * GUI::ROW_HEIGHT), MAX_NAME_LENGTH);
                breakpointEditMode = BEdit::NAME;
            }
        }
        break;

    case SDLK_ESCAPE:
        mode = GUI::DEBUG;
        break;
    case SDLK_g:
        // Switch to TraceLog screen
        mode = GUI::TRACELOG;
        resetMode();
        break;
    case SDLK_w:
        mode = GUI::WATCHPOINTS;
        resetMode();
        break;
    }
    return mode;
}

void BreakpointGui::breakpointTextEvent() {
  if (breakpointEditMode == BEdit::NAME) {
    if (gui->isEditOK()) {
      debugManager->setBreakpointName(breakpointSelection, gui->getStringValue());
    }

    if (!gui->isEditing()) {
      breakpointEditMode = BEdit::NOEDIT;
    }
  }
}

void BreakpointGui::drawBreakpoints() {
  boxRGBA(sdlRenderer, 32 * zoom, 32 * zoom, (screenWidth - 24) * zoom,
          (screenHeight - 24) * zoom, 0xF0, 0xF0, 0xE0, 0xE8);

  SDL_Color textColor = {0, 0x30, 0x30, 255};
  SDL_Color dimColor = {0x80, 0x80, 0x80, 255};
  SDL_Color menuColor = {0x30, 0x30, 0xA0, 255};
  SDL_Color bright = {0xD0, 0xFF, 0xD0, 255};

  int bpCount = debugManager->getBreakpointCount();

  // Title and navigation hint
  gui->print(GUI::COL1, 34, menuColor, "BREAKPOINTS");
  gui->print(GUI::COL4, 34, menuColor, "[W]atchpoints");
  gui->print(GUI::COL5, 34, menuColor, "Trace lo[G]");

  // Column headers - aligned with data columns
  gui->print(GUI::COL1, GUI::ROW2, textColor, " #    Address  Status");

  gui->print(GUI::COL3, GUI::ROW2, textColor, "Name");

  // Render 8 fixed rows
  for (int i = 0; i < 8; i++) {
    int row = GUI::ROW3 + (i * GUI::ROW_HEIGHT);
    bool isSelected = (i == breakpointSelection);

    // If editing this row, show the prefix and let gui.drawEdit() handle the
    // value
    if (breakpointEditMode == BEdit::LOCATION && isSelected) {
      gui->print(GUI::COL1, row, textColor, 0, bright, " %d    0x", i + 1);
    } else if (i < bpCount) {
      const Breakpoint *bp = debugManager->getBreakpoint(i);
      if (bp) {
        SDL_Color rowColor = bp->enabled ? textColor : dimColor;
        const char *enabledStr = bp->enabled ? bp->isTrace ? "[Trace]" : "[Break]" : "[ ]";

        if (bp->isPhysical) {
          gui->print(GUI::COL1, row, rowColor, isSelected ? 22 : 0, bright,
                    " %d    0x%05X  %s", i + 1, bp->address, enabledStr);
        } else {
          gui->print(GUI::COL1, row, rowColor, isSelected ? 22 : 0, bright,
                    " %d    0x#%04X  %s", i + 1, bp->address, enabledStr);
        }

        if (!(gui->isEditing() && isSelected)) {
          gui->print(GUI::COL3, row, rowColor, isSelected ? 22: 0, bright,
                    "%-12s", bp->name.c_str() );
        }
      }
    } else {
      // Empty slot - show dashes matching address width
      SDL_Color veryDimColor = {0x60, 0x60, 0x60, 255};
      gui->print(GUI::COL1, row, veryDimColor, isSelected ? 22 : 0, bright,
                " %d    -------  ---", i + 1);
    }
  }

  // Footer with key hints
  if (bpCount >= 8) {
    gui->print(GUI::COL1, GUI::END_ROW, menuColor, "Full");
  } else {
    gui->print(GUI::COL1, GUI::END_ROW, menuColor, "[A]dd");
  }
  gui->print(GUI::COL2 - 60, GUI::END_ROW, menuColor, "[D]elete");
  gui->print(GUI::COL3 - 110, GUI::END_ROW, menuColor, "[Space]:Toggle");
  gui->print(GUI::COL3 + 20, GUI::END_ROW, menuColor, "[Enter]:Edit");
  gui->print(GUI::COL4 + 20, GUI::END_ROW, menuColor, "[N]ame");
  gui->print(GUI::COL5, GUI::END_ROW, menuColor, "[ESC]:Exit");
}

void BreakpointGui::drawWatchpoints() {
  boxRGBA(sdlRenderer, 32 * zoom, 32 * zoom, (screenWidth - 24) * zoom,
          (screenHeight - 24) * zoom, 0xF0, 0xF0, 0xE0, 0xE8);

  SDL_Color textColor = {0, 0x30, 0x30, 255};
  SDL_Color dimColor = {0x80, 0x80, 0x80, 255};
  SDL_Color menuColor = {0x30, 0x30, 0xA0, 255};
  SDL_Color bright = {0xD0, 0xFF, 0xD0, 255};

  int wpCount = debugManager->getWatchpointCount();

  // Title and navigation hint
  gui->print(GUI::COL1, 34, menuColor, "WATCHPOINTS");
  gui->print(GUI::COL4, 34, menuColor, "[B]reakpoints");
  gui->print(GUI::COL5, 34, menuColor, "Trace lo[G]");

  // Column headers - aligned with data columns
  gui->print(GUI::COL1, GUI::ROW2, textColor,
            " #    Address   Range    Type  Enabled");

  // Render 8 fixed rows
  for (int i = 0; i < 8; i++) {
    int row = GUI::ROW3 + (i * GUI::ROW_HEIGHT);
    bool isSelected = (i == watchpointSelection);

    if (watchpointEditMode && isSelected) {
      // Editing this row - show edit state with temporary values
      const char *typeStr = (watchpointEditOnRead && watchpointEditOnWrite)
                                ? "RW"
                                : (watchpointEditOnRead ? "R " : "W ");

      if (watchpointEditField == 0) {
        // Editing address field - show prefix and let gui.drawEdit() handle
        // value
        gui->print(GUI::COL1, row, textColor, 0, bright, " %d    0x", i + 1);
      } else if (watchpointEditField == 1) {
        // Editing range field - show address and prefix for range
        if (watchpointEditIsPhysical) {
          gui->print(GUI::COL1, row, textColor, 0, bright, " %d    0x%05X  0x",
                    i + 1, watchpointEditAddress);
        } else {
          gui->print(GUI::COL1, row, textColor, 0, bright, " %d    0x#%04X  0x",
                    i + 1, watchpointEditAddress);
        }
      } else if (watchpointEditField == 2) {
        // Editing type field - highlight just the type with yellow background
        // Only show row highlight when editing existing (not when adding new)
        int highlight = watchpointAddMode ? 0 : 40;
        SDL_Color yellow = {0xFF, 0xFF, 0x80, 255};
        if (watchpointEditIsPhysical) {
          gui->print(GUI::COL1, row, textColor, highlight, bright,
                    " %d    0x%05X  0x%04X   ", i + 1, watchpointEditAddress,
                    watchpointEditRange);
          gui->print(GUI::COL1 + 198, row, textColor, 4, yellow, "[%s]",
                    typeStr);
          gui->print(GUI::COL1 + 234, row, textColor, 0, bright, "  [*]");
        } else {
          gui->print(GUI::COL1, row, textColor, highlight, bright,
                    " %d    0x#%04X  0x%04X   ", i + 1, watchpointEditAddress,
                    watchpointEditRange);
          gui->print(GUI::COL1 + 198, row, textColor, 4, yellow, "[%s]",
                    typeStr);
          gui->print(GUI::COL1 + 234, row, textColor, 0, bright, "  [*]");
        }
      }
    } else if (i < wpCount) {
      const Watchpoint *wp = debugManager->getWatchpoint(i);
      if (wp) {
        SDL_Color rowColor = wp->enabled ? textColor : dimColor;
        const char *enabledStr = wp->enabled ? "[*]" : "[ ]";
        const char *typeStr =
            (wp->onRead && wp->onWrite) ? "RW" : (wp->onRead ? "R " : "W ");

        if (wp->isPhysical) {
          gui->print(GUI::COL1, row, rowColor, isSelected ? 40 : 0, bright,
                    " %d    0x%05X  0x%04X   %s   %s", i + 1, wp->address,
                    wp->length, typeStr, enabledStr);
        } else {
          gui->print(GUI::COL1, row, rowColor, isSelected ? 40 : 0, bright,
                    " %d    0x#%04X  0x%04X   %s   %s", i + 1, wp->address,
                    wp->length, typeStr, enabledStr);
        }
      }
    } else {
      // Empty slot - dashes matching column widths
      SDL_Color veryDimColor = {0x60, 0x60, 0x60, 255};
      gui->print(GUI::COL1, row, veryDimColor, isSelected ? 40 : 0, bright,
                " %d    -------  ------    --    ---", i + 1);
    }
  }

  // Footer with key hints
  if (wpCount >= 8 && !watchpointEditMode) {
    gui->print(GUI::COL1, GUI::END_ROW, menuColor, "Full");
  } else {
    gui->print(GUI::COL1, GUI::END_ROW, menuColor, "[A]dd");
  }
  gui->print(GUI::COL2 - 40, GUI::END_ROW, menuColor, "[D]elete");
  gui->print(GUI::COL3 - 40, GUI::END_ROW, menuColor, "[Space]:Toggle");
  gui->print(GUI::COL4, GUI::END_ROW, menuColor, "[Enter]:Edit");
  gui->print(GUI::COL5, GUI::END_ROW, menuColor, "[ESC]:Exit");
}

void BreakpointGui::resetMode() {
    breakpointSelection = 0;
    
    watchpointSelection = 0;
    watchpointEditMode = false;
    watchpointEditField = 0;

    logStart = std::max(debugManager->getLogSize()-LOG_LIST_SIZE, 0);
}

GUI::Mode BreakpointGui::watchpointsMenu(SDL_Event windowEvent, GUI::Mode mode) {
  int wpCount = debugManager->getWatchpointCount();

  if (watchpointEditMode) {
    // Handle edit mode based on current field
    if (watchpointEditField == 0 || watchpointEditField == 1) {
      // Address or Range field - use hex/address entry
      if (gui->handleKey(windowEvent.key.keysym.sym)) {
        if (gui->isEditOK()) {
          uint32_t value = gui->getEditValue();

          if (watchpointEditField == 0) {
            // Address field complete - store address and physical state,
            // advance to range
            watchpointEditAddress = value;
            watchpointEditIsPhysical = !gui->isLogicalAddress();
            watchpointEditField = 1;
            // Start editing range with default or existing value
            int rangeX = watchpointEditIsPhysical ? (GUI::COL1 + 160)
                                                  : (GUI::COL1 + 160);
            gui->startEdit(watchpointEditRange, rangeX,
                          GUI::ROW3 + (watchpointSelection * GUI::ROW_HEIGHT),
                          0, 4, false, GUI::ET_HEX);
          } else {
            // Range field complete - store and advance to type field
            watchpointEditRange =
                (value == 0) ? 1 : value; // Default to 1 if empty
            watchpointEditField = 2;
            // Type field uses arrow keys, no gui.startEdit needed
          }
        }
        // Check if edit was canceled (e.g., by ESCAPE handled internally by
        // GUI)
        if (!gui->isEditing() && !gui->isEditOK()) {
          watchpointEditMode = false;
          watchpointEditField = 0;
          watchpointAddMode = false;
        }
      } else if (windowEvent.key.keysym.sym == SDLK_TAB) {
        // Tab advances to next field
        uint32_t value = gui->getEditValue();

        if (watchpointEditField == 0) {
          // Capture physical state before ending edit
          watchpointEditIsPhysical = !gui->isLogicalAddress();
          gui->endEdit(true);
          watchpointEditAddress = value;
          watchpointEditField = 1;
          int rangeX =
              watchpointEditIsPhysical ? (GUI::COL1 + 160) : (GUI::COL1 + 160);
          gui->startEdit(watchpointEditRange, rangeX,
                        GUI::ROW3 + (watchpointSelection * GUI::ROW_HEIGHT), 0,
                        4, false, GUI::ET_HEX);
        } else {
          gui->endEdit(true);
          watchpointEditRange = (value == 0) ? 1 : value;
          watchpointEditField = 2;
        }
      }
    } else if (watchpointEditField == 2) {
      // Type field - use arrow keys to cycle through R, W, RW
      switch (windowEvent.key.keysym.sym) {
      case SDLK_LEFT:
      case SDLK_RIGHT:
        // Cycle through R -> W -> RW -> R...
        if (watchpointEditOnRead && watchpointEditOnWrite) {
          // RW -> R
          watchpointEditOnRead = true;
          watchpointEditOnWrite = false;
        } else if (watchpointEditOnRead && !watchpointEditOnWrite) {
          // R -> W
          watchpointEditOnRead = false;
          watchpointEditOnWrite = true;
        } else {
          // W -> RW
          watchpointEditOnRead = true;
          watchpointEditOnWrite = true;
        }
        break;

      case SDLK_RETURN:
        // Confirm - commit the watchpoint
        {
          if (watchpointAddMode) {
            // Adding new watchpoint
            int newIndex = debugManager->addWatchpoint(
                watchpointEditAddress, watchpointEditRange,
                watchpointEditIsPhysical, watchpointEditOnRead,
                watchpointEditOnWrite);
            if (newIndex >= 0) {
              watchpointSelection = newIndex;
            }
          } else {
            // Editing existing - remove old and add new with same enabled state
            const Watchpoint *oldWp =
                debugManager->getWatchpoint(watchpointSelection);
            bool wasEnabled = oldWp ? oldWp->enabled : true;
            debugManager->removeWatchpoint(watchpointSelection);
            int newIndex = debugManager->addWatchpoint(
                watchpointEditAddress, watchpointEditRange,
                watchpointEditIsPhysical, watchpointEditOnRead,
                watchpointEditOnWrite);
            if (newIndex >= 0) {
              if (!wasEnabled) {
                debugManager->setWatchpointEnabled(newIndex, false);
              }
              watchpointSelection = newIndex;
            }
          }
        }
        watchpointEditMode = false;
        watchpointEditField = 0;
        watchpointAddMode = false;
        break;

      case SDLK_ESCAPE:
        // Cancel without saving
        watchpointEditMode = false;
        watchpointEditField = 0;
        watchpointAddMode = false;
        break;
      }
    }
    return mode;
  }

  switch (windowEvent.key.keysym.sym) {
  case SDLK_UP:
    if (watchpointSelection > 0) {
      watchpointSelection--;
    } else {
      watchpointSelection = 7; // Wrap to bottom
    }
    break;

  case SDLK_DOWN:
    if (watchpointSelection < 7) {
      watchpointSelection++;
    } else {
      watchpointSelection = 0; // Wrap to top
    }
    break;

  case SDLK_a:
    // Add new watchpoint if list not full
    if (wpCount < 8) {
      watchpointAddMode = true;
      watchpointEditMode = true;
      watchpointEditField = 0;
      watchpointEditAddress = 0;
      watchpointEditRange = 1;
      watchpointEditOnRead = false;
      watchpointEditOnWrite = true;     // Default to write-only
      watchpointEditIsPhysical = false; // Default to logical
      watchpointSelection = wpCount;    // Select the slot where new one will go
      // Start editing address field - default to logical (don't care)
      gui->startAddressEdit(0, false, GUI::COL1 + 78,
                           GUI::ROW3 + (wpCount * GUI::ROW_HEIGHT), 0);
    }
    break;

  case SDLK_d:
    // Delete selected watchpoint
    if (watchpointSelection < wpCount) {
      debugManager->removeWatchpoint(watchpointSelection);
      wpCount = debugManager->getWatchpointCount();
      // Move selection to next valid item, or previous if deleted last
      if (watchpointSelection >= wpCount && wpCount > 0) {
        watchpointSelection = wpCount - 1;
      } else if (wpCount == 0) {
        watchpointSelection = 0;
      }
    }
    break;

  case SDLK_SPACE:
    // Toggle enable/disable on selected populated slot
    if (watchpointSelection < wpCount) {
      const Watchpoint *wp = debugManager->getWatchpoint(watchpointSelection);
      if (wp) {
        debugManager->setWatchpointEnabled(watchpointSelection, !wp->enabled);
      }
    }
    break;

  case SDLK_RETURN:
    // Edit selected watchpoint
    if (watchpointSelection < wpCount) {
      const Watchpoint *wp = debugManager->getWatchpoint(watchpointSelection);
      if (wp) {
        watchpointAddMode = false;
        watchpointEditMode = true;
        watchpointEditField = 0;
        watchpointEditAddress = wp->address;
        watchpointEditRange = wp->length;
        watchpointEditOnRead = wp->onRead;
        watchpointEditOnWrite = wp->onWrite;
        watchpointEditIsPhysical = wp->isPhysical;
        // Start address edit with current physical/logical state
        gui->startAddressEdit(
            wp->address, wp->isPhysical, GUI::COL1 + 78,
            GUI::ROW3 + (watchpointSelection * GUI::ROW_HEIGHT), 0);
      }
    }
    break;

  case SDLK_b:
    // Switch to Breakpoints screen
    mode = GUI::BREAKPOINTS;
    resetMode();
    break;
  case SDLK_g:
    // Switch to TraceLog screen
    mode = GUI::TRACELOG;
    resetMode();
    break;
  case SDLK_ESCAPE:
    mode = GUI::DEBUG;
    break;
  }
  return mode;
}


void BreakpointGui::drawTraceLog() {
  boxRGBA(sdlRenderer, 32 * zoom, 32 * zoom, (screenWidth - 24) * zoom,
          (screenHeight - 24) * zoom, 0xF0, 0xF0, 0xE0, 0xE8);

  SDL_Color textColor = {0, 0x30, 0x30, 255};
  SDL_Color menuColor = {0x30, 0x30, 0xA0, 255};
  SDL_Color bright = {0xD0, 0xFF, 0xD0, 255};

  // Title and navigation hint
  gui->print(GUI::COL1, 34, menuColor, "TRACELOG");
  gui->print(GUI::COL4, 34, menuColor, "[B]reakpoints");
  gui->print(GUI::COL5, 34, menuColor, "[W]atchpoints");

  std::deque<TraceLog>* logs = debugManager->getTraceLogs();
  for (size_t i=logStart; i<logStart+LOG_LIST_SIZE; i++) {
    if (i<logs->size()) {
        int row = GUI::ROW3 + (i * GUI::ROW_HEIGHT);
        TraceLog log = logs->at(i);

        gui->print(GUI::COL1, row, textColor, 0, bright, "0x%05X", log.physicalAddress);

        if (!log.breakpoint->name.empty()) {
            gui->print(GUI::COL3, row, textColor, 0, bright, "%12s", log.breakpoint->name.c_str());
        }
    }
  }
  gui->print(GUI::COL5, GUI::END_ROW, menuColor, "[ESC]:Exit");
}

GUI::Mode BreakpointGui::traceLogMenu(SDL_Event windowEvent, GUI::Mode mode) {

    switch (windowEvent.key.keysym.sym)
    {
    case SDLK_ESCAPE:
        mode = GUI::DEBUG;
        break;

    case SDLK_b:
        mode = GUI::BREAKPOINTS;
        resetMode();
        break;

    case SDLK_w:
        mode = GUI::WATCHPOINTS;
        resetMode();
        break;
    }
    return mode;
}
# Story 2.3: Breakpoint & Watchpoint Indicators

Status: done

## Story

As a developer debugging Z80 code,
I want to see visual indicators for all breakpoints in the disassembly listing and the triggering line for watchpoints,
So that I can immediately see where breakpoints are set and understand why the debugger stopped.

## Acceptance Criteria

1. **Given** the DEBUG screen is displayed
   **When** there are enabled breakpoints set
   **Then** each breakpoint address visible in the listing shows a solid red circle with its BP number (1-8)

2. **Given** the DEBUG screen is displayed
   **When** there are disabled breakpoints set
   **Then** each disabled breakpoint address visible in the listing shows a semi-transparent red circle with its BP number (1-8)

3. **Given** the emulator stops due to a watchpoint trigger
   **When** the DEBUG screen is displayed
   **Then** an orange circle with the WP number (1-8) appears next to the instruction that performed the memory access

4. **Given** multiple breakpoints exist on addresses visible in the listing
   **When** the listing scrolls or the PC changes
   **Then** breakpoint indicators update to reflect which BP addresses are currently visible

5. **Given** a line has a breakpoint AND is the current PC
   **When** the DEBUG screen is displayed
   **Then** both the breakpoint indicator and PC highlighting are shown

6. **Given** the emulator is in RUN mode
   **When** the display updates
   **Then** no BP/WP indicators are drawn (indicators only appear in DEBUG mode)

## Tasks / Subtasks

- [x] Task 1: Modify DebugManager to return trigger index (AC: #3)
  - [x] 1.1 Change `checkBreakpoint()` return type from `bool` to `int` (-1 = no hit, 0-7 = index)
  - [x] 1.2 Change `checkWatchpoint()` return type from `bool` to `int` (-1 = no hit, 0-7 = index)
  - [x] 1.3 Update all call sites to handle new return type
  - [x] 1.4 Update tests for new return type semantics

- [x] Task 2: Add DebugManager method to find breakpoint at address (AC: #1, #2, #4)
  - [x] 2.1 Add `getBreakpointAtAddress(uint16_t addr)` method returning breakpoint info or nullptr
  - [x] 2.2 Method should return index and enabled state for any BP matching the address
  - [x] 2.3 Add unit tests for new method

- [x] Task 3: Add watchpoint trigger tracking to Beast class (AC: #3)
  - [x] 3.1 Add `StopReason` enum: `STOP_NONE`, `STOP_STEP`, `STOP_BREAKPOINT`, `STOP_WATCHPOINT`, `STOP_ESCAPE`
  - [x] 3.2 Add `stopReason` member variable to Beast class
  - [x] 3.3 Add `watchpointTriggerAddress` member variable (address of instruction that caused WP trigger)
  - [x] 3.4 Add `watchpointTriggerIndex` member variable (0-7)
  - [x] 3.5 Set `stopReason = STOP_WATCHPOINT`, capture triggering instruction address and index when WP fires
  - [x] 3.6 Clear watchpoint trigger info when entering RUN mode or stepping

- [x] Task 4: Draw breakpoint indicators in listing (AC: #1, #2, #4, #5, #6)
  - [x] 4.1 In `drawListing()`, for each line, query DebugManager for any BP at that address
  - [x] 4.2 If enabled BP found: draw solid red circle with BP number (1-8)
  - [x] 4.3 If disabled BP found: draw semi-transparent red circle with BP number (1-8)
  - [x] 4.4 Use `filledCircleRGBA()` from SDL2_gfx for the circles
  - [x] 4.5 Ensure indicators only drawn when in DEBUG mode

- [x] Task 5: Draw watchpoint trigger indicator in listing (AC: #3, #6)
  - [x] 5.1 In `drawListing()`, check if `stopReason == STOP_WATCHPOINT`
  - [x] 5.2 If watchpoint triggered and triggering address is visible, draw orange circle with WP number (1-8)
  - [x] 5.3 Ensure indicator only drawn when in DEBUG mode

## Dev Notes

### Architecture Compliance

**File Changes Required:**
- MODIFY: `src/debugmanager.hpp` - Change return types, add getBreakpointAtAddress()
- MODIFY: `src/debugmanager.cpp` - Implement index-returning check methods and address lookup
- MODIFY: `src/beast.hpp` - Add StopReason enum, watchpoint trigger tracking members
- MODIFY: `src/beast.cpp` - Track watchpoint triggers, draw indicators in listing
- MODIFY: `tests/test_debugmanager.cpp` - Update tests for new return types and methods

### Technical Specifications

**DebugManager API Changes:**
```cpp
// Before:
bool checkBreakpoint(uint16_t pc, uint8_t* memoryPage) const;
bool checkWatchpoint(uint16_t logicalAddress, uint32_t physicalAddress, bool isRead) const;

// After:
int checkBreakpoint(uint16_t pc, uint8_t* memoryPage) const;  // -1 or 0-7
int checkWatchpoint(uint16_t logicalAddress, uint32_t physicalAddress, bool isRead) const;  // -1 or 0-7

// New method for listing display:
struct BreakpointInfo {
    int index;      // 0-7
    bool enabled;
};
std::optional<BreakpointInfo> getBreakpointAtAddress(uint16_t addr) const;
```

**Beast Class Additions:**
```cpp
enum StopReason { STOP_NONE, STOP_STEP, STOP_BREAKPOINT, STOP_WATCHPOINT, STOP_ESCAPE };
StopReason stopReason = STOP_NONE;
uint16_t watchpointTriggerAddress = 0;  // Address of instruction that caused WP trigger
int watchpointTriggerIndex = -1;        // Which WP (0-7) was triggered
```

**Visual Specifications:**
- Enabled breakpoint: RGB(220, 50, 50, 255) - solid red
- Disabled breakpoint: RGB(220, 50, 50, 100) - semi-transparent red
- Watchpoint trigger: RGB(255, 160, 0, 255) - orange
- Circle radius: 5-6 pixels
- Position: Left margin of listing, before the address
- Number font: existing small font, white or contrasting color
- Numbers displayed as 1-8 (user-facing), stored internally as 0-7

**Breakpoint Indicator Drawing (in drawListing):**
```cpp
// For each line in listing:
for (int i = 0; i < numLines; i++) {
    uint16_t lineAddr = listingAddresses[i];

    // Check for breakpoint at this address
    auto bpInfo = debugManager->getBreakpointAtAddress(lineAddr);
    if (bpInfo) {
        int circleX = (GUI::COL1 - 12) * zoom;
        int circleY = (rowY + 7) * zoom;
        uint8_t alpha = bpInfo->enabled ? 255 : 100;
        filledCircleRGBA(sdlRenderer, circleX, circleY, 6*zoom, 220, 50, 50, alpha);
        // Draw number bpInfo->index + 1 (user-facing 1-8)
    }

    // Draw rest of line...
}
```

**Watchpoint Trigger Indicator Drawing:**
```cpp
// After drawing all listing lines, if watchpoint triggered:
if (stopReason == STOP_WATCHPOINT && watchpointTriggerIndex >= 0) {
    // Find if trigger address is visible in current listing
    for (int i = 0; i < numLines; i++) {
        if (listingAddresses[i] == watchpointTriggerAddress) {
            int circleX = (GUI::COL1 - 12) * zoom;
            int circleY = (rowY_for_line_i + 7) * zoom;
            filledCircleRGBA(sdlRenderer, circleX, circleY, 6*zoom, 255, 160, 0, 255);
            // Draw number watchpointTriggerIndex + 1 (user-facing 1-8)
            break;
        }
    }
}
```

**Watchpoint Trigger Capture (in run loop):**
```cpp
// When watchpoint triggers, capture the PC of the instruction that caused it:
int wpIndex = debugManager->checkWatchpoint(addr, mappedAddr, isRead);
if (wpIndex >= 0) {
    stopReason = STOP_WATCHPOINT;
    watchpointTriggerAddress = cpu.pc - instructionLength;  // Address of triggering instruction
    watchpointTriggerIndex = wpIndex;
    mode = DEBUG;
}
```

### References

- [Source: src/beast.cpp] drawListing() implementation with BP/WP indicators
- [Source: src/beast.cpp] Watchpoint and breakpoint check in run loop
- [Source: src/include/SDL2_gfxPrimitives.h] filledCircleRGBA declaration
- [Source: src/debugmanager.hpp] DebugManager API including getBreakpointAtAddress() overloads

## Dev Agent Record

### Agent Model Used
Claude Opus 4.5

### Debug Log References
None - implementation proceeded without issues.

### Completion Notes List
1. Task 1: Changed `checkBreakpoint()` and `checkWatchpoint()` return types from `bool` to `int` (-1 = no hit, 0-7 = index). Updated all call sites to use `>= 0` check.
2. Task 2: Added `BreakpointInfo` struct and `getBreakpointAtAddress()` method for display purposes.
3. Task 3: Added `StopReason` enum and tracking variables (`stopReason`, `watchpointTriggerAddress`, `watchpointTriggerIndex`) to Beast class. Set appropriately when watchpoint/breakpoint triggers or user steps/escapes.
4. Task 4-5: Modified `drawListing()` to track line addresses and draw colored circle indicators for breakpoints (red, enabled=solid/disabled=semi-transparent) and watchpoint triggers (orange). Indicators only drawn in DEBUG mode.
5. Visual refinements: Added dedicated 12pt `indicatorFont` for numbers. Positioned circles at COL1-9, rowY+10 with 6px radius. Numbers centered within circles using SDL_ttf. Indicators only shown on lines with actual opcodes (byteCount > 0).
6. Code review fixes: Added `breakpointTriggerIndex` tracking, `currentInstructionPC` for accurate WP trigger address, extended `getBreakpointAtAddress()` to support physical breakpoints, added color constants, fixed overlapping BP/WP indicators, freed indicatorFont in destructor.

### File List
- src/debugmanager.hpp (MODIFIED) - Added BreakpointInfo struct, changed checkBreakpoint/checkWatchpoint return types to int, added getBreakpointAtAddress() overloads for logical and physical BP support
- src/debugmanager.cpp (MODIFIED) - Implemented index-returning check methods and both getBreakpointAtAddress() overloads
- src/beast.hpp (MODIFIED) - Added StopReason enum, tracking member variables (including breakpointTriggerIndex, currentInstructionPC), indicatorFont, and indicator color constants
- src/beast.cpp (MODIFIED) - Track stop reasons with trigger indices, accurate instruction PC tracking, draw BP/WP indicators with overlap handling, free indicatorFont in destructor
- tests/test_debugmanager.cpp (MODIFIED) - Updated existing tests for new return types, added tests for getBreakpointAtAddress with physical breakpoints

### Change Log
- 2026-01-31: Implemented story 2-3 stop reason indicator feature
- 2026-01-31: Refined indicator visuals - 12pt font, 6px radius circles, proper centering and positioning, only show on lines with opcodes
- 2026-01-31: Code review fixes - Added breakpointTriggerIndex, physical BP display support, accurate WP trigger address, color constants, overlap handling, memory leak fix


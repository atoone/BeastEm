# Story 2.2: Watchpoints UI Screen

Status: done

## Story

As a developer debugging Z80 code,
I want a dedicated screen to manage watchpoints with address, range, and trigger type,
So that I can monitor specific memory regions for unexpected access.

## Acceptance Criteria

1. **Given** the Breakpoints screen
   **When** I press 'W'
   **Then** the Watchpoints screen appears with its title and key hints

2. **Given** the Watchpoints screen
   **When** I press 'B'
   **Then** I return to the Breakpoints screen

3. **Given** the Watchpoints screen with empty list
   **When** I press 'A', type "8000", Tab, type "20", Tab, select "W", Enter
   **Then** a watchpoint at $8000, range $20, write-only appears enabled

4. **Given** adding a watchpoint and pressing 'A'
   **When** I type address and press Enter without changing range/type
   **Then** watchpoint is created with defaults (range $01, type W)

5. **Given** a watchpoint list with items
   **When** I navigate and press Space on a selected item
   **Then** it toggles enabled/disabled with visual feedback (bright/dim)

6. **Given** a selected watchpoint
   **When** I press Enter
   **Then** I can edit address, range, and type fields with Tab navigation

7. **Given** a selected watchpoint
   **When** I press 'D'
   **Then** the watchpoint is removed and selection moves to next item

8. **Given** the Watchpoints screen
   **When** I press ESC
   **Then** I return to DEBUG mode with watchpoints preserved

## Tasks / Subtasks

- [x] Task 1: Add WATCHPOINTS mode to Beast class (AC: #1, #2, #8)
  - [x] 1.1 Add `WATCHPOINTS` to the `Mode` enum in `beast.hpp` after `BREAKPOINTS`
  - [x] 1.2 Add watchpoints screen selection enum values: `SEL_WP_LIST`, `SEL_WP_ADDRESS`, `SEL_WP_RANGE`, `SEL_WP_TYPE` in Selection enum
  - [x] 1.3 Add `watchpointSelection` member variable to track selected slot (0-7)
  - [x] 1.4 Add `watchpointEditMode` bool to track add/edit state
  - [x] 1.5 Add `watchpointEditField` int to track current field (0=address, 1=range, 2=type)
  - [x] 1.6 Handle 'W' key in BREAKPOINTS mode `breakpointsMenu()` to enter `mode = WATCHPOINTS`
  - [x] 1.7 Handle 'W' key in DEBUG mode `debugMenu()` to enter `mode = WATCHPOINTS` directly

- [x] Task 2: Implement watchpoints screen rendering (AC: #1, #5)
  - [x] 2.1 Add `drawWatchpoints()` method declaration to `beast.hpp`
  - [x] 2.2 Implement `drawWatchpoints()` in `beast.cpp` to render screen layout
  - [x] 2.3 Render title "WATCHPOINTS" at top left, "[B]:Breakpoints" hint at top right
  - [x] 2.4 Render column headers: "#", "Address", "Range", "Type", "Enabled"
  - [x] 2.5 Render 8 fixed rows - populated watchpoints or dashes for empty slots
  - [x] 2.6 Render watchpoint type as `R` (read), `W` (write), or `RW` (both)
  - [x] 2.7 Render enabled state as `[*]` (enabled) or `[ ]` (disabled)
  - [x] 2.8 Apply selection highlight to current row (use existing highlight pattern from drawBreakpoints)
  - [x] 2.9 Apply dim color to disabled watchpoints
  - [x] 2.10 Render key hints footer: "A:Add D:Delete Space:Toggle Enter:Edit B:Breakpoints ESC:Exit"
  - [x] 2.11 Call `drawWatchpoints()` from `mainLoop()` when `mode == WATCHPOINTS`

- [x] Task 3: Implement keyboard navigation (AC: #2, #8)
  - [x] 3.1 Add `watchpointsMenu(SDL_Event)` method declaration to `beast.hpp`
  - [x] 3.2 Implement Up/Down arrow handling to change `watchpointSelection` (0-7, wrap)
  - [x] 3.3 Implement ESC key to return to `mode = DEBUG`
  - [x] 3.4 Implement 'B' key to switch to `mode = BREAKPOINTS`
  - [x] 3.5 Wire `watchpointsMenu()` into `mainLoop()` event handling when `mode == WATCHPOINTS`

- [x] Task 4: Implement add watchpoint with multi-field entry (AC: #3, #4)
  - [x] 4.1 Handle 'A' key: if count < 8, enter edit mode for new watchpoint
  - [x] 4.2 Set default values: range = 1, type = W (write-only), enabled = true
  - [x] 4.3 Use `gui.startEdit()` for hex address entry (4-5 digits, logical/physical)
  - [x] 4.4 Handle Tab key to advance to range field
  - [x] 4.5 Use `gui.startEdit()` for hex range entry (2 digits, default "01")
  - [x] 4.6 Handle Tab key to advance to type field
  - [x] 4.7 Implement type cycling with Left/Right arrows: R → W → RW → R...
  - [x] 4.8 On Enter/confirm: call `debugManager->addWatchpoint(address, range, isPhysical, onRead, onWrite)`
  - [x] 4.9 Set selection to newly added watchpoint slot
  - [x] 4.10 If list full (8 items), 'A' key does nothing (update hint to "Full")

- [x] Task 5: Implement toggle enable/disable (AC: #5)
  - [x] 5.1 Handle Space key on selected populated slot
  - [x] 5.2 Call `debugManager->setWatchpointEnabled(index, !currentState)`
  - [x] 5.3 Visual update is immediate (redraw triggers)

- [x] Task 6: Implement edit watchpoint (AC: #6)
  - [x] 6.1 Handle Enter key on selected populated slot to enter edit mode
  - [x] 6.2 Start with address field focused, pre-filled with current value
  - [x] 6.3 Tab advances through address → range → type fields
  - [x] 6.4 On confirm: update watchpoint (remove old, add new with same enabled state)
  - [x] 6.5 On ESC during edit: cancel without changes

- [x] Task 7: Implement delete watchpoint (AC: #7)
  - [x] 7.1 Handle 'D' key on selected populated slot
  - [x] 7.2 Call `debugManager->removeWatchpoint(index)`
  - [x] 7.3 Move selection to next valid item, or previous if deleted last item
  - [x] 7.4 If list becomes empty, selection stays at 0

## Dev Notes

### Architecture Compliance

**File Changes Required (per architecture.md):**
- MODIFY: `src/beast.hpp` - Add WATCHPOINTS mode, selection values, member variables, method declarations
- MODIFY: `src/beast.cpp` - Implement watchpointsMenu(), drawWatchpoints(), wire into mainLoop(), add 'W' key handling
- NO NEW FILES - all changes integrate into existing Beast/GUI infrastructure

**Mode Integration (per architecture.md):**
```cpp
enum Mode {RUN, STEP, OUT, OVER, TAKE, DEBUG, FILES, BREAKPOINTS, WATCHPOINTS, QUIT};
```
- New mode integrates with existing state machine
- Entry: 'W' key from DEBUG mode or BREAKPOINTS mode
- Exit: ESC key returns to DEBUG, 'B' key switches to BREAKPOINTS

**Selection Enum Extension:**
```cpp
// Add after SEL_BP_ADDRESS:
SEL_WP_LIST, SEL_WP_ADDRESS, SEL_WP_RANGE, SEL_WP_TYPE
```

### UI Specifications (per UX Design)

**Screen Layout:**
```
┌─────────────────────────────────────────────────────────────┐
│  WATCHPOINTS                              [B]:Breakpoints   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  #  Address   Range  Type  Enabled                          │
│  ───────────────────────────────────                        │
│  1  $8000     $20    W     [*]      ◄── selected            │
│  2  $7FF0     $10    RW    [*]                              │
│  3  $C000     $01    R     [ ]          (disabled)          │
│  4  ────      ───    ──    ───                              │
│  5  ────      ───    ──    ───                              │
│  6  ────      ───    ──    ───                              │
│  7  ────      ───    ──    ───                              │
│  8  ────      ───    ──    ───                              │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│  A:Add  D:Delete  Space:Toggle  Enter:Edit  B:BP  ESC:Exit  │
└─────────────────────────────────────────────────────────────┘
```

**Keyboard Mapping (per UX spec):**
| Key | Action | Context |
|-----|--------|---------|
| W | Enter WP screen | From DEBUG mode |
| W | Switch to WP screen | From Breakpoints screen |
| B | Switch to BP screen | From Watchpoints screen |
| ↑/↓ | Navigate list | List view (wraps) |
| A | Add new item | List view (if not full) |
| Enter | Edit selected / Confirm | List view / Edit mode |
| Space | Toggle enable/disable | List view (if populated) |
| D | Delete selected | List view (if populated) |
| Tab | Next field | Edit mode (addr→range→type) |
| ←/→ | Cycle type option | Edit mode (type field only) |
| ESC | Exit to DEBUG | Any state |

**Visual Encoding (per UX spec):**
- Enabled items: Normal/bright text color
- Disabled items: Dimmed text color
- Selected item: Background highlight (use existing selection pattern)
- Empty slots: Dashes in very dim color
- Type display: `R` = read, `W` = write, `RW` = both

**Smart Defaults:**
- New watchpoints default to: range=1 byte, type=W (write-only), enabled=true
- This covers the most common use case (catching unexpected memory writes)

### Technical Requirements

**Rendering Integration:**
- Use existing `GUI::print()` for text rendering
- Use existing `GUI::COL1`, `ROW1` etc. layout constants
- Match existing debug view font/color patterns from `gui.hpp`
- Screen renders in same area as debug view
- Copy layout patterns from `drawBreakpoints()` for consistency

**Edit Mode Integration:**
- Reuse `gui.startEdit()` / `gui.handleKey()` / `gui.endEdit()` / `gui.getEditValue()` pattern
- Hex address entry: 4 digits = logical address, 5 digits = physical address
- Hex range entry: 4 digits (0001-FFFF), default "0001"
- Type selection: cycle with ←/→ arrows (no dropdown, just 3 options: R, W, RW)
- Use `GUI::ET_HEX` edit type for address and range fields

**Color Constants (from existing code):**
```cpp
SDL_Color textColor = {220, 220, 220, 255};  // Normal text
SDL_Color dimColor = {100, 100, 100, 255};   // Disabled/dim
SDL_Color highlightBg = {80, 80, 120, 255};  // Selection background
```

### Integration with DebugManager

**DebugManager Watchpoint API (from story 2-1):**
```cpp
int  addWatchpoint(uint32_t address, uint16_t length, bool isPhysical, bool onRead, bool onWrite);
bool removeWatchpoint(int index);
void setWatchpointEnabled(int index, bool enabled);
const Watchpoint* getWatchpoint(int index) const;
int  getWatchpointCount() const;
void clearAllWatchpoints();
int  findWatchpointByStartAddress(uint32_t address, bool isPhysical) const;
```

**Watchpoint Struct (from debugmanager.hpp):**
```cpp
struct Watchpoint {
    uint32_t address;      // 16-bit logical or 20-bit physical
    uint16_t length;       // Number of bytes to monitor
    bool     isPhysical;   // true = physical, false = logical
    bool     enabled;
    bool     onRead;       // Trigger on reads
    bool     onWrite;      // Trigger on writes
};
```

**Address Format Detection:**
- 4 hex digits (0-FFFF): logical address, `isPhysical = false`
- 5 hex digits (0-FFFFF): physical address, `isPhysical = true`
- Address length determines mode automatically

**Type Mapping:**
- `R` → onRead=true, onWrite=false
- `W` → onRead=false, onWrite=true
- `RW` → onRead=true, onWrite=true

### Previous Story Intelligence

**From Story 2-1 (Watchpoint Management Core):**
- DebugManager watchpoint API exists and is fully tested
- `addWatchpoint()` validates parameters (rejects length=0, no trigger type)
- Watchpoint checking already integrated into Beast::run() memory access loop
- Physical address calculation: `physicalAddress = (memoryPage[slot] << 14) | (logicalAddress & 0x3FFF)`
- `findWatchpointByStartAddress()` can be used to check for duplicates

**From Story 1-2 (Breakpoints UI Screen) - CRITICAL PATTERNS TO FOLLOW:**
- `drawBreakpoints()` implementation in beast.cpp - copy structure for `drawWatchpoints()`
- `breakpointsMenu()` event handling pattern - copy for `watchpointsMenu()`
- Selection highlighting with character width calculation
- Edit mode integration using gui.startEdit()/handleKey()/endEdit() pattern
- Address format detection via value > 0xFFFF (physical) vs <= 0xFFFF (logical)
- List navigation with wrapping at boundaries
- Key hint footer format and positioning

**Code Review Fixes from Story 1-2 (apply same fixes):**
- Selection highlight character counts must match actual content width
- Don't check `address != 0` before allowing edit (address 0x0000 is valid)
- Comments should accurately describe edit position calculations

**Files modified in Story 1-2 (reference for patterns):**
- `src/beast.hpp` - Mode enum, Selection enum, member variables, method declarations
- `src/beast.cpp` - breakpointsMenu(), drawBreakpoints(), mainLoop() wiring

### Existing Codebase Patterns

**Mode Handling Pattern (from beast.cpp):**
```cpp
// In mainLoop(), handle different modes:
switch(mode) {
    case DEBUG:
        debugMenu(windowEvent);
        break;
    case FILES:
        fileMenu(windowEvent);
        break;
    case BREAKPOINTS:
        breakpointsMenu(windowEvent);
        break;
    // Add: case WATCHPOINTS: watchpointsMenu(windowEvent); break;
}
```

**Drawing Pattern (from beast.cpp redrawScreen):**
```cpp
if(mode == DEBUG) {
    // Draw debug view...
} else if(mode == FILES) {
    // Draw file menu...
} else if(mode == BREAKPOINTS) {
    drawBreakpoints();
}
// Add: else if(mode == WATCHPOINTS) { drawWatchpoints(); }
```

**Edit Pattern (from beast.cpp breakpointsMenu):**
```cpp
gui.startEdit(value, x, y, offset, digits, false, GUI::ET_HEX);
// In event loop:
if(gui.isEditing()) {
    gui.handleKey(key);
    if(gui.isEditOK()) {
        value = gui.getEditValue();
        gui.endEdit(true);
    }
}
```

**Multi-field Edit Pattern (new for watchpoints):**
```cpp
// Track which field is being edited
int watchpointEditField = 0;  // 0=address, 1=range, 2=type

// On Tab key during edit:
watchpointEditField = (watchpointEditField + 1) % 3;
// Then start edit on new field

// On Enter key:
if (watchpointEditField == 2) {  // Type field
    // Commit the watchpoint
} else {
    // Advance to next field
}
```

### Naming Conventions (per architecture.md)

| Element | Convention | Examples |
|---------|------------|----------|
| Methods | camelCase | `drawWatchpoints()`, `watchpointsMenu()` |
| Member variables | camelCase | `watchpointSelection`, `watchpointEditMode`, `watchpointEditField` |
| Enum values | SCREAMING_SNAKE | `WATCHPOINTS`, `SEL_WP_LIST`, `SEL_WP_ADDRESS`, `SEL_WP_RANGE`, `SEL_WP_TYPE` |

### Column Layout Guidance

**Watchpoint row has more columns than breakpoint row:**
```
Breakpoint: # Address   Enabled
Watchpoint: # Address   Range  Type  Enabled
```

**Suggested column positions (adjust based on actual rendering tests):**
- Column 1 (#): COL1
- Column 2 (Address): COL1 + 30
- Column 3 (Range): COL1 + 130 (after address which can be up to 6 chars + $)
- Column 4 (Type): COL1 + 190
- Column 5 (Enabled): COL1 + 240

**Selection highlight width:** Must span from # column to end of Enabled column for consistent row highlighting.

### Anti-Patterns to Avoid

- Do NOT create a separate class for watchpoints UI - integrate into Beast class
- Do NOT use mouse-based interactions - keyboard only
- Do NOT add confirmation dialogs - trust the user, make undo easy
- Do NOT change the DebugManager watchpoint API - use it as-is from Story 2-1
- Do NOT add "are you sure?" prompts for delete - just delete
- Do NOT forget to handle the empty string case when editing range (treat as "01")
- Do NOT hardcode digit counts - physical addresses need 5 digits, logical need 4
- Do NOT forget to re-validate watchpoint on edit (DebugManager rejects invalid params)

### Testing Guidance

**Manual Testing Checklist:**
1. Press 'W' from DEBUG mode → Watchpoints screen appears
2. Press 'B' from Watchpoints → Breakpoints screen appears
3. Press 'W' from Breakpoints → Watchpoints screen appears
4. Press ESC from Watchpoints → Returns to DEBUG mode
5. Add watchpoint: A → "8000" → Tab → "20" → Tab → (verify W selected) → Enter
6. Verify watchpoint appears with correct values: $8000, $20, W, [*]
7. Press Space → Toggle to [ ] (disabled, dimmed)
8. Press Space → Toggle to [*] (enabled, bright)
9. Press Enter → Edit mode, verify fields can be changed
10. Press D → Delete, verify selection moves to next item
11. Add 8 watchpoints → Verify 'A' key does nothing (list full)
12. Navigate with Up/Down → Verify wrapping at boundaries
13. Run emulation → Verify watchpoint triggers correctly

**Edge Cases to Test:**
- Edit address to 0x0000 (valid, should work)
- Add watchpoint with 5-digit physical address (e.g., 84000)
- Add read-only watchpoint (R) and verify it only triggers on reads
- Add read-write watchpoint (RW) and verify it triggers on both
- Cancel add with ESC mid-entry (no watchpoint should be created)
- Cancel edit with ESC (original values preserved)

### References

- [Source: _bmad-output/planning-artifacts/architecture.md#UI-Architecture]
- [Source: _bmad-output/planning-artifacts/architecture.md#UI-Patterns]
- [Source: _bmad-output/planning-artifacts/ux-design-specification.md#Design-Direction]
- [Source: _bmad-output/planning-artifacts/ux-design-specification.md#User-Journey-Flows]
- [Source: _bmad-output/planning-artifacts/ux-design-specification.md#Component-Strategy]
- [Source: _bmad-output/planning-artifacts/epics.md#Story-2.2]
- [Source: _bmad-output/planning-artifacts/prd.md#Watchpoint-Management]
- [Source: _bmad-output/planning-artifacts/prd.md#Debugging-UI]
- [Source: src/beast.hpp] Mode enum, Selection enum patterns
- [Source: src/beast.cpp] breakpointsMenu(), drawBreakpoints() implementation patterns
- [Source: src/debugmanager.hpp] Watchpoint struct and API
- [Source: src/gui.hpp:16-50] GUI layout constants
- [Source: src/gui.hpp:62-69] Edit methods
- [Source: _bmad-output/implementation-artifacts/1-2-breakpoints-ui-screen.md] Breakpoints UI patterns and code review fixes
- [Source: _bmad-output/implementation-artifacts/2-1-watchpoint-management-core.md] Watchpoint API and integration notes

## Dev Agent Record

### Agent Model Used

Claude Opus 4.5 (claude-opus-4-5-20251101)

### Debug Log References

- Build successful with no errors
- All 34 DebugManager tests pass (14 breakpoint + 20 watchpoint tests)

### Completion Notes List

- Implemented WATCHPOINTS mode in Beast class with full state machine integration
- Added watchpoints screen rendering following breakpoints screen patterns
- Implemented multi-field edit flow using temporary storage variables for address/range/type
- Type field cycling (R -> W -> RW) via left/right arrow keys
- Add mode creates watchpoint only after full field entry and Enter confirmation
- Edit mode preserves enabled state when modifying existing watchpoint
- Navigation wraps at boundaries (0-7)
- 'W' key navigates to Watchpoints from both DEBUG and BREAKPOINTS modes
- 'B' key returns to Breakpoints screen, ESC returns to DEBUG mode
- Range field supports 4 hex digits (0x0000-0xFFFF) for full 16-bit range
- Removed redundant "B:BP" hint from footer (already shown at top as "[B]:Breakpoints")

### File List

- src/beast.hpp (modified) - Added WATCHPOINTS mode, selection enums, member variables, method declarations
- src/beast.cpp (modified) - Implemented drawWatchpoints(), watchpointsMenu(), wired into mainLoop()

### Change Log

- 2026-01-30: Implemented Story 2.2 Watchpoints UI Screen - all 7 tasks completed
- 2026-01-31: Code review completed - fixed 3 issues:
  - Removed unused `isPhys` variables in watchpointsMenu (lines 855, 875)
  - Added missing `watchpointAddMode = false` reset on ESC during address/range edit
  - Updated documentation: range field is 4 digits not 2


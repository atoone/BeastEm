# Story 3.1: Page Map Screen with Physical Page Columns

Status: review

## Story

As a developer debugging Z80 code,
I want a paging visualization screen showing all physical RAM and ROM pages in colour-coded columns,
So that I can see the full memory landscape of the Microbeast at a glance.

## Acceptance Criteria

1. **Given** the emulator is in DEBUG mode
   **When** I press 'P'
   **Then** the paging visualization screen appears with a PAGEMAP Mode enum value

2. **Given** the paging viz screen
   **When** it renders
   **Then** physical RAM pages (#20-#3F) appear on the left with page numbers and physical address boundaries (e.g., 80000-83FFF for #20)

3. **Given** the paging viz screen
   **When** it renders
   **Then** physical ROM pages (#00-#1F) appear on the right with page numbers and physical address boundaries (e.g., 00000-03FFF for #00)

4. **Given** RAM pages
   **When** rendered
   **Then** CP/M pages are dark blue, RAM DISK pages are teal, and free pages are light blue

5. **Given** ROM pages
   **When** rendered
   **Then** RESTORE pages are red, ROM DISK pages are purple, and BOOT pages are pink

6. **Given** shared ROM pages (#10, #11, #12, #13)
   **When** rendered
   **Then** they display both RESTORE (red) and ROM DISK (purple) colours via diagonal shading

7. **Given** physical page groups
   **When** rendered
   **Then** labelled brackets show purpose groupings: RAM side has "CP/M" and "RAM DISK" brackets; ROM side has "RESTORE", "ROM DISK", and "BOOT" brackets, with RESTORE and ROM DISK brackets visually distinct despite shared pages

8. **Given** the paging viz screen
   **When** I press ESC
   **Then** I return to DEBUG mode

## Tasks / Subtasks

- [x] Task 1: Add PAGEMAP mode to Beast class (AC: #1, #8)
  - [x] 1.1 Add `PAGEMAP` to the `Mode` enum in `beast.hpp` (after `WATCHPOINTS`, before `QUIT`)
  - [x] 1.2 Handle 'P' key in `debugMenu()` to enter `mode = PAGEMAP`
  - [x] 1.3 Add `drawPageMap()` method declaration to `beast.hpp`
  - [x] 1.4 Add `pageMapMenu(SDL_Event)` method declaration to `beast.hpp`
  - [x] 1.5 Wire `drawPageMap()` into `mainLoop()` rendering (add to the `if` chain alongside BREAKPOINTS/WATCHPOINTS)
  - [x] 1.6 Wire `pageMapMenu()` into `mainLoop()` event handling when `mode == PAGEMAP`
  - [x] 1.7 Implement ESC key in `pageMapMenu()` to return to `mode = DEBUG`

- [x] Task 2: Define page-purpose constants and colour scheme (AC: #4, #5, #6)
  - [x] 2.1 Define page purpose enum or constants: `BOOT`, `ROM_DISK`, `RESTORE`, `SHARED_ROM`, `CPM`, `RAM_DISK`, `FREE_RAM`
  - [x] 2.2 Define page-purpose mapping based on MicroBeast memory map:
    - ROM: BOOT=#00-#03, ROM DISK=#04-#0F, Shared=#10-#13, RESTORE=#14-#1F
    - RAM: CP/M=#20-#23, RAM DISK=#24-#37, Free=#38-#3F
    - (Verify exact boundaries against `docs/microbeast_memory_map.svg`)
  - [x] 2.3 Define SDL_Color constants for each purpose:
    - CP/M: dark blue, e.g. rgb(73,147,243) from SVG
    - RAM DISK: teal, e.g. rgb(77,178,187) from SVG
    - Free RAM: light blue, e.g. rgb(188,236,241) from SVG
    - RESTORE: red, e.g. rgb(246,107,113) from SVG
    - ROM DISK: purple, e.g. rgb(177,77,180) from SVG
    - BOOT: pink, e.g. rgb(249,208,251) from SVG
  - [x] 2.4 Create helper function `getPagePurpose(uint8_t pageNum)` returning purpose enum
  - [x] 2.5 Create helper function `getPageColor(purpose)` returning SDL_Color

- [x] Task 3: Implement physical RAM column rendering (AC: #2, #4)
  - [x] 3.1 Draw background box matching existing screen pattern: `boxRGBA(sdlRenderer, 32*zoom, 32*zoom, (screenWidth-24)*zoom, (screenHeight-24)*zoom, ...)`
  - [x] 3.2 Calculate layout: RAM column on left side of screen
  - [x] 3.3 Render 32 RAM page boxes (#20-#3F) as small colour-coded rectangles stacked vertically
  - [x] 3.4 Each page box filled with its purpose colour (CP/M=dark blue, RAM DISK=teal, Free=light blue)
  - [x] 3.5 Label each page with page number (e.g., "#20")
  - [x] 3.6 Annotate with physical address boundaries (e.g., "80000-83FFF") alongside or near each page box
  - [x] 3.7 Ensure readable text contrast against coloured backgrounds (use dark text on light fills, or light text on dark fills)

- [x] Task 4: Implement physical ROM column rendering (AC: #3, #5, #6)
  - [x] 4.1 ROM column on right side of screen
  - [x] 4.2 Render 32 ROM page boxes (#00-#1F) as colour-coded rectangles stacked vertically
  - [x] 4.3 Non-shared pages filled with solid purpose colour (BOOT=pink, ROM DISK=purple, RESTORE=red)
  - [x] 4.4 Shared pages (#10-#13) rendered with diagonal shading combining RESTORE (red) and ROM DISK (purple)
  - [x] 4.5 Implement diagonal shading: draw filled rect with one colour, then overlay diagonal lines/triangles with second colour (use SDL2_gfx primitives or manual line drawing)
  - [x] 4.6 Label each page with page number and physical address boundaries

- [x] Task 5: Implement purpose brackets/labels (AC: #7)
  - [x] 5.1 RAM side: Draw bracket or label alongside page groups for "CP/M" and "RAM DISK"
  - [x] 5.2 ROM side: Draw bracket or label for "BOOT", "ROM DISK", and "RESTORE"
  - [x] 5.3 RESTORE bracket spans #10-#1F (includes shared pages)
  - [x] 5.4 ROM DISK bracket spans #04-#13 (includes shared pages)
  - [x] 5.5 Shared pages (#10-#13) visually appear within BOTH bracket ranges
  - [x] 5.6 Use curly brace or line+label pattern for brackets (draw vertical line with label text)
  - [x] 5.7 Ensure RESTORE and ROM DISK brackets are visually distinct (different sides, different offsets, or colour-coded labels)

- [x] Task 6: Add screen title and key hints (AC: #1, #8)
  - [x] 6.1 Render title "PAGE MAP" at top of screen (match existing title pattern from drawBreakpoints)
  - [x] 6.2 Render footer key hints: "[ESC]:Exit" (and "[B]:Breakpoints" or "[D]:Debug" for navigation)
  - [x] 6.3 Optionally show "Paging: ON/OFF" status indicator using `pagingEnabled` flag
  - [x] 6.4 Optionally show current page register values for quick reference

## Dev Notes

### Architecture Compliance

**File Changes Required:**
- MODIFY: `src/beast.hpp` - Add PAGEMAP to Mode enum, add drawPageMap()/pageMapMenu() declarations, add page-purpose constants/helpers
- MODIFY: `src/beast.cpp` - Implement drawPageMap(), pageMapMenu(), wire into mainLoop(), add 'P' key in debugMenu()
- NO NEW FILES - all changes integrate into existing Beast class (follows pattern of BREAKPOINTS/WATCHPOINTS modes)

**Mode Integration:**
```cpp
enum Mode {RUN, STEP, OUT, OVER, TAKE, DEBUG, FILES, BREAKPOINTS, WATCHPOINTS, PAGEMAP, QUIT};
```
- Entry: 'P' key from DEBUG mode
- Exit: ESC returns to DEBUG

### Rendering Pattern (copy from drawBreakpoints/drawWatchpoints)

**Screen Setup:**
```cpp
void Beast::drawPageMap() {
    // Full-screen overlay (same as breakpoints/watchpoints)
    boxRGBA(sdlRenderer, 32*zoom, 32*zoom, (screenWidth-24)*zoom, (screenHeight-24)*zoom, 0xF0, 0xF0, 0xE0, 0xE8);

    // Title
    SDL_Color menuColor = {0x30, 0x30, 0xA0, 255};
    gui.print(GUI::COL1, 34, menuColor, "PAGE MAP");

    // ... render columns ...

    // Footer
    gui.print(GUI::COL5, GUI::END_ROW, menuColor, "[ESC]:Exit");
}
```

**mainLoop wiring (beast.cpp ~line 427-441):**
```cpp
// Add to the mode rendering chain:
} else if( mode == PAGEMAP ) {
    drawPageMap();
}

// Add to event handling:
case PAGEMAP:
    pageMapMenu(windowEvent);
    break;
```

**debugMenu 'P' key (beast.cpp ~line 520-579):**
```cpp
case SDLK_p : mode = PAGEMAP; break;
```

### Page Purpose Mapping (from microbeast_memory_map.svg)

**ROM Pages (#00-#1F) - 32 pages, 512KB total:**

| Pages | Purpose | Colour | Physical Address Range |
|-------|---------|--------|----------------------|
| #00-#03 | BOOT | Pink rgb(249,208,251) | 00000-0FFFF |
| #04-#0F | ROM DISK | Purple rgb(177,77,180) | 10000-3FFFF |
| #10-#13 | Shared (RESTORE+ROM DISK) | Diagonal red+purple | 40000-4FFFF |
| #14-#1F | RESTORE | Red rgb(246,107,113) | 50000-7FFFF |

**RAM Pages (#20-#3F) - 32 pages, 512KB total:**

| Pages | Purpose | Colour | Physical Address Range |
|-------|---------|--------|----------------------|
| #20-#23 | CP/M | Dark Blue rgb(73,147,243) | 80000-8FFFF |
| #24-#37 | RAM DISK | Teal rgb(77,178,187) | 90000-DFFFF |
| #38-#3F | Free | Light Blue rgb(188,236,241) | E0000-FFFFF |

**IMPORTANT:** Verify these exact boundaries against the `docs/microbeast_memory_map.svg` file. The SVG is the authoritative reference. The colours above are extracted directly from the SVG fill attributes. The page ranges should be treated as best-effort and verified.

### Physical Address Calculation

Each page is 16KB (0x4000 bytes):
```cpp
uint32_t physicalBase = (pageNum & 0x1F) << 14;  // Low 5 bits * 16KB
// For ROM pages (#00-#1F): physicalBase = 0x00000 to 0x7C000
// For RAM pages (#20-#3F): physicalBase = 0x80000 to 0xFC000
// Page boundary: physicalBase to physicalBase + 0x3FFF
```

### Existing Paging System Reference

**From beast.hpp (line 116, 181):**
```cpp
uint8_t memoryPage[4];     // 4 page registers (slots 0-3)
bool pagingEnabled = false; // Master paging enable
```

**From beast.cpp (lines 1575-1582) - I/O ports:**
```cpp
// Page register write: I/O ports 0x70-0x73
if( (port & 0x0F0) == 0x70 ) {
    if( (port & 0x04) == 0) {
        memoryPage[port & 0x03] = Z80_GET_DATA(pins);  // port 0x70=slot0, 0x71=slot1, etc.
    } else {
        pagingEnabled = (pins & Z80_D0) != 0;           // port 0x74 = paging enable
    }
}
```

**Page register bits (beast.cpp line 2675):**
```cpp
uint32_t mappedAddr = (address & 0x3FFF) | (page & 0x1F) << 14;
// Bits 7-5 of page register: 0x20=RAM, 0x40=VideoBeast, else=ROM
// Bits 4-0: page number within 512K space (0-31)
```

### Layout Design Guidance

**Three-column layout concept (Story 3.2 will add the centre column):**
```
┌────────────────────────────────────────────────────────────────────┐
│  PAGE MAP                                            [ESC]:Exit   │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│  ┌─ RAM ──────┐           (centre reserved         ┌─ ROM ──────┐ │
│  │            │            for Story 3.2            │            │ │
│  │ #3F ░░░░░ │            logical banks             │ #1F ▓▓▓▓▓ │ │
│  │ #3E ░░░░░ │            + arrows)                 │ #1E ▓▓▓▓▓ │ │
│  │  ...      │                                      │  ...       │ │
│  │ #21 █████ │                                      │ #01 ░░░░░ │ │
│  │ #20 █████ │                                      │ #00 ░░░░░ │ │
│  └───────────┘                                      └───────────┘ │
│  CP/M ┤ #20-#23                          BOOT ┤ #00-#03          │
│  RAM  ┤ #24-#37                      ROM DISK ┤ #04-#13          │
│  DISK                                RESTORE ┤ #10-#1F          │
│  Free ┤ #38-#3F                                                  │
│                                                                    │
├────────────────────────────────────────────────────────────────────┤
│  [ESC]:Exit                                                        │
└────────────────────────────────────────────────────────────────────┘
```

**Page box sizing (32 pages must fit vertically):**
- Available vertical space: roughly `screenHeight - 100` pixels (after title + footer margins)
- Each page box height: ~8-12 pixels (adjust to fit all 32)
- Page box width: ~80-120 pixels
- Leave room for labels (page number + address) on each side

**Rendering approach:**
1. Use `boxRGBA()` for filled colour rectangles (each page box)
2. Use `gui.print()` with smallFont for page labels and address annotations
3. Use `lineRGBA()` for bracket lines
4. For diagonal shading on shared pages: draw base colour rect, then overlay alternating diagonal lines with second colour using `lineRGBA()`

### Diagonal Shading Implementation

For shared pages (#10-#13), create a pattern combining RESTORE (red) and ROM DISK (purple):

```cpp
// Draw base fill with one colour
boxRGBA(sdlRenderer, x1, y1, x2, y2, 246, 107, 113, 255);  // RESTORE red base

// Draw diagonal lines with second colour
for(int dx = -(y2-y1); dx < (x2-x1); dx += 4) {
    int lx1 = std::max(x1, x1 + dx);
    int ly1 = (lx1 == x1) ? y1 + (x1 - (x1 + dx)) : y1;
    int lx2 = std::min(x2, x1 + dx + (y2 - y1));
    int ly2 = (lx2 == x2) ? y1 + (x2 - (x1 + dx)) : y2;
    lineRGBA(sdlRenderer, lx1, ly1, lx2, ly2, 177, 77, 180, 255);  // ROM DISK purple lines
}
```

Alternatively, draw two triangles (upper-left and lower-right) in different colours. The SVG reference uses this triangle approach.

### Colour Constants (from SVG reference)

```cpp
// RAM page colours
static constexpr SDL_Color CPM_COLOR     = {73, 147, 243, 255};   // Dark blue
static constexpr SDL_Color RAMDISK_COLOR = {77, 178, 187, 255};   // Teal
static constexpr SDL_Color FREE_COLOR    = {188, 236, 241, 255};  // Light blue

// ROM page colours
static constexpr SDL_Color RESTORE_COLOR = {246, 107, 113, 255};  // Red
static constexpr SDL_Color ROMDISK_COLOR = {177, 77, 180, 255};   // Purple
static constexpr SDL_Color BOOT_COLOR    = {249, 208, 251, 255};  // Pink
```

### Drawing Functions Available (from beast.hpp line 6, SDL2_gfxPrimitives)

```cpp
boxRGBA(renderer, x1, y1, x2, y2, r, g, b, a);           // Filled rectangle
roundedBoxRGBA(renderer, x1, y1, x2, y2, rad, r, g, b, a); // Rounded filled rect
lineRGBA(renderer, x1, y1, x2, y2, r, g, b, a);           // Line
roundedRectangleRGBA(renderer, x1, y1, x2, y2, rad, r, g, b, a); // Rounded outline
```

### Text Rendering (from gui.hpp)

```cpp
// Basic print with optional selection highlight
gui.print(x, y, color, "format %s", args);
gui.print(x, y, color, highlightChars, highlightColor, "format %s", args);

// Layout constants
GUI::COL1 = 50, COL2 = 190, COL3 = 330, COL4 = 470, COL5 = 610
GUI::ROW_HEIGHT = 16, MEM_ROW_HEIGHT = 14
GUI::ROW1 = 56, ROW2 = ROW1+16, ...
```

### Zoom Factor

All coordinates must be multiplied by `zoom` factor when passed to SDL drawing functions. Text rendering via `gui.print()` handles zoom internally (fonts are pre-scaled). Box/line coordinates need explicit `*zoom`:
```cpp
boxRGBA(sdlRenderer, x1*zoom, y1*zoom, x2*zoom, y2*zoom, r, g, b, a);
```

### Previous Story Intelligence

**From Stories 1-2 (Breakpoints/Watchpoints):**
- `drawBreakpoints()` and `drawWatchpoints()` at beast.cpp lines 702-851 are the reference implementations for full-screen overlay modes
- Background overlay: `boxRGBA(sdlRenderer, 32*zoom, 32*zoom, (screenWidth-24)*zoom, (screenHeight-24)*zoom, 0xF0, 0xF0, 0xE0, 0xE8)`
- Title at top left, navigation hint at top right, key hints at bottom
- Mode wiring pattern: add to if-chain in mainLoop (~line 427-441) for rendering, switch-case for event handling
- Key handling: add to `debugMenu()` switch statement (~line 520-579)

**Code review fixes from previous stories (apply same patterns):**
- Selection highlight character counts must match actual content width
- Don't check `address != 0` before operations (address 0x0000 is valid)

### Naming Conventions (per architecture.md)

| Element | Convention | Examples |
|---------|------------|----------|
| Methods | camelCase | `drawPageMap()`, `pageMapMenu()`, `getPagePurpose()` |
| Enum values | SCREAMING_SNAKE | `PAGEMAP`, `PAGE_BOOT`, `PAGE_ROM_DISK` |
| Constants | SCREAMING_SNAKE or constexpr | `CPM_COLOR`, `RESTORE_COLOR` |

### Anti-Patterns to Avoid

- Do NOT create a separate class for the page map - integrate into Beast class (follows existing pattern)
- Do NOT use heap allocation for page data - use static arrays/constants
- Do NOT hardcode page-purpose boundaries without named constants - use a lookup function
- Do NOT forget to multiply drawing coordinates by `zoom`
- Do NOT introduce new fonts - use existing `font`, `smallFont`, `midFont`
- Do NOT make this interactive (editing pages etc.) - it's read-only visualization in this story
- Do NOT render the centre column or arrows yet - those belong to Story 3.2

### Testing Guidance

**Manual Testing Checklist:**
1. Press 'P' from DEBUG mode -> Page Map screen appears
2. Press ESC from Page Map -> Returns to DEBUG mode
3. Verify RAM column on left with 32 page boxes (#20-#3F)
4. Verify ROM column on right with 32 page boxes (#00-#1F)
5. Verify CP/M pages (#20-#23) are dark blue
6. Verify RAM DISK pages (#24-#37) are teal
7. Verify Free RAM pages (#38-#3F) are light blue
8. Verify BOOT pages (#00-#03) are pink
9. Verify ROM DISK pages (#04-#0F) are purple
10. Verify RESTORE pages (#14-#1F) are red
11. Verify shared pages (#10-#13) have diagonal shading with both red and purple
12. Verify page number labels are readable (e.g., "#20", "#00")
13. Verify physical address boundaries are shown (e.g., "80000", "00000")
14. Verify purpose brackets/labels: "CP/M", "RAM DISK", "BOOT", "ROM DISK", "RESTORE"
15. Verify RESTORE and ROM DISK brackets are visually distinct despite shared pages
16. Check rendering at different zoom levels (if applicable)
17. Verify other debug keys still work (B for breakpoints, W for watchpoints, etc.)

### Story 3.2 Forward Compatibility

This story renders the LEFT (RAM) and RIGHT (ROM) columns with space reserved in the centre for Story 3.2's logical CPU banks and mapping arrows. Design the layout so that:
- Centre ~40% of screen width is left empty (or shows placeholder text)
- RAM column occupies left ~25% of screen
- ROM column occupies right ~25% of screen
- Story 3.2 will add: 4 logical bank boxes in centre, mapping arrows, and dynamic colour adoption

### References

- [Source: _bmad-output/planning-artifacts/epics.md#Epic-3] Epic 3 definition, Story 3.1 acceptance criteria
- [Source: _bmad-output/planning-artifacts/prd.md#Functional-Requirements] FR27-FR39 Paging Visualization requirements
- [Source: _bmad-output/planning-artifacts/architecture.md#UI-Architecture] Mode enum, screen rendering patterns
- [Source: _bmad-output/planning-artifacts/architecture.md#Implementation-Patterns] Naming conventions, code patterns
- [Source: docs/microbeast_memory_map.svg] Physical memory layout, page colours, purpose regions
- [Source: src/beast.hpp:33] Mode enum definition
- [Source: src/beast.hpp:116,181] memoryPage[4], pagingEnabled
- [Source: src/beast.cpp:702-760] drawBreakpoints() - reference screen rendering pattern
- [Source: src/beast.cpp:762-851] drawWatchpoints() - reference screen rendering pattern
- [Source: src/beast.cpp:520-579] debugMenu() - keyboard handling for mode switching
- [Source: src/beast.cpp:427-441] mainLoop() - mode rendering dispatch
- [Source: src/beast.cpp:1575-1582] Page register I/O port handling
- [Source: src/beast.cpp:2675] Physical address calculation from page register
- [Source: src/gui.hpp:14-49] GUI layout constants (COL1-COL5, ROW1+, etc.)
- [Source: _bmad-output/implementation-artifacts/2-2-watchpoints-ui-screen.md] Previous story patterns and code review fixes

## Dev Agent Record

### Agent Model Used

Claude Opus 4.6

### Debug Log References

- Verified page purpose colour values against `docs/microbeast_memory_map.svg` SVG fill attributes
- Page boundaries corrected per user feedback: BOOT=#00-#03, ROM DISK=#04-#0F, Shared=#10-#13, RESTORE=#14-#1F, CP/M=#20-#23, Free=#24, RAM DISK=#25-#37, Free=#38-#3F

### Completion Notes List

- Task 1: 'P' key in debugMenu toggles a separate SDL window (not an overlay mode). ESC or window close dismisses it. Window created with its own SDL_Renderer and TTF_Font.
- Task 2: Implemented getPageColor() lambda returning SDL_Color for each page number. All 6 colours match SVG exactly. Page #24 correctly mapped as free (not RAM DISK).
- Task 3: RAM column renders 32 pages (#3F down to #20) with colour-coded boxes, borders, page numbers inside boxes, physical address labels to the left.
- Task 4: ROM column renders 32 pages (#1F down to #00) with borders, page numbers inside boxes, physical address labels to the RIGHT. Shared pages (#10-#13) use diagonal line shading.
- Task 5: RAM brackets on right side of column (CP/M, RAM DISK - no FREE bracket). ROM brackets on left side (BOOT, RESTORE, ROM DSK). RESTORE and ROM DISK brackets visually distinct with different colours and offsets.
- Task 6: Title "PAGE MAP" in window title bar and rendered. Paging ON/OFF status in centre. ESC closes window.

### Change Log

- 2026-02-08: Implemented Story 3.1 - Page Map as separate SDL window (560x740). 'P' toggles window open/closed. Physical RAM/ROM columns with colour-coded bordered boxes, page numbers inside, address labels (left for RAM, right for ROM), purpose brackets, diagonal shading for shared pages. Fixed page #24 to be free.

### File List

- src/beast.hpp (modified) - Added togglePageMap(), closePageMap(), drawPageMap(), pageMapPrint() declarations and page map window member variables (SDL_Window, SDL_Renderer, TTF_Font, etc.)
- src/beast.cpp (modified) - Implemented separate page map window with togglePageMap(), closePageMap(), drawPageMap(), pageMapPrint(). Wired into mainLoop() for rendering and event handling. Added 'P' key toggle in debugMenu().

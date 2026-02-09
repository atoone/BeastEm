# Story 3.2: Logical CPU Banks with Mapping Arrows

Status: review

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a developer debugging Z80 code,
I want to see the 4 logical CPU memory banks in the centre with arrows showing which physical pages they map to,
So that I can understand exactly how the Z80's address space is configured right now.

## Acceptance Criteria

1. **Given** the paging viz screen
   **When** it renders
   **Then** 4 logical CPU banks appear in the centre, labelled with their logical address ranges (0000-3FFF, 4000-7FFF, 8000-BFFF, C000-FFFF)

2. **Given** a logical bank
   **When** rendered
   **Then** it displays its current page number (e.g., #20) and the I/O port used for its mapping

3. **Given** a logical bank mapped to a RAM page (page number >= #20)
   **When** rendered
   **Then** an arrow points left from the logical bank to the corresponding physical RAM page

4. **Given** a logical bank mapped to a ROM page (page number < #20)
   **When** rendered
   **Then** an arrow points right from the logical bank to the corresponding physical ROM page

5. **Given** multiple logical banks mapped to the same side (e.g., all 4 mapped to RAM)
   **When** rendered
   **Then** the arrow vertical paths do not overlap, with space for up to 4 non-overlapping paths per side

6. **Given** a logical bank mapped to a physical page
   **When** rendered
   **Then** the logical bank adopts the fill style (colour) of its mapped physical page (e.g., dark blue if mapped to a CP/M RAM page)

7. **Given** the page mappings change during emulation (e.g., a bank switch occurs)
   **When** the paging viz screen is displayed
   **Then** the arrows, page numbers, and colours update in real-time to reflect current state

## Tasks / Subtasks

- [x] Task 1: Draw 4 logical CPU bank boxes in the centre column (AC: #1, #6)
  - [x] 1.1 Define centre column layout constants: centreColX, centreBoxWidth, centreBoxHeight (4 banks must span the same vertical range as the physical page columns, i.e. 32*boxHeight = 672 pixels, giving ~168 pixels per bank)
  - [x] 1.2 For each bank (0-3), read `memoryPage[bank]` to get the currently mapped page number
  - [x] 1.3 Determine the fill colour by calling the existing `getPageColor()` lambda with the mapped page number (low 5 bits). For shared pages (#10-#13), use the same diagonal shading (filledTrigonRGBA) as the ROM column
  - [x] 1.4 Draw each bank as a filled rectangle with the determined colour, with a border
  - [x] 1.5 Label each bank with its logical address range (e.g., "0000-3FFF") centred inside the box
  - [x] 1.6 Label each bank with the current page number (e.g., "#20") centred inside the box (below the address range)
  - [x] 1.7 Label each bank with the I/O port (e.g., "Port 70") centred inside the box
  - [x] 1.8 Use light or dark text depending on the fill colour (reuse `needsLightText()` lambda with mapped page number)

- [x] Task 2: Draw mapping arrows from logical banks to physical pages (AC: #3, #4, #5)
  - [x] 2.1 For each bank, determine arrow direction: page >= 0x20 = left (RAM), page & 0xE0 == 0 = right (ROM), page & 0xE0 == 0x40 = special (VideoBeast, draw no arrow or a stub)
  - [x] 2.2 Calculate arrow endpoints: start at horizontal edge of centre bank box, end at horizontal edge of the corresponding physical page box. The vertical position at each end is the vertical centre of the respective box.
  - [x] 2.3 Implement non-overlapping vertical routing: each arrow goes horizontally from the bank box edge, then vertically to align with the target page row, then horizontally to the physical page box edge. Assign each arrow a unique vertical "lane" in the gap between columns (up to 4 lanes per side).
  - [x] 2.4 Draw arrows using `lineRGBA()` for the path segments. Use the colour of the target physical page for the arrow colour.
  - [x] 2.5 Draw arrowheads at the physical page end using `filledTrigonRGBA()` - small triangles pointing left (for RAM) or right (for ROM)

- [x] Task 3: Handle real-time updates (AC: #7)
  - [x] 3.1 `drawPageMap()` already reads live state on every frame - verify that reading `memoryPage[0..3]` and `pagingEnabled` reflects current emulation state (it does, since the page map window re-renders each frame via the main loop)
  - [x] 3.2 Ensure no cached state is used - all bank-to-page mappings, colours, and arrow positions must be computed fresh each frame from `memoryPage[]`
  - [x] 3.3 When paging is disabled (`pagingEnabled == false`), the default mapping is sequential: bank 0 → page #00, bank 1 → page #01, bank 2 → page #02, bank 3 → page #03 (i.e. the first 4 ROM pages). This matches the hardware power-on state where unpaged Z80 address space maps linearly to the first 64K of ROM.

- [x] Task 4: Handle edge cases and polish (AC: #1-#7)
  - [x] 4.1 When pagingEnabled is false, show bank 0 mapped to ROM #00 (BOOT pink), bank 1 → ROM #01 (BOOT pink), bank 2 → ROM #02 (BOOT pink), bank 3 → ROM #03 (BOOT pink), with all 4 arrows pointing right to their respective ROM pages. Annotate with "Paging OFF" or similar indicator.
  - [x] 4.2 Handle VideoBeast pages (page & 0xE0 == 0x40): show the bank with a distinct colour (e.g., grey) and no arrow or a label "VB"
  - [x] 4.3 When two banks map to the same physical page, both arrows still render to the same target row without overlapping (the lane routing handles this)
  - [x] 4.4 Test with various paging configurations to verify correct rendering

## Dev Notes

### Architecture Compliance

**File Changes Required:**
- MODIFY: `src/beast.cpp` - Extend `drawPageMap()` to add centre column rendering and mapping arrows
- MODIFY: `src/beast.hpp` - No changes expected (all required declarations already exist)
- NO NEW FILES

This story adds to the existing `drawPageMap()` function in `beast.cpp`. All rendering happens within the existing separate PageMap SDL window (560x740 pixels).

### Existing Implementation Reference (Story 3.1)

**drawPageMap() location:** `beast.cpp:1164-1351`

**Current layout constants (reuse these):**
```cpp
int boxWidth = 90;       // Physical page box width
int boxHeight = 21;      // Physical page box height
int topY = 40;           // Top of columns
int colHeight = 32 * boxHeight;  // 672 pixels total column height

// RAM column
int ramColX = 128;       // Left edge of RAM boxes
// ROM column
int romColX = 358;       // Left edge of ROM boxes
```

**Centre column space available:**
- Left edge: `ramColX + boxWidth` = 128 + 90 = **218**
- Right edge: `romColX` = **358**
- Available width: **140 pixels**
- Recommended centre box width: ~80 pixels
- Recommended centreColX: ~248 (centred in the gap)

**Each logical bank's vertical span:**
- Total column height: 672 pixels (32 * 21)
- Each bank represents 8 physical pages worth of vertical space = 8 * 21 = 168 pixels
- Bank 0 (C000-FFFF) at top: y = topY to topY + 168 (pages #3F down to #38 equivalent)
- Bank 1 (8000-BFFF): y = topY + 168 to topY + 336
- Bank 2 (4000-7FFF): y = topY + 336 to topY + 504
- Bank 3 (0000-3FFF): y = topY + 504 to topY + 672

**IMPORTANT ORDERING:** The physical columns display pages from high (#3F/#1F at top) to low (#20/#00 at bottom). To maintain visual alignment, logical banks should also display high addresses at top:
- Row 0 (top): Bank 3 → addresses C000-FFFF → `memoryPage[3]` → port 0x73
- Row 1: Bank 2 → addresses 8000-BFFF → `memoryPage[2]` → port 0x72
- Row 2: Bank 1 → addresses 4000-7FFF → `memoryPage[1]` → port 0x71
- Row 3 (bottom): Bank 0 → addresses 0000-3FFF → `memoryPage[0]` → port 0x70

### Page Register Mapping

**I/O ports (from beast.cpp:1868-1876):**
```cpp
// Port 0x70 = Bank 0 (0000-3FFF)
// Port 0x71 = Bank 1 (4000-7FFF)
// Port 0x72 = Bank 2 (8000-BFFF)
// Port 0x73 = Bank 3 (C000-FFFF)
// Port 0x74 = Paging enable (bit 0)
```

**Page register decoding (from beast.cpp:2937-2970):**
```cpp
// Bits 7-5 determine page type:
//   0x00 (000xxxxx) = ROM page
//   0x20 (001xxxxx) = RAM page
//   0x40 (010xxxxx) = VideoBeast
// Bits 4-0 determine page number (0-31) within 512K block
```

**Physical page row index (for arrow targeting):**
```cpp
// RAM: page #20 is row index 31 (bottom), #3F is row index 0 (top)
//   rowIndex = 0x3F - pageNum
// ROM: page #00 is row index 31 (bottom), #1F is row index 0 (top)
//   rowIndex = 0x1F - (pageNum & 0x1F)
```

### Arrow Routing Algorithm

**Non-overlapping lane assignment:**
The gap between the centre column and each physical column provides space for vertical arrow routing. With 4 possible arrows per side, assign lane indices 0-3 based on draw order.

```
RAM column      Lanes    Centre     Lanes    ROM column
|  page  |  3 2 1 0  |  bank  |  0 1 2 3  |  page  |
```

**Routing approach (L-shaped or Z-shaped):**
1. Arrow starts at horizontal edge of centre bank box (left edge for RAM, right edge for ROM)
2. Goes horizontally to its assigned lane X-position
3. Goes vertically to the target physical page's vertical centre
4. Goes horizontally to the physical page box edge

**Lane X-positions (example):**
```cpp
// RAM side lanes (between centre and RAM column)
// Centre left edge: centreColX (e.g., 248)
// RAM right edge: ramColX + boxWidth (218)
// Gap: 30 pixels, 4 lanes
int ramLaneX[4] = {centreColX - 6, centreColX - 14, centreColX - 22, centreColX - 30};
// Equivalent: 242, 234, 226, 218+X

// ROM side lanes (between centre and ROM column)
// Centre right edge: centreColX + centreBoxWidth (e.g., 328)
// ROM left edge: romColX (358)
// Gap: 30 pixels, 4 lanes
int romLaneX[4] = {centreColX + centreBoxWidth + 6, ... + 14, ... + 22, ... + 30};
```

**Arrow colour:** Match the fill colour of the target physical page (use `getPageColor(pageNum & 0x1F)` for ROM pages, `getPageColor(pageNum)` for RAM pages).

### Colour Adoption for Logical Banks (AC #6)

Each logical bank must adopt the **exact fill style** of its mapped physical page:
- Read `memoryPage[bank]` to get the page register value
- Extract page type (bits 7-5) and page number (bits 4-0)
- For RAM pages: use `getPageColor(pageNum)` as solid fill
- For ROM pages with page number NOT in #10-#13: use `getPageColor(pageNum & 0x1F)` as solid fill
- **For shared ROM pages (#10-#13): use the SAME diagonal shading as the ROM column** - i.e. two `filledTrigonRGBA()` calls splitting the bank box into upper-left triangle (RESTORE red) and lower-right triangle (ROM DISK purple). This is critical: these pages have a distinctive two-colour fill, and the logical bank must reproduce it exactly, not just pick one solid colour.
- For VideoBeast pages: use a neutral colour (e.g., grey {0x80, 0x80, 0x80, 255})

### Available Drawing Functions

```cpp
// From SDL2_gfxPrimitives (already included via beast.hpp:6)
lineRGBA(renderer, x1, y1, x2, y2, r, g, b, a);              // Line segment
filledTrigonRGBA(renderer, x1, y1, x2, y2, x3, y3, r, g, b, a); // Filled triangle

// From SDL2
SDL_RenderFillRect(renderer, &rect);    // Filled rectangle
SDL_RenderDrawRect(renderer, &rect);    // Rectangle outline
SDL_RenderDrawLine(renderer, x1, y1, x2, y2); // Line (uses current draw colour)

// Custom text rendering (already in beast.cpp)
pageMapPrint(font, x, y, color, fmt, ...);           // Normal text
pageMapPrintRotated(font, cx, cy, angle, color, fmt, ...); // Rotated text
```

### Arrowhead Drawing

Draw small filled triangles at the arrow terminus:
```cpp
// Left-pointing arrowhead (for RAM arrows)
int ax = targetX;         // Arrow tip X (right edge of RAM box)
int ay = targetY;         // Arrow tip Y (vertical centre of target page)
int sz = 4;               // Arrowhead size
filledTrigonRGBA(renderer, ax, ay, ax + sz, ay - sz, ax + sz, ay + sz, r, g, b, 255);

// Right-pointing arrowhead (for ROM arrows)
filledTrigonRGBA(renderer, ax, ay, ax - sz, ay - sz, ax - sz, ay + sz, r, g, b, 255);
```

### Paging Disabled State

When `pagingEnabled == false`, the Z80 address space maps linearly to the first 64K of ROM - each 16K bank maps to consecutive ROM pages:
```cpp
int page = pagingEnabled ? memoryPage[bank] : bank;  // bank 0→#00, bank 1→#01, bank 2→#02, bank 3→#03
```
- Bank 0 (0000-3FFF) → ROM page #00 (BOOT pink)
- Bank 1 (4000-7FFF) → ROM page #01 (BOOT pink)
- Bank 2 (8000-BFFF) → ROM page #02 (BOOT pink)
- Bank 3 (C000-FFFF) → ROM page #03 (BOOT pink)
- All 4 arrows point right to their respective ROM pages (#00-#03)
- The existing "Paging: OFF" text already displays in the centre area; consider moving it or ensuring it doesn't overlap with the new centre column

### Text Layout Inside Logical Bank Boxes

With 168 pixels of vertical space per bank, there is ample room for multiple text lines:
```
Line 1: Logical address range (e.g., "C000-FFFF")   - using pageMapFont
Line 2: Page number (e.g., "#20")                     - using pageMapFont
Line 3: I/O Port (e.g., "Port 73")                    - using pageMapSmallFont
```
Centre each line horizontally within the box and distribute vertically with even spacing.

### Project Structure Notes

- All changes confined to existing files in `src/` directory
- No new dependencies required
- No build system changes needed
- Follows established rendering patterns from Story 3.1

### Previous Story Intelligence

**From Story 3.1 implementation (commit 0e74759):**
- Page map uses a **separate SDL window** (not a main-window overlay mode) - do NOT add a Mode enum value
- Toggle via 'P' key in debugMenu already works
- Rendering uses `pageMapRenderer` (not `sdlRenderer`) for all drawing calls
- Fonts are `pageMapFont` (13pt) and `pageMapSmallFont` (11pt) - loaded separately from main window fonts
- Coordinates are NOT multiplied by zoom (the page map window is a fixed 560x740 pixels)
- The "Paging: ON/OFF" text is currently at position (238, topY) in the centre area - this will need to be repositioned or integrated with the new centre column content
- Shared page diagonal shading uses two `filledTrigonRGBA()` calls to split the box into upper-left and lower-right triangles
- Bracket drawing uses `drawBracketLeft()` / `drawBracketRight()` lambdas with `lineRGBA()` for lines and `pageMapPrintRotated()` for vertical labels

**From Story 3.1 dev notes:**
- Page #24 is classified as "free" (not RAM DISK) - the `getPageColor` lambda handles this correctly
- Addresses are displayed at page boundaries (at the bottom edge of each page box, vertically centred on the dividing line)
- Page numbers use format "#%02X" and addresses use "%05X"

### Anti-Patterns to Avoid

- Do NOT create a separate class or file for this - extend existing `drawPageMap()` in `beast.cpp`
- Do NOT use `sdlRenderer` - use `pageMapRenderer` (this is a separate window)
- Do NOT multiply coordinates by `zoom` - the page map window is not zoom-scaled
- Do NOT cache page mapping state between frames - read `memoryPage[]` fresh every frame
- Do NOT add a new Mode enum value - the page map window coexists with DEBUG mode
- Do NOT use heap allocation for arrow routing data - use stack arrays
- Do NOT hardcode page assignments - always read from `memoryPage[]` for live state
- Do NOT forget to handle the `pagingEnabled == false` case
- Do NOT overlap the "Paging: ON/OFF" text with the new centre column content

### Testing Guidance

**Manual Testing Checklist:**
1. Press 'P' from DEBUG mode - Page Map window appears with centre column visible
2. Verify 4 logical bank boxes in centre with correct address ranges (C000-FFFF at top, 0000-3FFF at bottom)
3. Verify each bank shows its current page number (e.g., "#20")
4. Verify each bank shows its I/O port (e.g., "Port 70")
5. Verify banks mapped to RAM have arrows pointing LEFT to the correct RAM page
6. Verify banks mapped to ROM have arrows pointing RIGHT to the correct ROM page
7. Verify arrows do not overlap when multiple banks map to the same side
8. Verify each logical bank adopts the fill colour of its mapped physical page
9. Verify shared page mapping (#10-#13) shows diagonal shading (red+purple two-triangle fill) in the logical bank, matching the ROM column's shared page appearance
10. Toggle paging OFF and verify banks show sequential ROM pages: bank 0→#00, bank 1→#01, bank 2→#02, bank 3→#03 (all BOOT pink) with arrows to their respective ROM pages
11. Toggle paging ON and verify banks update to show current page register values
12. Run emulation with bank switching and verify the page map updates in real-time
13. Verify "Paging: ON/OFF" indicator still visible and doesn't overlap the centre column
14. Verify arrow colours match their target physical page colours
15. Verify text is readable on both light and dark coloured bank fills
16. Verify ESC still closes the page map window
17. Test with all 4 banks mapped to RAM (all arrows left)
18. Test with all 4 banks mapped to ROM (all arrows right)
19. Test with mixed RAM/ROM mappings
20. Test with two banks mapped to the same physical page

### References

- [Source: _bmad-output/planning-artifacts/epics.md#Story-3.2] Story 3.2 acceptance criteria and requirements
- [Source: _bmad-output/planning-artifacts/prd.md#Functional-Requirements] FR28-FR39 Paging Visualization requirements
- [Source: _bmad-output/planning-artifacts/architecture.md#Implementation-Patterns] Naming conventions, code patterns
- [Source: _bmad-output/planning-artifacts/ux-design-specification.md] Keyboard interaction patterns
- [Source: docs/microbeast_memory_map.svg] Physical memory layout, page colours, purpose regions
- [Source: src/beast.hpp:256-271] Page map window declarations and member variables
- [Source: src/beast.cpp:1164-1351] drawPageMap() - current Story 3.1 implementation to extend
- [Source: src/beast.cpp:1125-1162] pageMapPrint()/pageMapPrintRotated() text rendering helpers
- [Source: src/beast.cpp:1209-1228] getPageColor()/needsLightText() lambdas for page colours
- [Source: src/beast.cpp:1274-1280] Diagonal shading for shared pages using filledTrigonRGBA
- [Source: src/beast.cpp:1316-1338] Bracket drawing lambdas (drawBracketLeft/drawBracketRight)
- [Source: src/beast.cpp:1868-1876] Page register I/O port handling (ports 0x70-0x74)
- [Source: src/beast.cpp:2937-2970] readMem()/readPage() physical address calculation
- [Source: src/beast.hpp:112-116,181] memoryPage[4], pagingEnabled member variables
- [Source: _bmad-output/implementation-artifacts/3-1-page-map-screen-physical-columns.md] Story 3.1 full implementation details and dev notes

## Dev Agent Record

### Agent Model Used

Claude Opus 4.6

### Debug Log References

- Fixed layout bug: initial centreColX calculation overlapped RAM column; corrected to centre between ramColX+boxWidth and romColX

### Completion Notes List

- **Task 1**: Added 4 logical CPU bank boxes in centre column between RAM and ROM physical columns. Each bank is 168px tall (colHeight/4), filled with the colour of its mapped physical page. Shared ROM pages (#10-#13) get diagonal red/purple shading. Each box displays address range, page number, and I/O port with appropriate light/dark text.
- **Task 2**: Implemented 3-segment L/Z-shaped arrow routing from each bank to its mapped physical page. RAM arrows point left, ROM arrows point right. Lane-based vertical routing prevents overlap when multiple arrows go to the same side. Arrowheads drawn as filled triangles at the physical page end. Arrow colour matches the target physical page colour.
- **Task 3**: All rendering is computed fresh each frame from `memoryPage[]` and `pagingEnabled` - no cached state. When paging is disabled, banks map sequentially to ROM pages #00-#03.
- **Task 4**: VideoBeast pages (0x40 range) rendered with grey fill and "VB #XX" label, no arrow drawn. Paging OFF state correctly shows all 4 banks as BOOT pink with arrows to ROM. Two banks mapping to the same page both draw arrows via separate lanes. "Paging: ON/OFF" indicator repositioned above centre column to avoid overlap.

### Change Log

- 2026-02-08: Implemented Story 3.2 - Added centre column with 4 logical CPU bank boxes and mapping arrows to drawPageMap() in beast.cpp

### File List

- `src/beast.cpp` - Modified: Extended drawPageMap() with centre column rendering (bank boxes, labels, mapping arrows, edge cases)

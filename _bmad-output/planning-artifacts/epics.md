---
stepsCompleted:
  - step-01-validate-prerequisites
  - step-02-design-epics
  - step-03-create-stories
  - step-04-final-validation
status: complete
completedAt: '2026-02-08'
inputDocuments:
  - _bmad-output/planning-artifacts/prd.md
  - _bmad-output/planning-artifacts/architecture.md
  - _bmad-output/planning-artifacts/ux-design-specification.md
  - docs/microbeast_memory_map.svg
project_name: beastem
user_name: Ant
date: '2026-02-08'
---

# beastem - Epic Breakdown

## Overview

This document provides the complete epic and story breakdown for beastem, decomposing the requirements from the PRD, UX Design, Architecture, and user-specified paging visualization feature into implementable stories.

## Requirements Inventory

### Functional Requirements

**Breakpoint Management (FR1-FR8):**

- FR1: User can create a breakpoint at a specified memory address
- FR2: User can optionally associate a breakpoint with a specific memory bank
- FR3: User can enable or disable a breakpoint
- FR4: User can edit an existing breakpoint's address and bank
- FR5: User can delete a breakpoint
- FR6: User can quick-toggle a breakpoint's enabled state from the list view
- FR7: User can view all configured breakpoints and their states
- FR8: System limits user breakpoints to 8 maximum

**Watchpoint Management (FR9-FR17):**

- FR9: User can create a watchpoint at a specified memory address
- FR10: User can specify a watchpoint's range (number of bytes to monitor)
- FR11: User can specify watchpoint trigger type (read, write, or both)
- FR12: User can enable or disable a watchpoint
- FR13: User can edit an existing watchpoint's address, range, and type
- FR14: User can delete a watchpoint
- FR15: User can quick-toggle a watchpoint's enabled state from the list view
- FR16: User can view all configured watchpoints and their states
- FR17: System limits user watchpoints to 8 maximum

**Debugging UI (FR18-FR22):**

- FR18: User can access a dedicated breakpoints/watchpoints screen
- FR19: User can switch between Breakpoints and Watchpoints tabs
- FR20: User can see at a glance which breakpoints/watchpoints are enabled
- FR21: UI displays breakpoint addresses in hexadecimal format
- FR22: UI displays watchpoint addresses and ranges in hexadecimal format

**Emulation Integration (FR23-FR26):**

- FR23: System pauses emulation when PC matches an enabled breakpoint address
- FR24: System pauses emulation when memory access matches an enabled watchpoint
- FR25: System supports 2 hidden system breakpoints for programmatic use
- FR26: Breakpoint/watchpoint checking does not degrade emulation accuracy

**Paging Visualization (FR27-FR39):**

- FR27: User can access the paging visualization window by pressing 'P' from DEBUG mode
- FR28: Paging visualization displays 4 logical CPU memory banks (16KB each, 0000-FFFF) in the centre
- FR29: Physical RAM pages (#20-#3F) are displayed to the left of the logical banks
- FR30: Physical ROM pages (#00-#1F) are displayed to the right of the logical banks
- FR31: RAM pages use a blue/green colour scheme: CP/M pages = dark blue, RAM DISK pages = teal, free pages = light blue
- FR32: ROM pages use a red/purple colour scheme: RESTORE pages = red, ROM DISK pages = purple, BOOT pages = pink
- FR33: Shared pages (#10-#13) display both ROM colours via diagonal shading
- FR34: Logical CPU pages display their page number and mapping I/O port, annotated with logical addresses
- FR35: Physical pages display their page number, annotated with physical address boundaries
- FR36: Physical pages are grouped into labelled brackets by purpose (RAM: CP/M, RAM DISK; ROM: RESTORE, ROM DISK, BOOT)
- FR37: Arrows show current mappings from logical pages to their mapped physical pages (RAM arrows to left, ROM arrows to right)
- FR38: Arrow routing provides space for up to 4 non-overlapping vertical paths per side
- FR39: Mapped logical pages adopt the fill style of the physical page they are mapped to

### NonFunctional Requirements

**Performance (NFR1-NFR4):**

- NFR1: Breakpoint checking adds acceptable overhead only when breakpoints are enabled
- NFR2: Watchpoint checking adds acceptable overhead only when watchpoints are enabled
- NFR3: With no breakpoints/watchpoints enabled, emulation performance is unaffected
- NFR4: UI operations (add/edit/delete) complete without perceptible delay

**Accuracy (NFR5-NFR7):**

- NFR5: Breakpoints trigger on the exact instruction at the specified address (no misses, no false triggers)
- NFR6: Watchpoints detect all memory accesses within the monitored range (no misses)
- NFR7: Bank-specific breakpoints only trigger when the correct bank is paged in

**UI Consistency (NFR8-NFR10):**

- NFR8: Breakpoint/watchpoint UI follows existing BeastEm visual style and interaction patterns
- NFR9: Keyboard navigation consistent with other BeastEm screens
- NFR10: Hexadecimal address display matches existing debugger formatting conventions

**Integration (NFR11-NFR13):**

- NFR11: Feature integrates with existing single-step execution without conflicts
- NFR12: System breakpoints accessible programmatically for future DeZog integration
- NFR13: Breakpoint/watchpoint state queryable by other debugger components

**Paging Visualization (NFR14-NFR16):**

- NFR14: Paging visualization updates in real-time as page mappings change during emulation
- NFR15: Paging visualization follows existing BeastEm visual style
- NFR16: Paging visualization renders without perceptible delay

### Additional Requirements

**From Architecture Document:**

- Brownfield C++ project - integrate with existing codebase patterns
- New files required: `src/debugmanager.hpp`, `src/debugmanager.cpp`
- Fixed arrays: `breakpoints[8]`, `watchpoints[8]`, `systemBreakpoints[2]`
- Separate DebugManager class following existing component patterns (I2c, VideoBeast)
- Hybrid checking strategy: breakpoints at `z80_opdone()`, watchpoints inline in `readMem()`/`writeMem()`
- User-selectable logical (16-bit address) OR physical (20-bit address) mode
- New `Mode::BREAKPOINTS` enum value for UI state machine
- Follow existing naming conventions: PascalCase classes, camelCase methods, SCREAMING_SNAKE constants
- Build integration: add `debugmanager.cpp` to CMakeLists.txt

**From UX Design Document:**

- Keyboard-only operation required (no mouse interactions)
- Single-key actions: A=Add, D=Delete, Space=Toggle, Enter=Edit, ESC=Exit
- B/W keys to switch between Breakpoints/Watchpoints screens
- Fixed 8-slot display showing all slots (populated or empty as dashes)
- Visual encoding: enabled items bright, disabled items dim
- On-screen key hints always visible in footer
- Smart defaults: new items enabled, watchpoints default to write-only with 1-byte range
- Instant feedback (<16ms) for all interactions
- Selection highlight on current list item
- Tab key advances between fields during watchpoint entry
- RobotoMono for hex values/addresses, Roboto for UI text/labels

**From Memory Map Reference (microbeast_memory_map.svg):**

- Physical memory layout: 512KB ROM (00000-7FFFF, pages #00-#1F) + 512KB RAM (80000-FFFFF, pages #20-#3F)
- Each page is 16KB; Z80 sees 4 x 16KB slots mapped via page registers
- ROM regions: BOOT, RESTORE, ROM DISK (pages #10-#13 shared between RESTORE and ROM DISK)
- RAM regions: CP/M, RAM DISK
- Page register I/O ports map logical slots to physical pages

**User-Specified Paging Visualization:**

- New dedicated window/screen for paging visualization, accessed via 'P' from DEBUG mode
- Centre column: 4 logical memory banks showing Z80's view
- Left column: physical RAM pages with blue/green colour scheme
- Right column: physical ROM pages with red/purple colour scheme
- Diagonal shading for shared pages (#10-#13)
- Mapping arrows from logical to physical pages, non-overlapping, up to 4 per side
- Logical pages adopt fill style of their mapped physical page
- RESTORE and ROM DISK brackets must be visually distinct despite shared pages

### FR Coverage Map

| FR | Epic | Status | Description |
|----|------|--------|-------------|
| FR1 | Epic 1 | DONE | Create breakpoint at address |
| FR2 | Epic 1 | DONE | Associate breakpoint with bank |
| FR3 | Epic 1 | DONE | Enable/disable breakpoint |
| FR4 | Epic 1 | DONE | Edit breakpoint address/bank |
| FR5 | Epic 1 | DONE | Delete breakpoint |
| FR6 | Epic 1 | DONE | Quick-toggle from list |
| FR7 | Epic 1 | DONE | View all breakpoints |
| FR8 | Epic 1 | DONE | Limit to 8 breakpoints |
| FR9 | Epic 2 | DONE | Create watchpoint at address |
| FR10 | Epic 2 | DONE | Specify watchpoint range |
| FR11 | Epic 2 | DONE | Specify trigger type (R/W/RW) |
| FR12 | Epic 2 | DONE | Enable/disable watchpoint |
| FR13 | Epic 2 | DONE | Edit watchpoint properties |
| FR14 | Epic 2 | DONE | Delete watchpoint |
| FR15 | Epic 2 | DONE | Quick-toggle from list |
| FR16 | Epic 2 | DONE | View all watchpoints |
| FR17 | Epic 2 | DONE | Limit to 8 watchpoints |
| FR18 | Epic 1 | DONE | Dedicated BP/WP screen |
| FR19 | Epic 2 | DONE | Switch between tabs |
| FR20 | Epic 1 | DONE | See enabled state at glance |
| FR21 | Epic 1 | DONE | Hex address display (BP) |
| FR22 | Epic 2 | DONE | Hex address/range display (WP) |
| FR23 | Epic 1 | DONE | Pause on breakpoint hit |
| FR24 | Epic 2 | DONE | Pause on watchpoint hit |
| FR25 | Epic 2 | DONE | 2 hidden system breakpoints |
| FR26 | Epic 1 | DONE | No accuracy degradation |
| FR27 | Epic 3 | TODO | Access paging viz via 'P' from DEBUG |
| FR28 | Epic 3 | TODO | Display 4 logical CPU banks in centre |
| FR29 | Epic 3 | TODO | Physical RAM pages on left |
| FR30 | Epic 3 | TODO | Physical ROM pages on right |
| FR31 | Epic 3 | TODO | RAM colour scheme (dark blue/teal/light blue) |
| FR32 | Epic 3 | TODO | ROM colour scheme (red/purple/pink) |
| FR33 | Epic 3 | TODO | Diagonal shading for shared pages |
| FR34 | Epic 3 | TODO | Logical page labels (page #, I/O port, addresses) |
| FR35 | Epic 3 | TODO | Physical page labels (page #, address boundaries) |
| FR36 | Epic 3 | TODO | Purpose brackets for physical page groups |
| FR37 | Epic 3 | TODO | Mapping arrows from logical to physical |
| FR38 | Epic 3 | TODO | Non-overlapping arrow routing (4 paths/side) |
| FR39 | Epic 3 | TODO | Logical pages adopt mapped page fill style |

## Epic List

### Epic 1: Breakpoint Debugging (COMPLETED)

Marcus can set multiple breakpoints across his code and have the emulator pause at exact addresses, enabling efficient game loop debugging without modifying source code.

**FRs covered:** FR1, FR2, FR3, FR4, FR5, FR6, FR7, FR8, FR18, FR20, FR21, FR23, FR26
**Status:** Implemented.

### Epic 2: Watchpoint Debugging (COMPLETED)

Marcus can monitor memory regions for unexpected reads/writes, catching buffer overflows and memory corruption that would otherwise take hours to debug.

**FRs covered:** FR9, FR10, FR11, FR12, FR13, FR14, FR15, FR16, FR17, FR19, FR22, FR24, FR25
**Status:** Implemented.

### Epic 3: Paging Visualization

Marcus can see at a glance how the Z80's logical address space maps to physical RAM and ROM pages, understanding the memory banking state during debugging.

**What gets built:**
- New PAGEMAP Mode enum value and 'P' key entry from DEBUG mode
- Three-column layout: physical RAM (left), logical CPU banks (centre), physical ROM (right)
- Colour-coded physical pages: RAM blue/green (CP/M dark blue, RAM DISK teal, free light blue), ROM red/purple (RESTORE red, ROM DISK purple, BOOT pink)
- Diagonal shading for shared ROM pages (#10-#13)
- Page labels with page numbers, I/O ports, and address annotations
- Purpose brackets grouping physical pages (CP/M, RAM DISK, RESTORE, ROM DISK, BOOT)
- Mapping arrows from logical to physical pages with non-overlapping routing
- Logical pages adopt the fill style of their mapped physical page
- Real-time updates as page mappings change

**FRs covered:** FR27, FR28, FR29, FR30, FR31, FR32, FR33, FR34, FR35, FR36, FR37, FR38, FR39
**NFRs addressed:** NFR14, NFR15, NFR16

---

## Epic 3: Paging Visualization

Marcus can see at a glance how the Z80's logical address space maps to physical RAM and ROM pages, understanding the memory banking state during debugging.

### Story 3.1: Page Map Screen with Physical Page Columns

As a developer debugging Z80 code,
I want a paging visualization screen showing all physical RAM and ROM pages in colour-coded columns,
So that I can see the full memory landscape of the Microbeast at a glance.

**Acceptance Criteria:**

**Given** the emulator is in DEBUG mode
**When** I press 'P'
**Then** the paging visualization screen appears with a PAGEMAP Mode enum value

**Given** the paging viz screen
**When** it renders
**Then** physical RAM pages (#20-#3F) appear on the left with page numbers and physical address boundaries (e.g., 80000-83FFF for #20)

**Given** the paging viz screen
**When** it renders
**Then** physical ROM pages (#00-#1F) appear on the right with page numbers and physical address boundaries (e.g., 00000-03FFF for #00)

**Given** RAM pages
**When** rendered
**Then** CP/M pages are dark blue, RAM DISK pages are teal, and free pages are light blue

**Given** ROM pages
**When** rendered
**Then** RESTORE pages are red, ROM DISK pages are purple, and BOOT pages are pink

**Given** shared ROM pages (#10, #11, #12, #13)
**When** rendered
**Then** they display both RESTORE (red) and ROM DISK (purple) colours via diagonal shading

**Given** physical page groups
**When** rendered
**Then** labelled brackets show purpose groupings: RAM side has "CP/M" and "RAM DISK" brackets; ROM side has "RESTORE", "ROM DISK", and "BOOT" brackets, with RESTORE and ROM DISK brackets visually distinct despite shared pages

**Given** the paging viz screen
**When** I press ESC
**Then** I return to DEBUG mode

### Story 3.2: Logical CPU Banks with Mapping Arrows

As a developer debugging Z80 code,
I want to see the 4 logical CPU memory banks in the centre with arrows showing which physical pages they map to,
So that I can understand exactly how the Z80's address space is configured right now.

**Acceptance Criteria:**

**Given** the paging viz screen
**When** it renders
**Then** 4 logical CPU banks appear in the centre, labelled with their logical address ranges (0000-3FFF, 4000-7FFF, 8000-BFFF, C000-FFFF)

**Given** a logical bank
**When** rendered
**Then** it displays its current page number (e.g., #20) and the I/O port used for its mapping

**Given** a logical bank mapped to a RAM page (page number >= #20)
**When** rendered
**Then** an arrow points left from the logical bank to the corresponding physical RAM page

**Given** a logical bank mapped to a ROM page (page number < #20)
**When** rendered
**Then** an arrow points right from the logical bank to the corresponding physical ROM page

**Given** multiple logical banks mapped to the same side (e.g., all 4 mapped to RAM)
**When** rendered
**Then** the arrow vertical paths do not overlap, with space for up to 4 non-overlapping paths per side

**Given** a logical bank mapped to a physical page
**When** rendered
**Then** the logical bank adopts the fill style (colour) of its mapped physical page (e.g., dark blue if mapped to a CP/M RAM page)

**Given** the page mappings change during emulation (e.g., a bank switch occurs)
**When** the paging viz screen is displayed
**Then** the arrows, page numbers, and colours update in real-time to reflect current state

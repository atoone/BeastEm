---
stepsCompleted: [1, 2, 3, 4, 5, 6, 7, 8]
inputDocuments:
  - _bmad-output/planning-artifacts/prd.md
  - docs/index.md
  - docs/project-overview.md
  - docs/architecture.md
  - docs/source-tree-analysis.md
  - docs/development-guide.md
workflowType: 'architecture'
project_name: 'beastem'
user_name: 'Ant'
date: '2026-01-30'
lastStep: 8
status: 'complete'
completedAt: '2026-01-30'
---

# Architecture Decision Document

_This document builds collaboratively through step-by-step discovery. Sections are appended as we work through each architectural decision together._

## Project Context Analysis

### Requirements Overview

**Functional Requirements:**
26 functional requirements across 4 domains:
- Breakpoint Management (FR1-8): CRUD operations for 8 user breakpoints with address/bank/enabled state
- Watchpoint Management (FR9-17): CRUD operations for 8 user + 2 system breakpoints with address/range/type
- Debugging UI (FR18-22): Dedicated tabbed screen for breakpoint/watchpoint management
- Emulation Integration (FR23-26): Accurate triggering in CPU execution loop

**Non-Functional Requirements:**
13 NFRs driving architectural decisions:
- Performance: Zero overhead when disabled, acceptable overhead when enabled
- Accuracy: 100% trigger accuracy - no misses, no false triggers
- UI Consistency: Match existing BeastEm patterns and keyboard navigation
- Integration: Work with existing debugger, expose programmatic API for future DeZog

**Scale & Complexity:**
- Primary domain: Desktop application / Emulator internals
- Complexity level: Medium
- Estimated architectural components: 3-4 (breakpoint manager, watchpoint manager, UI screen, emulation hooks)

### Technical Constraints & Dependencies

- **Existing Codebase:** Brownfield C++ project with established patterns
- **Execution Loop:** Must integrate with `z80_tick()` cycle-accurate emulation
- **Memory System:** Must hook into `readMem()`/`writeMem()` with bank awareness
- **UI Framework:** SDL2-based custom rendering (no widget toolkit)
- **Platform:** Cross-platform (Windows, Linux, macOS) via SDL2

### Cross-Cutting Concerns Identified

- **Performance vs Features:** Watchpoint range checking on every memory access needs optimization
- **Bank-Aware Addressing:** Breakpoints need to understand 64K logical vs 1MB physical addressing
- **UI State Management:** New screen must integrate with existing debug mode state machine
- **Future API Surface:** System breakpoints need clean programmatic interface for DeZog integration

## Starter Template Evaluation

### Primary Technology Domain

Desktop Application / Z80 Emulator - **Brownfield C++ project**

### Starter Options Considered

**Not Applicable** - This is a brownfield project with an established technology stack.

### Existing Technical Foundation

| Aspect | Current Stack |
|--------|---------------|
| Language | C++17/20 |
| Build System | CMake |
| UI Framework | SDL2 ecosystem |
| CPU Emulation | Floooh Chips (header-only) |
| Platform Support | Windows, Linux, macOS |

### Architectural Decisions Already Established

**Language & Runtime:**
- C++ with modern standards (C++17/20)
- No runtime dependencies beyond SDL2

**Build Tooling:**
- CMake with platform detection
- Custom FindSDL2_*.cmake modules

**Code Organization:**
- Main entry in `beastem.cpp`
- Core classes in `src/` directory
- Headers alongside implementations (.hpp/.cpp pairs)
- External dependencies in `deps/`

**Development Experience:**
- Standard CMake build workflow
- Debug mode built into emulator (ESC key)
- Hot-reload for ROM files via file watching

**Note:** New feature implementation will follow existing patterns and integrate with established architecture.

## Core Architectural Decisions

### Decision Priority Analysis

**Critical Decisions (Block Implementation):**
- Data structure design for breakpoints/watchpoints
- Emulation loop integration strategy
- Addressing mode handling

**Important Decisions (Shape Architecture):**
- Manager class organization
- UI screen structure

**Deferred Decisions (Post-MVP):**
- DeZog DZRP 2.0 protocol details
- Persistence/save-load of breakpoints

### Data Structures

**Decision:** Fixed arrays for breakpoints and watchpoints

```cpp
Breakpoint breakpoints[8];       // User breakpoints
Watchpoint watchpoints[8];       // User watchpoints
Watchpoint systemBreakpoints[2]; // Hidden system breakpoints
```

**Rationale:** Fixed limits (8/8/2) are defined in PRD. Fixed arrays provide predictable memory layout, zero heap allocation during emulation, and fast iteration for the performance-critical checking loop.

### Component Architecture

**Decision:** Separate DebugManager class

**Rationale:**
- Clean separation of concerns from Beast emulation core
- Provides clear API surface for future DeZog integration
- Easier to test in isolation
- Follows existing pattern of separate component classes (I2c, VideoBeast, etc.)

**Location:** `src/debugmanager.cpp` / `src/debugmanager.hpp`

### Emulation Integration

**Decision:** Hybrid checking strategy

| Check Type | Integration Point | Rationale |
|------------|-------------------|-----------|
| Breakpoints | `z80_opdone()` instruction boundary | Sufficient accuracy, lower overhead |
| Watchpoints | Inline in `readMem()`/`writeMem()` | Must catch every memory access for 100% accuracy |

**Rationale:** Breakpoints only need to fire at instruction boundaries (when PC matches). Watchpoints must intercept every memory read/write to catch all accesses within monitored ranges.

### Addressing Mode

**Decision:** User-selectable logical OR physical addressing

| Mode | Format | Behavior |
|------|--------|----------|
| Logical | 16-bit address (e.g., `$4000`) | Triggers regardless of which bank is paged in |
| Physical | 20-bit address (e.g., `$84000`) | Triggers only for exact physical location |

**Rationale:** Follows existing BeastEm pattern for file loading where users can specify either logical or physical addresses. Provides flexibility - logical for "break when code at $4000 runs" vs physical for "break in this specific ROM routine."

### UI Architecture

**Decision:** New Mode enum value (`BREAKPOINTS` or similar)

**Rationale:**
- Integrates with existing state machine (RUN, DEBUG, STEP, OVER, OUT, TAKE)
- Gives breakpoint/watchpoint management its own dedicated screen as PRD specifies
- Follows established UI patterns
- Clear entry/exit via keyboard shortcut

### Decision Impact Analysis

**Implementation Sequence:**
1. Define data structures (Breakpoint, Watchpoint structs)
2. Create DebugManager class with CRUD operations
3. Integrate breakpoint checking at instruction boundaries
4. Integrate watchpoint checking in memory access functions
5. Add new UI mode and rendering
6. Wire up keyboard navigation and editing

**Cross-Component Dependencies:**
- DebugManager needs read access to current memory page state (for logical→physical resolution)
- Beast::readMem/writeMem need to call DebugManager for watchpoint checks
- Beast::run loop needs to call DebugManager for breakpoint checks
- GUI needs to render new mode screen

## Implementation Patterns & Consistency Rules

### Pattern Categories Defined

**Critical Conflict Points Identified:**
5 areas where AI agents could make different choices - all now standardized.

### Naming Patterns

**C++ Naming Conventions (Follow Existing Codebase):**

| Element | Convention | Example |
|---------|------------|---------|
| Classes | PascalCase | `DebugManager`, `Breakpoint` |
| Methods | camelCase | `addBreakpoint()`, `checkWatchpoint()` |
| Member variables | camelCase | `breakpointCount`, `isPhysical` |
| Enum types | PascalCase | `Mode`, `WatchType` |
| Enum values | SCREAMING_SNAKE | `BREAKPOINTS`, `SEL_BREAKPOINT` |
| Constants | SCREAMING_SNAKE | `MAX_BREAKPOINTS`, `MAX_WATCHPOINTS` |
| Files | lowercase | `debugmanager.hpp`, `debugmanager.cpp` |

### Structure Patterns

**Data Structures:**

```cpp
struct Breakpoint {
    uint32_t address;      // 16-bit logical or 20-bit physical
    bool     isPhysical;   // true = physical, false = logical
    bool     enabled;
};

struct Watchpoint {
    uint32_t address;
    uint16_t length;
    bool     isPhysical;
    bool     enabled;
    bool     onRead;
    bool     onWrite;
};
```

**File Organization:**
- New files: `src/debugmanager.hpp`, `src/debugmanager.cpp`
- Follow existing header pattern: `#pragma once`, includes, class definition
- Implementation in .cpp with matching structure

**Class Organization:**
- Public methods first, then private
- Enum declarations inside class when class-specific
- Member initialization inline where sensible

### Code Patterns

**DebugManager API Pattern:**

```cpp
class DebugManager {
public:
    // Breakpoint CRUD
    int  addBreakpoint(uint32_t address, bool isPhysical);  // Returns index or -1
    bool removeBreakpoint(int index);
    void setBreakpointEnabled(int index, bool enabled);
    Breakpoint* getBreakpoint(int index);
    int  getBreakpointCount();

    // Watchpoint CRUD
    int  addWatchpoint(uint32_t address, uint16_t length, bool isPhysical, bool onRead, bool onWrite);
    bool removeWatchpoint(int index);
    void setWatchpointEnabled(int index, bool enabled);
    Watchpoint* getWatchpoint(int index);
    int  getWatchpointCount();

    // System breakpoints (hidden, for DeZog)
    int  addSystemBreakpoint(uint32_t address, uint16_t length, bool onRead, bool onWrite);
    void clearSystemBreakpoints();

    // Emulation integration
    bool checkBreakpoint(uint16_t pc, uint8_t* memoryPage);
    bool checkWatchpoint(uint32_t physicalAddress, bool isRead);
    bool hasActiveBreakpoints();
    bool hasActiveWatchpoints();

private:
    static const int MAX_BREAKPOINTS = 8;
    static const int MAX_WATCHPOINTS = 8;
    static const int MAX_SYSTEM_BREAKPOINTS = 2;

    Breakpoint breakpoints[MAX_BREAKPOINTS];
    Watchpoint watchpoints[MAX_WATCHPOINTS];
    Watchpoint systemBreakpoints[MAX_SYSTEM_BREAKPOINTS];
    int breakpointCount = 0;
    int watchpointCount = 0;
    int systemBreakpointCount = 0;
};
```

**Integration Patterns:**

```cpp
// Breakpoint check at instruction boundary (in Beast::run)
if (z80_opdone(&cpu) && debugManager->hasActiveBreakpoints()) {
    if (debugManager->checkBreakpoint(Z80_GET_ADDR(pins), memoryPage)) {
        mode = DEBUG;
    }
}

// Watchpoint check in memory access (in Beast::readMem/writeMem)
if (debugManager->hasActiveWatchpoints()) {
    if (debugManager->checkWatchpoint(physicalAddr, isRead)) {
        mode = DEBUG;
    }
}
```

### UI Patterns

**Mode Integration:**
- Add `BREAKPOINTS` to existing `Mode` enum
- New keyboard shortcut to enter mode (e.g., 'B' from DEBUG)
- ESC or similar to return to DEBUG mode

**Screen Layout:**
- Follow existing GUI patterns (COL1, ROW1, etc. constants)
- Tab-style navigation between Breakpoints/Watchpoints lists
- Selection highlighting consistent with DEBUG mode
- Hex address display using existing formatting

### Enforcement Guidelines

**All AI Agents MUST:**
- Follow camelCase for methods and member variables
- Use SCREAMING_SNAKE for enum values and constants
- Place new files in `src/` directory with lowercase names
- Use `#pragma once` for header guards
- Follow existing indentation (4 spaces)
- Use `uint8_t`, `uint16_t`, `uint32_t` for explicit sizes
- Check `hasActiveBreakpoints()`/`hasActiveWatchpoints()` before iteration for performance

**Anti-Patterns to Avoid:**
- Don't use `std::vector` for fixed-size arrays (use C arrays with count)
- Don't add heap allocations in the hot path
- Don't create new enum types outside classes unless shared
- Don't use different naming conventions than existing code

## Project Structure & Boundaries

### Existing Project Directory Structure

```
beastem/
├── CMakeLists.txt              # Build configuration
├── beastem.cpp                 # Entry point, CLI parsing, SDL init
├── README.md
├── VIDEOBEAST.md
├── LICENSE
│
├── src/                        # Core source files
│   ├── beast.cpp/.hpp          # Main emulator core
│   ├── z80.h                   # CPU emulation (Floooh)
│   ├── z80pio.h                # Parallel I/O
│   ├── uart16c550.h            # Serial UART
│   ├── i2c.cpp/.hpp            # I2C bus
│   ├── display.cpp/.hpp        # LED display
│   ├── digit.cpp/.hpp          # Digit rendering
│   ├── rtc.cpp/.hpp            # Real-time clock
│   ├── videobeast.cpp/.hpp     # Video chip
│   ├── gui.cpp/.hpp            # Debug UI rendering
│   ├── instructions.cpp/.hpp   # Z80 disassembly
│   ├── listing.cpp/.hpp        # Assembly listing parser
│   ├── binaryFile.cpp/.hpp     # File loading
│   ├── assets.cpp/.hpp         # Asset path resolution
│   ├── debug.cpp/.hpp          # Debug logging
│   └── include/                # Vendored headers (SDL, etc.)
│
├── deps/                       # External dependencies
│   └── nativefiledialog-extended-1.1.1/
│
├── cmake/                      # CMake modules
│   └── FindSDL2_*.cmake
│
├── assets/                     # Runtime assets
│   └── *.bin, *.ttf, etc.
│
└── docs/                       # Documentation
    └── *.md
```

### New Files to Add

```
src/
├── debugmanager.cpp            # NEW: Breakpoint/watchpoint manager
├── debugmanager.hpp            # NEW: Manager class definition
```

### Architectural Boundaries

**Component Boundaries:**

| Component | Responsibility | Boundary |
|-----------|---------------|----------|
| `DebugManager` | Breakpoint/watchpoint storage & checking | Owns all BP/WP data, exposes query API |
| `Beast` | Emulation coordination | Calls DebugManager at integration points |
| `GUI` | Screen rendering | Reads DebugManager state for display |

**Integration Points:**

```
┌─────────────────────────────────────────────────────────────┐
│                         Beast                               │
│                                                             │
│  ┌─────────────┐    ┌──────────────┐    ┌───────────────┐  │
│  │   run()     │───▶│ DebugManager │◀───│  readMem()    │  │
│  │ (BP check)  │    │              │    │  writeMem()   │  │
│  └─────────────┘    │  - BPs [8]   │    │  (WP check)   │  │
│                     │  - WPs [8]   │    └───────────────┘  │
│                     │  - SysWPs[2] │                       │
│                     └──────────────┘                       │
│                            ▲                               │
│                            │                               │
│  ┌─────────────────────────┴───────────────────────────┐  │
│  │                      GUI                             │  │
│  │  Mode::BREAKPOINTS screen                            │  │
│  │  - Renders BP/WP lists                               │  │
│  │  - Handles add/edit/delete                           │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### Requirements to Structure Mapping

**FR Category → Location:**

| Requirement Category | Primary Location | Integration Points |
|---------------------|------------------|-------------------|
| Breakpoint Management (FR1-8) | `debugmanager.cpp` | `beast.cpp` (checking) |
| Watchpoint Management (FR9-17) | `debugmanager.cpp` | `beast.cpp` (checking) |
| Debugging UI (FR18-22) | `gui.cpp`, `beast.cpp` | Mode enum, rendering |
| Emulation Integration (FR23-26) | `beast.cpp` | `run()`, `readMem()`, `writeMem()` |

**Specific File Changes:**

| File | Changes Required |
|------|-----------------|
| `src/debugmanager.hpp` | NEW - Class definition, structs |
| `src/debugmanager.cpp` | NEW - Implementation |
| `src/beast.hpp` | Add `DebugManager*` member, `BREAKPOINTS` to Mode enum |
| `src/beast.cpp` | Integration calls in `run()`, `readMem()`, `writeMem()` |
| `src/gui.hpp` | Add rendering method declarations for BP/WP screen |
| `src/gui.cpp` | Implement BP/WP screen rendering |
| `CMakeLists.txt` | Add `debugmanager.cpp` to sources |

### Data Flow

**Breakpoint Check Flow:**
```
z80_tick() → z80_opdone() → Beast::run()
                               │
                               ▼
                    debugManager->hasActiveBreakpoints()
                               │
                               ▼ (if true)
                    debugManager->checkBreakpoint(pc, memoryPage)
                               │
                               ▼ (if hit)
                         mode = DEBUG
```

**Watchpoint Check Flow:**
```
Beast::readMem() / Beast::writeMem()
              │
              ▼
   debugManager->hasActiveWatchpoints()
              │
              ▼ (if true)
   debugManager->checkWatchpoint(physAddr, isRead)
              │
              ▼ (if hit)
         mode = DEBUG
```

### Build Integration

**CMakeLists.txt addition:**
```cmake
set(SOURCES
    # ... existing sources ...
    src/debugmanager.cpp        # Add this line
)
```

No new dependencies required - uses existing SDL2 and standard C++.

## Architecture Validation Results

### Coherence Validation ✅

**Decision Compatibility:**
All architectural decisions are compatible:
- Fixed arrays work well with C++ performance requirements
- DebugManager class follows existing component patterns (I2c, VideoBeast)
- Hybrid checking strategy aligns with emulation loop structure
- UI mode approach matches existing state machine

**Pattern Consistency:**
Implementation patterns are consistent with existing codebase:
- Naming conventions match beast.cpp, gui.cpp, i2c.cpp patterns
- File organization follows established src/*.cpp pattern
- Class structure mirrors existing component design

**Structure Alignment:**
Project structure properly supports architecture:
- New files integrate cleanly into existing layout
- No restructuring required
- Clear boundaries between components

### Requirements Coverage Validation ✅

**Functional Requirements Coverage:**
All 26 FRs have direct architectural support:
- FR1-8 (Breakpoints): DebugManager CRUD API
- FR9-17 (Watchpoints): DebugManager CRUD API with range/type support
- FR18-22 (UI): Mode::BREAKPOINTS screen with tab navigation
- FR23-26 (Integration): Hybrid checking in emulation loop

**Non-Functional Requirements Coverage:**
All 13 NFRs are architecturally addressed:
- NFR1-4 (Performance): Fast-path checks via hasActiveBreakpoints/hasActiveWatchpoints
- NFR5-7 (Accuracy): Correct integration points ensure 100% trigger accuracy
- NFR8-10 (UI Consistency): Follows existing GUI patterns and constants
- NFR11-13 (Integration): Clean API enables future DeZog integration

### Implementation Readiness Validation ✅

**Decision Completeness:**
- All critical decisions documented with rationale
- Data structures fully specified
- Integration points clearly defined
- No ambiguous areas for AI agents

**Structure Completeness:**
- All new files identified (debugmanager.hpp/.cpp)
- All modified files listed with required changes
- Build integration specified (CMakeLists.txt)
- Component boundaries well-defined

**Pattern Completeness:**
- Naming conventions comprehensive
- Code examples provided for all patterns
- Anti-patterns documented
- Integration patterns with code snippets

### Gap Analysis Results

**Critical Gaps:** None identified

**Important Gaps:** None identified

**Nice-to-Have (Future Enhancements):**
- Persistence/save-load of breakpoints (explicitly deferred per PRD)
- DeZog DZRP 2.0 protocol details (Phase 2)
- Jump-to-address from breakpoint list (explicitly not in MVP)

### Architecture Completeness Checklist

**✅ Requirements Analysis**
- [x] Project context thoroughly analyzed
- [x] Scale and complexity assessed (Medium)
- [x] Technical constraints identified (brownfield C++)
- [x] Cross-cutting concerns mapped (performance, UI state)

**✅ Architectural Decisions**
- [x] Data structures: Fixed arrays
- [x] Component design: Separate DebugManager class
- [x] Integration strategy: Hybrid BP/WP checking
- [x] Addressing: User-selectable logical/physical
- [x] UI: New Mode enum value

**✅ Implementation Patterns**
- [x] Naming conventions established (camelCase, SCREAMING_SNAKE)
- [x] Structure patterns defined (class organization, file layout)
- [x] Code patterns with examples (API design, integration)
- [x] Anti-patterns documented

**✅ Project Structure**
- [x] New files identified (debugmanager.hpp/.cpp)
- [x] Modified files mapped (beast.*, gui.*, CMakeLists.txt)
- [x] Integration points specified
- [x] Requirements to structure mapping complete

### Architecture Readiness Assessment

**Overall Status:** READY FOR IMPLEMENTATION

**Confidence Level:** High

**Key Strengths:**
- Clean separation via DebugManager class
- Performance-optimized with fast-path checks
- Follows all existing codebase patterns
- Clear API for future DeZog integration
- Comprehensive code examples

**Areas for Future Enhancement:**
- DeZog DZRP 2.0 protocol (Phase 2)
- Breakpoint/watchpoint persistence (if requested)

### Implementation Handoff

**AI Agent Guidelines:**
- Follow all architectural decisions exactly as documented
- Use implementation patterns consistently
- Place new files in src/ directory
- Follow existing naming conventions
- Respect DebugManager as the single source of truth for BP/WP state

**First Implementation Priority:**
1. Create `src/debugmanager.hpp` with struct definitions and class declaration
2. Create `src/debugmanager.cpp` with CRUD implementation
3. Add to CMakeLists.txt
4. Integrate into Beast class

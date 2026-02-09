# Story 1.1: Breakpoint Management Core

Status: done

## Story

As a developer debugging Z80 code,
I want the emulator to support breakpoints that pause execution at specific addresses,
So that I can examine CPU state at exact points in my code.

## Acceptance Criteria

1. **Given** the DebugManager class is instantiated
   **When** `addBreakpoint(0x4080, false)` is called
   **Then** a logical breakpoint at $4080 is stored and index returned

2. **Given** a breakpoint exists at logical address $4080
   **When** the Z80 executes an instruction at PC=$4080
   **Then** `checkBreakpoint()` returns true and emulation enters DEBUG mode

3. **Given** a physical breakpoint at $20080 (bank 8, offset $0080)
   **When** code at logical $4080 executes with bank 8 paged into slot 1
   **Then** the breakpoint triggers only for that specific physical location
   _(Physical address format: bits 18-14 = bank number, bits 13-0 = offset within 16KB page)_

4. **Given** a disabled breakpoint at $4080
   **When** the Z80 executes at $4080
   **Then** `checkBreakpoint()` returns false and execution continues

5. **Given** no breakpoints are enabled
   **When** `hasActiveBreakpoints()` is called
   **Then** it returns false (allowing fast-path skip of checking loop)

6. **Given** 8 breakpoints already exist
   **When** `addBreakpoint()` is called
   **Then** it returns -1 indicating the list is full

## Tasks / Subtasks

- [x] Task 1: Create DebugManager class with breakpoint data structures (AC: #1, #6)
  - [x] 1.1 Create `src/debugmanager.hpp` with Breakpoint struct and class declaration
  - [x] 1.2 Create `src/debugmanager.cpp` with implementation
  - [x] 1.3 Add `src/debugmanager.cpp` to CMakeLists.txt SOURCES

- [x] Task 2: Implement Breakpoint CRUD operations (AC: #1, #6)
  - [x] 2.1 Implement `addBreakpoint(uint32_t address, bool isPhysical)` - returns index or -1
  - [x] 2.2 Implement `removeBreakpoint(int index)` - returns bool
  - [x] 2.3 Implement `setBreakpointEnabled(int index, bool enabled)`
  - [x] 2.4 Implement `getBreakpoint(int index)` - returns Breakpoint* or nullptr
  - [x] 2.5 Implement `getBreakpointCount()` - returns int

- [x] Task 3: Implement breakpoint checking logic (AC: #2, #3, #4, #5)
  - [x] 3.1 Implement `hasActiveBreakpoints()` - fast-path check returns bool
  - [x] 3.2 Implement `checkBreakpoint(uint16_t pc, uint8_t* memoryPage)` - returns bool
  - [x] 3.3 Handle logical address matching (16-bit comparison)
  - [x] 3.4 Handle physical address matching (20-bit with page resolution)
  - [x] 3.5 Skip disabled breakpoints in check loop

- [x] Task 4: Integrate DebugManager into Beast class (AC: #2, #3)
  - [x] 4.1 Add `DebugManager* debugManager` member to Beast class
  - [x] 4.2 Instantiate DebugManager in Beast constructor
  - [x] 4.3 Add breakpoint check at instruction boundary in `run()` loop
  - [x] 4.4 Call `debugManager->checkBreakpoint()` when `z80_opdone()` returns true
  - [x] 4.5 Set `mode = DEBUG` when breakpoint triggers

## Dev Notes

### Architecture Compliance

**File Locations (per architecture.md):**
- NEW: `src/debugmanager.hpp` - Class definition with Breakpoint struct
- NEW: `src/debugmanager.cpp` - Implementation
- MODIFY: `src/beast.hpp` - Add DebugManager* member
- MODIFY: `src/beast.cpp` - Integration calls in run()
- MODIFY: `CMakeLists.txt` - Add debugmanager.cpp to sources

**Data Structures (per architecture.md):**
```cpp
struct Breakpoint {
    uint32_t address;      // 16-bit logical or 20-bit physical
    bool     isPhysical;   // true = physical, false = logical
    bool     enabled;
};

class DebugManager {
public:
    // Breakpoint CRUD
    int  addBreakpoint(uint32_t address, bool isPhysical);
    bool removeBreakpoint(int index);
    void setBreakpointEnabled(int index, bool enabled);
    Breakpoint* getBreakpoint(int index);
    int  getBreakpointCount();

    // Emulation integration
    bool checkBreakpoint(uint16_t pc, uint8_t* memoryPage);
    bool hasActiveBreakpoints();

private:
    static const int MAX_BREAKPOINTS = 8;
    Breakpoint breakpoints[MAX_BREAKPOINTS];
    int breakpointCount = 0;
};
```

**Integration Pattern (per architecture.md):**
```cpp
// In Beast::run() at instruction boundary
if (z80_opdone(&cpu) && debugManager->hasActiveBreakpoints()) {
    if (debugManager->checkBreakpoint(Z80_GET_ADDR(pins), memoryPage)) {
        mode = DEBUG;
    }
}
```

### Naming Conventions (per architecture.md)

| Element | Convention | Examples |
|---------|------------|----------|
| Classes | PascalCase | `DebugManager`, `Breakpoint` |
| Methods | camelCase | `addBreakpoint()`, `checkBreakpoint()` |
| Member variables | camelCase | `breakpointCount`, `isPhysical` |
| Constants | SCREAMING_SNAKE | `MAX_BREAKPOINTS` |
| Files | lowercase | `debugmanager.hpp`, `debugmanager.cpp` |

### Technical Requirements

**Performance Requirements (NFR1, NFR3):**
- Breakpoint checking adds overhead ONLY when breakpoints are enabled
- With no breakpoints enabled, emulation performance is unaffected
- Use `hasActiveBreakpoints()` fast-path check before iterating

**Accuracy Requirements (NFR5, NFR7):**
- Breakpoints trigger on the EXACT instruction at the specified address
- No misses, no false triggers
- Bank-specific breakpoints only trigger when correct bank is paged in

**Addressing Modes (per architecture.md):**
- Logical (16-bit): Triggers regardless of which bank is paged in
- Physical (20-bit): Triggers only for exact physical location
- Address length determines mode: 4 hex digits = logical, 5 hex digits = physical

### Existing Codebase Patterns

**Beast.hpp observations (lines 30-32):**
```cpp
enum Mode {RUN, STEP, OUT, OVER, TAKE, DEBUG, FILES, QUIT};
```
- `DEBUG` mode already exists - use this when breakpoint triggers

**Beast.hpp observations (lines 145-146):**
```cpp
uint64_t breakpoint = NOT_SET;  // Single breakpoint exists
uint64_t lastBreakpoint = 0;
```
- Single breakpoint mechanism exists but this story creates the new multi-breakpoint DebugManager

**Beast.hpp observations (lines 158-161):**
```cpp
bool       pagingEnabled = false;
uint8_t    readMem(uint16_t address);
uint8_t    readPage(int page, uint16_t address);
void       writeMem(int page, uint16_t address, uint8_t data);
```
- `pagingEnabled` controls memory banking
- `memoryPage[4]` array at line 111 tracks current page configuration

**Z80 Integration:**
- `z80_opdone()` marks instruction boundaries (per architecture.md)
- `Z80_GET_ADDR(pins)` gets current PC value
- Floooh Chips library - header-only at `src/z80.h`

### Project Structure Notes

**Source tree alignment:**
- New files go in `src/` directory (matches i2c.cpp, videobeast.cpp, gui.cpp pattern)
- Header/implementation pairs: .hpp/.cpp (matches existing pattern)
- Use `#pragma once` for header guards (matches all existing headers)

**Build integration:**
- Add to CMakeLists.txt SOURCES list (see existing pattern with other .cpp files)
- No new dependencies required - uses standard C++17

### Anti-Patterns to Avoid

- Do NOT use `std::vector` for fixed-size breakpoint array (use C array with count)
- Do NOT add heap allocations in the hot path
- Do NOT create new enum types outside classes unless shared
- Do NOT use different naming conventions than existing code
- Do NOT forget the fast-path check before iteration

### References

- [Source: _bmad-output/planning-artifacts/architecture.md#Core-Architectural-Decisions]
- [Source: _bmad-output/planning-artifacts/architecture.md#Implementation-Patterns]
- [Source: _bmad-output/planning-artifacts/architecture.md#Project-Structure-Boundaries]
- [Source: _bmad-output/planning-artifacts/epics.md#Story-1.1]
- [Source: _bmad-output/planning-artifacts/prd.md#Functional-Requirements]
- [Source: _bmad-output/planning-artifacts/prd.md#Non-Functional-Requirements]
- [Source: src/beast.hpp:30-32] Mode enum definition
- [Source: src/beast.hpp:145-146] Existing single breakpoint mechanism
- [Source: src/beast.hpp:111] memoryPage array for bank tracking

## Dev Agent Record

### Agent Model Used

Claude Opus 4.5 (claude-opus-4-5-20251101)

### Debug Log References

- All 12 unit tests passing for DebugManager class

### Completion Notes List

- Created DebugManager class with Breakpoint struct following architecture.md patterns
- Implemented all CRUD operations: addBreakpoint, removeBreakpoint, setBreakpointEnabled, getBreakpoint, getBreakpointCount
- Implemented breakpoint checking with hasActiveBreakpoints() fast-path optimization
- Physical address matching uses 20-bit calculation: (page & 0x1F) << 14 | (pc & 0x3FFF)
- Logical address matching uses simple 16-bit comparison
- Integrated DebugManager into Beast class at instruction boundaries (z80_opdone)
- Added comprehensive test suite with tests for all 6 acceptance criteria
- Build verified on Linux platform with C++23

### File List

- NEW: src/debugmanager.hpp - DebugManager class declaration with Breakpoint struct
- NEW: src/debugmanager.cpp - DebugManager implementation
- NEW: tests/test_debugmanager.cpp - Unit tests for DebugManager (14 tests)
- MODIFIED: src/beast.hpp - Added DebugManager include and member pointer
- MODIFIED: src/beast.cpp - Instantiate DebugManager, breakpoint check in run() loop, cleanup in destructor
- MODIFIED: CMakeLists.txt - Added debugmanager.cpp to sources, added test_debugmanager target

### Change Log

- 2026-01-30: Initial implementation of Story 1-1 Breakpoint Management Core - all tasks complete
- 2026-01-30: Code review fixes applied:
  - Added clearAllBreakpoints() method (M1)
  - Added findBreakpointByAddress() method (M2)
  - Changed getBreakpoint() to return const pointer (M3)
  - Zero-initialized breakpoints array (M4)
  - Fixed AC#3 physical address notation (H1)
  - Added const correctness to query methods
  - Added 2 new unit tests for new methods


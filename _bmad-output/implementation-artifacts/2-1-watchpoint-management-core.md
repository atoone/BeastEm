# Story 2.1: Watchpoint Management Core

Status: done

## Story

As a developer hunting memory corruption,
I want the emulator to monitor memory regions and pause when accessed,
So that I can catch buffer overflows and unexpected memory writes instantly.

## Acceptance Criteria

1. **Given** the DebugManager class
   **When** `addWatchpoint(0x8000, 32, false, false, true)` is called
   **Then** a write-only watchpoint monitoring $8000-$801F is stored and index returned

2. **Given** a write watchpoint on $8000-$801F
   **When** code writes to address $8010
   **Then** `checkWatchpoint()` returns true and emulation enters DEBUG mode

3. **Given** a read watchpoint on $C000-$C0FF
   **When** code reads from $C050
   **Then** the watchpoint triggers

4. **Given** a read/write watchpoint on $8000-$800F
   **When** code either reads or writes within that range
   **Then** the watchpoint triggers

5. **Given** a write watchpoint on $8000-$801F
   **When** code reads from $8010 (but doesn't write)
   **Then** the watchpoint does NOT trigger

6. **Given** a disabled watchpoint
   **When** memory in its range is accessed
   **Then** `checkWatchpoint()` returns false and execution continues

7. **Given** no watchpoints are enabled
   **When** `hasActiveWatchpoints()` is called
   **Then** it returns false (allowing fast-path skip in memory access functions)

8. **Given** 8 user watchpoints already exist
   **When** `addWatchpoint()` is called
   **Then** it returns -1 indicating the list is full

9. **Given** system breakpoints are needed programmatically
   **When** `addSystemBreakpoint()` is called
   **Then** it uses the separate 2-slot system array (not visible to user)

## Tasks / Subtasks

- [x] Task 1: Add Watchpoint struct to debugmanager.hpp (AC: #1)
  - [x] 1.1 Define `Watchpoint` struct with fields: `address` (uint32_t), `length` (uint16_t), `isPhysical` (bool), `enabled` (bool), `onRead` (bool), `onWrite` (bool)
  - [x] 1.2 Add `static const int MAX_WATCHPOINTS = 8;` constant
  - [x] 1.3 Add `Watchpoint watchpoints[MAX_WATCHPOINTS] = {};` array
  - [x] 1.4 Add `int watchpointCount = 0;` member

- [x] Task 2: Implement Watchpoint CRUD operations (AC: #1, #8)
  - [x] 2.1 Implement `addWatchpoint(uint32_t address, uint16_t length, bool isPhysical, bool onRead, bool onWrite)` - returns index or -1
  - [x] 2.2 Implement `removeWatchpoint(int index)` - returns bool, compacts array
  - [x] 2.3 Implement `setWatchpointEnabled(int index, bool enabled)`
  - [x] 2.4 Implement `getWatchpoint(int index)` - returns const Watchpoint* or nullptr
  - [x] 2.5 Implement `getWatchpointCount()` - returns int
  - [x] 2.6 Implement `clearAllWatchpoints()`
  - [x] 2.7 Implement `findWatchpointByAddress(uint32_t address, bool isPhysical)` - returns index or -1

- [x] Task 3: Implement watchpoint checking logic (AC: #2, #3, #4, #5, #6, #7)
  - [x] 3.1 Implement `hasActiveWatchpoints()` - fast-path check returns bool
  - [x] 3.2 Implement `checkWatchpoint(uint16_t logicalAddress, uint32_t physicalAddress, bool isRead)` - returns bool
  - [x] 3.3 Handle range checking: `address >= wp.address && address < wp.address + wp.length`
  - [x] 3.4 Handle read/write filtering: check `onRead`/`onWrite` flags against `isRead` parameter
  - [x] 3.5 Handle logical vs physical address resolution: logical watchpoints match Z80 address regardless of banking; physical watchpoints match only when specific memory location accessed
  - [x] 3.6 Skip disabled watchpoints in check loop

- [x] Task 4: Integrate watchpoint checking into Beast memory operations (AC: #2, #3, #4, #5)
  - [x] 4.1 Modify Beast::run() Z80 memory access to call `debugManager->checkWatchpoint()` with `isRead=true` for reads
  - [x] 4.2 Modify Beast::run() Z80 memory access to call `debugManager->checkWatchpoint()` with `isRead=false` for writes
  - [x] 4.3 Use `hasActiveWatchpoints()` fast-path before calling `checkWatchpoint()`
  - [x] 4.4 Set `mode = DEBUG` when watchpoint triggers
  - [x] 4.5 Physical address already calculated in run() loop: `mappedAddr = (addr & 0x3FFF) | ((page & 0x1F) << 14)`

- [x] Task 5: Add unit tests for watchpoint functionality (All ACs)
  - [x] 5.1 Test `addWatchpoint` returns valid index and stores watchpoint
  - [x] 5.2 Test `addWatchpoint` returns -1 when 8 watchpoints exist
  - [x] 5.3 Test `removeWatchpoint` compacts array correctly
  - [x] 5.4 Test `checkWatchpoint` triggers on write within range
  - [x] 5.5 Test `checkWatchpoint` triggers on read within range
  - [x] 5.6 Test `checkWatchpoint` does NOT trigger on read when write-only
  - [x] 5.7 Test `checkWatchpoint` does NOT trigger on write when read-only
  - [x] 5.8 Test `checkWatchpoint` does NOT trigger when disabled
  - [x] 5.9 Test `hasActiveWatchpoints` returns false when none enabled
  - [x] 5.10 Test `hasActiveWatchpoints` returns true when at least one enabled

## Dev Notes

### Architecture Compliance

**File Changes Required (per architecture.md):**
- MODIFY: `src/debugmanager.hpp` - Add Watchpoint struct, watchpoint CRUD declarations, checking declarations
- MODIFY: `src/debugmanager.cpp` - Implement watchpoint CRUD and checking logic
- MODIFY: `src/beast.cpp` - Integrate watchpoint checking into `readMem()` and `writeMem()`
- MODIFY: `tests/test_debugmanager.cpp` - Add watchpoint unit tests

**NO NEW FILES - extend existing DebugManager infrastructure**

**Data Structures (per architecture.md):**
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

**Integration Pattern (per architecture.md):**
```cpp
// In Beast::readMem() and Beast::writeMem()
if (debugManager->hasActiveWatchpoints()) {
    uint32_t physicalAddress = calculatePhysicalAddress(address);
    if (debugManager->checkWatchpoint(physicalAddress, isRead)) {
        mode = DEBUG;
    }
}
```

### Technical Requirements

**Performance Requirements (NFR2, NFR3):**
- Watchpoint checking adds overhead ONLY when watchpoints are enabled
- With no watchpoints enabled, emulation performance is unaffected
- Use `hasActiveWatchpoints()` fast-path check before iterating
- This is CRITICAL - memory access functions are called on EVERY memory operation

**Accuracy Requirements (NFR6):**
- Watchpoints detect ALL memory accesses within the monitored range (no misses)
- Range checking must be inclusive of start, exclusive of end: `[address, address+length)`
- Read/write filtering must be exact - no false triggers

**Address Resolution:**
- Logical address (16-bit): Use `memoryPage[]` array to resolve to physical
- Physical address (20-bit): Use directly
- Physical address calculation: `physicalAddress = (memoryPage[slot] << 14) | (logicalAddress & 0x3FFF)` where `slot = logicalAddress >> 14`

### Previous Story Intelligence

**From Story 1-1 and 1-2 (Breakpoints):**
- DebugManager already exists at `src/debugmanager.hpp/.cpp`
- Breakpoint struct pattern established - follow same for Watchpoint
- CRUD pattern established: add, remove, setEnabled, get, getCount, clearAll, findByAddress
- Physical address format: bits 18-14 = bank number, bits 13-0 = offset within 16KB page
- Unit tests exist at `tests/test_debugmanager.cpp` - extend with watchpoint tests
- `hasActiveBreakpoints()` pattern exists - replicate for `hasActiveWatchpoints()`

**Code Review Learnings from Epic 1:**
- Zero-initialize arrays: `Watchpoint watchpoints[MAX_WATCHPOINTS] = {};`
- Use `const` return types for getters: `const Watchpoint* getWatchpoint(int index) const;`
- Add helper methods like `findByAddress()` and `clearAll()` from the start

**Files created in Epic 1:**
- `src/debugmanager.hpp` - Extends with Watchpoint (MODIFY)
- `src/debugmanager.cpp` - Extends with watchpoint implementation (MODIFY)
- `tests/test_debugmanager.cpp` - Extends with watchpoint tests (MODIFY)

### Existing Codebase Patterns

**Current DebugManager API (from debugmanager.hpp):**
```cpp
// User breakpoint CRUD (8 slots)
int  addBreakpoint(uint32_t address, bool isPhysical);
bool removeBreakpoint(int index);
void setBreakpointEnabled(int index, bool enabled);
const Breakpoint* getBreakpoint(int index) const;
int  getBreakpointCount() const;
void clearAllBreakpoints();
int  findBreakpointByAddress(uint32_t address, bool isPhysical) const;

// System breakpoints (2 slots)
void setSystemBreakpoint(int index, uint32_t address, bool isPhysical);
void clearSystemBreakpoint(int index);
void clearAllSystemBreakpoints();
const Breakpoint* getSystemBreakpoint(int index) const;

// Emulation integration
bool checkBreakpoint(uint16_t pc, uint8_t* memoryPage) const;
bool hasActiveBreakpoints() const;
```

**Mirror this pattern for watchpoints - same structure, different parameters.**

**Beast memory operations (from beast.hpp/cpp):**
```cpp
uint8_t readMem(uint16_t address);
void writeMem(int page, uint16_t address, uint8_t data);
```
- `memoryPage[4]` array tracks current page configuration
- Each page is 16KB (0x4000 bytes)
- Physical address = `(memoryPage[slot] << 14) | (address & 0x3FFF)`

### Naming Conventions (per architecture.md)

| Element | Convention | Examples |
|---------|------------|----------|
| Structs | PascalCase | `Watchpoint` |
| Methods | camelCase | `addWatchpoint()`, `checkWatchpoint()`, `hasActiveWatchpoints()` |
| Member variables | camelCase | `watchpointCount`, `onRead`, `onWrite` |
| Constants | SCREAMING_SNAKE | `MAX_WATCHPOINTS` |

### Anti-Patterns to Avoid

- Do NOT use `std::vector` for fixed-size watchpoint array (use C array with count)
- Do NOT add heap allocations in the memory access hot path
- Do NOT forget the fast-path check before iteration - `hasActiveWatchpoints()` is CRITICAL for performance
- Do NOT break existing breakpoint functionality while adding watchpoints
- Do NOT use range checking that could miss edge cases (off-by-one errors)
- Do NOT duplicate physical address calculation logic - extract to helper if needed

### References

- [Source: _bmad-output/planning-artifacts/architecture.md#Core-Architectural-Decisions]
- [Source: _bmad-output/planning-artifacts/architecture.md#Implementation-Patterns]
- [Source: _bmad-output/planning-artifacts/architecture.md#Data-Flow]
- [Source: _bmad-output/planning-artifacts/epics.md#Story-2.1]
- [Source: _bmad-output/planning-artifacts/prd.md#Watchpoint-Management]
- [Source: _bmad-output/planning-artifacts/prd.md#Non-Functional-Requirements]
- [Source: src/debugmanager.hpp] Current DebugManager API
- [Source: src/debugmanager.cpp] DebugManager implementation patterns
- [Source: src/beast.hpp:158-161] Memory operation declarations
- [Source: src/beast.hpp:111] memoryPage array for bank tracking
- [Source: _bmad-output/implementation-artifacts/1-1-breakpoint-management-core.md] Previous story learnings
- [Source: _bmad-output/implementation-artifacts/1-2-breakpoints-ui-screen.md] Code review fixes

## Dev Agent Record

### Agent Model Used

Claude Opus 4.5 (claude-opus-4-5-20251101)

### Debug Log References

None - implementation proceeded without issues

### Completion Notes List

- **Task 1 completed**: Added Watchpoint struct to debugmanager.hpp with all required fields (address, length, isPhysical, enabled, onRead, onWrite), MAX_WATCHPOINTS constant, zero-initialized array, and count member.
- **Task 2 completed**: Implemented all CRUD operations following the existing breakpoint pattern - addWatchpoint, removeWatchpoint, setWatchpointEnabled, getWatchpoint, getWatchpointCount, clearAllWatchpoints, findWatchpointByAddress.
- **Task 3 completed**: Implemented hasActiveWatchpoints() for fast-path checking and checkWatchpoint(logicalAddress, physicalAddress, isRead) with range checking [address, address+length), read/write filtering, and disabled watchpoint skipping. Supports both logical watchpoints (trigger on Z80 address regardless of banking) and physical watchpoints (trigger only on specific physical memory location).
- **Task 4 completed**: Integrated watchpoint checking into Beast::run() Z80 memory access section. NOTE: Integration was done in the emulation hot-loop (run() function) where actual Z80 memory operations occur, not in the utility readMem()/writeMem() functions which are only used for debugging displays. Passes both logical and physical addresses to enable proper logical/physical watchpoint behavior. Fast-path check ensures zero overhead when no watchpoints are active.
- **Task 5 completed**: Added 18 unit tests covering all acceptance criteria - add/remove/clear operations, max capacity, range checking, read/write filtering, disabled state, hasActiveWatchpoints states, and logical vs physical address matching.

### Change Log

- 2026-01-30: Implemented watchpoint management core - all 5 tasks completed with full test coverage
- 2026-01-30: Fixed logical vs physical address handling - logical watchpoints now trigger on Z80 address regardless of banking; physical watchpoints honour bank settings
- 2026-01-30: **Code Review Fixes Applied:**
  - Fixed physical address calculation when paging disabled (was using incomplete address)
  - Added validation to reject useless watchpoints (length=0 or no trigger type)
  - Fixed potential integer overflow in range checking
  - Renamed `findWatchpointByAddress` → `findWatchpointByStartAddress` for API clarity
  - Added missing unit tests for empty watchpoint list and parameter validation
  - Improved test clarity by using 0xDEADBEEF for irrelevant physical addresses

### File List

- MODIFIED: `src/debugmanager.hpp` - Added Watchpoint struct, CRUD declarations, checking method declarations (checkWatchpoint takes both logical and physical addresses); renamed findWatchpointByStartAddress
- MODIFIED: `src/debugmanager.cpp` - Implemented watchpoint CRUD and checking logic with logical/physical address support, parameter validation, overflow-safe range check (~100 new lines)
- MODIFIED: `src/beast.cpp` - Integrated watchpoint checking into Z80 memory access loop, passing both logical and physical addresses; fixed paging-disabled case (~18 new lines)
- MODIFIED: `tests/test_debugmanager.cpp` - Added 20 watchpoint unit tests including logical vs physical address tests, validation tests, empty list test (~260 new lines)


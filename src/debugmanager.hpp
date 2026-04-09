#pragma once
#include <string>
#include <deque>
#include <vector>
#include <cstdint>
#include <optional>
#include <functional>
#include "z80.h"

enum TraceValue {PC, SP, A, F, B, C, D, E, H, L, BC, DE, HL, IX, IY};

enum TraceType {BYTE, WORD, ADDRESS, STRING};

struct TraceMap {
    // A Map converts a traced value to a (string) label
    uint16_t    value;
    std::string name;
};

struct Trace {
    // A trace keeps track of data at the point the trace is triggered.
    TraceValue traceValue;  // Which register is being monitored
    TraceType  traceType;
    int        modifier = 0xFFFF;  // Mask for BYTE and WORD values, length of data for ADDRESS, terminator for STRING
    bool       isMap    = false;
    std::vector<TraceMap>   map;
    std::string unknownMap = "UNKNOWN";  // The value to display when a map is not matched
};

struct Breakpoint {
    uint32_t address;      // 16-bit logical or 20-bit physical
    bool     isPhysical;   // true = physical, false = logical
    bool     enabled;
    bool     isTrace;
    std::string name = "";
    std::vector<Trace>    traces;
};

struct Watchpoint {
    uint32_t address;      // 16-bit logical or 20-bit physical
    uint16_t length;       // Number of bytes to monitor
    bool     isPhysical;   // true = physical, false = logical
    bool     enabled;
    bool     onRead;       // Trigger on reads
    bool     onWrite;      // Trigger on writes
};

struct BreakpointInfo {
    int index;      // 0-7
    bool enabled;
    bool isTrace;
};

struct DataLog {
    TraceValue  traceValue;
    uint16_t    logicalAddress;
    uint32_t    physicalAddress;
    uint16_t    length;
    uint8_t*    data;
};

struct TraceLog {
    uint32_t    physicalAddress;
    uint64_t    tick;
    uint16_t    pc, sp, af, bc, de, hl, ix, iy;
    const Breakpoint* breakpoint;
    std::vector<DataLog> data;
};

class DebugManager {
public:
    // User breakpoint CRUD (8 slots)
    int  addBreakpoint(uint32_t address, bool isPhysical);
    int  addBreakpoint(Breakpoint* breakpoint);
    bool removeBreakpoint(int index);
    void updateBreakpoint(int index, uint32_t address, bool isPhysical);
    void setBreakpointEnabled(int index, bool enabled);
    void setBreakpointIsTrace(int index, bool isTrace);
    void setBreakpointName(int index, std::string name);
    const Breakpoint* getBreakpoint(int index) const;
    int  getBreakpointCount() const;
    void clearAllBreakpoints();
    int  findBreakpointByAddress(uint32_t address, bool isPhysical) const;

    // System breakpoints (2 slots, reserved for DeZog/step-over)
    void setSystemBreakpoint(int index, uint32_t address, bool isPhysical);
    void clearSystemBreakpoint(int index);
    void clearAllSystemBreakpoints();
    const Breakpoint* getSystemBreakpoint(int index) const;

    // Emulation integration (checks both user and system breakpoints)
    // Returns index of triggered breakpoint (0-7 for user, 8-9 for system) or -1 if none
    int checkBreakpoint(uint16_t pc, uint8_t* memoryPage) const;
    bool hasActiveBreakpoints() const;

    // Get breakpoint info for a logical address (for display purposes)
    // Simple version: only checks logical breakpoints
    std::optional<BreakpointInfo> getBreakpointAtAddress(uint16_t addr) const;
    // Extended version: checks both logical and physical breakpoints using current memory mapping
    std::optional<BreakpointInfo> getBreakpointAtAddress(uint16_t addr, uint8_t* memoryPage, bool pagingEnabled) const;

    // User watchpoint CRUD (8 slots)
    int  addWatchpoint(uint32_t address, uint16_t length, bool isPhysical, bool onRead, bool onWrite);
    bool removeWatchpoint(int index);
    void setWatchpointEnabled(int index, bool enabled);
    const Watchpoint* getWatchpoint(int index) const;
    int  getWatchpointCount() const;
    void clearAllWatchpoints();
    int  findWatchpointByStartAddress(uint32_t address, bool isPhysical) const;

    // Watchpoint emulation integration
    bool hasActiveWatchpoints() const;
    // Returns index of triggered watchpoint (0-7) or -1 if none
    int checkWatchpoint(uint16_t logicalAddress, uint32_t physicalAddress, bool isRead) const;

    void logTrace(const Breakpoint* breakpoint, z80_t cpu, uint32_t physicalAddress, uint64_t tick, std::function<uint8_t(uint16_t)> memRead);

    std::deque<TraceLog> *getTraceLogs();
    int getLogSize();

private:
    static const int MAX_BREAKPOINTS = 8;
    static const int MAX_SYSTEM_BREAKPOINTS = 2;
    static const int MAX_WATCHPOINTS = 8;

    static const int DEFAULT_TRACE_SIZE = 1000;

    Breakpoint* breakpoints[MAX_BREAKPOINTS] = {};
    Breakpoint systemBreakpoints[MAX_SYSTEM_BREAKPOINTS] = {};
    Watchpoint watchpoints[MAX_WATCHPOINTS] = {};
    int breakpointCount = 0;
    int watchpointCount = 0;
    bool activeBreakpoints = false;
    bool activeWatchpoints = false;

    void updateActiveBreakpoints();
    void updateActiveWatchpoints();

    std::deque<TraceLog> traceLogs;
    size_t maxTraceLogSize = DEFAULT_TRACE_SIZE;

    const Breakpoint BDOS_Trace = Breakpoint{ 0x05, false, true, true, "BDOS", {
        {C, BYTE, 0xFF, true, {
            {0, "P_TERMCPM: Reset"}, 
            {1, "C_READ: Console input"},
            {2, "C_WRITE: Console output"},
            {3, "A_READ: Aux input"},
            {4, "A_WRITE: Aux write"},
            {5, "L_WRITE: Printer output"},
            {6, "C_RAWIO: Direct console I/O"},
            {7, "GET_IO: Get I/O byte"},
            {8, "SET_IO: Set I/O byte"},
            {9, "C_WRITESTR: Output String"}, 
            {10, "C_READSTR: Buffered console input"},
            {11, "C_STAT: Console status"},
            {12, "S_BDOSVER: BDOS Version"},
            {13, "DRV_ALLRESET: Reset discs"},
            {14, "DRV_SET: Select disc"},
            {15, "F_OPEN: Open file"},
            {16, "F_CLOSE: Close file"},
            {17, "F_SFIRST: Search first"},
            {18, "F_SNEXT: Search next"},
            {19, "F_DELETE: Delete file"},
            {20, "F_READ: Read next record"},
            {21, "F_WRITE: Write next record"},
            {22, "F_MAKE: Create file"},
            {23, "F_RENAME: Rename file"},
            {24, "DRV_LOGINVEC: Logged in drives"},
            {25, "DRV_GET: Get current drive"},
            {26, "F_DMAOFF: Set DMA address"},
            {27, "DRV_ALLOCVEC: Get allocation map"},
            {28, "DRV_SETRO: Write protect disk"},
            {29, "DRV_ROVEC: Get read-only drives"},
            {30, "F_ATTRIB: Set file attributes"},
            {31, "DRV_DPB: Get DPB Address"},
            {32, "F_USERNUM: Get/set user number"},
            {33, "F_READRAND: Random access read"},
            {34, "F_WRITERAND: Random access write"},
            {35, "F_SIZE: Compute file size"},
            {36, "F_RANDREC: Update random access ptr"},
            {37, "DRV_RESET: Reset drives"},
            {40, "F_WRITEZF: Write random, zero fill"}
        }},
        {E, BYTE},
        {DE, ADDRESS, 36}
    } };
};

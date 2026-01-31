#pragma once
#include <cstdint>
#include <optional>

struct Breakpoint {
    uint32_t address;      // 16-bit logical or 20-bit physical
    bool     isPhysical;   // true = physical, false = logical
    bool     enabled;
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
};

class DebugManager {
public:
    // User breakpoint CRUD (8 slots)
    int  addBreakpoint(uint32_t address, bool isPhysical);
    bool removeBreakpoint(int index);
    void setBreakpointEnabled(int index, bool enabled);
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

private:
    static const int MAX_BREAKPOINTS = 8;
    static const int MAX_SYSTEM_BREAKPOINTS = 2;
    static const int MAX_WATCHPOINTS = 8;
    Breakpoint breakpoints[MAX_BREAKPOINTS] = {};
    Breakpoint systemBreakpoints[MAX_SYSTEM_BREAKPOINTS] = {};
    Watchpoint watchpoints[MAX_WATCHPOINTS] = {};
    int breakpointCount = 0;
    int watchpointCount = 0;
};

#pragma once
#include <cstdint>

struct Breakpoint {
    uint32_t address;      // 16-bit logical or 20-bit physical
    bool     isPhysical;   // true = physical, false = logical
    bool     enabled;
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
    bool checkBreakpoint(uint16_t pc, uint8_t* memoryPage) const;
    bool hasActiveBreakpoints() const;

private:
    static const int MAX_BREAKPOINTS = 8;
    static const int MAX_SYSTEM_BREAKPOINTS = 2;
    Breakpoint breakpoints[MAX_BREAKPOINTS] = {};
    Breakpoint systemBreakpoints[MAX_SYSTEM_BREAKPOINTS] = {};
    int breakpointCount = 0;
};

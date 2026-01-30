#pragma once
#include <cstdint>

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
    const Breakpoint* getBreakpoint(int index) const;
    int  getBreakpointCount() const;
    void clearAllBreakpoints();
    int  findBreakpointByAddress(uint32_t address, bool isPhysical) const;

    // Emulation integration
    bool checkBreakpoint(uint16_t pc, uint8_t* memoryPage) const;
    bool hasActiveBreakpoints() const;

private:
    static const int MAX_BREAKPOINTS = 8;
    Breakpoint breakpoints[MAX_BREAKPOINTS] = {};  // Zero-initialize
    int breakpointCount = 0;
};

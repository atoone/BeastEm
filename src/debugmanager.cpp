#include "debugmanager.hpp"

int DebugManager::addBreakpoint(uint32_t address, bool isPhysical) {
    if (breakpointCount >= MAX_BREAKPOINTS) {
        return -1;
    }

    breakpoints[breakpointCount].address = address;
    breakpoints[breakpointCount].isPhysical = isPhysical;
    breakpoints[breakpointCount].enabled = true;

    return breakpointCount++;
}

bool DebugManager::removeBreakpoint(int index) {
    if (index < 0 || index >= breakpointCount) {
        return false;
    }

    // Shift remaining breakpoints down
    for (int i = index; i < breakpointCount - 1; i++) {
        breakpoints[i] = breakpoints[i + 1];
    }
    breakpointCount--;
    return true;
}

void DebugManager::setBreakpointEnabled(int index, bool enabled) {
    if (index >= 0 && index < breakpointCount) {
        breakpoints[index].enabled = enabled;
    }
}

const Breakpoint* DebugManager::getBreakpoint(int index) const {
    if (index < 0 || index >= breakpointCount) {
        return nullptr;
    }
    return &breakpoints[index];
}

int DebugManager::getBreakpointCount() const {
    return breakpointCount;
}

void DebugManager::clearAllBreakpoints() {
    breakpointCount = 0;
}

int DebugManager::findBreakpointByAddress(uint32_t address, bool isPhysical) const {
    for (int i = 0; i < breakpointCount; i++) {
        if (breakpoints[i].address == address && breakpoints[i].isPhysical == isPhysical) {
            return i;
        }
    }
    return -1;
}

// System breakpoint methods
void DebugManager::setSystemBreakpoint(int index, uint32_t address, bool isPhysical) {
    if (index >= 0 && index < MAX_SYSTEM_BREAKPOINTS) {
        systemBreakpoints[index].address = address;
        systemBreakpoints[index].isPhysical = isPhysical;
        systemBreakpoints[index].enabled = true;
    }
}

void DebugManager::clearSystemBreakpoint(int index) {
    if (index >= 0 && index < MAX_SYSTEM_BREAKPOINTS) {
        systemBreakpoints[index].enabled = false;
        systemBreakpoints[index].address = 0;
    }
}

void DebugManager::clearAllSystemBreakpoints() {
    for (int i = 0; i < MAX_SYSTEM_BREAKPOINTS; i++) {
        systemBreakpoints[i].enabled = false;
        systemBreakpoints[i].address = 0;
    }
}

const Breakpoint* DebugManager::getSystemBreakpoint(int index) const {
    if (index < 0 || index >= MAX_SYSTEM_BREAKPOINTS) {
        return nullptr;
    }
    return &systemBreakpoints[index];
}

bool DebugManager::hasActiveBreakpoints() const {
    // Check user breakpoints
    for (int i = 0; i < breakpointCount; i++) {
        if (breakpoints[i].enabled) {
            return true;
        }
    }
    // Check system breakpoints
    for (int i = 0; i < MAX_SYSTEM_BREAKPOINTS; i++) {
        if (systemBreakpoints[i].enabled) {
            return true;
        }
    }
    return false;
}

// Helper to check a single breakpoint against PC
static bool checkSingleBreakpoint(const Breakpoint& bp, uint16_t pc, uint8_t* memoryPage) {
    if (!bp.enabled) {
        return false;
    }

    if (bp.isPhysical) {
        // Physical address matching (20-bit with page resolution)
        int pageIndex = (pc >> 14) & 0x03;
        uint8_t page = memoryPage[pageIndex];
        uint32_t physicalAddr = (pc & 0x3FFF) | ((uint32_t)(page & 0x1F) << 14);
        return physicalAddr == bp.address;
    } else {
        // Logical address matching (16-bit comparison)
        return (uint16_t)bp.address == pc;
    }
}

bool DebugManager::checkBreakpoint(uint16_t pc, uint8_t* memoryPage) const {
    // Check user breakpoints
    for (int i = 0; i < breakpointCount; i++) {
        if (checkSingleBreakpoint(breakpoints[i], pc, memoryPage)) {
            return true;
        }
    }
    // Check system breakpoints
    for (int i = 0; i < MAX_SYSTEM_BREAKPOINTS; i++) {
        if (checkSingleBreakpoint(systemBreakpoints[i], pc, memoryPage)) {
            return true;
        }
    }
    return false;
}

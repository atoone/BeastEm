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

int DebugManager::checkBreakpoint(uint16_t pc, uint8_t* memoryPage) const {
    // Check user breakpoints
    for (int i = 0; i < breakpointCount; i++) {
        if (checkSingleBreakpoint(breakpoints[i], pc, memoryPage)) {
            return i;
        }
    }
    // Check system breakpoints
    for (int i = 0; i < MAX_SYSTEM_BREAKPOINTS; i++) {
        if (checkSingleBreakpoint(systemBreakpoints[i], pc, memoryPage)) {
            return MAX_BREAKPOINTS + i;  // Return 8-9 for system breakpoints
        }
    }
    return -1;
}

std::optional<BreakpointInfo> DebugManager::getBreakpointAtAddress(uint16_t addr) const {
    for (int i = 0; i < breakpointCount; i++) {
        // Only check logical breakpoints for display purposes
        if (!breakpoints[i].isPhysical && (uint16_t)breakpoints[i].address == addr) {
            return BreakpointInfo{i, breakpoints[i].enabled};
        }
    }
    return std::nullopt;
}

std::optional<BreakpointInfo> DebugManager::getBreakpointAtAddress(uint16_t addr, uint8_t* memoryPage, bool pagingEnabled) const {
    // Calculate physical address for the given logical address
    uint32_t physicalAddr;
    if (pagingEnabled) {
        int pageIndex = (addr >> 14) & 0x03;
        uint8_t page = memoryPage[pageIndex];
        physicalAddr = (addr & 0x3FFF) | ((uint32_t)(page & 0x1F) << 14);
    } else {
        physicalAddr = addr;
    }

    for (int i = 0; i < breakpointCount; i++) {
        if (breakpoints[i].isPhysical) {
            // Physical breakpoint: compare against calculated physical address
            if (breakpoints[i].address == physicalAddr) {
                return BreakpointInfo{i, breakpoints[i].enabled};
            }
        } else {
            // Logical breakpoint: compare against logical address
            if ((uint16_t)breakpoints[i].address == addr) {
                return BreakpointInfo{i, breakpoints[i].enabled};
            }
        }
    }
    return std::nullopt;
}

// Watchpoint CRUD methods
int DebugManager::addWatchpoint(uint32_t address, uint16_t length, bool isPhysical, bool onRead, bool onWrite) {
    if (watchpointCount >= MAX_WATCHPOINTS) {
        return -1;
    }

    // Reject useless watchpoints: must have non-zero length and at least one trigger type
    if (length == 0 || (!onRead && !onWrite)) {
        return -1;
    }

    watchpoints[watchpointCount].address = address;
    watchpoints[watchpointCount].length = length;
    watchpoints[watchpointCount].isPhysical = isPhysical;
    watchpoints[watchpointCount].enabled = true;
    watchpoints[watchpointCount].onRead = onRead;
    watchpoints[watchpointCount].onWrite = onWrite;

    return watchpointCount++;
}

bool DebugManager::removeWatchpoint(int index) {
    if (index < 0 || index >= watchpointCount) {
        return false;
    }

    // Shift remaining watchpoints down
    for (int i = index; i < watchpointCount - 1; i++) {
        watchpoints[i] = watchpoints[i + 1];
    }
    watchpointCount--;
    return true;
}

void DebugManager::setWatchpointEnabled(int index, bool enabled) {
    if (index >= 0 && index < watchpointCount) {
        watchpoints[index].enabled = enabled;
    }
}

const Watchpoint* DebugManager::getWatchpoint(int index) const {
    if (index < 0 || index >= watchpointCount) {
        return nullptr;
    }
    return &watchpoints[index];
}

int DebugManager::getWatchpointCount() const {
    return watchpointCount;
}

void DebugManager::clearAllWatchpoints() {
    watchpointCount = 0;
}

int DebugManager::findWatchpointByStartAddress(uint32_t address, bool isPhysical) const {
    for (int i = 0; i < watchpointCount; i++) {
        if (watchpoints[i].address == address && watchpoints[i].isPhysical == isPhysical) {
            return i;
        }
    }
    return -1;
}

bool DebugManager::hasActiveWatchpoints() const {
    for (int i = 0; i < watchpointCount; i++) {
        if (watchpoints[i].enabled) {
            return true;
        }
    }
    return false;
}

int DebugManager::checkWatchpoint(uint16_t logicalAddress, uint32_t physicalAddress, bool isRead) const {
    for (int i = 0; i < watchpointCount; i++) {
        const Watchpoint& wp = watchpoints[i];

        if (!wp.enabled) {
            continue;
        }

        // Check read/write filter
        if (isRead && !wp.onRead) {
            continue;
        }
        if (!isRead && !wp.onWrite) {
            continue;
        }

        // Choose address based on watchpoint type:
        // - Logical (isPhysical=false): matches regardless of bank settings
        // - Physical (isPhysical=true): honours bank settings
        uint32_t addressToCheck = wp.isPhysical ? physicalAddress : logicalAddress;

        // Range check: [address, address + length)
        // Use subtraction to avoid overflow: addressToCheck - wp.address < length
        if (addressToCheck >= wp.address && (addressToCheck - wp.address) < wp.length) {
            return i;
        }
    }
    return -1;
}

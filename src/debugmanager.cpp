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

bool DebugManager::hasActiveBreakpoints() const {
    for (int i = 0; i < breakpointCount; i++) {
        if (breakpoints[i].enabled) {
            return true;
        }
    }
    return false;
}

bool DebugManager::checkBreakpoint(uint16_t pc, uint8_t* memoryPage) const {
    for (int i = 0; i < breakpointCount; i++) {
        if (!breakpoints[i].enabled) {
            continue;
        }

        if (breakpoints[i].isPhysical) {
            // Physical address matching (20-bit with page resolution)
            // Calculate physical address from PC and current memory page
            int pageIndex = (pc >> 14) & 0x03;
            uint8_t page = memoryPage[pageIndex];
            uint32_t physicalAddr = (pc & 0x3FFF) | ((uint32_t)(page & 0x1F) << 14);

            if (physicalAddr == breakpoints[i].address) {
                return true;
            }
        } else {
            // Logical address matching (16-bit comparison)
            if ((uint16_t)breakpoints[i].address == pc) {
                return true;
            }
        }
    }
    return false;
}

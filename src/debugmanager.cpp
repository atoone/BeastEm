#include "debugmanager.hpp"
#include <functional>

bool DebugManager::addBreakpoint(uint32_t address, bool isPhysical) {
  if (breakpointCount >= MAX_BREAKPOINTS) {
    return false;
  }

  if (breakpoints[breakpointCount] == nullptr) {
    breakpoints[breakpointCount] = new Breakpoint;
  }

  breakpoints[breakpointCount]->address = address;
  breakpoints[breakpointCount]->isPhysical = isPhysical;
  breakpoints[breakpointCount]->enabled = true;
  breakpoints[breakpointCount]->isTrace = false;

  activeBreakpoints = true;
  breakpointCount++;
  return true;
}

bool DebugManager::addBreakpoint(const Breakpoint *breakpoint) {
  if (breakpointCount >= MAX_BREAKPOINTS) {
    return false;
  }

  for (size_t i=0; i<breakpointCount; i++) {
    if( breakpoints[i]->name == breakpoint->name) return false;
  }

  if (breakpoints[breakpointCount] == nullptr) {
    breakpoints[breakpointCount] = new Breakpoint;
  }

  copyBreakpoint(breakpoint, breakpoints[breakpointCount]);

  activeBreakpoints = true;
  breakpointCount++;
  return true;
}

void DebugManager::copyBreakpoint(const Breakpoint* source, Breakpoint* dest) {
  dest->address = source->address;
  dest->enabled = source->enabled;
  dest->isPhysical = source->isPhysical;
  dest->isTrace    = source->isTrace;
  dest->name       = source->name;
  dest->traces     = source->traces;
}

bool DebugManager::removeBreakpoint(size_t index) {
  if (index >= breakpointCount) {
    return false;
  }

  // Shift remaining breakpoints down
  for (size_t i = index; i < breakpointCount - 1; i++) {
    breakpoints[i] = breakpoints[i + 1];
  }
  breakpoints[--breakpointCount] = nullptr;
  
  updateActiveBreakpoints();
  return true;
}

void DebugManager::updateBreakpoint(size_t index, uint32_t address, bool isPhysical) {
  if (index < breakpointCount) {
    breakpoints[index]->address = address;
    breakpoints[index]->isPhysical = isPhysical;
  }
}

void DebugManager::setBreakpointEnabled(size_t index, bool enabled) {
  if (index < breakpointCount) {
    breakpoints[index]->enabled = enabled;
    updateActiveBreakpoints();
  }
}

void DebugManager::setBreakpointIsTrace(size_t index, bool isTrace) {
  if (index < breakpointCount) {
    breakpoints[index]->isTrace = isTrace;
  }
}

void DebugManager::setBreakpointName(size_t index, std::string name) {
  if (index < breakpointCount) {
    breakpoints[index]->name.assign(name);
  }
}

const Breakpoint *DebugManager::getBreakpoint(size_t index) const {
  if (index >= breakpointCount) {
    return nullptr;
  }
  return breakpoints[index];
}

size_t DebugManager::getBreakpointCount() const { return breakpointCount; }

void DebugManager::clearAllBreakpoints() {
  breakpointCount = 0;
  updateActiveBreakpoints();
}

bool DebugManager::findBreakpointByAddress(uint32_t address,
                                          bool isPhysical, size_t& index) const {
  for (size_t i = 0; i < breakpointCount; i++) {
    if (breakpoints[i]->address == address &&
        breakpoints[i]->isPhysical == isPhysical) {
      index = i;
      return true;
    }
  }
  return false;
}

// System breakpoint methods
void DebugManager::setSystemBreakpoint(size_t index, uint32_t address,
                                       bool isPhysical) {
  if (index < MAX_SYSTEM_BREAKPOINTS) {
    systemBreakpoints[index].address = address;
    systemBreakpoints[index].isPhysical = isPhysical;
    systemBreakpoints[index].enabled = true;
    activeBreakpoints = true;
  }
}

void DebugManager::clearSystemBreakpoint(size_t index) {
  if (index < MAX_SYSTEM_BREAKPOINTS) {
    systemBreakpoints[index].enabled = false;
    systemBreakpoints[index].address = 0;
    updateActiveBreakpoints();
  }
}

void DebugManager::clearAllSystemBreakpoints() {
  for (size_t i = 0; i < MAX_SYSTEM_BREAKPOINTS; i++) {
    systemBreakpoints[i].enabled = false;
    systemBreakpoints[i].address = 0;
  }
  updateActiveBreakpoints();
}

const Breakpoint *DebugManager::getSystemBreakpoint(size_t index) const {
  if (index >= MAX_SYSTEM_BREAKPOINTS) {
    return nullptr;
  }
  return &systemBreakpoints[index];
}

bool DebugManager::hasActiveBreakpoints() const { return activeBreakpoints; }

void DebugManager::updateActiveBreakpoints() {
  for (size_t i = 0; i < breakpointCount; i++) {
    if (breakpoints[i]->enabled) {
      activeBreakpoints = true;
      return;
    }
  }
  for (size_t i = 0; i < MAX_SYSTEM_BREAKPOINTS; i++) {
    if (systemBreakpoints[i].enabled) {
      activeBreakpoints = true;
      return;
    }
  }
  activeBreakpoints = false;
}

// Helper to check a single breakpoint against PC
static bool checkSingleBreakpoint(const Breakpoint &bp, uint16_t pc,
                                  uint8_t *memoryPage) {
  if (!bp.enabled) {
    return false;
  }

  if (bp.isPhysical) {
    // Physical address matching (20-bit: page << 14 | offset)
    int pageIndex = (pc >> 14) & 0x03;
    uint8_t page = memoryPage[pageIndex];
    uint32_t physicalAddr = (pc & 0x3FFF) | ((uint32_t)page << 14);
    return physicalAddr == bp.address;
  } else {
    // Logical address matching (16-bit comparison)
    return (uint16_t)bp.address == pc;
  }
}

const Breakpoint* DebugManager::checkBreakpoint(uint16_t pc, uint8_t *memoryPage) const {
  if (!activeBreakpoints) return nullptr;

  // Check user breakpoints
  for (size_t i = 0; i < breakpointCount; i++) {
    if (checkSingleBreakpoint(*breakpoints[i], pc, memoryPage)) {
      return breakpoints[i]; // i
    }
  }
  // Check system breakpoints
  for (size_t i = 0; i < MAX_SYSTEM_BREAKPOINTS; i++) {
    if (checkSingleBreakpoint(systemBreakpoints[i], pc, memoryPage)) {
      return &systemBreakpoints[i]; // Return 8-9 for system breakpoints
    }
  }
  return nullptr;
}

std::optional<BreakpointInfo>
DebugManager::getBreakpointAtAddress(uint16_t addr) const {
  for (size_t i = 0; i < breakpointCount; i++) {
    // Only check logical breakpoints for display purposes
    if (!breakpoints[i]->isPhysical &&
        (uint16_t)breakpoints[i]->address == addr) {
      return BreakpointInfo{i, breakpoints[i]->enabled, breakpoints[i]->isTrace};
    }
  }
  return std::nullopt;
}

std::optional<BreakpointInfo>
DebugManager::getBreakpointAtAddress(uint16_t addr, uint8_t *memoryPage,
                                     bool pagingEnabled) const {
  // Calculate physical address for the given logical address
  uint32_t physicalAddr;
  if (pagingEnabled) {
    int pageIndex = (addr >> 14) & 0x03;
    uint8_t page = memoryPage[pageIndex];
    physicalAddr = (addr & 0x3FFF) | ((uint32_t)page << 14);
  } else {
    physicalAddr = addr;
  }

  for (size_t i = 0; i < breakpointCount; i++) {
    if (breakpoints[i]->isPhysical) {
      // Physical breakpoint: compare against calculated physical address
      if (breakpoints[i]->address == physicalAddr) {
        return BreakpointInfo{i, breakpoints[i]->enabled, breakpoints[i]->isTrace};
      }
    } else {
      // Logical breakpoint: compare against logical address
      if ((uint16_t)breakpoints[i]->address == addr) {
        return BreakpointInfo{i, breakpoints[i]->enabled, breakpoints[i]->isTrace};
      }
    }
  }
  return std::nullopt;
}

// Watchpoint CRUD methods
bool DebugManager::addWatchpoint(uint32_t address, uint16_t length,
                                bool isPhysical, bool onRead, bool onWrite) {
  if (watchpointCount >= MAX_WATCHPOINTS) {
    return false;
  }

  // Reject useless watchpoints: must have non-zero length and at least one
  // trigger type
  if (length == 0 || (!onRead && !onWrite)) {
    return false;
  }

  watchpoints[watchpointCount].address = address;
  watchpoints[watchpointCount].length = length;
  watchpoints[watchpointCount].isPhysical = isPhysical;
  watchpoints[watchpointCount].enabled = true;
  watchpoints[watchpointCount].onRead = onRead;
  watchpoints[watchpointCount].onWrite = onWrite;

  activeWatchpoints = true;
  watchpointCount++;
  return true;
}

bool DebugManager::removeWatchpoint(size_t index) {
  if (index >= watchpointCount) {
    return false;
  }

  // Shift remaining watchpoints down
  for (size_t i = index; i < watchpointCount - 1; i++) {
    watchpoints[i] = watchpoints[i + 1];
  }
  watchpointCount--;
  updateActiveWatchpoints();
  return true;
}

void DebugManager::setWatchpointEnabled(size_t index, bool enabled) {
  if (index < watchpointCount) {
    watchpoints[index].enabled = enabled;
    updateActiveWatchpoints();
  }
}

const Watchpoint *DebugManager::getWatchpoint(size_t index) const {
  if (index >= watchpointCount) {
    return nullptr;
  }
  return &watchpoints[index];
}

size_t DebugManager::getWatchpointCount() const { return watchpointCount; }

void DebugManager::clearAllWatchpoints() {
  watchpointCount = 0;
  activeWatchpoints = false;
}

bool DebugManager::findWatchpointByStartAddress(uint32_t address,
                                               bool isPhysical, size_t& index) const {
  for (size_t i = 0; i < watchpointCount; i++) {
    if (watchpoints[i].address == address &&
        watchpoints[i].isPhysical == isPhysical) {
      index = i;
      return true;
    }
  }
  return false;
}

bool DebugManager::hasActiveWatchpoints() const { return activeWatchpoints; }

void DebugManager::updateActiveWatchpoints() {
  for (size_t i = 0; i < watchpointCount; i++) {
    if (watchpoints[i].enabled) {
      activeWatchpoints = true;
      return;
    }
  }
  activeWatchpoints = false;
}

bool DebugManager::checkWatchpoint(uint16_t logicalAddress,
                                  uint32_t physicalAddress, bool isRead, size_t &index) const {
  if (!activeWatchpoints) {
    return false;
  }

  for (size_t i = 0; i < watchpointCount; i++) {
    const Watchpoint &wp = watchpoints[i];

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
    if (addressToCheck >= wp.address &&
        (addressToCheck - wp.address) < wp.length) {
      index = i;
      return true;
    }
  }
  return false;
}

void DebugManager::logTrace(const Breakpoint *breakpoint, z80_t cpu, uint32_t physicalAddress, uint8_t memoryPage[4], bool pageEnabled, uint64_t tick, std::function<uint8_t(uint16_t)> memRead) {
  if(traceLogs.size() > maxTraceLogSize) {
    TraceLog trace = traceLogs[0];
    for (auto data: trace.data) {
      free(data.data);
    }

    traceLogs.pop_front();
  }

  TraceLog traceLog = {physicalAddress, tick, (uint16_t)(cpu.pc-1), cpu.sp, cpu.af, cpu.bc, cpu.de, cpu.hl, cpu.ix, cpu.iy, memoryPage[0], memoryPage[1], memoryPage[2], memoryPage[3], pageEnabled, breakpoint, {}};

  for (auto &trace:breakpoint->traces) {
    if (trace.traceType == TraceType::ADDRESS) {
      uint16_t address = 0;

      switch(trace.traceValue) {
        case TraceValue::BC : address = cpu.bc; break;
        case TraceValue::DE : address = cpu.de; break;
        case TraceValue::HL : address = cpu.hl; break;
        case TraceValue::SP : address = cpu.sp; break;
        case TraceValue::PC : address = cpu.pc; break;
        case TraceValue::IX : address = cpu.ix; break;
        case TraceValue::IY : address = cpu.iy; break;
        default: 
           address = 0;
      }

      uint16_t length = trace.modifier;
      if (length > 48) length = 48;

      uint8_t* data = (uint8_t *)malloc(sizeof(uint8_t)*length);
      uint32_t physicalAddress = (uint32_t)(pageEnabled? (address & 0x3FFF) | (memoryPage[(address>>14) & 0x03] << 14): address);
      DataLog log = {trace.traceValue, address, physicalAddress, length, data};

      for (size_t i=0; i<length; i++) {
        data[i] = memRead(address++);
      }
      traceLog.data.push_back(log);
    }
  }
  traceLogs.push_back(traceLog);

  // TODO: If the trace includes address references, add DataLogs for the current data at that address
}

std::deque<TraceLog>* DebugManager::getTraceLogs() {
  return &traceLogs;
}

size_t DebugManager::getLogSize() {
  return traceLogs.size();
}

void DebugManager::clearAllLogs() {
  traceLogs.clear();
}
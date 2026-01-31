#include <iostream>
#include <cassert>
#include <cstring>
#include <optional>
#include "../src/debugmanager.hpp"

// Simple test framework
#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running " #name "..." << std::flush; \
    test_##name(); \
    std::cout << " PASSED" << std::endl; \
} while(0)

#define ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        std::cerr << "\nAssertion failed: " << #actual << " == " << #expected << std::endl; \
        std::cerr << "  Expected: " << (expected) << std::endl; \
        std::cerr << "  Actual:   " << (actual) << std::endl; \
        exit(1); \
    } \
} while(0)

#define ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        std::cerr << "\nAssertion failed: " << #condition << std::endl; \
        exit(1); \
    } \
} while(0)

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))
#define ASSERT_NOT_NULL(ptr) ASSERT_TRUE((ptr) != nullptr)
#define ASSERT_NULL(ptr) ASSERT_TRUE((ptr) == nullptr)

// AC#1: Given the DebugManager class is instantiated
//       When `addBreakpoint(0x4080, false)` is called
//       Then a logical breakpoint at $4080 is stored and index returned
TEST(add_logical_breakpoint) {
    DebugManager dm;
    int index = dm.addBreakpoint(0x4080, false);

    ASSERT_EQ(0, index);
    ASSERT_EQ(1, dm.getBreakpointCount());

    const Breakpoint* bp = dm.getBreakpoint(index);
    ASSERT_NOT_NULL(bp);
    ASSERT_EQ((uint32_t)0x4080, bp->address);
    ASSERT_FALSE(bp->isPhysical);
    ASSERT_TRUE(bp->enabled);
}

// AC#2: Given a breakpoint exists at logical address $4080
//       When the Z80 executes an instruction at PC=$4080
//       Then `checkBreakpoint()` returns index (>=0) and emulation enters DEBUG mode
TEST(check_logical_breakpoint_triggers) {
    DebugManager dm;
    dm.addBreakpoint(0x4080, false);

    uint8_t memoryPage[4] = {0, 0, 0, 0};

    ASSERT_EQ(0, dm.checkBreakpoint(0x4080, memoryPage));
    ASSERT_EQ(-1, dm.checkBreakpoint(0x4081, memoryPage));
    ASSERT_EQ(-1, dm.checkBreakpoint(0x407F, memoryPage));
}

// AC#3: Given a physical breakpoint at $84080
//       When code at logical $4080 executes with bank 8 paged in
//       Then the breakpoint triggers only for that specific physical location
TEST(check_physical_breakpoint_triggers) {
    DebugManager dm;
    // Physical address: $84080 = page 8 (0x08), offset 0x4080
    // Physical = (page & 0x1F) << 14 | (pc & 0x3FFF)
    // For PC=0x4080, pageIndex = (0x4080 >> 14) = 1
    // Physical = (8 & 0x1F) << 14 | (0x4080 & 0x3FFF) = 0x20000 | 0x0080 = 0x20080
    dm.addBreakpoint(0x20080, true);  // Physical address

    // With bank 8 paged into slot 1
    uint8_t memoryPage[4] = {0, 8, 0, 0};  // Page 8 in slot 1

    ASSERT_EQ(0, dm.checkBreakpoint(0x4080, memoryPage));

    // With different bank - should NOT trigger
    uint8_t memoryPage2[4] = {0, 9, 0, 0};  // Page 9 in slot 1
    ASSERT_EQ(-1, dm.checkBreakpoint(0x4080, memoryPage2));
}

// AC#4: Given a disabled breakpoint at $4080
//       When the Z80 executes at $4080
//       Then `checkBreakpoint()` returns -1 and execution continues
TEST(disabled_breakpoint_does_not_trigger) {
    DebugManager dm;
    int index = dm.addBreakpoint(0x4080, false);
    dm.setBreakpointEnabled(index, false);

    uint8_t memoryPage[4] = {0, 0, 0, 0};

    ASSERT_EQ(-1, dm.checkBreakpoint(0x4080, memoryPage));

    // Re-enable and verify it triggers again (returns index 0)
    dm.setBreakpointEnabled(index, true);
    ASSERT_EQ(0, dm.checkBreakpoint(0x4080, memoryPage));
}

// AC#5: Given no breakpoints are enabled
//       When `hasActiveBreakpoints()` is called
//       Then it returns false (allowing fast-path skip of checking loop)
TEST(has_active_breakpoints_empty) {
    DebugManager dm;
    ASSERT_FALSE(dm.hasActiveBreakpoints());
}

TEST(has_active_breakpoints_disabled) {
    DebugManager dm;
    int index = dm.addBreakpoint(0x4080, false);
    ASSERT_TRUE(dm.hasActiveBreakpoints());

    dm.setBreakpointEnabled(index, false);
    ASSERT_FALSE(dm.hasActiveBreakpoints());
}

TEST(has_active_breakpoints_multiple) {
    DebugManager dm;
    int idx1 = dm.addBreakpoint(0x1000, false);
    int idx2 = dm.addBreakpoint(0x2000, false);

    ASSERT_TRUE(dm.hasActiveBreakpoints());

    dm.setBreakpointEnabled(idx1, false);
    ASSERT_TRUE(dm.hasActiveBreakpoints());  // Still has idx2 active

    dm.setBreakpointEnabled(idx2, false);
    ASSERT_FALSE(dm.hasActiveBreakpoints());  // All disabled
}

// AC#6: Given 8 breakpoints already exist
//       When `addBreakpoint()` is called
//       Then it returns -1 indicating the list is full
TEST(add_breakpoint_returns_minus_one_when_full) {
    DebugManager dm;

    // Add 8 breakpoints (maximum)
    for (int i = 0; i < 8; i++) {
        int index = dm.addBreakpoint(0x1000 + i * 0x100, false);
        ASSERT_EQ(i, index);
    }

    ASSERT_EQ(8, dm.getBreakpointCount());

    // 9th should fail
    int index = dm.addBreakpoint(0x9000, false);
    ASSERT_EQ(-1, index);
    ASSERT_EQ(8, dm.getBreakpointCount());  // Count unchanged
}

// Additional tests for CRUD operations
TEST(remove_breakpoint) {
    DebugManager dm;
    dm.addBreakpoint(0x1000, false);
    dm.addBreakpoint(0x2000, false);
    dm.addBreakpoint(0x3000, false);

    ASSERT_EQ(3, dm.getBreakpointCount());

    // Remove middle breakpoint
    ASSERT_TRUE(dm.removeBreakpoint(1));
    ASSERT_EQ(2, dm.getBreakpointCount());

    // Verify remaining breakpoints
    const Breakpoint* bp0 = dm.getBreakpoint(0);
    const Breakpoint* bp1 = dm.getBreakpoint(1);
    ASSERT_NOT_NULL(bp0);
    ASSERT_NOT_NULL(bp1);
    ASSERT_EQ((uint32_t)0x1000, bp0->address);
    ASSERT_EQ((uint32_t)0x3000, bp1->address);  // 0x3000 shifted down

    // Remove invalid index
    ASSERT_FALSE(dm.removeBreakpoint(-1));
    ASSERT_FALSE(dm.removeBreakpoint(10));
}

TEST(get_breakpoint_invalid_index) {
    DebugManager dm;
    dm.addBreakpoint(0x1000, false);

    ASSERT_NULL(dm.getBreakpoint(-1));
    ASSERT_NULL(dm.getBreakpoint(1));
    ASSERT_NULL(dm.getBreakpoint(100));
}

TEST(set_breakpoint_enabled_invalid_index) {
    DebugManager dm;
    dm.addBreakpoint(0x1000, false);

    // These should not crash - just no-op for invalid indexes
    dm.setBreakpointEnabled(-1, false);
    dm.setBreakpointEnabled(10, false);

    // Original breakpoint should still be enabled
    ASSERT_TRUE(dm.getBreakpoint(0)->enabled);
}

TEST(logical_breakpoint_ignores_banking) {
    DebugManager dm;
    dm.addBreakpoint(0x4080, false);  // Logical breakpoint

    // Should trigger regardless of what's paged in (returns index 0)
    uint8_t memPage1[4] = {0, 8, 0, 0};
    uint8_t memPage2[4] = {0, 9, 0, 0};
    uint8_t memPage3[4] = {0, 0, 0, 0};

    ASSERT_EQ(0, dm.checkBreakpoint(0x4080, memPage1));
    ASSERT_EQ(0, dm.checkBreakpoint(0x4080, memPage2));
    ASSERT_EQ(0, dm.checkBreakpoint(0x4080, memPage3));
}

// Tests for new API methods (code review additions)
TEST(clear_all_breakpoints) {
    DebugManager dm;
    dm.addBreakpoint(0x1000, false);
    dm.addBreakpoint(0x2000, false);
    dm.addBreakpoint(0x3000, true);

    ASSERT_EQ(3, dm.getBreakpointCount());
    ASSERT_TRUE(dm.hasActiveBreakpoints());

    dm.clearAllBreakpoints();

    ASSERT_EQ(0, dm.getBreakpointCount());
    ASSERT_FALSE(dm.hasActiveBreakpoints());
}

TEST(find_breakpoint_by_address) {
    DebugManager dm;
    dm.addBreakpoint(0x1000, false);  // index 0
    dm.addBreakpoint(0x2000, true);   // index 1
    dm.addBreakpoint(0x3000, false);  // index 2

    // Find existing breakpoints
    ASSERT_EQ(0, dm.findBreakpointByAddress(0x1000, false));
    ASSERT_EQ(1, dm.findBreakpointByAddress(0x2000, true));
    ASSERT_EQ(2, dm.findBreakpointByAddress(0x3000, false));

    // Address exists but wrong type
    ASSERT_EQ(-1, dm.findBreakpointByAddress(0x1000, true));
    ASSERT_EQ(-1, dm.findBreakpointByAddress(0x2000, false));

    // Non-existent address
    ASSERT_EQ(-1, dm.findBreakpointByAddress(0x9999, false));
    ASSERT_EQ(-1, dm.findBreakpointByAddress(0x9999, true));
}

// ==============================================================
// Watchpoint Tests - AC#1 through AC#9
// ==============================================================

// AC#1: Given the DebugManager class
//       When `addWatchpoint(0x8000, 32, false, false, true)` is called
//       Then a write-only watchpoint monitoring $8000-$801F is stored and index returned
TEST(add_watchpoint_write_only) {
    DebugManager dm;
    int index = dm.addWatchpoint(0x8000, 32, false, false, true);

    ASSERT_EQ(0, index);
    ASSERT_EQ(1, dm.getWatchpointCount());

    const Watchpoint* wp = dm.getWatchpoint(index);
    ASSERT_NOT_NULL(wp);
    ASSERT_EQ((uint32_t)0x8000, wp->address);
    ASSERT_EQ((uint16_t)32, wp->length);
    ASSERT_FALSE(wp->isPhysical);
    ASSERT_FALSE(wp->onRead);
    ASSERT_TRUE(wp->onWrite);
    ASSERT_TRUE(wp->enabled);
}

// AC#8: Given 8 user watchpoints already exist
//       When `addWatchpoint()` is called
//       Then it returns -1 indicating the list is full
TEST(add_watchpoint_returns_minus_one_when_full) {
    DebugManager dm;

    // Add 8 watchpoints (maximum)
    for (int i = 0; i < 8; i++) {
        int index = dm.addWatchpoint(0x1000 + i * 0x100, 16, false, true, true);
        ASSERT_EQ(i, index);
    }

    ASSERT_EQ(8, dm.getWatchpointCount());

    // 9th should fail
    int index = dm.addWatchpoint(0x9000, 16, false, true, true);
    ASSERT_EQ(-1, index);
    ASSERT_EQ(8, dm.getWatchpointCount());  // Count unchanged
}

// Test removeWatchpoint compacts array correctly
TEST(remove_watchpoint) {
    DebugManager dm;
    dm.addWatchpoint(0x1000, 16, false, true, true);
    dm.addWatchpoint(0x2000, 32, false, true, true);
    dm.addWatchpoint(0x3000, 64, false, true, true);

    ASSERT_EQ(3, dm.getWatchpointCount());

    // Remove middle watchpoint
    ASSERT_TRUE(dm.removeWatchpoint(1));
    ASSERT_EQ(2, dm.getWatchpointCount());

    // Verify remaining watchpoints
    const Watchpoint* wp0 = dm.getWatchpoint(0);
    const Watchpoint* wp1 = dm.getWatchpoint(1);
    ASSERT_NOT_NULL(wp0);
    ASSERT_NOT_NULL(wp1);
    ASSERT_EQ((uint32_t)0x1000, wp0->address);
    ASSERT_EQ((uint32_t)0x3000, wp1->address);  // 0x3000 shifted down

    // Remove invalid index
    ASSERT_FALSE(dm.removeWatchpoint(-1));
    ASSERT_FALSE(dm.removeWatchpoint(10));
}

// Test clearAllWatchpoints
TEST(clear_all_watchpoints) {
    DebugManager dm;
    dm.addWatchpoint(0x1000, 16, false, true, true);
    dm.addWatchpoint(0x2000, 32, false, true, false);
    dm.addWatchpoint(0x3000, 64, true, false, true);

    ASSERT_EQ(3, dm.getWatchpointCount());
    ASSERT_TRUE(dm.hasActiveWatchpoints());

    dm.clearAllWatchpoints();

    ASSERT_EQ(0, dm.getWatchpointCount());
    ASSERT_FALSE(dm.hasActiveWatchpoints());
}

// Test findWatchpointByStartAddress (renamed for clarity - only matches start address)
TEST(find_watchpoint_by_start_address) {
    DebugManager dm;
    dm.addWatchpoint(0x1000, 16, false, true, true);  // index 0
    dm.addWatchpoint(0x2000, 32, true, true, true);   // index 1
    dm.addWatchpoint(0x3000, 64, false, true, true);  // index 2

    // Find existing watchpoints by their start address
    ASSERT_EQ(0, dm.findWatchpointByStartAddress(0x1000, false));
    ASSERT_EQ(1, dm.findWatchpointByStartAddress(0x2000, true));
    ASSERT_EQ(2, dm.findWatchpointByStartAddress(0x3000, false));

    // Address exists but wrong type
    ASSERT_EQ(-1, dm.findWatchpointByStartAddress(0x1000, true));
    ASSERT_EQ(-1, dm.findWatchpointByStartAddress(0x2000, false));

    // Non-existent address
    ASSERT_EQ(-1, dm.findWatchpointByStartAddress(0x9999, false));
    ASSERT_EQ(-1, dm.findWatchpointByStartAddress(0x9999, true));

    // Address within range but not start address - should NOT find
    ASSERT_EQ(-1, dm.findWatchpointByStartAddress(0x1008, false));  // inside 0x1000-0x100F
}

// Test getWatchpoint with invalid index
TEST(get_watchpoint_invalid_index) {
    DebugManager dm;
    dm.addWatchpoint(0x1000, 16, false, true, true);

    ASSERT_NULL(dm.getWatchpoint(-1));
    ASSERT_NULL(dm.getWatchpoint(1));
    ASSERT_NULL(dm.getWatchpoint(100));
}

// Test setWatchpointEnabled with invalid index
TEST(set_watchpoint_enabled_invalid_index) {
    DebugManager dm;
    dm.addWatchpoint(0x1000, 16, false, true, true);

    // These should not crash - just no-op for invalid indexes
    dm.setWatchpointEnabled(-1, false);
    dm.setWatchpointEnabled(10, false);

    // Original watchpoint should still be enabled
    ASSERT_TRUE(dm.getWatchpoint(0)->enabled);
}

// AC#2: Given a write watchpoint on $8000-$801F
//       When code writes to address $8010
//       Then `checkWatchpoint()` returns index (>=0)
TEST(check_watchpoint_triggers_on_write) {
    DebugManager dm;
    dm.addWatchpoint(0x8000, 32, false, false, true);  // write-only, logical

    // Write to address within range - should trigger (logical address matches)
    // Use 0xDEADBEEF for physical address to show it's ignored for logical watchpoints
    ASSERT_EQ(0, dm.checkWatchpoint(0x8010, 0xDEADBEEF, false));

    // Write to addresses at boundaries
    ASSERT_EQ(0, dm.checkWatchpoint(0x8000, 0xDEADBEEF, false));  // start of range
    ASSERT_EQ(0, dm.checkWatchpoint(0x801F, 0xDEADBEEF, false));  // end of range (inclusive)

    // Write outside range - should NOT trigger
    ASSERT_EQ(-1, dm.checkWatchpoint(0x7FFF, 0xDEADBEEF, false));  // before range
    ASSERT_EQ(-1, dm.checkWatchpoint(0x8020, 0xDEADBEEF, false));  // after range
}

// AC#3: Given a read watchpoint on $C000-$C0FF
//       When code reads from $C050
//       Then the watchpoint triggers (returns index >=0)
TEST(check_watchpoint_triggers_on_read) {
    DebugManager dm;
    dm.addWatchpoint(0xC000, 256, false, true, false);  // read-only, logical

    // Read from address within range - should trigger
    // Use 0xDEADBEEF for physical address to show it's ignored for logical watchpoints
    ASSERT_EQ(0, dm.checkWatchpoint(0xC050, 0xDEADBEEF, true));

    // Read at boundaries
    ASSERT_EQ(0, dm.checkWatchpoint(0xC000, 0xDEADBEEF, true));  // start of range
    ASSERT_EQ(0, dm.checkWatchpoint(0xC0FF, 0xDEADBEEF, true));  // end of range (inclusive)

    // Read outside range - should NOT trigger
    ASSERT_EQ(-1, dm.checkWatchpoint(0xBFFF, 0xDEADBEEF, true));  // before range
    ASSERT_EQ(-1, dm.checkWatchpoint(0xC100, 0xDEADBEEF, true));  // after range
}

// AC#4: Given a read/write watchpoint on $8000-$800F
//       When code either reads or writes within that range
//       Then the watchpoint triggers (returns index >=0)
TEST(check_watchpoint_read_write_both_trigger) {
    DebugManager dm;
    dm.addWatchpoint(0x8000, 16, false, true, true);  // both read and write, logical

    // Both read and write should trigger (0xDEADBEEF shows physical is ignored)
    ASSERT_EQ(0, dm.checkWatchpoint(0x8008, 0xDEADBEEF, true));   // read
    ASSERT_EQ(0, dm.checkWatchpoint(0x8008, 0xDEADBEEF, false));  // write
}

// AC#5: Given a write watchpoint on $8000-$801F
//       When code reads from $8010 (but doesn't write)
//       Then the watchpoint does NOT trigger
TEST(check_watchpoint_write_only_ignores_reads) {
    DebugManager dm;
    dm.addWatchpoint(0x8000, 32, false, false, true);  // write-only, logical

    // Read should NOT trigger (0xDEADBEEF shows physical is ignored)
    ASSERT_EQ(-1, dm.checkWatchpoint(0x8010, 0xDEADBEEF, true));  // isRead=true means read

    // Write should trigger (returns index 0)
    ASSERT_EQ(0, dm.checkWatchpoint(0x8010, 0xDEADBEEF, false));  // isRead=false means write
}

// Converse: read-only watchpoint ignores writes
TEST(check_watchpoint_read_only_ignores_writes) {
    DebugManager dm;
    dm.addWatchpoint(0x8000, 32, false, true, false);  // read-only, logical

    // Write should NOT trigger (0xDEADBEEF shows physical is ignored)
    ASSERT_EQ(-1, dm.checkWatchpoint(0x8010, 0xDEADBEEF, false));  // isRead=false means write

    // Read should trigger (returns index 0)
    ASSERT_EQ(0, dm.checkWatchpoint(0x8010, 0xDEADBEEF, true));  // isRead=true means read
}

// AC#6: Given a disabled watchpoint
//       When memory in its range is accessed
//       Then `checkWatchpoint()` returns -1 and execution continues
TEST(check_watchpoint_disabled_does_not_trigger) {
    DebugManager dm;
    int index = dm.addWatchpoint(0x8000, 32, false, true, true);
    dm.setWatchpointEnabled(index, false);

    // Should NOT trigger when disabled (0xDEADBEEF shows physical is ignored)
    ASSERT_EQ(-1, dm.checkWatchpoint(0x8010, 0xDEADBEEF, true));
    ASSERT_EQ(-1, dm.checkWatchpoint(0x8010, 0xDEADBEEF, false));

    // Re-enable and verify it triggers again (returns index 0)
    dm.setWatchpointEnabled(index, true);
    ASSERT_EQ(0, dm.checkWatchpoint(0x8010, 0xDEADBEEF, true));
    ASSERT_EQ(0, dm.checkWatchpoint(0x8010, 0xDEADBEEF, false));
}

// Test: Logical watchpoint triggers regardless of physical address (bank setting)
TEST(check_watchpoint_logical_ignores_banking) {
    DebugManager dm;
    dm.addWatchpoint(0x8010, 16, false, true, true);  // logical watchpoint at $8010

    // Logical address matches - should trigger regardless of physical address (returns index 0)
    // Same logical address, different physical addresses (different banks)
    ASSERT_EQ(0, dm.checkWatchpoint(0x8010, 0x00010, true));   // bank 0
    ASSERT_EQ(0, dm.checkWatchpoint(0x8010, 0x04010, true));   // bank 1
    ASSERT_EQ(0, dm.checkWatchpoint(0x8010, 0x08010, true));   // bank 2
    ASSERT_EQ(0, dm.checkWatchpoint(0x8010, 0x20010, true));   // RAM bank

    // Different logical address - should NOT trigger even if physical matches
    ASSERT_EQ(-1, dm.checkWatchpoint(0x4010, 0x00010, true));  // different logical
}

// Test: Physical watchpoint only triggers when physical address matches
TEST(check_watchpoint_physical_honours_banking) {
    DebugManager dm;
    // Physical watchpoint at $08010 (bank 2, offset $0010)
    dm.addWatchpoint(0x08010, 16, true, true, true);  // physical watchpoint

    // Only triggers when physical address matches (returns index 0)
    ASSERT_EQ(0, dm.checkWatchpoint(0x8010, 0x08010, true));   // physical matches
    ASSERT_EQ(0, dm.checkWatchpoint(0x4010, 0x08010, true));   // different logical, same physical

    // Should NOT trigger for different physical addresses
    ASSERT_EQ(-1, dm.checkWatchpoint(0x8010, 0x00010, true));  // bank 0 - wrong physical
    ASSERT_EQ(-1, dm.checkWatchpoint(0x8010, 0x04010, true));  // bank 1 - wrong physical
    ASSERT_EQ(-1, dm.checkWatchpoint(0x8010, 0x20010, true));  // RAM - wrong physical
}

// Test checkWatchpoint returns -1 when no watchpoints exist
TEST(check_watchpoint_empty_list) {
    DebugManager dm;
    // Should return -1 and not crash when no watchpoints
    ASSERT_EQ(-1, dm.checkWatchpoint(0x8000, 0x08000, true));
    ASSERT_EQ(-1, dm.checkWatchpoint(0x8000, 0x08000, false));
}

// Test addWatchpoint rejects invalid parameters
TEST(add_watchpoint_rejects_invalid) {
    DebugManager dm;

    // Reject length=0
    ASSERT_EQ(-1, dm.addWatchpoint(0x8000, 0, false, true, true));
    ASSERT_EQ(0, dm.getWatchpointCount());  // Not added

    // Reject onRead=false AND onWrite=false (useless watchpoint)
    ASSERT_EQ(-1, dm.addWatchpoint(0x8000, 16, false, false, false));
    ASSERT_EQ(0, dm.getWatchpointCount());  // Not added

    // Valid watchpoint should still work
    ASSERT_EQ(0, dm.addWatchpoint(0x8000, 16, false, true, false));  // read-only OK
    ASSERT_EQ(1, dm.addWatchpoint(0x9000, 16, false, false, true));  // write-only OK
    ASSERT_EQ(2, dm.getWatchpointCount());
}

// AC#7: Given no watchpoints are enabled
//       When `hasActiveWatchpoints()` is called
//       Then it returns false (allowing fast-path skip in memory access functions)
TEST(has_active_watchpoints_empty) {
    DebugManager dm;
    ASSERT_FALSE(dm.hasActiveWatchpoints());
}

TEST(has_active_watchpoints_disabled) {
    DebugManager dm;
    int index = dm.addWatchpoint(0x8000, 32, false, true, true);
    ASSERT_TRUE(dm.hasActiveWatchpoints());

    dm.setWatchpointEnabled(index, false);
    ASSERT_FALSE(dm.hasActiveWatchpoints());
}

TEST(has_active_watchpoints_multiple) {
    DebugManager dm;
    int idx1 = dm.addWatchpoint(0x1000, 16, false, true, true);
    int idx2 = dm.addWatchpoint(0x2000, 16, false, true, true);

    ASSERT_TRUE(dm.hasActiveWatchpoints());

    dm.setWatchpointEnabled(idx1, false);
    ASSERT_TRUE(dm.hasActiveWatchpoints());  // Still has idx2 active

    dm.setWatchpointEnabled(idx2, false);
    ASSERT_FALSE(dm.hasActiveWatchpoints());  // All disabled
}

// ==============================================================
// Story 2-3: Index-returning check methods
// ==============================================================

// Test checkBreakpoint returns index instead of bool
TEST(check_breakpoint_returns_index) {
    DebugManager dm;
    dm.addBreakpoint(0x1000, false);  // index 0
    dm.addBreakpoint(0x2000, false);  // index 1
    dm.addBreakpoint(0x3000, false);  // index 2

    uint8_t memoryPage[4] = {0, 0, 0, 0};

    // Should return the index of the triggered breakpoint
    ASSERT_EQ(0, dm.checkBreakpoint(0x1000, memoryPage));
    ASSERT_EQ(1, dm.checkBreakpoint(0x2000, memoryPage));
    ASSERT_EQ(2, dm.checkBreakpoint(0x3000, memoryPage));

    // Should return -1 when no breakpoint matches
    ASSERT_EQ(-1, dm.checkBreakpoint(0x4000, memoryPage));
    ASSERT_EQ(-1, dm.checkBreakpoint(0x0FFF, memoryPage));
}

// Test checkBreakpoint returns -1 for disabled breakpoints
TEST(check_breakpoint_returns_minus_one_when_disabled) {
    DebugManager dm;
    int idx = dm.addBreakpoint(0x1000, false);
    dm.setBreakpointEnabled(idx, false);

    uint8_t memoryPage[4] = {0, 0, 0, 0};

    ASSERT_EQ(-1, dm.checkBreakpoint(0x1000, memoryPage));

    // Re-enable and verify it returns index
    dm.setBreakpointEnabled(idx, true);
    ASSERT_EQ(0, dm.checkBreakpoint(0x1000, memoryPage));
}

// Test checkWatchpoint returns index instead of bool
TEST(check_watchpoint_returns_index) {
    DebugManager dm;
    dm.addWatchpoint(0x8000, 16, false, true, true);  // index 0
    dm.addWatchpoint(0x9000, 16, false, true, true);  // index 1
    dm.addWatchpoint(0xA000, 16, false, true, true);  // index 2

    // Should return the index of the triggered watchpoint
    ASSERT_EQ(0, dm.checkWatchpoint(0x8008, 0xDEAD, true));
    ASSERT_EQ(1, dm.checkWatchpoint(0x9008, 0xDEAD, true));
    ASSERT_EQ(2, dm.checkWatchpoint(0xA008, 0xDEAD, true));

    // Should return -1 when no watchpoint matches
    ASSERT_EQ(-1, dm.checkWatchpoint(0xB000, 0xDEAD, true));
    ASSERT_EQ(-1, dm.checkWatchpoint(0x7000, 0xDEAD, true));
}

// Test checkWatchpoint returns -1 for disabled watchpoints
TEST(check_watchpoint_returns_minus_one_when_disabled) {
    DebugManager dm;
    int idx = dm.addWatchpoint(0x8000, 16, false, true, true);
    dm.setWatchpointEnabled(idx, false);

    ASSERT_EQ(-1, dm.checkWatchpoint(0x8008, 0xDEAD, true));

    // Re-enable and verify it returns index
    dm.setWatchpointEnabled(idx, true);
    ASSERT_EQ(0, dm.checkWatchpoint(0x8008, 0xDEAD, true));
}

// Test getBreakpointAtAddress returns info for breakpoints
TEST(get_breakpoint_at_address) {
    DebugManager dm;
    dm.addBreakpoint(0x1000, false);  // index 0, enabled
    dm.addBreakpoint(0x2000, false);  // index 1, enabled
    dm.setBreakpointEnabled(1, false);  // disable index 1

    // Should find enabled breakpoint
    auto info0 = dm.getBreakpointAtAddress(0x1000);
    ASSERT_TRUE(info0.has_value());
    ASSERT_EQ(0, info0->index);
    ASSERT_TRUE(info0->enabled);

    // Should find disabled breakpoint
    auto info1 = dm.getBreakpointAtAddress(0x2000);
    ASSERT_TRUE(info1.has_value());
    ASSERT_EQ(1, info1->index);
    ASSERT_FALSE(info1->enabled);

    // Should return nullopt for non-existent address
    auto info2 = dm.getBreakpointAtAddress(0x3000);
    ASSERT_FALSE(info2.has_value());
}

// Test getBreakpointAtAddress extended version with physical breakpoints
TEST(get_breakpoint_at_address_physical) {
    DebugManager dm;
    // Physical breakpoint at physical address 0x20080 (page 8, offset 0x0080)
    // This maps to logical 0x4080 when page 8 is in slot 1
    dm.addBreakpoint(0x20080, true);   // index 0, physical
    dm.addBreakpoint(0x1000, false);   // index 1, logical

    // With page 8 in slot 1: logical 0x4080 -> physical 0x20080
    uint8_t memPage1[4] = {0, 8, 0, 0};

    // Should find physical breakpoint when mapping matches
    auto info0 = dm.getBreakpointAtAddress(0x4080, memPage1, true);
    ASSERT_TRUE(info0.has_value());
    ASSERT_EQ(0, info0->index);
    ASSERT_TRUE(info0->enabled);

    // With different page: logical 0x4080 -> physical 0x24080 (page 9)
    uint8_t memPage2[4] = {0, 9, 0, 0};
    auto info1 = dm.getBreakpointAtAddress(0x4080, memPage2, true);
    ASSERT_FALSE(info1.has_value());  // Different physical address

    // Should still find logical breakpoints
    auto info2 = dm.getBreakpointAtAddress(0x1000, memPage1, true);
    ASSERT_TRUE(info2.has_value());
    ASSERT_EQ(1, info2->index);

    // With paging disabled, physical = logical
    uint8_t memPage3[4] = {0, 0, 0, 0};
    auto info3 = dm.getBreakpointAtAddress(0x1000, memPage3, false);
    ASSERT_TRUE(info3.has_value());
    ASSERT_EQ(1, info3->index);
}

// Test getBreakpointAtAddress simple version ignores physical breakpoints
TEST(get_breakpoint_at_address_simple_ignores_physical) {
    DebugManager dm;
    // Physical breakpoint at logical address 0x4080 (stored as physical, so isPhysical=true)
    dm.addBreakpoint(0x4080, true);    // index 0, physical - should NOT be found by simple version
    dm.addBreakpoint(0x1000, false);   // index 1, logical

    // Simple version should only find logical breakpoints
    // Even though 0x4080 matches the stored address, it's a physical BP so not found
    auto info0 = dm.getBreakpointAtAddress(0x4080);
    ASSERT_FALSE(info0.has_value());  // Physical BP not visible to simple version

    auto info1 = dm.getBreakpointAtAddress(0x1000);
    ASSERT_TRUE(info1.has_value());
    ASSERT_EQ(1, info1->index);
}

int main() {
    std::cout << "=== DebugManager Tests ===" << std::endl;

    // AC#1
    RUN_TEST(add_logical_breakpoint);

    // AC#2
    RUN_TEST(check_logical_breakpoint_triggers);

    // AC#3
    RUN_TEST(check_physical_breakpoint_triggers);

    // AC#4
    RUN_TEST(disabled_breakpoint_does_not_trigger);

    // AC#5
    RUN_TEST(has_active_breakpoints_empty);
    RUN_TEST(has_active_breakpoints_disabled);
    RUN_TEST(has_active_breakpoints_multiple);

    // AC#6
    RUN_TEST(add_breakpoint_returns_minus_one_when_full);

    // Additional CRUD tests
    RUN_TEST(remove_breakpoint);
    RUN_TEST(get_breakpoint_invalid_index);
    RUN_TEST(set_breakpoint_enabled_invalid_index);
    RUN_TEST(logical_breakpoint_ignores_banking);

    // Code review additions
    RUN_TEST(clear_all_breakpoints);
    RUN_TEST(find_breakpoint_by_address);

    std::cout << "\n=== Watchpoint Tests ===" << std::endl;

    // AC#1, #8
    RUN_TEST(add_watchpoint_write_only);
    RUN_TEST(add_watchpoint_returns_minus_one_when_full);

    // CRUD tests
    RUN_TEST(remove_watchpoint);
    RUN_TEST(clear_all_watchpoints);
    RUN_TEST(find_watchpoint_by_start_address);
    RUN_TEST(get_watchpoint_invalid_index);
    RUN_TEST(set_watchpoint_enabled_invalid_index);

    // AC#2, #3, #4, #5, #6, #7 - checkWatchpoint tests
    RUN_TEST(check_watchpoint_empty_list);
    RUN_TEST(add_watchpoint_rejects_invalid);
    RUN_TEST(check_watchpoint_triggers_on_write);
    RUN_TEST(check_watchpoint_triggers_on_read);
    RUN_TEST(check_watchpoint_read_write_both_trigger);
    RUN_TEST(check_watchpoint_write_only_ignores_reads);
    RUN_TEST(check_watchpoint_read_only_ignores_writes);
    RUN_TEST(check_watchpoint_disabled_does_not_trigger);
    RUN_TEST(has_active_watchpoints_empty);
    RUN_TEST(has_active_watchpoints_disabled);
    RUN_TEST(has_active_watchpoints_multiple);

    // Logical vs Physical address tests
    RUN_TEST(check_watchpoint_logical_ignores_banking);
    RUN_TEST(check_watchpoint_physical_honours_banking);

    std::cout << "\n=== Story 2-3: Index-returning check methods ===" << std::endl;
    RUN_TEST(check_breakpoint_returns_index);
    RUN_TEST(check_breakpoint_returns_minus_one_when_disabled);
    RUN_TEST(check_watchpoint_returns_index);
    RUN_TEST(check_watchpoint_returns_minus_one_when_disabled);
    RUN_TEST(get_breakpoint_at_address);
    RUN_TEST(get_breakpoint_at_address_physical);
    RUN_TEST(get_breakpoint_at_address_simple_ignores_physical);

    std::cout << "\n=== All tests PASSED ===" << std::endl;
    return 0;
}

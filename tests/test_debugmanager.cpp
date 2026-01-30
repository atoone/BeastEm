#include <iostream>
#include <cassert>
#include <cstring>
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
//       Then `checkBreakpoint()` returns true and emulation enters DEBUG mode
TEST(check_logical_breakpoint_triggers) {
    DebugManager dm;
    dm.addBreakpoint(0x4080, false);

    uint8_t memoryPage[4] = {0, 0, 0, 0};

    ASSERT_TRUE(dm.checkBreakpoint(0x4080, memoryPage));
    ASSERT_FALSE(dm.checkBreakpoint(0x4081, memoryPage));
    ASSERT_FALSE(dm.checkBreakpoint(0x407F, memoryPage));
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

    ASSERT_TRUE(dm.checkBreakpoint(0x4080, memoryPage));

    // With different bank - should NOT trigger
    uint8_t memoryPage2[4] = {0, 9, 0, 0};  // Page 9 in slot 1
    ASSERT_FALSE(dm.checkBreakpoint(0x4080, memoryPage2));
}

// AC#4: Given a disabled breakpoint at $4080
//       When the Z80 executes at $4080
//       Then `checkBreakpoint()` returns false and execution continues
TEST(disabled_breakpoint_does_not_trigger) {
    DebugManager dm;
    int index = dm.addBreakpoint(0x4080, false);
    dm.setBreakpointEnabled(index, false);

    uint8_t memoryPage[4] = {0, 0, 0, 0};

    ASSERT_FALSE(dm.checkBreakpoint(0x4080, memoryPage));

    // Re-enable and verify it triggers again
    dm.setBreakpointEnabled(index, true);
    ASSERT_TRUE(dm.checkBreakpoint(0x4080, memoryPage));
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

    // Should trigger regardless of what's paged in
    uint8_t memPage1[4] = {0, 8, 0, 0};
    uint8_t memPage2[4] = {0, 9, 0, 0};
    uint8_t memPage3[4] = {0, 0, 0, 0};

    ASSERT_TRUE(dm.checkBreakpoint(0x4080, memPage1));
    ASSERT_TRUE(dm.checkBreakpoint(0x4080, memPage2));
    ASSERT_TRUE(dm.checkBreakpoint(0x4080, memPage3));
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
//       Then `checkWatchpoint()` returns true
TEST(check_watchpoint_triggers_on_write) {
    DebugManager dm;
    dm.addWatchpoint(0x8000, 32, false, false, true);  // write-only, logical

    // Write to address within range - should trigger (logical address matches)
    // Use 0xDEADBEEF for physical address to show it's ignored for logical watchpoints
    ASSERT_TRUE(dm.checkWatchpoint(0x8010, 0xDEADBEEF, false));

    // Write to addresses at boundaries
    ASSERT_TRUE(dm.checkWatchpoint(0x8000, 0xDEADBEEF, false));  // start of range
    ASSERT_TRUE(dm.checkWatchpoint(0x801F, 0xDEADBEEF, false));  // end of range (inclusive)

    // Write outside range - should NOT trigger
    ASSERT_FALSE(dm.checkWatchpoint(0x7FFF, 0xDEADBEEF, false));  // before range
    ASSERT_FALSE(dm.checkWatchpoint(0x8020, 0xDEADBEEF, false));  // after range
}

// AC#3: Given a read watchpoint on $C000-$C0FF
//       When code reads from $C050
//       Then the watchpoint triggers
TEST(check_watchpoint_triggers_on_read) {
    DebugManager dm;
    dm.addWatchpoint(0xC000, 256, false, true, false);  // read-only, logical

    // Read from address within range - should trigger
    // Use 0xDEADBEEF for physical address to show it's ignored for logical watchpoints
    ASSERT_TRUE(dm.checkWatchpoint(0xC050, 0xDEADBEEF, true));

    // Read at boundaries
    ASSERT_TRUE(dm.checkWatchpoint(0xC000, 0xDEADBEEF, true));  // start of range
    ASSERT_TRUE(dm.checkWatchpoint(0xC0FF, 0xDEADBEEF, true));  // end of range (inclusive)

    // Read outside range - should NOT trigger
    ASSERT_FALSE(dm.checkWatchpoint(0xBFFF, 0xDEADBEEF, true));  // before range
    ASSERT_FALSE(dm.checkWatchpoint(0xC100, 0xDEADBEEF, true));  // after range
}

// AC#4: Given a read/write watchpoint on $8000-$800F
//       When code either reads or writes within that range
//       Then the watchpoint triggers
TEST(check_watchpoint_read_write_both_trigger) {
    DebugManager dm;
    dm.addWatchpoint(0x8000, 16, false, true, true);  // both read and write, logical

    // Both read and write should trigger (0xDEADBEEF shows physical is ignored)
    ASSERT_TRUE(dm.checkWatchpoint(0x8008, 0xDEADBEEF, true));   // read
    ASSERT_TRUE(dm.checkWatchpoint(0x8008, 0xDEADBEEF, false));  // write
}

// AC#5: Given a write watchpoint on $8000-$801F
//       When code reads from $8010 (but doesn't write)
//       Then the watchpoint does NOT trigger
TEST(check_watchpoint_write_only_ignores_reads) {
    DebugManager dm;
    dm.addWatchpoint(0x8000, 32, false, false, true);  // write-only, logical

    // Read should NOT trigger (0xDEADBEEF shows physical is ignored)
    ASSERT_FALSE(dm.checkWatchpoint(0x8010, 0xDEADBEEF, true));  // isRead=true means read

    // Write should trigger
    ASSERT_TRUE(dm.checkWatchpoint(0x8010, 0xDEADBEEF, false));  // isRead=false means write
}

// Converse: read-only watchpoint ignores writes
TEST(check_watchpoint_read_only_ignores_writes) {
    DebugManager dm;
    dm.addWatchpoint(0x8000, 32, false, true, false);  // read-only, logical

    // Write should NOT trigger (0xDEADBEEF shows physical is ignored)
    ASSERT_FALSE(dm.checkWatchpoint(0x8010, 0xDEADBEEF, false));  // isRead=false means write

    // Read should trigger
    ASSERT_TRUE(dm.checkWatchpoint(0x8010, 0xDEADBEEF, true));  // isRead=true means read
}

// AC#6: Given a disabled watchpoint
//       When memory in its range is accessed
//       Then `checkWatchpoint()` returns false and execution continues
TEST(check_watchpoint_disabled_does_not_trigger) {
    DebugManager dm;
    int index = dm.addWatchpoint(0x8000, 32, false, true, true);
    dm.setWatchpointEnabled(index, false);

    // Should NOT trigger when disabled (0xDEADBEEF shows physical is ignored)
    ASSERT_FALSE(dm.checkWatchpoint(0x8010, 0xDEADBEEF, true));
    ASSERT_FALSE(dm.checkWatchpoint(0x8010, 0xDEADBEEF, false));

    // Re-enable and verify it triggers again
    dm.setWatchpointEnabled(index, true);
    ASSERT_TRUE(dm.checkWatchpoint(0x8010, 0xDEADBEEF, true));
    ASSERT_TRUE(dm.checkWatchpoint(0x8010, 0xDEADBEEF, false));
}

// Test: Logical watchpoint triggers regardless of physical address (bank setting)
TEST(check_watchpoint_logical_ignores_banking) {
    DebugManager dm;
    dm.addWatchpoint(0x8010, 16, false, true, true);  // logical watchpoint at $8010

    // Logical address matches - should trigger regardless of physical address
    // Same logical address, different physical addresses (different banks)
    ASSERT_TRUE(dm.checkWatchpoint(0x8010, 0x00010, true));   // bank 0
    ASSERT_TRUE(dm.checkWatchpoint(0x8010, 0x04010, true));   // bank 1
    ASSERT_TRUE(dm.checkWatchpoint(0x8010, 0x08010, true));   // bank 2
    ASSERT_TRUE(dm.checkWatchpoint(0x8010, 0x20010, true));   // RAM bank

    // Different logical address - should NOT trigger even if physical matches
    ASSERT_FALSE(dm.checkWatchpoint(0x4010, 0x00010, true));  // different logical
}

// Test: Physical watchpoint only triggers when physical address matches
TEST(check_watchpoint_physical_honours_banking) {
    DebugManager dm;
    // Physical watchpoint at $08010 (bank 2, offset $0010)
    dm.addWatchpoint(0x08010, 16, true, true, true);  // physical watchpoint

    // Only triggers when physical address matches
    ASSERT_TRUE(dm.checkWatchpoint(0x8010, 0x08010, true));   // physical matches
    ASSERT_TRUE(dm.checkWatchpoint(0x4010, 0x08010, true));   // different logical, same physical

    // Should NOT trigger for different physical addresses
    ASSERT_FALSE(dm.checkWatchpoint(0x8010, 0x00010, true));  // bank 0 - wrong physical
    ASSERT_FALSE(dm.checkWatchpoint(0x8010, 0x04010, true));  // bank 1 - wrong physical
    ASSERT_FALSE(dm.checkWatchpoint(0x8010, 0x20010, true));  // RAM - wrong physical
}

// Test checkWatchpoint returns false when no watchpoints exist
TEST(check_watchpoint_empty_list) {
    DebugManager dm;
    // Should return false and not crash when no watchpoints
    ASSERT_FALSE(dm.checkWatchpoint(0x8000, 0x08000, true));
    ASSERT_FALSE(dm.checkWatchpoint(0x8000, 0x08000, false));
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

    std::cout << "\n=== All tests PASSED ===" << std::endl;
    return 0;
}

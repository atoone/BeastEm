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

    std::cout << "\n=== All tests PASSED ===" << std::endl;
    return 0;
}

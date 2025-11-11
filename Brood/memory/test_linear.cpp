#include <larvae/larvae.h>
#include <hive/core/memory/linear_allocator.h>
#include <cstring>

namespace {
    // Helper for readability
    constexpr size_t operator""_KB(unsigned long long kb) { return kb * 1024; }
    constexpr size_t operator""_MB(unsigned long long mb) { return mb * 1024 * 1024; }

    // =============================================================================
    // Basic Functionality
    // =============================================================================

    auto test1 = larvae::RegisterTest("LinearAllocator", "ConstructorInitializesCorrectly", []() {
        hive::memory::LinearAllocator allocator{1024};

        larvae::AssertEqual(allocator.GetUsedMemory(), 0u);
        larvae::AssertEqual(allocator.GetTotalMemory(), 1024u);
        larvae::AssertStringEqual(allocator.GetName(), "LinearAllocator");
    });

    auto test2 = larvae::RegisterTest("LinearAllocator", "AllocateReturnsValidPointer", []() {
        hive::memory::LinearAllocator allocator{1024};

        void* ptr = allocator.Allocate(64, 8);

        larvae::AssertNotNull(ptr);
        larvae::AssertEqual(allocator.GetUsedMemory(), 64u);
    });

    auto test3 = larvae::RegisterTest("LinearAllocator", "AllocateUpdatesUsedMemory", []() {
        hive::memory::LinearAllocator allocator{1024};

        larvae::AssertEqual(allocator.GetUsedMemory(), 0u);

        allocator.Allocate(100, 8);
        larvae::AssertEqual(allocator.GetUsedMemory(), 100u);

        allocator.Allocate(200, 8);
        larvae::AssertEqual(allocator.GetUsedMemory(), 300u);
    });

    auto test4 = larvae::RegisterTest("LinearAllocator", "MultipleAllocationsAreSequential", []() {
        hive::memory::LinearAllocator allocator{1024};

        void* ptr1 = allocator.Allocate(64, 8);
        void* ptr2 = allocator.Allocate(64, 8);
        void* ptr3 = allocator.Allocate(64, 8);

        larvae::AssertNotNull(ptr1);
        larvae::AssertNotNull(ptr2);
        larvae::AssertNotNull(ptr3);

        // Pointers should be in increasing order
        larvae::AssertTrue(ptr2 > ptr1);
        larvae::AssertTrue(ptr3 > ptr2);

        larvae::AssertEqual(allocator.GetUsedMemory(), 192u);
    });

    // =============================================================================
    // Alignment
    // =============================================================================

    auto test5 = larvae::RegisterTest("LinearAllocator", "AllocateRespectsAlignment", []() {
        hive::memory::LinearAllocator allocator{1024};

        void* ptr16 = allocator.Allocate(10, 16);
        larvae::AssertEqual(reinterpret_cast<uintptr_t>(ptr16) % 16, 0u);

        void* ptr32 = allocator.Allocate(10, 32);
        larvae::AssertEqual(reinterpret_cast<uintptr_t>(ptr32) % 32, 0u);

        void* ptr64 = allocator.Allocate(10, 64);
        larvae::AssertEqual(reinterpret_cast<uintptr_t>(ptr64) % 64, 0u);
    });

    auto test6 = larvae::RegisterTest("LinearAllocator", "AllocateWithMisalignedStart", []() {
        hive::memory::LinearAllocator allocator{1024};

        // Allocate 1 byte to misalign current pointer
        allocator.Allocate(1, 1);

        // Next allocation should still be properly aligned
        void* ptr = allocator.Allocate(64, 16);
        larvae::AssertEqual(reinterpret_cast<uintptr_t>(ptr) % 16, 0u);

        // Used memory includes padding
        larvae::AssertGreaterThan(allocator.GetUsedMemory(), 65u);
    });

    // =============================================================================
    // Out of Memory
    // =============================================================================

    auto test7 = larvae::RegisterTest("LinearAllocator", "AllocateReturnsNullWhenOutOfMemory", []() {
        hive::memory::LinearAllocator allocator{128};

        void* ptr1 = allocator.Allocate(64, 8);
        void* ptr2 = allocator.Allocate(64, 8);
        void* ptr3 = allocator.Allocate(64, 8);

        larvae::AssertNotNull(ptr1);
        larvae::AssertNotNull(ptr2);
        larvae::AssertNull(ptr3); // Out of memory
    });

    auto test8 = larvae::RegisterTest("LinearAllocator", "AllocateSizeLargerThanCapacity", []() {
        hive::memory::LinearAllocator allocator{1024};

        void* ptr = allocator.Allocate(2048, 8);

        larvae::AssertNull(ptr);
        larvae::AssertEqual(allocator.GetUsedMemory(), 0u); // Failed allocation doesn't change state
    });

    // =============================================================================
    // Reset
    // =============================================================================

    auto test9 = larvae::RegisterTest("LinearAllocator", "ResetFreesAllMemory", []() {
        hive::memory::LinearAllocator allocator{1024};

        allocator.Allocate(100, 8);
        allocator.Allocate(100, 8);
        allocator.Allocate(100, 8);

        larvae::AssertEqual(allocator.GetUsedMemory(), 300u);

        allocator.Reset();

        larvae::AssertEqual(allocator.GetUsedMemory(), 0u);

        // Should be able to allocate again
        void* ptr = allocator.Allocate(100, 8);
        larvae::AssertNotNull(ptr);
    });

    auto test10 = larvae::RegisterTest("LinearAllocator", "ResetAllowsReuse", []() {
        hive::memory::LinearAllocator allocator{256};

        void* ptr1 = allocator.Allocate(100, 8);
        void* ptr2 = allocator.Allocate(100, 8);

        allocator.Reset();

        // After reset, should get same addresses
        void* ptr3 = allocator.Allocate(100, 8);
        void* ptr4 = allocator.Allocate(100, 8);

        larvae::AssertEqual(ptr1, ptr3);
        larvae::AssertEqual(ptr2, ptr4);
    });

    // =============================================================================
    // Markers
    // =============================================================================

    auto test11 = larvae::RegisterTest("LinearAllocator", "GetMarkerReturnsCurrentPosition", []() {
        hive::memory::LinearAllocator allocator{1024};

        allocator.Allocate(100, 8);
        void* marker1 = allocator.GetMarker();

        allocator.Allocate(200, 8);
        void* marker2 = allocator.GetMarker();

        // Markers should be different (pointer advanced)
        larvae::AssertTrue(marker2 > marker1);
    });

    auto test12 = larvae::RegisterTest("LinearAllocator", "ResetToMarkerRestoresPosition", []() {
        hive::memory::LinearAllocator allocator{1024};

        allocator.Allocate(100, 8);
        void* marker = allocator.GetMarker();

        allocator.Allocate(200, 8);
        larvae::AssertEqual(allocator.GetUsedMemory(), 300u);

        allocator.ResetToMarker(marker);

        larvae::AssertEqual(allocator.GetUsedMemory(), 100u);

        // Can allocate again from marker point
        void* ptr = allocator.Allocate(50, 8);
        larvae::AssertNotNull(ptr);
    });

    auto test13 = larvae::RegisterTest("LinearAllocator", "NestedMarkers", []() {
        hive::memory::LinearAllocator allocator{1024};

        allocator.Allocate(100, 8);
        void* marker1 = allocator.GetMarker();

        allocator.Allocate(200, 8);
        void* marker2 = allocator.GetMarker();

        allocator.Allocate(300, 8);
        larvae::AssertEqual(allocator.GetUsedMemory(), 600u);

        // Reset to marker2 - should free only third allocation
        allocator.ResetToMarker(marker2);
        larvae::AssertEqual(allocator.GetUsedMemory(), 300u);

        // Reset to marker1 - should free second and third allocations
        allocator.ResetToMarker(marker1);
        larvae::AssertEqual(allocator.GetUsedMemory(), 100u);
    });

    // =============================================================================
    // Memory Access
    // =============================================================================

    auto test14 = larvae::RegisterTest("LinearAllocator", "AllocatedMemoryIsReadable", []() {
        hive::memory::LinearAllocator allocator{1024};

        void* ptr = allocator.Allocate(256, 8);
        larvae::AssertNotNull(ptr);

        auto* byte_ptr = static_cast<unsigned char*>(ptr);

        // Read from different positions
        volatile unsigned char v1 = byte_ptr[0];
        volatile unsigned char v2 = byte_ptr[128];
        volatile unsigned char v3 = byte_ptr[255];

        (void)v1; (void)v2; (void)v3;

        larvae::AssertTrue(true); // If we got here, reading succeeded
    });

    auto test15 = larvae::RegisterTest("LinearAllocator", "AllocatedMemoryIsWritable", []() {
        hive::memory::LinearAllocator allocator{1024};

        void* ptr = allocator.Allocate(256, 8);
        auto* byte_ptr = static_cast<unsigned char*>(ptr);

        // Fill with pattern
        std::memset(byte_ptr, 0x42, 256);

        // Verify pattern
        for (size_t i = 0; i < 256; ++i)
        {
            larvae::AssertEqual(byte_ptr[i], static_cast<unsigned char>(0x42));
        }
    });

    auto test16 = larvae::RegisterTest("LinearAllocator", "MultipleAllocationsAreIsolated", []() {
        hive::memory::LinearAllocator allocator{1024};

        void* ptr1 = allocator.Allocate(100, 8);
        void* ptr2 = allocator.Allocate(100, 8);

        auto* bytes1 = static_cast<unsigned char*>(ptr1);
        auto* bytes2 = static_cast<unsigned char*>(ptr2);

        std::memset(bytes1, 0xAA, 100);
        std::memset(bytes2, 0xBB, 100);

        // Verify isolation
        larvae::AssertEqual(bytes1[0], static_cast<unsigned char>(0xAA));
        larvae::AssertEqual(bytes1[99], static_cast<unsigned char>(0xAA));
        larvae::AssertEqual(bytes2[0], static_cast<unsigned char>(0xBB));
        larvae::AssertEqual(bytes2[99], static_cast<unsigned char>(0xBB));
    });

    // =============================================================================
    // New/Delete Templates
    // =============================================================================

    auto test17 = larvae::RegisterTest("LinearAllocator", "NewConstructsObject", []() {
        hive::memory::LinearAllocator allocator{1024};

        struct TestObject
        {
            int value;
            TestObject(int v) : value{v} {}
        };

        TestObject* obj = allocator.New<TestObject>(42);

        larvae::AssertNotNull(obj);
        larvae::AssertEqual(obj->value, 42);
    });

    auto test18 = larvae::RegisterTest("LinearAllocator", "DeleteCallsDestructor", []() {
        hive::memory::LinearAllocator allocator{1024};

        struct TestObject
        {
            bool* destroyed;
            TestObject(bool* d) : destroyed{d} { *destroyed = false; }
            ~TestObject() { *destroyed = true; }
        };

        bool destroyed = false;
        TestObject* obj = allocator.New<TestObject>(&destroyed);

        larvae::AssertFalse(destroyed);

        allocator.Delete(obj);

        larvae::AssertTrue(destroyed);
    });

    auto test19 = larvae::RegisterTest("LinearAllocator", "NewArrayAllocatesMultipleObjects", []() {
        hive::memory::LinearAllocator allocator{1024};

        int* array = static_cast<int*>(allocator.Allocate(10 * sizeof(int), alignof(int)));
        larvae::AssertNotNull(array);

        // Fill array
        for (int i = 0; i < 10; ++i)
        {
            array[i] = i * 10;
        }

        // Verify values
        for (int i = 0; i < 10; ++i)
        {
            larvae::AssertEqual(array[i], i * 10);
        }
    });

    // =============================================================================
    // Edge Cases
    // =============================================================================

    auto test20 = larvae::RegisterTest("LinearAllocator", "AllocateZeroBytes", []() {
        hive::memory::LinearAllocator allocator{1024};

        void* ptr = allocator.Allocate(0, 8);

        // Implementation-defined behavior
        // Should not crash regardless of return value
        (void)ptr;
        larvae::AssertTrue(true);
    });

    auto test21 = larvae::RegisterTest("LinearAllocator", "DeallocateIsNoOp", []() {
        hive::memory::LinearAllocator allocator{1024};

        void* ptr = allocator.Allocate(100, 8);
        size_t used_before = allocator.GetUsedMemory();

        allocator.Deallocate(ptr);

        size_t used_after = allocator.GetUsedMemory();

        // Deallocate should not change used memory (it's a no-op)
        larvae::AssertEqual(used_before, used_after);
    });

    auto test22 = larvae::RegisterTest("LinearAllocator", "DeallocateNullptrIsSafe", []() {
        hive::memory::LinearAllocator allocator{1024};

        allocator.Deallocate(nullptr);

        larvae::AssertTrue(true); // Should not crash
    });

    // =============================================================================
    // Move Semantics
    // =============================================================================

    auto test23 = larvae::RegisterTest("LinearAllocator", "MoveConstructorTransfersOwnership", []() {
        hive::memory::LinearAllocator allocator1{1024};
        allocator1.Allocate(100, 8);

        hive::memory::LinearAllocator allocator2{std::move(allocator1)};

        larvae::AssertEqual(allocator2.GetUsedMemory(), 100u);
        larvae::AssertEqual(allocator2.GetTotalMemory(), 1024u);
    });

    auto test24 = larvae::RegisterTest("LinearAllocator", "MoveAssignmentTransfersOwnership", []() {
        hive::memory::LinearAllocator allocator1{1024};
        allocator1.Allocate(100, 8);

        hive::memory::LinearAllocator allocator2{512};

        allocator2 = std::move(allocator1);

        larvae::AssertEqual(allocator2.GetUsedMemory(), 100u);
        larvae::AssertEqual(allocator2.GetTotalMemory(), 1024u);
    });

    // =============================================================================
    // Performance
    // =============================================================================

    auto test25 = larvae::RegisterTest("LinearAllocator", "ManySmallAllocations", []() {
        hive::memory::LinearAllocator allocator{10_MB};

        // Allocate 10000 small blocks
        for (int i = 0; i < 10000; ++i)
        {
            void* ptr = allocator.Allocate(16, 8);
            larvae::AssertNotNull(ptr);
        }

        larvae::AssertGreaterEqual(allocator.GetUsedMemory(), 160000u);
    });

    auto test26 = larvae::RegisterTest("LinearAllocator", "LargeAllocation", []() {
        hive::memory::LinearAllocator allocator{10_MB};

        void* ptr = allocator.Allocate(5_MB, 16);

        larvae::AssertNotNull(ptr);
        larvae::AssertGreaterEqual(allocator.GetUsedMemory(), 5_MB);

        // Verify memory is usable
        auto* byte_ptr = static_cast<unsigned char*>(ptr);
        byte_ptr[0] = 0xFF;
        byte_ptr[5_MB - 1] = 0xFF;

        larvae::AssertEqual(byte_ptr[0], static_cast<unsigned char>(0xFF));
        larvae::AssertEqual(byte_ptr[5_MB - 1], static_cast<unsigned char>(0xFF));
    });

    // =============================================================================
    // Fixture-based Tests
    // =============================================================================

    class LinearAllocatorFixture : public larvae::TestFixture
    {
    public:
        void SetUp() override
        {
            allocator = new hive::memory::LinearAllocator{4_KB};
        }

        void TearDown() override
        {
            delete allocator;
        }

        hive::memory::LinearAllocator* allocator{nullptr};
    };

    auto test27 = larvae::RegisterTestWithFixture<LinearAllocatorFixture>(
        "LinearAllocatorFixture", "FixtureBasicAllocation",
        [](LinearAllocatorFixture& f) {
            void* ptr = f.allocator->Allocate(256, 8);

            larvae::AssertNotNull(ptr);
            larvae::AssertEqual(f.allocator->GetUsedMemory(), 256u);
        });

    auto test28 = larvae::RegisterTestWithFixture<LinearAllocatorFixture>(
        "LinearAllocatorFixture", "FixtureResetBetweenTests",
        [](LinearAllocatorFixture& f) {
            // Each test gets fresh allocator from SetUp
            larvae::AssertEqual(f.allocator->GetUsedMemory(), 0u);

            f.allocator->Allocate(512, 8);

            larvae::AssertEqual(f.allocator->GetUsedMemory(), 512u);
            // TearDown will clean up automatically
        });
}

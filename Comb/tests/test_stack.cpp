#include <larvae/larvae.h>
#include <comb/stack_allocator.h>
#include <functional>

namespace {
    constexpr size_t operator""_KB(unsigned long long kb) { return kb * 1024; }
    constexpr size_t operator""_MB(unsigned long long mb) { return mb * 1024 * 1024; }

    // =============================================================================
    // Basic Functionality
    // =============================================================================

    auto test1 = larvae::RegisterTest("StackAllocator", "BasicAllocation", []() {
        comb::StackAllocator stack{1024};

        void* ptr1 = stack.Allocate(64, 8);
        larvae::AssertNotNull(ptr1);
        larvae::AssertEqual(stack.GetUsedMemory(), 64u);

        void* ptr2 = stack.Allocate(128, 8);
        larvae::AssertNotNull(ptr2);
        larvae::AssertEqual(stack.GetUsedMemory(), 192u);

        larvae::AssertEqual(static_cast<char*>(ptr2), static_cast<char*>(ptr1) + 64);
    });

    auto test2 = larvae::RegisterTest("StackAllocator", "AlignedAllocation", []() {
        comb::StackAllocator stack{1024};

        void* ptr1 = stack.Allocate(1, 1);
        larvae::AssertNotNull(ptr1);
        size_t after_first = stack.GetUsedMemory();
        larvae::AssertEqual(after_first, 1u);

        void* ptr2 = stack.Allocate(4, 16);
        larvae::AssertNotNull(ptr2);

        // Verify ptr2 is 16-byte aligned
        larvae::AssertTrue(comb::IsAligned(reinterpret_cast<size_t>(ptr2), 16));

        // Calculate expected memory usage:
        // ptr1 uses 1 byte, then we need padding to align ptr2 to 16 bytes, then 4 bytes for ptr2
        // The padding depends on the base address alignment, so we calculate it dynamically
        size_t ptr1_addr = reinterpret_cast<size_t>(ptr1);
        size_t ptr2_addr = reinterpret_cast<size_t>(ptr2);
        size_t expected_used = (ptr2_addr - ptr1_addr) + 4;

        larvae::AssertEqual(stack.GetUsedMemory(), expected_used);
    });

    auto test3 = larvae::RegisterTest("StackAllocator", "OutOfMemory", []() {
        comb::StackAllocator stack{128};

        void* ptr1 = stack.Allocate(100, 8);
        larvae::AssertNotNull(ptr1);

        void* ptr2 = stack.Allocate(50, 8);
        larvae::AssertNull(ptr2);

        larvae::AssertEqual(stack.GetUsedMemory(), 100u);
    });

    // =============================================================================
    // Marker Operations
    // =============================================================================

    auto test4 = larvae::RegisterTest("StackAllocator", "MarkerSaveRestore", []() {
        comb::StackAllocator stack{1024};

        auto marker1 = stack.GetMarker();
        larvae::AssertEqual(marker1, 0u);

        void* ptr1 = stack.Allocate(64, 8);
        larvae::AssertNotNull(ptr1);

        auto marker2 = stack.GetMarker();
        larvae::AssertEqual(marker2, 64u);

        void* ptr2 = stack.Allocate(128, 8);
        larvae::AssertNotNull(ptr2);
        larvae::AssertEqual(stack.GetUsedMemory(), 192u);

        stack.FreeToMarker(marker2);
        larvae::AssertEqual(stack.GetUsedMemory(), 64u);

        void* ptr3 = stack.Allocate(128, 8);
        larvae::AssertNotNull(ptr3);
        larvae::AssertEqual(ptr3, ptr2);

        stack.FreeToMarker(marker1);
        larvae::AssertEqual(stack.GetUsedMemory(), 0u);
    });

    auto test5 = larvae::RegisterTest("StackAllocator", "NestedScopes", []() {
        comb::StackAllocator stack{1024};

        auto outer_marker = stack.GetMarker();
        void* outer_data = stack.Allocate(100, 8);
        larvae::AssertNotNull(outer_data);
        larvae::AssertEqual(stack.GetUsedMemory(), 100u);

        {
            auto inner1_marker = stack.GetMarker();
            void* inner1_data1 = stack.Allocate(50, 8);
            void* inner1_data2 = stack.Allocate(30, 8);
            larvae::AssertNotNull(inner1_data1);
            larvae::AssertNotNull(inner1_data2);
            larvae::AssertEqual(stack.GetUsedMemory(), 190u);

            stack.FreeToMarker(inner1_marker);
            larvae::AssertEqual(stack.GetUsedMemory(), 100u);
        }

        {
            auto inner2_marker = stack.GetMarker();
            void* inner2_data = stack.Allocate(200, 8);
            larvae::AssertNotNull(inner2_data);
            larvae::AssertEqual(stack.GetUsedMemory(), 304u);

            stack.FreeToMarker(inner2_marker);
            larvae::AssertEqual(stack.GetUsedMemory(), 100u);
        }

        stack.FreeToMarker(outer_marker);
        larvae::AssertEqual(stack.GetUsedMemory(), 0u);
    });

    auto test6 = larvae::RegisterTest("StackAllocator", "Reset", []() {
        comb::StackAllocator stack{1024};

        (void)stack.Allocate(100, 8);
        (void)stack.Allocate(200, 8);
        (void)stack.Allocate(150, 8);

        larvae::AssertEqual(stack.GetUsedMemory(), 454u);

        stack.Reset();
        larvae::AssertEqual(stack.GetUsedMemory(), 0u);

        void* ptr = stack.Allocate(500, 8);
        larvae::AssertNotNull(ptr);
    });

    // =============================================================================
    // Special Operations
    // =============================================================================

    auto test7 = larvae::RegisterTest("StackAllocator", "DeallocateIsNoOp", []() {
        comb::StackAllocator stack{1024};

        void* ptr = stack.Allocate(100, 8);
        larvae::AssertNotNull(ptr);
        larvae::AssertEqual(stack.GetUsedMemory(), 100u);

        stack.Deallocate(ptr);
        larvae::AssertEqual(stack.GetUsedMemory(), 100u);
    });

    auto test8 = larvae::RegisterTest("StackAllocator", "NewDeleteHelpers", []() {
        struct TestStruct
        {
            int x;
            float y;
            bool constructed{false};
            bool destructed{false};

            TestStruct(int x_, float y_)
                : x{x_}, y{y_}, constructed{true}
            {}

            ~TestStruct()
            {
                destructed = true;
            }
        };

        comb::StackAllocator stack{1024};

        auto marker = stack.GetMarker();

        TestStruct* obj = comb::New<TestStruct>(stack, 42, 3.14f);
        larvae::AssertNotNull(obj);
        larvae::AssertTrue(obj->constructed);
        larvae::AssertEqual(obj->x, 42);
        larvae::AssertEqual(obj->y, 3.14f);

        comb::Delete(stack, obj);
        larvae::AssertTrue(obj->destructed);

        larvae::AssertGreaterThan(stack.GetUsedMemory(), 0u);

        stack.FreeToMarker(marker);
        larvae::AssertEqual(stack.GetUsedMemory(), 0u);
    });

    auto test9 = larvae::RegisterTest("StackAllocator", "GetFreeMemory", []() {
        comb::StackAllocator stack{1024};

        larvae::AssertEqual(stack.GetFreeMemory(), 1024u);

        (void)stack.Allocate(100, 8);
        larvae::AssertEqual(stack.GetFreeMemory(), 924u);

        (void)stack.Allocate(200, 8);
        larvae::AssertEqual(stack.GetFreeMemory(), 720u);

        stack.Reset();
        larvae::AssertEqual(stack.GetFreeMemory(), 1024u);
    });

    auto test10 = larvae::RegisterTest("StackAllocator", "SatisfiesAllocatorConcept", []() {
        static_assert(comb::Allocator<comb::StackAllocator>, "Must satisfy Allocator concept");
    });

    // =============================================================================
    // Stress Tests
    // =============================================================================

    auto test11 = larvae::RegisterTest("StackAllocator", "ManySmallAllocations", []() {
        comb::StackAllocator stack{10_KB};

        auto marker = stack.GetMarker();

        for (int i = 0; i < 100; ++i)
        {
            void* ptr = stack.Allocate(64, 8);
            larvae::AssertNotNull(ptr);
        }

        larvae::AssertEqual(stack.GetUsedMemory(), 6400u);

        stack.FreeToMarker(marker);
        larvae::AssertEqual(stack.GetUsedMemory(), 0u);
    });

    auto test12 = larvae::RegisterTest("StackAllocator", "FrameAllocationPattern", []() {
        comb::StackAllocator frameStack{1_MB};

        for (int frame = 0; frame < 10; ++frame)
        {
            auto frame_marker = frameStack.GetMarker();

            void* render_commands = frameStack.Allocate(1024, 16);
            void* ui_vertices = frameStack.Allocate(2048, 16);
            void* temp_buffer = frameStack.Allocate(512, 8);

            larvae::AssertNotNull(render_commands);
            larvae::AssertNotNull(ui_vertices);
            larvae::AssertNotNull(temp_buffer);

            frameStack.FreeToMarker(frame_marker);
            larvae::AssertEqual(frameStack.GetUsedMemory(), 0u);
        }
    });

    auto test13 = larvae::RegisterTest("StackAllocator", "RecursivePattern", []() {
        comb::StackAllocator recursionStack{10_KB};

        std::function<int(int)> fibonacci = [&](int depth) -> int
        {
            if (depth <= 1)
                return depth;

            auto marker = recursionStack.GetMarker();

            void* temp = recursionStack.Allocate(128, 8);
            larvae::AssertNotNull(temp);

            int result = fibonacci(depth - 1) + fibonacci(depth - 2);

            recursionStack.FreeToMarker(marker);
            return result;
        };

        int result = fibonacci(10);
        larvae::AssertEqual(result, 55);

        larvae::AssertEqual(recursionStack.GetUsedMemory(), 0u);
    });

    // =============================================================================
    // Move Semantics
    // =============================================================================

    auto test14 = larvae::RegisterTest("StackAllocator", "MoveConstructor", []() {
        comb::StackAllocator stack1{1024};
        (void)stack1.Allocate(100, 8);

        size_t used_before = stack1.GetUsedMemory();

        comb::StackAllocator stack2{std::move(stack1)};

        larvae::AssertEqual(stack2.GetUsedMemory(), used_before);
        larvae::AssertEqual(stack1.GetUsedMemory(), 0u);
        larvae::AssertEqual(stack1.GetTotalMemory(), 0u);
    });

    auto test15 = larvae::RegisterTest("StackAllocator", "MoveAssignment", []() {
        comb::StackAllocator stack1{1024};
        (void)stack1.Allocate(100, 8);

        comb::StackAllocator stack2{512};
        (void)stack2.Allocate(50, 8);

        size_t used1 = stack1.GetUsedMemory();

        stack2 = std::move(stack1);

        larvae::AssertEqual(stack2.GetUsedMemory(), used1);
        larvae::AssertEqual(stack1.GetUsedMemory(), 0u);
    });

    // =============================================================================
    // Alignment Tests
    // =============================================================================

    auto test16 = larvae::RegisterTest("StackAllocator", "AlignmentStress", []() {
        comb::StackAllocator stack{10_KB};

        // Test each alignment starting from fresh state
        size_t alignments[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};

        for (size_t align : alignments)
        {
            stack.Reset();

            void* ptr = stack.Allocate(1, align);
            larvae::AssertNotNull(ptr);

            // Verify alignment using modulo
            size_t addr = reinterpret_cast<size_t>(ptr);
            larvae::AssertEqual(addr % align, 0u);
        }
    });
}

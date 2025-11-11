#pragma once

#include <comb/allocator_concepts.h>
#include <hive/core/assert.h>
#include <comb/utils.h>
#include <comb/platform.h>
#include <cstddef>

namespace comb
{
    /**
     * Stack allocator with LIFO (Last-In-First-Out) deallocation using markers
     *
     * Similar to LinearAllocator but supports scoped deallocations via markers.
     * Allocates memory sequentially by bumping a pointer forward.
     * Deallocations are done by restoring to a saved marker position.
     *
     * Satisfies the comb::Allocator concept.
     *
     * Use cases:
     * - Scoped temporary allocations (nested function calls)
     * - Recursive algorithms with cleanup at each level
     * - Frame temps with multiple Reset points
     * - Per-scope resource management
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────┐
     * │ [Alloc 1][Alloc 2][Alloc 3]...[Alloc N]  [Free Space]      │
     * │  ←──────── Used Memory ────────────→                       │
     * └────────────────────────────────────────────────────────────┘
     *  ↑                                   ↑                       ↑
     *  base                                current (marker)        capacity
     *
     * Marker mechanism:
     * - GetMarker() saves current allocation position
     * - FreeToMarker(m) restores to saved position
     * - All allocations after marker are freed
     *
     * Performance characteristics:
     * - Allocation: O(1) - pointer bump (similar to LinearAllocator)
     * - GetMarker: O(1) - instant (just returns offset)
     * - FreeToMarker: O(1) - instant (single pointer write)
     * - Thread-safe: No (use per-thread allocators)
     * - Fragmentation: None (sequential allocation)
     *
     * Limitations:
     * - Must free in LIFO order using markers
     * - Individual Deallocate() is no-op (use markers instead)
     * - Not thread-safe (requires external synchronization)
     * - Fixed capacity (set at construction)
     *
     * Comparison with LinearAllocator:
     * | Feature          | LinearAllocator | StackAllocator |
     * |------------------|-----------------|----------------|
     * | Allocation       | O(1)            | O(1)           |
     * | Individual free  | No              | No             |
     * | Scoped free      | No              | Yes (markers)  |
     * | Reset all        | Yes             | Yes            |
     * | Use case         | Frame temps     | Scoped temps   |
     *
     * Example:
     * @code
     *   comb::StackAllocator stack{1024 * 1024};  // 1 MB
     *
     *   // Outer scope
     *   auto marker1 = stack.GetMarker();
     *   auto* data1 = comb::New<MyData>(stack, args...);
     *
     *   {
     *       // Inner scope
     *       auto marker2 = stack.GetMarker();
     *       auto* temp1 = comb::New<TempData>(stack, args...);
     *       auto* temp2 = comb::New<TempData>(stack, args...);
     *
     *       // Use temp1, temp2...
     *
     *       stack.FreeToMarker(marker2);  // Free temp1, temp2
     *   }
     *
     *   // data1 still valid here
     *
     *   stack.FreeToMarker(marker1);  // Free everything
     * @endcode
     */
    class StackAllocator
    {
    public:
        /**
         * Marker type - represents a saved position in the allocator
         * Can be used to restore allocator state to a previous point
         */
        using Marker = size_t;

        /**
         * Construct stack allocator with given capacity
         * @param capacity Size in bytes to allocate from OS
         */
        explicit StackAllocator(size_t capacity)
            : capacity_{capacity}
            , current_{0}
        {
            hive::Assert(capacity > 0, "Stack capacity must be > 0");

            // Allocate memory from OS
            memory_block_ = AllocatePages(capacity);
            hive::Assert(memory_block_ != nullptr, "Failed to allocate stack memory");
        }

        /**
         * Destructor - frees memory back to OS
         */
        ~StackAllocator()
        {
            if (memory_block_)
            {
                FreePages(memory_block_, capacity_);
            }
        }

        // Disable copy (allocators manage memory, shouldn't be copied)
        StackAllocator(const StackAllocator&) = delete;
        StackAllocator& operator=(const StackAllocator&) = delete;

        // Allow move (transfer ownership)
        StackAllocator(StackAllocator&& other) noexcept
            : memory_block_{other.memory_block_}
            , capacity_{other.capacity_}
            , current_{other.current_}
        {
            other.memory_block_ = nullptr;
            other.capacity_ = 0;
            other.current_ = 0;
        }

        StackAllocator& operator=(StackAllocator&& other) noexcept
        {
            if (this != &other)
            {
                if (memory_block_)
                {
                    FreePages(memory_block_, capacity_);
                }

                memory_block_ = other.memory_block_;
                capacity_ = other.capacity_;
                current_ = other.current_;

                other.memory_block_ = nullptr;
                other.capacity_ = 0;
                other.current_ = 0;
            }
            return *this;
        }

        /**
         * Allocate memory with specified size and alignment
         * Bumps current pointer forward
         *
         * @param size Number of bytes to allocate
         * @param alignment Required alignment (must be power of 2)
         * @return Pointer to allocated memory, or nullptr if out of space
         *
         * IMPORTANT: Does NOT support individual deallocation.
         * Use GetMarker() + FreeToMarker() for scoped cleanup.
         */
        [[nodiscard]] void* Allocate(size_t size, size_t alignment)
        {
            hive::Assert(size > 0, "Cannot allocate 0 bytes");
            hive::Assert(IsPowerOfTwo(alignment), "Alignment must be power of 2");

            // Calculate the actual current address
            const uintptr_t current_addr = reinterpret_cast<uintptr_t>(memory_block_) + current_;

            // Align the address (not just the offset!)
            const uintptr_t aligned_addr = AlignUp(current_addr, alignment);

            // Calculate the new offset from base
            const size_t aligned_current = aligned_addr - reinterpret_cast<uintptr_t>(memory_block_);
            const size_t padding = aligned_current - current_;

            // Check if we have enough space
            const size_t required = padding + size;
            const size_t remaining = capacity_ - current_;

            if (required > remaining)
            {
                return nullptr;  // Out of memory
            }

            // Allocate
            current_ = aligned_current + size;
            return reinterpret_cast<void*>(aligned_addr);
        }

        /**
         * Deallocate memory - NO-OP for StackAllocator
         *
         * @param ptr Pointer to deallocate (ignored)
         *
         * NOTE: Individual deallocation is not supported.
         * Use GetMarker() + FreeToMarker() for scoped cleanup,
         * or Reset() to free all memory.
         */
        void Deallocate(void* ptr)
        {
            // StackAllocator doesn't support individual deallocation
            // Use markers instead
            (void)ptr;
        }

        /**
         * Get current marker position
         * Save this to later restore allocator state
         *
         * @return Marker representing current allocation position
         */
        [[nodiscard]] Marker GetMarker() const
        {
            return current_;
        }

        /**
         * Free all allocations back to a saved marker
         * Restores allocator to state when marker was created
         *
         * @param marker Marker obtained from GetMarker()
         *
         * IMPORTANT: Marker must be valid (from this allocator).
         * Passing invalid marker is undefined behavior.
         * Markers must be freed in LIFO order (stack discipline).
         */
        void FreeToMarker(Marker marker)
        {
            hive::Assert(marker <= current_, "Invalid marker (beyond current position)");
            hive::Assert(marker <= capacity_, "Invalid marker (beyond capacity)");

            current_ = marker;
        }

        /**
         * Reset allocator - frees all allocations
         * Equivalent to FreeToMarker(0)
         */
        void Reset()
        {
            current_ = 0;
        }

        /**
         * Get number of bytes currently allocated
         * @return Bytes used
         */
        [[nodiscard]] size_t GetUsedMemory() const
        {
            return current_;
        }

        /**
         * Get total capacity of allocator
         * @return Total bytes available
         */
        [[nodiscard]] size_t GetTotalMemory() const
        {
            return capacity_;
        }

        /**
         * Get allocator name for debugging
         * @return "StackAllocator"
         */
        [[nodiscard]] const char* GetName() const
        {
            return "StackAllocator";
        }

        /**
         * Get number of free bytes remaining
         * @return Bytes available for allocation
         */
        [[nodiscard]] size_t GetFreeMemory() const
        {
            return capacity_ - current_;
        }

    private:
        void* memory_block_{nullptr};   // Base memory block
        size_t capacity_{0};             // Total capacity in bytes
        size_t current_{0};              // Current allocation offset
    };

    static_assert(Allocator<StackAllocator>, "StackAllocator must satisfy Allocator concept");
}

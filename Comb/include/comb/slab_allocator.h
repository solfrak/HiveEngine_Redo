#pragma once

#include <comb/allocator_concepts.h>
#include <hive/core/assert.h>
#include <comb/utils.h>
#include <comb/platform.h>
#include <cstddef>
#include <array>

namespace comb
{
    /**
     * Slab allocator with multiple size classes and free-lists
     *
     * Manages multiple "slabs" (pools) of different object sizes.
     * Each slab is a PoolAllocator for a specific size class.
     * Allocations are routed to the smallest slab that can fit the request.
     *
     * Satisfies the comb::Allocator concept.
     *
     * Use cases:
     * - General-purpose allocation with known size distribution
     * - Multiple object types with different sizes
     * - Need fast allocation + deallocation with memory reuse
     * - Alternative to malloc with better performance
     *
     * Memory layout (example with 3 size classes: 32, 64, 128):
     * ┌──────────────────────────────────────────────────────┐
     * │ Slab 0 (32B):  [obj][obj][obj]...[obj] + free-list   │
     * │ Slab 1 (64B):  [obj][obj][obj]...[obj] + free-list   │
     * │ Slab 2 (128B): [obj][obj][obj]...[obj] + free-list   │
     * └──────────────────────────────────────────────────────┘
     *
     * Each slab operates independently with its own free-list.
     *
     * Performance characteristics:
     * - Allocation: O(1) - find slab O(K) + pop from free-list O(1), K = size classes
     * - Deallocation: O(1) - push to free-list
     * - Typical K = 8-16 size classes (small overhead)
     * - Thread-safe: No (use per-thread or add synchronization)
     * - Fragmentation: None (pre-allocated slabs)
     *
     * Limitations:
     * - Fixed size classes (set at compile time)
     * - Fixed capacity per slab
     * - Returns nullptr when slab exhausted (no hidden allocations)
     * - Not thread-safe by default
     *
     * Size class selection:
     * - Sizes are rounded up to next power of 2
     * - Common pattern: 16, 32, 64, 128, 256, 512, 1024, 2048
     * - Should cover 95% of your allocation sizes
     *
     * Example:
     * @code
     *   // Create slab allocator with 1000 objects per size class
     *   // Size classes: 32, 64, 128, 256, 512 bytes
     *   comb::SlabAllocator<1000, 32, 64, 128, 256, 512> slabs;
     *
     *   // Allocate 60 bytes - routed to 64-byte slab
     *   void* ptr1 = slabs.Allocate(60, 8);
     *   if (!ptr1) {
     *       // 64-byte slab exhausted
     *       return;
     *   }
     *
     *   // Allocate 200 bytes - routed to 256-byte slab
     *   void* ptr2 = slabs.Allocate(200, 8);
     *
     *   // Free memory - returns to appropriate free-list
     *   slabs.Deallocate(ptr1);  // Returns to 64-byte slab
     *   slabs.Deallocate(ptr2);  // Returns to 256-byte slab
     *
     *   // Memory immediately available for reuse
     *   void* ptr3 = slabs.Allocate(60, 8);  // Reuses ptr1's slot
     *
     *   // Reset all slabs at once
     *   slabs.Reset();
     * @endcode
     */
    template<size_t ObjectsPerSlab, size_t... SizeClasses>
    class SlabAllocator
    {
        static_assert(sizeof...(SizeClasses) > 0, "Must provide at least one size class");
        static_assert(ObjectsPerSlab > 0, "Must allocate at least one object per slab");

    private:
        // Size classes rounded to powers of 2 and sorted
        static constexpr auto sizes_ = MakeArray(NextPowerOfTwo(SizeClasses)...);
        static_assert(IsSorted(sizes_), "Size classes must be sorted");

        static constexpr size_t NumSlabs = sizeof...(SizeClasses);

        // Each slab: memory block + free-list head + usage counter
        struct Slab
        {
            void* memory_block{nullptr};
            void* free_list_head{nullptr};
            size_t used_count{0};
            size_t slot_size{0};
            size_t total_size{0};

            void Initialize(size_t size)
            {
                slot_size = size;
                total_size = ObjectsPerSlab * slot_size;

                // Allocate memory from OS
                memory_block = AllocatePages(total_size);
                hive::Assert(memory_block != nullptr, "Failed to allocate slab memory");

                // Build free-list
                RebuildFreeList();
            }

            void Destroy()
            {
                if (memory_block)
                {
                    FreePages(memory_block, total_size);
                    memory_block = nullptr;
                    free_list_head = nullptr;
                }
            }

            void RebuildFreeList()
            {
                char* current = static_cast<char*>(memory_block);
                free_list_head = current;

                for (size_t i = 0; i < ObjectsPerSlab - 1; ++i)
                {
                    char* next = current + slot_size;
                    *reinterpret_cast<void**>(current) = next;
                    current = next;
                }

                *reinterpret_cast<void**>(current) = nullptr;
                used_count = 0;
            }

            void* Allocate()
            {
                if (!free_list_head)
                {
                    return nullptr;
                }

                void* ptr = free_list_head;
                free_list_head = *static_cast<void**>(free_list_head);
                ++used_count;
                return ptr;
            }

            void Deallocate(void* ptr)
            {
                if (!ptr)
                    return;

                hive::Assert(used_count > 0, "Deallocate called more than Allocate");

                *static_cast<void**>(ptr) = free_list_head;
                free_list_head = ptr;
                --used_count;
            }

            bool Contains(void* ptr) const
            {
                if (!ptr || !memory_block)
                    return false;

                const char* start = static_cast<const char*>(memory_block);
                const char* end = start + (ObjectsPerSlab * slot_size);
                const char* p = static_cast<const char*>(ptr);

                return p >= start && p < end;
            }

            size_t GetUsedMemory() const
            {
                return used_count * slot_size;
            }

            size_t GetTotalMemory() const
            {
                return ObjectsPerSlab * slot_size;
            }

            size_t GetFreeCount() const
            {
                return ObjectsPerSlab - used_count;
            }
        };

        std::array<Slab, NumSlabs> slabs_{};

        // Find slab index for given size
        constexpr size_t FindSlabIndex(size_t size) const
        {
            for (size_t i = 0; i < sizes_.size(); ++i)
            {
                if (size <= sizes_[i])
                {
                    return i;
                }
            }
            return NumSlabs;  // No slab large enough
        }

    public:
        SlabAllocator(const SlabAllocator&) = delete;
        SlabAllocator& operator=(const SlabAllocator&) = delete;

        /**
         * Construct slab allocator and initialize all slabs
         */
        SlabAllocator()
        {
            for (size_t i = 0; i < NumSlabs; ++i)
            {
                slabs_[i].Initialize(sizes_[i]);
            }
        }

        /**
         * Destructor - frees all slab memory
         */
        ~SlabAllocator()
        {
            for (auto& slab : slabs_)
            {
                slab.Destroy();
            }
        }

        // Allow move
        SlabAllocator(SlabAllocator&& other) noexcept
            : slabs_{std::move(other.slabs_)}
        {
            // Invalidate other
            for (auto& slab : other.slabs_)
            {
                slab.memory_block = nullptr;
                slab.free_list_head = nullptr;
                slab.used_count = 0;
            }
        }

        SlabAllocator& operator=(SlabAllocator&& other) noexcept
        {
            if (this != &other)
            {
                // Destroy our slabs
                for (auto& slab : slabs_)
                {
                    slab.Destroy();
                }

                // Move from other
                slabs_ = std::move(other.slabs_);

                // Invalidate other
                for (auto& slab : other.slabs_)
                {
                    slab.memory_block = nullptr;
                    slab.free_list_head = nullptr;
                    slab.used_count = 0;
                }
            }
            return *this;
        }

        /**
         * Allocate memory from appropriate slab
         *
         * @param size Number of bytes to allocate
         * @param alignment Required alignment (must be <= alignof(std::max_align_t))
         * @return Pointer to allocated memory, or nullptr if:
         *         - No slab can fit the requested size
         *         - Appropriate slab is exhausted
         *
         * IMPORTANT: Does NOT fallback to operator new. Returns nullptr when out of memory.
         */
        [[nodiscard]] void* Allocate(size_t size, size_t alignment)
        {
            hive::Assert(alignment <= alignof(std::max_align_t),
                         "SlabAllocator alignment limited to max_align_t");

            const size_t slab_index = FindSlabIndex(size);

            // No slab large enough
            if (slab_index >= NumSlabs)
            {
                return nullptr;
            }

            return slabs_[slab_index].Allocate();
        }

        /**
         * Deallocate memory back to appropriate slab
         *
         * @param ptr Pointer to deallocate (can be nullptr)
         *
         * IMPORTANT: Pointer must have been allocated from THIS allocator.
         * Finds which slab owns the pointer and returns it to that slab's free-list.
         */
        void Deallocate(void* ptr)
        {
            if (!ptr)
                return;

            // Find which slab owns this pointer
            for (auto& slab : slabs_)
            {
                if (slab.Contains(ptr))
                {
                    slab.Deallocate(ptr);
                    return;
                }
            }

            // Pointer not from any slab - error
            hive::Assert(false, "Pointer not allocated from this SlabAllocator");
        }

        /**
         * Reset all slabs - marks all memory as free
         * Rebuilds free-lists for all slabs
         */
        void Reset()
        {
            for (auto& slab : slabs_)
            {
                slab.RebuildFreeList();
            }
        }

        /**
         * Get total bytes currently allocated across all slabs
         */
        [[nodiscard]] size_t GetUsedMemory() const
        {
            size_t total = 0;
            for (const auto& slab : slabs_)
            {
                total += slab.GetUsedMemory();
            }
            return total;
        }

        /**
         * Get total capacity across all slabs
         */
        [[nodiscard]] size_t GetTotalMemory() const
        {
            size_t total = 0;
            for (const auto& slab : slabs_)
            {
                total += slab.GetTotalMemory();
            }
            return total;
        }

        /**
         * Get allocator name for debugging
         */
        [[nodiscard]] const char* GetName() const
        {
            return "SlabAllocator";
        }

        /**
         * Get number of size classes
         */
        [[nodiscard]] constexpr size_t GetSlabCount() const
        {
            return NumSlabs;
        }

        /**
         * Get size classes array
         */
        [[nodiscard]] constexpr auto GetSizeClasses() const
        {
            return sizes_;
        }

        /**
         * Get usage stats for a specific slab
         */
        [[nodiscard]] size_t GetSlabUsedCount(size_t slab_index) const
        {
            hive::Assert(slab_index < NumSlabs, "Slab index out of range");
            return slabs_[slab_index].used_count;
        }

        /**
         * Get free count for a specific slab
         */
        [[nodiscard]] size_t GetSlabFreeCount(size_t slab_index) const
        {
            hive::Assert(slab_index < NumSlabs, "Slab index out of range");
            return slabs_[slab_index].GetFreeCount();
        }
    };

    template<size_t N, size_t... Sizes>
    concept ValidSlabAllocator = Allocator<SlabAllocator<N, Sizes...>>;
}

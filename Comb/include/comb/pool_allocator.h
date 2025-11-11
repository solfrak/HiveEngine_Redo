#pragma once

#include <comb/allocator_concepts.h>
#include <hive/core/assert.h>
#include <comb/platform.h>
#include <cstddef>
#include <algorithm>

namespace comb
{
    /**
     * Pool allocator for fixed-size objects with free-list recycling
     *
     * Pre-allocates a pool of N objects of type T and manages them via free-list.
     * Provides ultra-fast O(1) allocation and deallocation with memory reuse.
     * Perfect for ECS entities, components, particles, and other fixed-size objects.
     *
     * Satisfies the comb::Allocator concept.
     *
     * Use cases:
     * - ECS entities and components (fixed types)
     * - Particle systems (allocate/free particles constantly)
     * - Object pools for frequently created/destroyed objects
     * - Game objects with predictable lifecycle
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────┐
     * │ [Object 0][Object 1][Object 2]...[Object N-1]      │
     * │    ↓         ↓         ↓                           │
     * │  free    in-use     free                           │
     * │    │                  │                            │
     * │    └──────────────────┘                            │
     * │  (free-list links free objects together)           │
     * └────────────────────────────────────────────────────┘
     *
     * Free-list mechanism:
     * - Each free slot stores a pointer to next free slot
     * - Allocation: pop from free-list head (O(1))
     * - Deallocation: push to free-list head (O(1))
     * - No fragmentation (all objects same size)
     *
     * Performance characteristics:
     * - Allocation: O(1) - pop from free-list
     * - Deallocation: O(1) - push to free-list
     * - Reset: O(1) - rebuild free-list
     * - Thread-safe: No (use per-thread pools or add mutex)
     * - Fragmentation: None (fixed-size objects)
     *
     * Limitations:
     * - Fixed object size (one pool per type)
     * - Fixed capacity (set at construction)
     * - Returns nullptr when pool exhausted (no hidden allocations)
     * - Not thread-safe (use PoolAllocator per thread or add synchronization)
     *
     * Example:
     * @code
     *   // Create pool for 1000 Enemy objects
     *   comb::PoolAllocator<Enemy> enemyPool{1000};
     *
     *   // Allocate enemy (O(1))
     *   Enemy* enemy = comb::New<Enemy>(enemyPool, health, position);
     *   if (!enemy) {
     *       // Pool exhausted - handle error
     *       return;
     *   }
     *
     *   // Use enemy...
     *
     *   // Free enemy - returns to free-list (O(1))
     *   comb::Delete(enemyPool, enemy);
     *
     *   // Memory immediately available for reuse!
     *   Enemy* another = comb::New<Enemy>(enemyPool); // Reuses freed memory
     *
     *   // Reset pool - frees all objects
     *   enemyPool.Reset();
     * @endcode
     */
    template<typename T>
    class PoolAllocator
    {
    public:
        /**
         * Construct pool allocator with capacity for N objects
         * @param capacity Number of objects to pre-allocate
         */
        explicit PoolAllocator(size_t capacity)
            : capacity_{capacity}
            , used_count_{0}
        {
            hive::Assert(capacity > 0, "Pool capacity must be > 0");

            // Each slot needs to be large enough to store either:
            // - T object, OR
            // - void* pointer (for free-list)
            constexpr size_t slot_size = (sizeof(T) > sizeof(void*)) ? sizeof(T) : sizeof(void*);

            // Allocate memory from OS
            total_size_ = capacity * slot_size;
            memory_block_ = AllocatePages(total_size_);
            hive::Assert(memory_block_ != nullptr, "Failed to allocate pool memory");

            // Build initial free-list
            Reset();
        }

        /**
         * Destructor - frees memory back to OS
         */
        ~PoolAllocator()
        {
            if (memory_block_)
            {
                FreePages(memory_block_, total_size_);
            }
        }

        // Disable copy (pools manage memory, shouldn't be copied)
        PoolAllocator(const PoolAllocator&) = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;

        // Allow move (transfer ownership)
        PoolAllocator(PoolAllocator&& other) noexcept
            : memory_block_{other.memory_block_}
            , free_list_head_{other.free_list_head_}
            , capacity_{other.capacity_}
            , used_count_{other.used_count_}
            , total_size_{other.total_size_}
        {
            other.memory_block_ = nullptr;
            other.free_list_head_ = nullptr;
            other.capacity_ = 0;
            other.used_count_ = 0;
            other.total_size_ = 0;
        }

        PoolAllocator& operator=(PoolAllocator&& other) noexcept
        {
            if (this != &other)
            {
                if (memory_block_)
                {
                    FreePages(memory_block_, total_size_);
                }

                memory_block_ = other.memory_block_;
                free_list_head_ = other.free_list_head_;
                capacity_ = other.capacity_;
                used_count_ = other.used_count_;
                total_size_ = other.total_size_;

                other.memory_block_ = nullptr;
                other.free_list_head_ = nullptr;
                other.capacity_ = 0;
                other.used_count_ = 0;
                other.total_size_ = 0;
            }
            return *this;
        }

        /**
         * Allocate memory for one object from the pool
         * Pops from free-list (O(1))
         *
         * @param size Number of bytes (must be <= sizeof(T), ignored)
         * @param alignment Required alignment (must be <= alignof(T), ignored)
         * @return Pointer to memory, or nullptr if pool exhausted
         *
         * Note: size and alignment parameters required by Allocator concept,
         * but ignored since pool only handles T-sized objects.
         */
        [[nodiscard]] void* Allocate(size_t size, size_t alignment)
        {
            hive::Assert(size <= sizeof(T), "PoolAllocator can only allocate sizeof(T) bytes");
            hive::Assert(alignment <= alignof(T), "PoolAllocator alignment limited to alignof(T)");

            // Pool exhausted
            if (!free_list_head_)
            {
                return nullptr;
            }

            // Pop from free-list
            void* ptr = free_list_head_;
            free_list_head_ = *static_cast<void**>(free_list_head_);
            ++used_count_;

            return ptr;
        }

        /**
         * Deallocate memory back to the pool
         * Pushes to free-list (O(1))
         *
         * @param ptr Pointer to deallocate (can be nullptr)
         *
         * IMPORTANT: Pointer must have been allocated from THIS pool.
         * No validation is performed - deallocating wrong pointer is undefined behavior.
         */
        void Deallocate(void* ptr)
        {
            if (!ptr)
                return;

            hive::Assert(used_count_ > 0, "Deallocate called more times than Allocate");

            // Push to free-list head
            *static_cast<void**>(ptr) = free_list_head_;
            free_list_head_ = ptr;
            --used_count_;
        }

        /**
         * Reset pool - marks all objects as free
         * Rebuilds free-list to initial state
         * Does NOT call destructors on objects!
         */
        void Reset()
        {
            constexpr size_t slot_size = (sizeof(T) > sizeof(void*)) ? sizeof(T) : sizeof(void*);

            // Rebuild free-list using std::byte for raw memory manipulation
            auto* current = static_cast<std::byte*>(memory_block_);
            free_list_head_ = current;

            for (size_t i = 0; i < capacity_ - 1; ++i)
            {
                auto* next = current + slot_size;
                *reinterpret_cast<void**>(current) = next;
                current = next;
            }

            // Last slot points to nullptr
            *reinterpret_cast<void**>(current) = nullptr;

            used_count_ = 0;
        }

        /**
         * Get number of objects currently allocated
         * @return Number of objects in use
         */
        [[nodiscard]] size_t GetUsedMemory() const
        {
            return used_count_ * sizeof(T);
        }

        /**
         * Get total capacity of pool
         * @return Total bytes for all objects
         */
        [[nodiscard]] size_t GetTotalMemory() const
        {
            return capacity_ * sizeof(T);
        }

        /**
         * Get allocator name for debugging
         * @return "PoolAllocator"
         */
        [[nodiscard]] const char* GetName() const
        {
            return "PoolAllocator";
        }

        /**
         * Get pool capacity
         * @return Maximum number of objects
         */
        [[nodiscard]] size_t GetCapacity() const
        {
            return capacity_;
        }

        /**
         * Get number of objects currently in use
         * @return Number of allocated objects
         */
        [[nodiscard]] size_t GetUsedCount() const
        {
            return used_count_;
        }

        /**
         * Get number of free slots available
         * @return Number of objects that can still be allocated
         */
        [[nodiscard]] size_t GetFreeCount() const
        {
            return capacity_ - used_count_;
        }

    private:
        void* memory_block_{nullptr};      // Pre-allocated memory block
        void* free_list_head_{nullptr};    // Head of free-list
        size_t capacity_{0};               // Total number of objects
        size_t used_count_{0};             // Number of objects currently allocated
        size_t total_size_{0};             // Total size in bytes (for FreePages)
    };

    template<typename T>
    concept ValidPoolAllocator = Allocator<PoolAllocator<T>>;
}

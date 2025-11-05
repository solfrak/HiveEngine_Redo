#pragma once

#include <cstddef>
#include <utility>
#include <type_traits>

namespace hive::memory
{
    /**
     * Base class for custom memory allocators
     */
    class Allocator
    {
    public:
        virtual ~Allocator() = default;

        /**
         * Allocate aligned memory
         * @param size Number of bytes to allocate
         * @param alignment Required alignment (must be power of 2)
         * @return Pointer to allocated memory, or nullptr on failure
         */
        virtual void* Allocate(size_t size, size_t alignment) = 0;

        /**
         * Deallocate memory
         * @param ptr Pointer to memory allocated by this allocator
         */
        virtual void Deallocate(void* ptr) = 0;

        /**
         * Allocate and construct an object
         * @tparam T Type to construct
         * @tparam Args Constructor argument types
         * @param args Constructor arguments
         * @return Pointer to constructed object, or nullptr on failure
         */
        template<typename T, typename... Args>
        T* New(Args&&... args)
        {
            void* mem = Allocate(sizeof(T), alignof(T));
            if (!mem) return nullptr;
            return new (mem) T{std::forward<Args>(args)...};
        }

        /**
         * Destroy and deallocate an object
         * @tparam T Type to destroy
         * @param ptr Pointer to object (can be nullptr)
         */
        template<typename T>
        void Delete(T* ptr)
        {
            if (!ptr) return;

            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                ptr->~T();
            }

            Deallocate(ptr);
        }

        /**
         * Get currently used memory
         * @return Bytes allocated and not yet freed
         */
        virtual size_t GetUsedMemory() const { return 0; }

        /**
         * Get total memory capacity
         * @return Total bytes available to this allocator
         */
        virtual size_t GetTotalMemory() const { return 0; }

        /**
         * Get allocator name for debugging
         * @return Human-readable allocator name
         */
        virtual const char* GetName() const = 0;

    protected:
        void* base_{nullptr};
        size_t capacity_{0};
    };
}

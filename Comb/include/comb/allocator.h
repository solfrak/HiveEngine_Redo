#pragma once

#include <cstddef>
#include <utility>

namespace comb
{
    class Allocator
    {
    public:
        virtual ~Allocator() = default;

        virtual void* Allocate(size_t size, size_t alignment) = 0;
        virtual void Deallocate(void* ptr) = 0;

        template<typename T, typename... Args>
        T* New(Args&&... args)
        {
            void* mem = Allocate(sizeof(T), alignof(T));
            if (!mem) return nullptr;
            return new (mem) T(std::forward<Args>(args)...);
        }

        template<typename T>
        void Delete(T* ptr)
        {
            if (!ptr) return;
            ptr->~T();
            Deallocate(ptr);
        }

        virtual size_t GetUsedMemory() const { return 0; }
        virtual size_t GetTotalMemory() const { return 0; }
        virtual const char* GetName() const = 0;

    protected:
        void* m_Base{nullptr};
        size_t m_Capacity{0};
    };

}

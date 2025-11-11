#include <comb/precomp.h>
#include <comb/linear_allocator.h>
#include <comb/platform.h>
#include <comb/utils.h>
#include <hive/core/assert.h>

namespace comb
{
    LinearAllocator::LinearAllocator(size_t capacity)
        : base_{AllocatePages(capacity)}
        , current_{base_}
        , capacity_{capacity}
    {
        hive::Assert(base_ != nullptr, "Failed to allocate memory for LinearAllocator");
    }

    LinearAllocator::~LinearAllocator()
    {
        if (base_)
        {
            FreePages(base_, capacity_);
            base_ = nullptr;
            current_ = nullptr;
        }
    }

    LinearAllocator::LinearAllocator(LinearAllocator&& other) noexcept
        : base_{other.base_}
        , current_{other.current_}
        , capacity_{other.capacity_}
    {
        other.base_ = nullptr;
        other.current_ = nullptr;
        other.capacity_ = 0;
    }

    LinearAllocator& LinearAllocator::operator=(LinearAllocator&& other) noexcept
    {
        if (this != &other)
        {
            if (base_)
            {
                FreePages(base_, capacity_);
            }

            base_ = other.base_;
            current_ = other.current_;
            capacity_ = other.capacity_;

            other.base_ = nullptr;
            other.current_ = nullptr;
            other.capacity_ = 0;
        }

        return *this;
    }

    void* LinearAllocator::Allocate(size_t size, size_t alignment)
    {
        hive::Assert(IsPowerOfTwo(alignment), "Alignment must be a power of 2");
        hive::Assert(size > 0, "Cannot allocate 0 bytes");

        const uintptr_t current_addr = reinterpret_cast<uintptr_t>(current_);
        const uintptr_t aligned_addr = AlignUp(current_addr, alignment);
        const size_t padding = aligned_addr - current_addr;
        const size_t required = padding + size;
        const size_t used = current_addr - reinterpret_cast<uintptr_t>(base_);
        const size_t remaining = capacity_ - used;

        if (required > remaining)
        {
            return nullptr;
        }

        void* result = reinterpret_cast<void*>(aligned_addr);
        current_ = reinterpret_cast<void*>(aligned_addr + size);

        return result;
    }

    void LinearAllocator::Deallocate(void* ptr)
    {
        (void)ptr;
    }

    void LinearAllocator::Reset()
    {
        current_ = base_;
    }

    void* LinearAllocator::GetMarker() const
    {
        return current_;
    }

    void LinearAllocator::ResetToMarker(void* marker)
    {
        const uintptr_t marker_addr = reinterpret_cast<uintptr_t>(marker);
        const uintptr_t base_addr = reinterpret_cast<uintptr_t>(base_);
        const uintptr_t end_addr = base_addr + capacity_;

        hive::Assert(marker_addr >= base_addr && marker_addr <= end_addr,
                     "Marker is outside allocator memory range");

        current_ = marker;
    }

    size_t LinearAllocator::GetUsedMemory() const
    {
        return reinterpret_cast<uintptr_t>(current_) - reinterpret_cast<uintptr_t>(base_);
    }

    size_t LinearAllocator::GetTotalMemory() const
    {
        return capacity_;
    }

    const char* LinearAllocator::GetName() const
    {
        return "LinearAllocator";
    }
}

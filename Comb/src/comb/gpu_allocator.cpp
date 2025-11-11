/**
 * GPU Allocator Implementation - Vulkan Backend
 */

#ifdef COMB_HAS_VULKAN

#include <comb/precomp.h>
#include <comb/gpu_allocator.h>
#include <comb/utils.h>
#include <comb/combmodule.h>
#include <hive/core/log.h>

#include <algorithm>
#include <mutex>
#include <vector>

namespace comb
{

namespace
{
const hive::LogCategory LogGPUAlloc("GPUAllocator", &comb::LogCombRoot);

constexpr VkMemoryPropertyFlags GetMemoryProperties(MemoryType type)
{
    switch (type)
    {
    case MemoryType::DeviceLocal:
        return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    case MemoryType::HostVisible:
        return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    case MemoryType::HostCached:
        return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    }
    return 0;
}

const char* ToString(MemoryType type)
{
    switch (type)
    {
    case MemoryType::DeviceLocal:
        return "DeviceLocal";
    case MemoryType::HostVisible:
        return "HostVisible";
    case MemoryType::HostCached:
        return "HostCached";
    }
    return "Unknown";
}

} // anonymous namespace

/**
 * Vulkan memory block (256MB)
 */
struct GPUAllocator<VulkanAPI>::MemoryBlock
{
    /**
     * Free region within a memory block
     */
    struct FreeRegion
    {
        VkDeviceSize offset;
        VkDeviceSize size;

        bool operator<(const FreeRegion& other) const { return offset < other.offset; }
    };

    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkDeviceSize size{0};
    VkDeviceSize used{0};
    void* mappedPtr{nullptr};
    uint32_t memoryTypeIndex{0};

    std::vector<FreeRegion> freeRegions;

    MemoryBlock(VkDeviceSize blockSize, uint32_t typeIndex)
        : size{blockSize}, memoryTypeIndex{typeIndex}
    {
        freeRegions.push_back({0, blockSize});
    }

    FreeRegion* FindFreeRegion(VkDeviceSize allocSize, VkDeviceSize alignment)
    {
        for (auto& region : freeRegions)
        {
            VkDeviceSize alignedOffset = AlignUp(region.offset, alignment);
            VkDeviceSize padding = alignedOffset - region.offset;

            if (region.size >= allocSize + padding)
            {
                return &region;
            }
        }
        return nullptr;
    }

    VkDeviceSize Allocate(VkDeviceSize allocSize, VkDeviceSize alignment)
    {
        FreeRegion* region = FindFreeRegion(allocSize, alignment);
        if (!region)
            return VK_WHOLE_SIZE;

        VkDeviceSize alignedOffset = AlignUp(region->offset, alignment);
        VkDeviceSize padding = alignedOffset - region->offset;

        region->offset = alignedOffset + allocSize;
        region->size -= (allocSize + padding);

        if (region->size == 0)
        {
            std::erase_if(freeRegions,
                          [](const FreeRegion& r) { return r.size == 0; });
        }

        used += allocSize + padding;
        return alignedOffset;
    }

    void Deallocate(VkDeviceSize offset, VkDeviceSize allocSize)
    {
        freeRegions.push_back({offset, allocSize});
        std::sort(freeRegions.begin(), freeRegions.end());

        CoalesceFreeRegions();

        used -= allocSize;
    }

    void CoalesceFreeRegions()
    {
        if (freeRegions.size() <= 1)
            return;

        std::vector<FreeRegion> coalesced;
        coalesced.reserve(freeRegions.size());

        FreeRegion current = freeRegions[0];
        for (size_t i = 1; i < freeRegions.size(); ++i)
        {
            const FreeRegion& next = freeRegions[i];

            if (current.offset + current.size == next.offset)
            {
                current.size += next.size;
            }
            else
            {
                coalesced.push_back(current);
                current = next;
            }
        }
        coalesced.push_back(current);

        freeRegions = std::move(coalesced);
    }

    // Get fragmentation ratio (0.0 = no fragmentation, 1.0 = highly fragmented)
    float GetFragmentation() const
    {
        if (freeRegions.empty())
            return 0.0f;

        // Ideal: 1 large free region
        // Worst: Many small free regions
        return 1.0f - (1.0f / static_cast<float>(freeRegions.size()));
    }
};

/**
 * Memory pool (collection of blocks for one memory type)
 */
struct GPUAllocator<VulkanAPI>::MemoryPool
{
    MemoryType type;
    uint32_t memoryTypeIndex;
    VkDeviceSize blockSize;
    std::vector<std::unique_ptr<MemoryBlock>> blocks;
    mutable std::mutex mutex;

    MemoryPool(MemoryType memType, uint32_t typeIndex, VkDeviceSize size)
        : type{memType}, memoryTypeIndex{typeIndex}, blockSize{size}
    {
    }

    VkDeviceSize GetAllocatedSize() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        VkDeviceSize total = 0;
        for (const auto& block : blocks)
            total += block->used;
        return total;
    }

    VkDeviceSize GetTotalSize() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return blockSize * blocks.size();
    }

    size_t GetAllocationCount() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        size_t totalFree = 0;
        for (const auto& block : blocks)
            totalFree += block->freeRegions.size();

        return (blocks.size() > totalFree) ? (blocks.size() - totalFree) : 0;
    }

    float GetFragmentation() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (blocks.empty())
            return 0.0f;

        float totalFrag = 0.0f;
        for (const auto& block : blocks)
            totalFrag += block->GetFragmentation();

        return totalFrag / static_cast<float>(blocks.size());
    }
};

// ============================================================================
// GPUAllocator<VulkanAPI> Implementation
// ============================================================================

GPUAllocator<VulkanAPI>::GPUAllocator(VkDevice device, VkPhysicalDevice physicalDevice, const GPUAllocatorConfig& config)
    : device_{device}, physicalDevice_{physicalDevice}, config_{config}
{
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties_);

    hive::LogInfo(LogGPUAlloc, "GPU Allocator initialized");
    hive::LogInfo(LogGPUAlloc, "  Block size: {} MB", config_.blockSize / (1024 * 1024));
    hive::LogInfo(LogGPUAlloc, "  Max blocks per type: {}", config_.maxBlocks);
}

GPUAllocator<VulkanAPI>::~GPUAllocator()
{
    auto freePool = [this](std::unique_ptr<MemoryPool>& pool) {
        if (!pool)
            return;

        std::lock_guard<std::mutex> lock(pool->mutex);
        for (auto& block : pool->blocks)
        {
            if (block->mappedPtr)
                vkUnmapMemory(device_, block->memory);
            if (block->memory != VK_NULL_HANDLE)
                vkFreeMemory(device_, block->memory, nullptr);
        }
        pool->blocks.clear();
    };

    freePool(deviceLocalPool_);
    freePool(hostVisiblePool_);
    freePool(hostCachedPool_);

    hive::LogInfo(LogGPUAlloc, "GPU Allocator shutdown");
}

uint32_t GPUAllocator<VulkanAPI>::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
    for (uint32_t i = 0; i < memoryProperties_.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1 << i)) && (memoryProperties_.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    hive::LogError(LogGPUAlloc, "Failed to find suitable memory type (filter: {:#X}, properties: {:#X})",
                    typeFilter, properties);
    return UINT32_MAX;
}

GPUAllocator<VulkanAPI>::MemoryPool& GPUAllocator<VulkanAPI>::GetPool(MemoryType type)
{
    switch (type)
    {
    case MemoryType::DeviceLocal:
        if (!deviceLocalPool_)
        {
            uint32_t typeIndex = FindMemoryType(UINT32_MAX, GetMemoryProperties(type));
            deviceLocalPool_ = std::make_unique<MemoryPool>(type, typeIndex, config_.blockSize);
        }
        return *deviceLocalPool_;

    case MemoryType::HostVisible:
        if (!hostVisiblePool_)
        {
            uint32_t typeIndex = FindMemoryType(UINT32_MAX, GetMemoryProperties(type));
            hostVisiblePool_ = std::make_unique<MemoryPool>(type, typeIndex, config_.blockSize);
        }
        return *hostVisiblePool_;

    case MemoryType::HostCached:
        if (!hostCachedPool_)
        {
            uint32_t typeIndex = FindMemoryType(UINT32_MAX, GetMemoryProperties(type));
            hostCachedPool_ = std::make_unique<MemoryPool>(type, typeIndex, config_.blockSize);
        }
        return *hostCachedPool_;
    }

    hive::Assert(false, "Invalid memory type");
    return *deviceLocalPool_;  // Unreachable
}

const GPUAllocator<VulkanAPI>::MemoryPool& GPUAllocator<VulkanAPI>::GetPool(MemoryType type) const
{
    return const_cast<GPUAllocator*>(this)->GetPool(type);
}

GPUAllocator<VulkanAPI>::MemoryBlock* GPUAllocator<VulkanAPI>::AllocateBlock(MemoryPool& pool, VkDeviceSize size) const {
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = size;
    allocInfo.memoryTypeIndex = pool.memoryTypeIndex;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkResult result = vkAllocateMemory(device_, &allocInfo, nullptr, &memory);
    if (result != VK_SUCCESS)
    {
        hive::LogError(LogGPUAlloc, "Failed to allocate Vulkan memory (size: {} MB, type: {})",
                        size / (1024 * 1024), ToString(pool.type));
        return nullptr;
    }

    auto block = std::make_unique<MemoryBlock>(size, pool.memoryTypeIndex);
    block->memory = memory;

    if (pool.type == MemoryType::HostVisible || pool.type == MemoryType::HostCached)
    {
        result = vkMapMemory(device_, memory, 0, size, 0, &block->mappedPtr);
        if (result != VK_SUCCESS)
        {
            hive::LogError(LogGPUAlloc, "Failed to map Vulkan memory");
            vkFreeMemory(device_, memory, nullptr);
            return nullptr;
        }
    }

    hive::LogInfo(LogGPUAlloc, "Allocated new block: {} MB ({})",
                   size / (1024 * 1024), ToString(pool.type));

    MemoryBlock* blockPtr = block.get();
    pool.blocks.push_back(std::move(block));
    return blockPtr;
}

void GPUAllocator<VulkanAPI>::FreeBlock(MemoryPool& pool, MemoryBlock* block) const {
    if (block->mappedPtr)
        vkUnmapMemory(device_, block->memory);

    if (block->memory != VK_NULL_HANDLE)
        vkFreeMemory(device_, block->memory, nullptr);

    std::erase_if(pool.blocks,
                  [block](const auto& b) { return b.get() == block; });
}

VulkanAllocation GPUAllocator<VulkanAPI>::Allocate(VkDeviceSize size, VkDeviceSize alignment, MemoryType type)
{
    hive::Assert(size > 0, "Cannot allocate 0 bytes");

    MemoryPool& pool = GetPool(type);
    std::lock_guard<std::mutex> lock(pool.mutex);

    for (size_t i = 0; i < pool.blocks.size(); ++i)
    {
        MemoryBlock* block = pool.blocks[i].get();
        VkDeviceSize offset = block->Allocate(size, alignment);
        if (offset != VK_WHOLE_SIZE)
        {
            VulkanAllocation alloc{};
            alloc.memory = block->memory;
            alloc.offset = offset;
            alloc.size = size;
            alloc.mappedPtr = block->mappedPtr ? static_cast<std::byte*>(block->mappedPtr) + offset : nullptr;
            alloc.blockIndex = static_cast<uint32_t>(i);
            alloc.memoryTypeIndex = block->memoryTypeIndex;

            return alloc;
        }
    }

    if (pool.blocks.size() >= config_.maxBlocks)
    {
        hive::LogError(LogGPUAlloc, "Max blocks reached for {} ({} blocks)",
                        ToString(type), config_.maxBlocks);
        return {};
    }

    VkDeviceSize blockSize = std::max(pool.blockSize, size + alignment);
    MemoryBlock* newBlock = AllocateBlock(pool, blockSize);
    if (!newBlock)
        return {};

    VkDeviceSize offset = newBlock->Allocate(size, alignment);
    hive::Assert(offset != VK_WHOLE_SIZE, "Allocation from new block should always succeed");

    VulkanAllocation alloc{};
    alloc.memory = newBlock->memory;
    alloc.offset = offset;
    alloc.size = size;
    alloc.mappedPtr = newBlock->mappedPtr ? static_cast<std::byte*>(newBlock->mappedPtr) + offset : nullptr;
    alloc.blockIndex = static_cast<uint32_t>(pool.blocks.size() - 1);
    alloc.memoryTypeIndex = newBlock->memoryTypeIndex;

    return alloc;
}

VulkanAllocation GPUAllocator<VulkanAPI>::AllocateForBuffer(VkBuffer buffer, MemoryType type)
{
    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(device_, buffer, &memReq);

    uint32_t memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, GetMemoryProperties(type));
    if (memoryTypeIndex == UINT32_MAX)
        return {};

    return Allocate(memReq.size, memReq.alignment, type);
}

VulkanAllocation GPUAllocator<VulkanAPI>::AllocateForImage(VkImage image, MemoryType type)
{
    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(device_, image, &memReq);

    uint32_t memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, GetMemoryProperties(type));
    if (memoryTypeIndex == UINT32_MAX)
        return {};

    return Allocate(memReq.size, memReq.alignment, type);
}

void GPUAllocator<VulkanAPI>::Deallocate(VulkanAllocation& allocation)
{
    if (!allocation.IsValid())
        return;

    MemoryType type = MemoryType::DeviceLocal;
    if (hostVisiblePool_ && allocation.memoryTypeIndex == hostVisiblePool_->memoryTypeIndex)
        type = MemoryType::HostVisible;
    else if (hostCachedPool_ && allocation.memoryTypeIndex == hostCachedPool_->memoryTypeIndex)
        type = MemoryType::HostCached;

    MemoryPool& pool = GetPool(type);
    std::lock_guard<std::mutex> lock(pool.mutex);

    if (allocation.blockIndex >= pool.blocks.size())
    {
        hive::LogError(LogGPUAlloc, "Invalid block index in deallocation");
        return;
    }

    MemoryBlock* block = pool.blocks[allocation.blockIndex].get();
    block->Deallocate(allocation.offset, allocation.size);

    allocation = {};
}

void* GPUAllocator<VulkanAPI>::Map(const VulkanAllocation& allocation)
{
    if (!allocation.IsValid())
        return nullptr;

    if (allocation.mappedPtr)
        return allocation.mappedPtr;

    hive::LogWarning(LogGPUAlloc, "Mapping non-host-visible memory not supported");
    return nullptr;
}

void GPUAllocator<VulkanAPI>::Unmap(const VulkanAllocation& allocation)
{
    (void)allocation;
}

void GPUAllocator<VulkanAPI>::Flush(const VulkanAllocation& allocation) const {
    if (!allocation.IsValid() || !allocation.mappedPtr)
        return;

    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = allocation.memory;
    range.offset = allocation.offset;
    range.size = allocation.size;

    vkFlushMappedMemoryRanges(device_, 1, &range);
}

void GPUAllocator<VulkanAPI>::Invalidate(const VulkanAllocation& allocation) const {
    if (!allocation.IsValid() || !allocation.mappedPtr)
        return;

    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = allocation.memory;
    range.offset = allocation.offset;
    range.size = allocation.size;

    vkInvalidateMappedMemoryRanges(device_, 1, &range);
}

VkDeviceSize GPUAllocator<VulkanAPI>::GetAllocatedSize(MemoryType type) const
{
    return GetPool(type).GetAllocatedSize();
}

VkDeviceSize GPUAllocator<VulkanAPI>::GetTotalSize(MemoryType type) const
{
    return GetPool(type).GetTotalSize();
}

size_t GPUAllocator<VulkanAPI>::GetAllocationCount(MemoryType type) const
{
    return GetPool(type).GetAllocationCount();
}

GPUMemoryStats GPUAllocator<VulkanAPI>::GetStats(MemoryType type) const
{
    const MemoryPool& pool = GetPool(type);

    GPUMemoryStats stats{};
    stats.allocatedBytes = pool.GetAllocatedSize();
    stats.totalBytes = pool.GetTotalSize();
    stats.allocationCount = pool.GetAllocationCount();
    stats.blockCount = pool.blocks.size();
    stats.fragmentationRatio = pool.GetFragmentation();

    return stats;
}

void GPUAllocator<VulkanAPI>::PrintStats() const
{
    hive::LogInfo(LogGPUAlloc, "=== GPU Memory Statistics ===");

    auto printPool = [](const char* name, const GPUMemoryStats& stats) {
        float usedPercent = stats.totalBytes > 0 ? (static_cast<float>(stats.allocatedBytes) * 100.0f / static_cast<float>(stats.totalBytes)) : 0.0f;

        hive::LogInfo(LogGPUAlloc, "{}:", name);
        hive::LogInfo(LogGPUAlloc, "  Allocated: {} MB / {} MB ({:.1f}%)",
                       stats.allocatedBytes / (1024 * 1024),
                       stats.totalBytes / (1024 * 1024),
                       usedPercent);
        hive::LogInfo(LogGPUAlloc, "  Allocations: {}", stats.allocationCount);
        hive::LogInfo(LogGPUAlloc, "  Blocks: {}", stats.blockCount);
        hive::LogInfo(LogGPUAlloc, "  Fragmentation: {:.1f}%", stats.fragmentationRatio * 100.0f);
    };

    if (deviceLocalPool_)
        printPool("DeviceLocal (VRAM)", GetStats(MemoryType::DeviceLocal));

    if (hostVisiblePool_)
        printPool("HostVisible (Staging)", GetStats(MemoryType::HostVisible));

    if (hostCachedPool_)
        printPool("HostCached (Readback)", GetStats(MemoryType::HostCached));
}

} // namespace comb

#endif // COMB_HAS_VULKAN

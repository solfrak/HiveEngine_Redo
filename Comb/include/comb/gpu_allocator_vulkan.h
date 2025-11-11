/**
 * GPU Memory Allocator - Vulkan Specialization
 *
 * Manages VRAM allocation with sub-allocation strategy to avoid hitting
 * Vulkan's VkDeviceMemory allocation limits (~4096 allocations max).
 *
 * Architecture:
 * - Allocates large blocks (256MB) from Vulkan
 * - Sub-allocates smaller chunks from these blocks
 * - Supports multiple memory types (DeviceLocal, HostVisible, HostCached)
 * - Automatic coalescing of free regions
 *
 * Performance characteristics:
 * - Block allocation: O(1) - Find free block with enough space
 * - Sub-allocation: O(1) - Bump allocator within block
 * - Deallocation: O(1) - Add to free list
 * - Coalescing: O(N) where N = number of free regions in block
 * - Thread-safe: Yes (mutex per memory pool)
 *
 * Limitations:
 * - Fixed block size (256MB) - configurable per pool
 * - No automatic defragmentation (manual compaction required)
 * - Memory type must be known at allocation time
 *
 * Use cases:
 * - Textures (DeviceLocal)
 * - Vertex/Index buffers (DeviceLocal)
 * - Staging buffers (HostVisible)
 * - Uniform buffers (HostVisible)
 * - Readback buffers (HostCached)
 *
 * Example:
 * @code
 *   comb::GPUAllocator<comb::VulkanAPI> gpu{device, physicalDevice};
 *
 *   // Allocate texture memory
 *   auto alloc = gpu.Allocate(16_MB, 4096, comb::MemoryType::DeviceLocal);
 *   vkBindImageMemory(image, alloc.memory, alloc.offset);
 *
 *   // Allocate staging buffer
 *   auto staging = gpu.Allocate(1_MB, 256, comb::MemoryType::HostVisible);
 *   void* mapped = gpu.Map(staging);
 *   memcpy(mapped, data, size);
 *   gpu.Unmap(staging);
 *
 *   // Cleanup
 *   gpu.Deallocate(alloc);
 *   gpu.Deallocate(staging);
 * @endcode
 *
 * Note: This specialization is only available when Vulkan support is enabled (COMB_HAS_VULKAN).
 */

#pragma once

// GPU Allocator only available when Vulkan is enabled
#ifdef COMB_HAS_VULKAN

#include <comb/gpu_allocator_common.h>
#include <vulkan/vulkan.h>

namespace comb
{

/**
 * GPU memory allocation handle (Vulkan)
 */
struct VulkanAllocation
{
    VkDeviceMemory memory{VK_NULL_HANDLE};  // Vulkan memory object
    VkDeviceSize offset{0};                 // Offset within memory block
    VkDeviceSize size{0};                   // Allocated size
    void* mappedPtr{nullptr};               // Mapped pointer (if host-visible)
    uint32_t blockIndex{0};                 // Internal: block index
    uint32_t memoryTypeIndex{0};            // Vulkan memory type index

    [[nodiscard]] bool IsValid() const { return memory != VK_NULL_HANDLE; }
};

/**
 * GPU Memory Allocator - Vulkan Specialization
 *
 * Template specialization for Vulkan API.
 * Zero-overhead: All calls are direct, no virtual dispatch.
 *
 * Thread-safe allocator for Vulkan device memory with sub-allocation.
 * Manages large blocks and sub-allocates from them to reduce VkDeviceMemory count.
 */
template <>
class GPUAllocator<VulkanAPI>
{
public:
    // Type aliases for this specialization
    using Allocation = VulkanAllocation;

    /**
     * Construct GPU allocator
     *
     * @param device Vulkan logical device
     * @param physicalDevice Vulkan physical device
     * @param config Allocator configuration
     */
    GPUAllocator(VkDevice device, VkPhysicalDevice physicalDevice, const GPUAllocatorConfig& config = {});

    /**
     * Destructor - frees all Vulkan memory blocks
     */
    ~GPUAllocator();

    // Non-copyable
    GPUAllocator(const GPUAllocator&) = delete;
    GPUAllocator& operator=(const GPUAllocator&) = delete;

    /**
     * Allocate GPU memory
     *
     * @param size Size in bytes
     * @param alignment Required alignment (usually from VkMemoryRequirements)
     * @param type Memory type (DeviceLocal, HostVisible, HostCached)
     * @return Allocation handle (check IsValid())
     */
    [[nodiscard]] Allocation Allocate(VkDeviceSize size, VkDeviceSize alignment, MemoryType type);

    /**
     * Allocate memory for Vulkan buffer
     *
     * @param buffer Vulkan buffer
     * @param type Memory type
     * @return Allocation handle
     */
    [[nodiscard]] Allocation AllocateForBuffer(VkBuffer buffer, MemoryType type);

    /**
     * Allocate memory for Vulkan image
     *
     * @param image Vulkan image
     * @param type Memory type
     * @return Allocation handle
     */
    [[nodiscard]] Allocation AllocateForImage(VkImage image, MemoryType type);

    /**
     * Deallocate GPU memory
     *
     * @param allocation Allocation to free
     */
    void Deallocate(Allocation& allocation);

    /**
     * Map host-visible memory to CPU address space
     *
     * @param allocation Allocation to map (must be HostVisible or HostCached)
     * @return Mapped pointer, or nullptr on failure
     */
    [[nodiscard]] void* Map(const Allocation& allocation);

    /**
     * Unmap previously mapped memory
     *
     * @param allocation Allocation to unmap
     */
    void Unmap(const Allocation& allocation);

    /**
     * Flush mapped memory (make CPU writes visible to GPU)
     *
     * @param allocation Allocation to flush
     */
    void Flush(const Allocation& allocation) const;

    /**
     * Invalidate mapped memory (make GPU writes visible to CPU)
     *
     * @param allocation Allocation to invalidate
     */
    void Invalidate(const Allocation& allocation) const;

    /**
     * Get total allocated VRAM
     *
     * @param type Memory type
     * @return Total bytes allocated
     */
    [[nodiscard]] VkDeviceSize GetAllocatedSize(MemoryType type) const;

    /**
     * Get total VRAM capacity
     *
     * @param type Memory type
     * @return Total bytes capacity (across all blocks)
     */
    [[nodiscard]] VkDeviceSize GetTotalSize(MemoryType type) const;

    /**
     * Get number of active allocations
     *
     * @param type Memory type
     * @return Allocation count
     */
    [[nodiscard]] size_t GetAllocationCount(MemoryType type) const;

    /**
     * Get memory usage statistics
     */
    [[nodiscard]] GPUMemoryStats GetStats(MemoryType type) const;

    /**
     * Print detailed memory statistics to log
     */
    void PrintStats() const;

private:
    // Forward declarations
    struct MemoryBlock;
    struct MemoryPool;

    /**
     * Find Vulkan memory type index
     *
     * @param typeFilter Memory type bits from VkMemoryRequirements
     * @param properties Required memory properties
     * @return Memory type index, or UINT32_MAX if not found
     */
    [[nodiscard]] uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    /**
     * Get memory pool for given type
     */
    [[nodiscard]] MemoryPool& GetPool(MemoryType type);
    [[nodiscard]] const MemoryPool& GetPool(MemoryType type) const;

    /**
     * Allocate new Vulkan memory block
     */
    MemoryBlock* AllocateBlock(MemoryPool& pool, VkDeviceSize size) const;

    /**
     * Free Vulkan memory block
     */
    void FreeBlock(MemoryPool& pool, MemoryBlock* block) const;

    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    VkPhysicalDeviceMemoryProperties memoryProperties_;
    GPUAllocatorConfig config_;

    // Memory pools (one per type)
    std::unique_ptr<MemoryPool> deviceLocalPool_;
    std::unique_ptr<MemoryPool> hostVisiblePool_;
    std::unique_ptr<MemoryPool> hostCachedPool_;
};

} // namespace comb

#endif // COMB_HAS_VULKAN

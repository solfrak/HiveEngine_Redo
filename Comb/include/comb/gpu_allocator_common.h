/**
 * GPU Allocator Common Types
 *
 * Shared types and interfaces for multi-API GPU memory allocation.
 * This file contains API-agnostic definitions used by all GPU allocator backends.
 *
 * Supported backends:
 * - Vulkan (Windows, Linux, macOS)
 * - DirectX 12 (Windows only) [Future]
 * - Metal (macOS only) [Future]
 *
 * Design:
 * - Zero-overhead abstraction using template specialization
 * - No virtual functions, no runtime polymorphism
 * - Compile-time API selection via policy-based design
 * - Each backend is a complete template specialization
 *
 * Usage:
 * @code
 *   // Explicit API selection
 *   comb::GPUAllocator<comb::VulkanAPI> allocator{device, physicalDevice};
 *   auto alloc = allocator.Allocate(size, alignment, comb::MemoryType::DeviceLocal);
 *
 *   // Or use default API alias (configured per-platform)
 *   comb::DefaultGPUAllocator allocator{device, physicalDevice};
 * @endcode
 */

#pragma once

#include <comb/precomp.h>

namespace comb
{

// ============================================================================
// API Tags (Zero-size types for template specialization)
// ============================================================================

/**
 * Vulkan API tag
 * Available on: Windows, Linux, macOS
 */
struct VulkanAPI
{
    static constexpr const char* Name = "Vulkan";
};

/**
 * DirectX 12 API tag
 * Available on: Windows only
 * Status: Not yet implemented
 */
struct D3D12API
{
    static constexpr const char* Name = "DirectX 12";
};

/**
 * Metal API tag
 * Available on: macOS only
 * Status: Not yet implemented
 */
struct MetalAPI
{
    static constexpr const char* Name = "Metal";
};

// ============================================================================
// Common Types (API-agnostic)
// ============================================================================

/**
 * GPU memory type categories
 *
 * These map to different memory types across APIs:
 * - Vulkan: VkMemoryPropertyFlags
 * - D3D12: D3D12_HEAP_TYPE
 * - Metal: MTLStorageMode
 */
enum class MemoryType : uint8_t
{
    DeviceLocal,  // GPU-only memory (VRAM) - fastest for GPU access
                  // Vulkan: DEVICE_LOCAL_BIT
                  // D3D12: D3D12_HEAP_TYPE_DEFAULT
                  // Metal: MTLStorageModePrivate

    HostVisible,  // CPU-writable, GPU-readable - for staging/dynamic data
                  // Vulkan: HOST_VISIBLE_BIT | HOST_COHERENT_BIT
                  // D3D12: D3D12_HEAP_TYPE_UPLOAD
                  // Metal: MTLStorageModeShared

    HostCached    // GPU-writable, CPU-readable - for readback
                  // Vulkan: HOST_VISIBLE_BIT | HOST_CACHED_BIT
                  // D3D12: D3D12_HEAP_TYPE_READBACK
                  // Metal: MTLStorageModeShared (with cache hints)
};

/**
 * GPU Allocator Configuration
 *
 * Controls memory allocation behavior for all backends.
 */
struct GPUAllocatorConfig
{
    size_t blockSize{256 * 1024 * 1024};  // 256 MB default block size
    uint32_t maxBlocks{64};                // Max blocks per memory type
    bool enableTracking{true};             // Track allocations (debug mode)
};

/**
 * Memory usage statistics
 *
 * Returned by GetStats() for each memory type.
 */
struct GPUMemoryStats
{
    size_t allocatedBytes;        // Actually used memory
    size_t totalBytes;            // Total capacity (all blocks)
    size_t allocationCount;       // Number of active allocations
    size_t blockCount;            // Number of allocated blocks
    float fragmentationRatio;     // 0.0 = no fragmentation, 1.0 = highly fragmented
};

// ============================================================================
// GPU Allocator Template (Forward Declaration)
// ============================================================================

/**
 * GPU Memory Allocator (Template)
 *
 * Template class specialized for each graphics API.
 * Uses policy-based design for zero-overhead abstraction.
 *
 * @tparam API Graphics API tag (VulkanAPI, D3D12API, MetalAPI)
 *
 * Each specialization implements:
 * - Constructor(api-specific device parameters, config)
 * - Allocate(size, alignment, type) -> API-specific allocation handle
 * - Deallocate(allocation)
 * - Map(allocation) -> void*
 * - Unmap(allocation)
 * - Flush(allocation)
 * - Invalidate(allocation)
 * - GetStats(type) -> GPUMemoryStats
 * - PrintStats()
 *
 * Backend-specific types:
 * - GPUAllocator<VulkanAPI>::Allocation  (contains VkDeviceMemory)
 * - GPUAllocator<D3D12API>::Allocation   (contains ID3D12Heap*)
 * - GPUAllocator<MetalAPI>::Allocation   (contains id<MTLBuffer>)
 */
template <typename API>
class GPUAllocator;

// Specializations are defined in:
// - gpu_allocator_vulkan.h  (GPUAllocator<VulkanAPI>)
// - gpu_allocator_d3d12.h   (GPUAllocator<D3D12API>)  [Future]
// - gpu_allocator_metal.h   (GPUAllocator<MetalAPI>)  [Future]

} // namespace comb

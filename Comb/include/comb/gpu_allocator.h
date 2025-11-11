/**
 * GPU Memory Allocator - Multi-API Support
 *
 * Zero-overhead GPU memory allocation supporting multiple graphics APIs.
 * Uses template specialization for compile-time API selection with no runtime cost.
 *
 * Supported APIs:
 * - Vulkan (Windows, Linux, macOS) - ✅ Implemented
 * - DirectX 12 (Windows only)      - 🚧 Future
 * - Metal (macOS only)              - 🚧 Future
 *
 * Design Philosophy:
 * - NO virtual functions (zero overhead)
 * - NO runtime polymorphism
 * - Template specialization for API selection
 * - Compile-time dispatch (inlineable)
 *
 * Usage (Explicit API selection):
 * @code
 *   // Vulkan
 *   comb::GPUAllocator<comb::VulkanAPI> allocator{device, physicalDevice};
 *   auto alloc = allocator.Allocate(size, alignment, comb::MemoryType::DeviceLocal);
 *   vkBindImageMemory(image, alloc.memory, alloc.offset);
 *
 *   // DirectX 12 (future)
 *   comb::GPUAllocator<comb::D3D12API> allocator{device};
 *   auto alloc = allocator.Allocate(size, alignment, comb::MemoryType::DeviceLocal);
 *   device->CreatePlacedResource(alloc.heap, alloc.offset, ...);
 * @endcode
 *
 * Usage (Platform default):
 * @code
 *   // Uses platform-appropriate default API
 *   comb::DefaultGPUAllocator allocator{...};
 * @endcode
 *
 * Migration from old API:
 * The old concrete GPUAllocator class has been replaced with a template.
 * If you were using:
 *   comb::GPUAllocator allocator{device, physicalDevice};
 *
 * Change to:
 *   comb::GPUAllocator<comb::VulkanAPI> allocator{device, physicalDevice};
 *
 * Or use the convenience alias:
 *   comb::DefaultGPUAllocator allocator{device, physicalDevice};
 */

#pragma once

#include <comb/gpu_allocator_common.h>

// ============================================================================
// Include API-Specific Specializations
// ============================================================================

#ifdef COMB_HAS_VULKAN
#include <comb/gpu_allocator_vulkan.h>
#endif

// Future: D3D12 support
// #ifdef COMB_HAS_D3D12
// #include <comb/gpu_allocator_d3d12.h>
// #endif

// Future: Metal support
// #ifdef COMB_HAS_METAL
// #include <comb/gpu_allocator_metal.h>
// #endif

// ============================================================================
// Platform Default Aliases
// ============================================================================

namespace comb
{

/**
 * Default GPU API for this platform
 *
 * Selection priority:
 * - Windows: Vulkan (if available), else D3D12
 * - macOS: Vulkan (if available), else Metal
 * - Linux: Vulkan
 */
#if defined(COMB_HAS_VULKAN)
    using DefaultGPUAPI = VulkanAPI;
    using DefaultGPUAllocator = GPUAllocator<VulkanAPI>;
    using DefaultGPUAllocation = VulkanAllocation;
#elif defined(COMB_HAS_D3D12)
    using DefaultGPUAPI = D3D12API;
    using DefaultGPUAllocator = GPUAllocator<D3D12API>;
    // using DefaultGPUAllocation = D3D12Allocation;  // Future
#elif defined(COMB_HAS_METAL)
    using DefaultGPUAPI = MetalAPI;
    using DefaultGPUAllocator = GPUAllocator<MetalAPI>;
    // using DefaultGPUAllocation = MetalAllocation;  // Future
#else
    #error "No GPU API available! Enable at least one of: COMB_HAS_VULKAN, COMB_HAS_D3D12, COMB_HAS_METAL"
#endif

} // namespace comb

/**
 * GPU Allocator Tests
 *
 * Note: These tests require a valid Vulkan instance and device.
 * They are integration tests, not pure unit tests.
 * Only compiled when COMB_HAS_VULKAN is defined.
 */

#ifdef COMB_HAS_VULKAN

#include <comb/gpu_allocator.h>
#include <larvae/larvae.h>

#include <vulkan/vulkan.h>
#include <cstring>

// Test fixture with Vulkan setup
class GPUAllocatorFixture : public larvae::TestFixture
{
public:
    void SetUp() override
    {
        // Create Vulkan instance
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "GPUAllocator Test";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "HiveEngine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
        larvae::AssertTrue(result == VK_SUCCESS);

        // Pick first physical device
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
        larvae::AssertTrue(deviceCount > 0);

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());
        physicalDevice_ = devices[0];

        // Create logical device
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = 0;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

        result = vkCreateDevice(physicalDevice_, &deviceCreateInfo, nullptr, &device_);
        larvae::AssertTrue(result == VK_SUCCESS);
    }

    void TearDown() override
    {
        if (device_ != VK_NULL_HANDLE)
            vkDestroyDevice(device_, nullptr);
        if (instance_ != VK_NULL_HANDLE)
            vkDestroyInstance(instance_, nullptr);
    }

    VkInstance instance_{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice_{VK_NULL_HANDLE};
    VkDevice device_{VK_NULL_HANDLE};
};

namespace {

// ============================================================================
// Basic Functionality Tests
// ============================================================================

auto test1 = larvae::RegisterTestWithFixture<GPUAllocatorFixture>("GPUAllocator", "Construction",
    [](GPUAllocatorFixture& f) {
        comb::GPUAllocatorConfig config{};
        config.blockSize = 256 * 1024 * 1024;  // 256 MB
        config.maxBlocks = 64;

        comb::DefaultGPUAllocator allocator{f.device_, f.physicalDevice_, config};

        // Should start with 0 allocations
        larvae::AssertEqual(allocator.GetAllocationCount(comb::MemoryType::DeviceLocal), 0u);
        larvae::AssertEqual(allocator.GetAllocationCount(comb::MemoryType::HostVisible), 0u);
        larvae::AssertEqual(allocator.GetAllocationCount(comb::MemoryType::HostCached), 0u);
    });

auto test2 = larvae::RegisterTestWithFixture<GPUAllocatorFixture>("GPUAllocator", "DeviceLocalAllocation",
    [](GPUAllocatorFixture& f) {
        comb::DefaultGPUAllocator allocator{f.device_, f.physicalDevice_};

        // Allocate 16 MB device-local memory
        constexpr VkDeviceSize size = 16 * 1024 * 1024;
        auto alloc = allocator.Allocate(size, 256, comb::MemoryType::DeviceLocal);

        larvae::AssertTrue(alloc.IsValid());
        larvae::AssertNotNull(alloc.memory);
        larvae::AssertEqual(alloc.size, size);
        larvae::AssertGreaterEqual(alloc.offset, 0u);

        // Stats should reflect allocation
        auto stats = allocator.GetStats(comb::MemoryType::DeviceLocal);
        larvae::AssertGreaterEqual(stats.allocatedBytes, size);
        larvae::AssertGreaterThan(stats.blockCount, 0u);

        allocator.Deallocate(alloc);

        // After deallocation
        stats = allocator.GetStats(comb::MemoryType::DeviceLocal);
        larvae::AssertEqual(stats.allocatedBytes, 0u);
    });

auto test3 = larvae::RegisterTestWithFixture<GPUAllocatorFixture>("GPUAllocator", "HostVisibleAllocation",
    [](GPUAllocatorFixture& f) {
        comb::DefaultGPUAllocator allocator{f.device_, f.physicalDevice_};

        // Allocate 1 MB host-visible memory
        constexpr VkDeviceSize size = 1 * 1024 * 1024;
        auto alloc = allocator.Allocate(size, 256, comb::MemoryType::HostVisible);

        larvae::AssertTrue(alloc.IsValid());
        larvae::AssertNotNull(alloc.memory);
        larvae::AssertNotNull(alloc.mappedPtr);  // Should be mapped

        // Write to mapped memory
        void* ptr = allocator.Map(alloc);
        larvae::AssertNotNull(ptr);
        larvae::AssertEqual(ptr, alloc.mappedPtr);

        std::memset(ptr, 0x42, size);

        allocator.Flush(alloc);
        allocator.Deallocate(alloc);
    });

auto test4 = larvae::RegisterTestWithFixture<GPUAllocatorFixture>("GPUAllocator", "MultipleAllocations",
    [](GPUAllocatorFixture& f) {
        comb::DefaultGPUAllocator allocator{f.device_, f.physicalDevice_};

        constexpr size_t count = 100;
        std::vector<comb::DefaultGPUAllocation> allocations;
        allocations.reserve(count);

        // Allocate 100 x 1MB buffers
        for (size_t i = 0; i < count; ++i)
        {
            auto alloc = allocator.Allocate(1024 * 1024, 256, comb::MemoryType::DeviceLocal);
            larvae::AssertTrue(alloc.IsValid());
            allocations.push_back(alloc);
        }

        // All should be valid
        for (const auto& alloc : allocations)
        {
            larvae::AssertTrue(alloc.IsValid());
        }

        // Deallocate all
        for (auto& alloc : allocations)
        {
            allocator.Deallocate(alloc);
        }

        // Stats should be back to 0
        auto stats = allocator.GetStats(comb::MemoryType::DeviceLocal);
        larvae::AssertEqual(stats.allocatedBytes, 0u);
    });

// ============================================================================
// Sub-allocation Tests
// ============================================================================

auto test5 = larvae::RegisterTestWithFixture<GPUAllocatorFixture>("GPUAllocator", "SubAllocationFromSameBlock",
    [](GPUAllocatorFixture& f) {
        comb::GPUAllocatorConfig config{};
        config.blockSize = 256 * 1024 * 1024;  // 256 MB
        comb::DefaultGPUAllocator allocator{f.device_, f.physicalDevice_, config};

        // Allocate 10 x 10MB - should all fit in single block
        std::vector<comb::DefaultGPUAllocation> allocations;
        for (int i = 0; i < 10; ++i)
        {
            auto alloc = allocator.Allocate(10 * 1024 * 1024, 256, comb::MemoryType::DeviceLocal);
            larvae::AssertTrue(alloc.IsValid());
            allocations.push_back(alloc);
        }

        // Should only have 1 block allocated
        auto stats = allocator.GetStats(comb::MemoryType::DeviceLocal);
        larvae::AssertEqual(stats.blockCount, 1u);

        // All allocations should share same VkDeviceMemory
        VkDeviceMemory firstMemory = allocations[0].memory;
        for (const auto& alloc : allocations)
        {
            larvae::AssertEqual(alloc.memory, firstMemory);
        }

        // Cleanup
        for (auto& alloc : allocations)
        {
            allocator.Deallocate(alloc);
        }
    });

auto test6 = larvae::RegisterTestWithFixture<GPUAllocatorFixture>("GPUAllocator", "MultipleBlocks",
    [](GPUAllocatorFixture& f) {
        comb::GPUAllocatorConfig config{};
        config.blockSize = 64 * 1024 * 1024;  // 64 MB blocks
        comb::DefaultGPUAllocator allocator{f.device_, f.physicalDevice_, config};

        // Allocate 10 x 64MB - should create 10 blocks
        std::vector<comb::DefaultGPUAllocation> allocations;
        for (int i = 0; i < 10; ++i)
        {
            auto alloc = allocator.Allocate(64 * 1024 * 1024, 256, comb::MemoryType::DeviceLocal);
            larvae::AssertTrue(alloc.IsValid());
            allocations.push_back(alloc);
        }

        auto stats = allocator.GetStats(comb::MemoryType::DeviceLocal);
        larvae::AssertEqual(stats.blockCount, 10u);

        // Cleanup
        for (auto& alloc : allocations)
        {
            allocator.Deallocate(alloc);
        }
    });

// ============================================================================
// Alignment Tests
// ============================================================================

auto test7 = larvae::RegisterTestWithFixture<GPUAllocatorFixture>("GPUAllocator", "AlignmentRespected",
    [](GPUAllocatorFixture& f) {
        comb::DefaultGPUAllocator allocator{f.device_, f.physicalDevice_};

        // Test various alignments
        const VkDeviceSize alignments[] = {256, 512, 1024, 4096};

        for (VkDeviceSize alignment : alignments)
        {
            auto alloc = allocator.Allocate(1024, alignment, comb::MemoryType::DeviceLocal);
            larvae::AssertTrue(alloc.IsValid());
            larvae::AssertEqual(alloc.offset % alignment, 0u);  // Offset must be aligned
            allocator.Deallocate(alloc);
        }
    });

// ============================================================================
// Stats Tests
// ============================================================================

auto test8 = larvae::RegisterTestWithFixture<GPUAllocatorFixture>("GPUAllocator", "StatsAccurate",
    [](GPUAllocatorFixture& f) {
        comb::DefaultGPUAllocator allocator{f.device_, f.physicalDevice_};

        auto statsBefore = allocator.GetStats(comb::MemoryType::DeviceLocal);
        larvae::AssertEqual(statsBefore.allocatedBytes, 0u);
        larvae::AssertEqual(statsBefore.blockCount, 0u);

        // Allocate 10 MB
        constexpr VkDeviceSize size = 10 * 1024 * 1024;
        auto alloc = allocator.Allocate(size, 256, comb::MemoryType::DeviceLocal);

        auto statsAfter = allocator.GetStats(comb::MemoryType::DeviceLocal);
        larvae::AssertGreaterEqual(statsAfter.allocatedBytes, size);
        larvae::AssertGreaterThan(statsAfter.blockCount, 0u);
        larvae::AssertGreaterThan(statsAfter.totalBytes, 0u);

        allocator.Deallocate(alloc);
    });

auto test9 = larvae::RegisterTestWithFixture<GPUAllocatorFixture>("GPUAllocator", "PrintStats",
    [](GPUAllocatorFixture& f) {
        comb::DefaultGPUAllocator allocator{f.device_, f.physicalDevice_};

        // Allocate some memory
        auto alloc1 = allocator.Allocate(16 * 1024 * 1024, 256, comb::MemoryType::DeviceLocal);
        auto alloc2 = allocator.Allocate(1 * 1024 * 1024, 256, comb::MemoryType::HostVisible);

        // Should not crash
        allocator.PrintStats();

        allocator.Deallocate(alloc1);
        allocator.Deallocate(alloc2);
    });

} // anonymous namespace

#else // COMB_HAS_VULKAN

// Placeholder test when Vulkan is not enabled
#include <larvae/larvae.h>

namespace {

auto testSkipped = larvae::RegisterTest("GPUAllocator", "Skipped", []() {
    larvae::LogInfo("GPU Allocator tests skipped - Vulkan support not enabled (swarm_use_vulkan=OFF)");
    larvae::LogInfo("To enable: Set swarm_use_vulkan=ON in CMake and ensure Vulkan SDK is installed");
});

} // anonymous namespace

#endif // COMB_HAS_VULKAN

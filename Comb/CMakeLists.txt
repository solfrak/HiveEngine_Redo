cmake_minimum_required(VERSION 3.30.5)

add_library(comb STATIC)
target_include_directories(comb PUBLIC include PRIVATE src)
target_precompile_headers(comb PRIVATE include/comb/precomp.h)

# Link against Hive (core module)
target_link_libraries(comb PUBLIC hive)

# Base allocator sources (always present)
target_sources(comb PRIVATE
    src/comb/combmodule.cpp
    src/comb/platform.cpp
    src/comb/linear_allocator.cpp
)

# GPU allocator (optional, based on swarm_use_vulkan)
if(swarm_use_vulkan)
    find_package(Vulkan REQUIRED)
    target_link_libraries(comb PUBLIC Vulkan::Vulkan)
    target_compile_definitions(comb PUBLIC COMB_HAS_VULKAN=1)

    target_sources(comb PRIVATE
        src/comb/gpu_allocator.cpp
    )

    message(STATUS "Comb: GPU Allocator enabled (Vulkan)")
else()
    message(STATUS "Comb: GPU Allocator disabled (no graphics API)")
endif()

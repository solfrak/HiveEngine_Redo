
# ============================================================================
# fmt library (logging and formatting)
# ============================================================================

# Configure fmt WITHOUT exceptions (game engine mode)
set(FMT_EXCEPTIONS OFF CACHE BOOL "Disable exceptions in fmt" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "Don't install fmt" FORCE)

# Add fmt as subdirectory
add_subdirectory(extern/fmt EXCLUDE_FROM_ALL)

# ============================================================================
# Hive library
# ============================================================================

add_library(hive STATIC)
target_include_directories(hive PUBLIC include PRIVATE src)
target_precompile_headers(hive PRIVATE include/hive/precomp.h)

# Link fmt library
target_link_libraries(hive PUBLIC fmt::fmt)

target_sources(hive PRIVATE
    src/hive/core/assert.cpp
    src/hive/core/log.cpp
    src/hive/core/module.cpp
    src/hive/core/moduleregistry.cpp
)


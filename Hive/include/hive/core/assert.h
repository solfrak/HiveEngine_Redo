#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #define HIVE_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define HIVE_PLATFORM_LINUX 1
#elif defined(__APPLE__) && defined(__MACH__)
    #define HIVE_PLATFORM_MACOS 1
#else
    #define HIVE_PLATFORM_UNKNOWN 1
#endif

#if defined(_MSC_VER)
    #define HIVE_COMPILER_MSVC 1
#elif defined(__clang__)
    #define HIVE_COMPILER_CLANG 1
#elif defined(__GNUC__)
    #define HIVE_COMPILER_GCC 1
#endif

#if defined(NDEBUG) || defined(_NDEBUG)
    #define HIVE_BUILD_RELEASE 1
    #define HIVE_BUILD_DEBUG 0
#else
    #define HIVE_BUILD_RELEASE 0
    #define HIVE_BUILD_DEBUG 1
#endif

#if HIVE_COMPILER_MSVC
    #define HIVE_DEBUG_BREAK() __debugbreak()
#elif HIVE_COMPILER_CLANG || HIVE_COMPILER_GCC
    #if HIVE_PLATFORM_LINUX || HIVE_PLATFORM_MACOS
        #include <signal.h>
        #define HIVE_DEBUG_BREAK() raise(SIGTRAP)
    #else
        #define HIVE_DEBUG_BREAK() __builtin_trap()
    #endif
#else
    #define HIVE_DEBUG_BREAK() ((void)0)
#endif

#if HIVE_COMPILER_MSVC
    #define HIVE_FORCE_INLINE __forceinline
#elif HIVE_COMPILER_CLANG || HIVE_COMPILER_GCC
    #define HIVE_FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define HIVE_FORCE_INLINE inline
#endif

#if HIVE_COMPILER_CLANG || HIVE_COMPILER_GCC
    #define HIVE_LIKELY(x) __builtin_expect(!!(x), 1)
    #define HIVE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define HIVE_LIKELY(x) (x)
    #define HIVE_UNLIKELY(x) (x)
#endif

namespace hive
{
    bool HandleAssertionFailure(
        const char* file,
        int line,
        const char* function,
        const char* expression,
        const char* message = nullptr);
}

// HIVE_ASSERT: Debug only, zero cost in release, expr not evaluated in release
#if HIVE_BUILD_DEBUG
    #define HIVE_ASSERT(expr) \
        do { \
            if (HIVE_UNLIKELY(!(expr))) { \
                ::hive::HandleAssertionFailure(__FILE__, __LINE__, __func__, #expr); \
            } \
        } while(0)

    #define HIVE_ASSERT_MSG(expr, msg) \
        do { \
            if (HIVE_UNLIKELY(!(expr))) { \
                ::hive::HandleAssertionFailure(__FILE__, __LINE__, __func__, #expr, msg); \
            } \
        } while(0)
#else
    #define HIVE_ASSERT(expr) ((void)0)
    #define HIVE_ASSERT_MSG(expr, msg) ((void)0)
#endif

// HIVE_VERIFY: Always evaluates expr (even in release), reports failure in debug only
#if HIVE_BUILD_DEBUG
    #define HIVE_VERIFY(expr) \
        do { \
            if (HIVE_UNLIKELY(!(expr))) { \
                ::hive::HandleAssertionFailure(__FILE__, __LINE__, __func__, #expr); \
            } \
        } while(0)

    #define HIVE_VERIFY_MSG(expr, msg) \
        do { \
            if (HIVE_UNLIKELY(!(expr))) { \
                ::hive::HandleAssertionFailure(__FILE__, __LINE__, __func__, #expr, msg); \
            } \
        } while(0)
#else
    #define HIVE_VERIFY(expr) ((void)(expr))
    #define HIVE_VERIFY_MSG(expr, msg) ((void)(expr))
#endif

// HIVE_CHECK: Always evaluates and reports, even in release (use sparingly)
#define HIVE_CHECK(expr) \
    do { \
        if (HIVE_UNLIKELY(!(expr))) { \
            ::hive::HandleAssertionFailure(__FILE__, __LINE__, __func__, #expr); \
        } \
    } while(0)

#define HIVE_CHECK_MSG(expr, msg) \
    do { \
        if (HIVE_UNLIKELY(!(expr))) { \
            ::hive::HandleAssertionFailure(__FILE__, __LINE__, __func__, #expr, msg); \
        } \
    } while(0)

#define HIVE_STATIC_ASSERT(expr, msg) static_assert(expr, msg)
#if HIVE_BUILD_DEBUG
    #define HIVE_UNREACHABLE() \
        do { \
            ::hive::HandleAssertionFailure(__FILE__, __LINE__, __func__, "UNREACHABLE CODE", "This code path should never be executed"); \
            HIVE_DEBUG_BREAK(); \
        } while(0)
#else
    #if HIVE_COMPILER_MSVC
        #define HIVE_UNREACHABLE() __assume(0)
    #elif HIVE_COMPILER_CLANG || HIVE_COMPILER_GCC
        #define HIVE_UNREACHABLE() __builtin_unreachable()
    #else
        #define HIVE_UNREACHABLE() ((void)0)
    #endif
#endif

#define HIVE_NOT_IMPLEMENTED() \
    do { \
        ::hive::HandleAssertionFailure(__FILE__, __LINE__, __func__, "NOT_IMPLEMENTED", "This functionality has not been implemented yet"); \
        HIVE_DEBUG_BREAK(); \
    } while(0)



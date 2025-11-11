#include <larvae/larvae.h>
#include <comb/stack_allocator.h>
#include <cstdlib>
#include <vector>
#include <functional>

namespace
{
    constexpr size_t operator""_KB(unsigned long long kb) { return kb * 1024; }
    constexpr size_t operator""_MB(unsigned long long mb) { return mb * 1024 * 1024; }

    auto bench1 = larvae::RegisterBenchmark("StackAllocator", "SmallAllocations_64B", [](larvae::BenchmarkState& state) {
        comb::StackAllocator allocator{100_MB};

        while (state.KeepRunning())
        {
            void* ptr = allocator.Allocate(64, 8);
            larvae::DoNotOptimize(ptr);

            if (allocator.GetUsedMemory() > 90_MB)
            {
                allocator.Reset();
            }
        }

        state.SetBytesProcessed(state.iterations() * 64);
        state.SetItemsProcessed(state.iterations());
    });

    auto bench2 = larvae::RegisterBenchmark("StackAllocator", "MediumAllocations_1KB", [](larvae::BenchmarkState& state) {
        comb::StackAllocator allocator{1024_MB};

        while (state.KeepRunning())
        {
            void* ptr = allocator.Allocate(1_KB, 8);
            larvae::DoNotOptimize(ptr);

            if (allocator.GetUsedMemory() > 1000_MB)
            {
                allocator.Reset();
            }
        }

        state.SetBytesProcessed(state.iterations() * 1_KB);
        state.SetItemsProcessed(state.iterations());
    });

    auto bench3 = larvae::RegisterBenchmark("StackAllocator", "WithReset", [](larvae::BenchmarkState& state) {
        comb::StackAllocator allocator{10_MB};

        while (state.KeepRunning())
        {
            for (int j = 0; j < 100; ++j)
            {
                void* ptr = allocator.Allocate(256, 8);
                larvae::DoNotOptimize(ptr);
            }

            allocator.Reset();
        }

        state.SetBytesProcessed(state.iterations() * 100 * 256);
        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench4 = larvae::RegisterBenchmark("StackAllocator", "AlignedAllocations_64B", [](larvae::BenchmarkState& state) {
        comb::StackAllocator allocator{100_MB};

        while (state.KeepRunning())
        {
            void* ptr = allocator.Allocate(64, 64);
            larvae::DoNotOptimize(ptr);

            if (allocator.GetUsedMemory() > 90_MB)
            {
                allocator.Reset();
            }
        }

        state.SetBytesProcessed(state.iterations() * 64);
        state.SetItemsProcessed(state.iterations());
    });

    auto bench5 = larvae::RegisterBenchmark("StackAllocator", "Markers", [](larvae::BenchmarkState& state) {
        comb::StackAllocator allocator{10_MB};

        while (state.KeepRunning())
        {
            auto marker1 = allocator.GetMarker();
            larvae::DoNotOptimize(marker1);

            void* ptr1 = allocator.Allocate(128, 8);
            larvae::DoNotOptimize(ptr1);

            auto marker2 = allocator.GetMarker();
            larvae::DoNotOptimize(marker2);

            void* ptr2 = allocator.Allocate(256, 8);
            larvae::DoNotOptimize(ptr2);

            allocator.FreeToMarker(marker2);
            allocator.FreeToMarker(marker1);

            if (allocator.GetUsedMemory() > 9_MB)
            {
                allocator.Reset();
            }
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench6 = larvae::RegisterBenchmark("StackAllocator", "NestedScopes", [](larvae::BenchmarkState& state) {
        comb::StackAllocator allocator{10_MB};

        while (state.KeepRunning())
        {
            auto outer_marker = allocator.GetMarker();

            void* outer_data = allocator.Allocate(512, 8);
            larvae::DoNotOptimize(outer_data);

            for (int j = 0; j < 10; ++j)
            {
                auto inner_marker = allocator.GetMarker();

                void* inner_data1 = allocator.Allocate(128, 8);
                void* inner_data2 = allocator.Allocate(64, 8);
                larvae::DoNotOptimize(inner_data1);
                larvae::DoNotOptimize(inner_data2);

                allocator.FreeToMarker(inner_marker);
            }

            allocator.FreeToMarker(outer_marker);

            if (allocator.GetUsedMemory() > 9_MB)
            {
                allocator.Reset();
            }
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench7 = larvae::RegisterBenchmark("StackAllocator", "FrameAllocationPattern", [](larvae::BenchmarkState& state) {
        comb::StackAllocator allocator{50_MB};

        while (state.KeepRunning())
        {
            auto frame_marker = allocator.GetMarker();

            void* render_commands = allocator.Allocate(10 * 1024, 16);
            void* ui_vertices = allocator.Allocate(20 * 1024, 16);
            void* temp_buffer = allocator.Allocate(5 * 1024, 8);
            void* particle_data = allocator.Allocate(15 * 1024, 16);

            larvae::DoNotOptimize(render_commands);
            larvae::DoNotOptimize(ui_vertices);
            larvae::DoNotOptimize(temp_buffer);
            larvae::DoNotOptimize(particle_data);

            allocator.FreeToMarker(frame_marker);
        }

        state.SetBytesProcessed(state.iterations() * 50 * 1024);
        state.SetItemsProcessed(state.iterations());
    });

    auto bench8 = larvae::RegisterBenchmark("StackAllocator", "GetMarkerOverhead", [](larvae::BenchmarkState& state) {
        comb::StackAllocator allocator{1_MB};

        while (state.KeepRunning())
        {
            auto marker = allocator.GetMarker();
            larvae::DoNotOptimize(marker);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench9 = larvae::RegisterBenchmark("StackAllocator", "FreeToMarkerOverhead", [](larvae::BenchmarkState& state) {
        comb::StackAllocator allocator{10_MB};

        for (int i = 0; i < 1000; ++i)
        {
            allocator.Allocate(1024, 8);
        }

        auto marker = allocator.GetMarker();

        while (state.KeepRunning())
        {
            allocator.Allocate(128, 8);
            allocator.FreeToMarker(marker);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench10 = larvae::RegisterBenchmark("StackAllocator", "RecursivePattern", [](larvae::BenchmarkState& state) {
        comb::StackAllocator allocator{10_MB};

        std::function<int(int, int)> simulate_recursion = [&](int depth, int max_depth) -> int
        {
            if (depth >= max_depth)
                return depth;

            auto marker = allocator.GetMarker();

            void* temp = allocator.Allocate(256, 8);
            larvae::DoNotOptimize(temp);

            int result = simulate_recursion(depth + 1, max_depth);

            allocator.FreeToMarker(marker);
            return result;
        };

        while (state.KeepRunning())
        {
            int result = simulate_recursion(0, 20);
            larvae::DoNotOptimize(result);
            allocator.Reset();
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench11 = larvae::RegisterBenchmark("malloc", "ScopedAllocations", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            void* outer = malloc(512);
            larvae::DoNotOptimize(outer);

            for (int j = 0; j < 10; ++j)
            {
                void* inner1 = malloc(128);
                void* inner2 = malloc(64);
                larvae::DoNotOptimize(inner1);
                larvae::DoNotOptimize(inner2);

                free(inner2);
                free(inner1);
            }

            free(outer);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench12 = larvae::RegisterBenchmark("StackAllocator", "WorstCaseAlignment", [](larvae::BenchmarkState& state) {
        comb::StackAllocator allocator{100_MB};

        size_t alignments[] = {1, 2, 4, 8, 16, 32, 64, 128};
        const int num_alignments = sizeof(alignments) / sizeof(alignments[0]);

        size_t i = 0;
        while (state.KeepRunning())
        {
            size_t align = alignments[i % num_alignments];
            void* ptr = allocator.Allocate(17, align);
            larvae::DoNotOptimize(ptr);

            if (allocator.GetUsedMemory() > 90_MB)
            {
                allocator.Reset();
            }

            ++i;
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench13 = larvae::RegisterBenchmark("StackAllocator", "NewDeleteHelpers", [](larvae::BenchmarkState& state) {
        struct SmallObject
        {
            int data[4];
            SmallObject() { data[0] = 42; }
        };

        comb::StackAllocator allocator{10_MB};

        while (state.KeepRunning())
        {
            auto marker = allocator.GetMarker();

            SmallObject* obj = comb::New<SmallObject>(allocator);
            larvae::DoNotOptimize(obj);
            comb::Delete(allocator, obj);

            allocator.FreeToMarker(marker);

            if (allocator.GetUsedMemory() > 9_MB)
            {
                allocator.Reset();
            }
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench14 = larvae::RegisterBenchmark("StackAllocator", "ManyMarkers", [](larvae::BenchmarkState& state) {
        comb::StackAllocator allocator{100_MB};
        std::vector<comb::StackAllocator::Marker> markers;
        markers.reserve(100);

        while (state.KeepRunning())
        {
            markers.clear();

            for (int j = 0; j < 100; ++j)
            {
                markers.push_back(allocator.GetMarker());
                void* ptr = allocator.Allocate(64, 8);
                larvae::DoNotOptimize(ptr);
            }

            for (int j = 99; j >= 0; --j)
            {
                allocator.FreeToMarker(markers[j]);
            }

            if (allocator.GetUsedMemory() > 90_MB)
            {
                allocator.Reset();
            }
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench15 = larvae::RegisterBenchmark("StackAllocator", "StringParsingPattern", [](larvae::BenchmarkState& state) {
        comb::StackAllocator allocator{10_MB};

        while (state.KeepRunning())
        {
            auto outer_marker = allocator.GetMarker();

            for (int line = 0; line < 100; ++line)
            {
                auto line_marker = allocator.GetMarker();

                void* line_buffer = allocator.Allocate(256, 8);
                void* token_buffer = allocator.Allocate(64, 8);

                larvae::DoNotOptimize(line_buffer);
                larvae::DoNotOptimize(token_buffer);

                allocator.FreeToMarker(line_marker);
            }

            allocator.FreeToMarker(outer_marker);
        }

        state.SetItemsProcessed(state.iterations());
    });
}

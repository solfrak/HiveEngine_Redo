#include <larvae/larvae.h>
#include <hive/core/memory/allocator.h>

class MockAllocator : public hive::memory::Allocator
{
public:
    explicit MockAllocator(size_t capacity)
        : alloc_count_{0}
        , dealloc_count_{0}
    {
        base_ = ::operator new(capacity);
        capacity_ = capacity;
    }

    ~MockAllocator() override
    {
        if (base_)
        {
            ::operator delete(base_);
        }
    }

    void* Allocate(size_t size, size_t alignment) override
    {
        ++alloc_count_;
        void* ptr = static_cast<char*>(base_) + used_memory_;
        used_memory_ += size;
        return ptr;
    }

    void Deallocate(void* ptr) override
    {
        ++dealloc_count_;
    }

    size_t GetUsedMemory() const override { return used_memory_; }
    size_t GetTotalMemory() const override { return capacity_; }
    const char* GetName() const override { return "MockAllocator"; }

    int GetAllocCount() const { return alloc_count_; }
    int GetDeallocCount() const { return dealloc_count_; }

private:
    size_t used_memory_ = 0;
    int alloc_count_;
    int dealloc_count_;
};

namespace {
    auto test1 = larvae::RegisterTest("MemoryAllocator", "AllocateReturnsValidPointer", []() {
        MockAllocator allocator(1024);
        void* ptr = allocator.Allocate(64, 8);
        larvae::AssertNotNull(ptr);
    });

    auto test2 = larvae::RegisterTest("MemoryAllocator", "AllocateIncreasesUsedMemory", []() {
        MockAllocator allocator(1024);
        larvae::AssertEqual(allocator.GetUsedMemory(), 0u);

        allocator.Allocate(64, 8);
        larvae::AssertEqual(allocator.GetUsedMemory(), 64u);
    });

    auto test3 = larvae::RegisterTest("MemoryAllocator", "NewConstructsObject", []() {
        MockAllocator allocator(1024);

        struct TestObject
        {
            int value;
            TestObject(int v) : value(v) {}
        };

        TestObject* obj = allocator.New<TestObject>(42);
        larvae::AssertNotNull(obj);
        larvae::AssertEqual(obj->value, 42);
    });

    auto test4 = larvae::RegisterTest("MemoryAllocator", "DeleteDestroysObject", []() {
        MockAllocator allocator(1024);

        struct TestObject
        {
            bool* destroyed;
            TestObject(bool* d) : destroyed(d) { *destroyed = false; }
            ~TestObject() { *destroyed = true; }
        };

        bool destroyed = false;
        TestObject* obj = allocator.New<TestObject>(&destroyed);
        larvae::AssertFalse(destroyed);

        allocator.Delete(obj);
        larvae::AssertTrue(destroyed);
    });

    auto test5 = larvae::RegisterTest("MemoryAllocator", "GetTotalMemoryReturnsCapacity", []() {
        MockAllocator allocator(2048);
        larvae::AssertEqual(allocator.GetTotalMemory(), 2048u);
    });

    auto test6 = larvae::RegisterTest("MemoryAllocator", "GetNameReturnsCorrectName", []() {
        MockAllocator allocator(1024);
        larvae::AssertStringEqual(allocator.GetName(), "MockAllocator");
    });

    class AllocatorFixture : public larvae::TestFixture
    {
    public:
        void SetUp() override
        {
            allocator = new MockAllocator(4096);
        }

        void TearDown() override
        {
            delete allocator;
        }

        MockAllocator* allocator = nullptr;
    };

    auto test7 = larvae::RegisterTestWithFixture<AllocatorFixture>("AllocatorFixture", "MultipleAllocations", [](AllocatorFixture& f) {
        void* ptr1 = f.allocator->Allocate(100, 8);
        void* ptr2 = f.allocator->Allocate(200, 8);
        void* ptr3 = f.allocator->Allocate(300, 8);

        larvae::AssertNotNull(ptr1);
        larvae::AssertNotNull(ptr2);
        larvae::AssertNotNull(ptr3);

        larvae::AssertEqual(f.allocator->GetAllocCount(), 3);
        larvae::AssertEqual(f.allocator->GetUsedMemory(), 600u);
    });

    auto test8 = larvae::RegisterTestWithFixture<AllocatorFixture>("AllocatorFixture", "DeallocateIncrementsCounter", [](AllocatorFixture& f) {
        void* ptr = f.allocator->Allocate(64, 8);

        larvae::AssertEqual(f.allocator->GetDeallocCount(), 0);

        f.allocator->Deallocate(ptr);

        larvae::AssertEqual(f.allocator->GetDeallocCount(), 1);
    });
}

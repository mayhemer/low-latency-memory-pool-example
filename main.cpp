#include <cstddef>
#include <atomic>
#include <assert.h>
#include <thread>
#include <stdio.h>

#include <cstdint>
#include <limits>
#include <type_traits>

template <size_t VALUE_SIZE, size_t POOL_CAPACITY>
struct memory_pool
{
    memory_pool();

    /**
     * Allcates a slot in the memory pool
     * @return nullptr when no slots available
     */
    std::byte *acquire();

    /**
     * Deallocate a slot if previously allocated.
     * Does nothing when the address does not belong to the buffer.
     * Doesn't check for double-free or false-free.
     */
    void release(std::byte *at);

private:
    // we need to fit the tag
    static_assert(POOL_CAPACITY < UINT32_MAX);

    // Marker for stack end (a non-existing index)
    static inline constexpr uint32_t kStackEnd = UINT32_MAX;

    // tag | index packed pointer.  Points at the first item available in the stack-linked-list,
    // most significant bits hold a monotinically (overlapping) tag to prevent ABA problem.
    std::atomic<uint64_t> head;

    // A linked list representing a stack.  Each item points to the next by default,
    // except the last one, which points to <end>.  Contains the tag | index combination.
    alignas(8) std::atomic<uint32_t> next[POOL_CAPACITY];

    // Extracts just the index part from the tagged head.
    static inline uint32_t extract_index(uint64_t source_head)
    {
        return uint32_t(source_head & UINT32_MAX);
    }
    // Builds the next tagged head by adding a tag+1 of the source head to the given index.
    static inline uint64_t build_next_tag(size_t index, uint64_t source_head)
    {
        return uint64_t{index} | ((source_head & (uint64_t{UINT32_MAX} << 32U)) + (1ULL << 32U));
    }

    // The buffer itself
    alignas(64) std::byte values[POOL_CAPACITY * VALUE_SIZE];
};

template <size_t VALUE_SIZE, size_t POOL_CAPACITY>
memory_pool<VALUE_SIZE, POOL_CAPACITY>::memory_pool()
{
    for (uint32_t i = 0; i < POOL_CAPACITY - 1; ++i)
    {
        next[i].store(i + 1U, std::memory_order_relaxed);
    }
    next[POOL_CAPACITY - 1].store(kStackEnd, std::memory_order_relaxed);

    head.store(0ULL, std::memory_order_release);
}

template <size_t VALUE_SIZE, size_t POOL_CAPACITY>
std::byte *memory_pool<VALUE_SIZE, POOL_CAPACITY>::acquire()
{
    uint64_t current_head = head.load(std::memory_order_acquire);
    while (true)
    {
        uint32_t index = extract_index(current_head);
        if (index == kStackEnd)
        {
            return nullptr;
        }

        uint32_t next_index = next[index].load(std::memory_order_relaxed);
        uint64_t next_head = build_next_tag(next_index, current_head);
        if (head.compare_exchange_weak(current_head, next_head,
                                       std::memory_order_release,
                                       std::memory_order_acquire))
        {
            return values + index;
        }
    }
}

template <size_t VALUE_SIZE, size_t POOL_CAPACITY>
void memory_pool<VALUE_SIZE, POOL_CAPACITY>::release(std::byte *at)
{
    if (!at || at < values)
    {
        return;
    }

    ptrdiff_t p = at - values;
    if (p >= POOL_CAPACITY || (p % VALUE_SIZE != 0U))
    {
        return;
    }

    size_t index = p / VALUE_SIZE;

    uint64_t current_head = head.load(std::memory_order_acquire);
    while (true)
    {
        next[index].store(current_head, std::memory_order_relaxed);

        uint64_t new_head = build_next_tag(index, current_head);
        if (head.compare_exchange_weak(current_head, new_head,
                                       std::memory_order_release,
                                       std::memory_order_acquire))
        {
            return;
        }
    }
}

int main()
{
    while (true)
    {
        memory_pool<1, 4> pool;

        auto test1 = [&]() mutable
        {
            std::thread::id this_id = std::this_thread::get_id();
            for (unsigned long long i = 0; i < 500; ++i)
            {
                auto p1 = pool.acquire();
                auto p2 = pool.acquire();
                auto p3 = pool.acquire();

                assert((p1 != p2) || !p1);
                assert((p1 != p3) || !p1);
                assert((p2 != p3) || !p2);

                pool.release(p2);
                pool.release(p1);
                pool.release(p3);

                // printf("%u pointers: %p %p %p\n", this_id, p1, p2, p3);
            }
        };

        std::jthread t1(test1);
        std::jthread t2(test1);
        std::jthread t3(test1);
    }

    return 0;
}

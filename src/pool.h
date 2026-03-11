#ifndef __POOL
#define __POOL

#include <cstddef>
#include <atomic>

template <size_t VALUE_SIZE, size_t POOL_CAPACITY>
struct memory_pool
{
    memory_pool();

    /**
     * Allocates a slot in the memory pool
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
    // We need to fit the tag to the head pointer: tag | index.
    static_assert(POOL_CAPACITY < UINT32_MAX);

    // Marker for stack end (a non-existing index)
    static inline constexpr uint32_t kStackEnd = UINT32_MAX;

    // tag | index packed pointer.  Points at the first item available in the stack-linked-list,
    // most significant bits hold a monotinically (overlapping) tag to prevent ABA problem.
    std::atomic<uint64_t> head;

    // A linked list representing a stack.  Each item points to the next by default,
    // except the last one, which points to <end>.  Contains just the index combination, 
    // tags not needed.
    alignas(alignof(uint32_t)) std::atomic<uint32_t> next[POOL_CAPACITY];

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
    next[POOL_CAPACITY - 1U].store(kStackEnd, std::memory_order_relaxed);

    head.store(0ULL, std::memory_order_release);
}

template <size_t VALUE_SIZE, size_t POOL_CAPACITY>
std::byte *memory_pool<VALUE_SIZE, POOL_CAPACITY>::acquire()
{
    uint64_t current_head = head.load(std::memory_order_acquire);
    for (;;)
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
    if (p >= ptrdiff_t(POOL_CAPACITY) || (p % VALUE_SIZE != 0U))
    {
        return;
    }

    size_t index = p / VALUE_SIZE;

    uint64_t current_head = head.load(std::memory_order_acquire);
    for (;;)
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

#endif

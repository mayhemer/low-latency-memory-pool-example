#include "pool.h"

#include <assert.h>
#include <thread>
#include <stdio.h>

int main()
{
    for (unsigned long long depth = 50; depth < (50 << 8); depth <<= 1)
    {
        for (unsigned long long test = 0; test < 1000; ++test)
        {
            memory_pool<1, 4> pool;

            // TODO: collect benchmarks
            auto test1 = [&, depth]() mutable
            {
                // std::thread::id this_id = std::this_thread::get_id();
                for (unsigned long long allocs = 0; allocs < depth; ++allocs)
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
    }

    return 0;
}

#define STRONG_MEMORY_ORDERING_FLAG true

#include <iostream>
#include <memory_management/global_allocator.h>
#include <cmath>
#include <random>
#include <chrono>
#include <vector>

class TestAllocator
{
    private:

        std::vector<bool> heap_data;
    
    public:

        TestAllocator(size_t sz): heap_data(sz, false){}

        void block(const std::pair<size_t, size_t>& segment)
        {
            size_t first    = segment.first;
            size_t last     = segment.first + segment.second;

            // std::cout << "block called " << first << "<>" << last << std::endl;

            if (last > this->heap_data.size())
            {
                throw std::invalid_argument("bad segment, out of bound access");
            }

            for (size_t i = first; i < last; ++i)
            {
                if (heap_data[i])
                {
                    throw std::runtime_error("bad segment, already blocked");
                }
            }

            for (size_t i = first; i < last; ++i)
            {
                heap_data[i] = true;
            }
        }

        void unblock(const std::pair<size_t, size_t>& segment)
        {
            size_t first    = segment.first;
            size_t last     = segment.first + segment.second;

            // std::cout << "unblock called " << first << "<>" << last << std::endl;

            if (last > this->heap_data.size())
            {
                throw std::invalid_argument("bad argument, out of bound access");
            }

            for (size_t i = first; i < last; ++i)
            {
                if (!heap_data[i])
                {
                    throw std::runtime_error("bad segment, already unblocked");
                }
            }

            for (size_t i = first; i < last; ++i)
            {
                heap_data[i] = false;
            }
        }

        auto has_memory_for(size_t sz) -> bool
        {
            size_t counter = 0u;

            for (size_t i = 0u; i < heap_data.size(); ++i)
            {
                if (heap_data[i] == true)
                {
                    if (counter >= sz)
                    {
                        return true;
                    }

                    counter = 0u;
                    continue;
                }

                counter += 1u;
            }

            return counter >= sz;
        }
};

template <class ...Args>
auto test_one(const std::shared_ptr<TestAllocator>& test_allocator,
              const std::shared_ptr<global_allocator::SegmentAllocator<Args...>>& actual_allocator,
              size_t sz) -> std::shared_ptr<void>
{
    if (test_allocator == nullptr)
    {
        std::abort();
    }

    if (actual_allocator == nullptr)
    {
        std::abort();
    }

    if (sz == 0u)
    {
        return nullptr;
    }

    // std::cout << "allocation called > " << sz << std::endl;

    std::optional<std::pair<size_t, size_t>> blk = actual_allocator->malloc(sz);

    if (!blk.has_value())
    {
        if (test_allocator->has_memory_for(sz))
        {
            throw std::runtime_error("mayday, mismatch allocator blocks");
        }

        return nullptr;
    }

    auto destructor = [=](void * polymorphic_blk)
    {
        std::pair<size_t, size_t> * semantic_blk = static_cast<std::pair<size_t, size_t> *>(polymorphic_blk);
        actual_allocator->free(*semantic_blk);
        test_allocator->unblock(*semantic_blk);

        delete semantic_blk;
    };

    test_allocator->block(blk.value());

    return std::unique_ptr<std::pair<size_t, size_t>, decltype(destructor)>(new std::pair<size_t, size_t>(blk.value()), destructor);
}

void free_blocks(std::vector<std::shared_ptr<void>>& mem_blk_vec)
{
    static auto randomizer = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

    if (mem_blk_vec.empty())
    {
        return;
    }

    size_t sz = randomizer() % mem_blk_vec.size();
    
    std::shuffle(mem_blk_vec.begin(), mem_blk_vec.end(), std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    mem_blk_vec.resize(sz);

    // for (size_t i = 0u; i < sz; ++i)
    // {
    //     size_t idx = randomizer() % mem_blk_vec.size();
    //     mem_blk_vec.erase(std::next(mem_blk_vec.begin(), idx));
    // }
}

void run_one_test()
{
    static auto randomizer      = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    size_t TREE_HEIGHT_RANGE    = 8u;
    size_t TEST_SZ_RANGE        = size_t{1} << 12;
    size_t tree_height          = randomizer() % TREE_HEIGHT_RANGE + 1u;
    size_t buf_sz               = std::pow(4u, (tree_height - 1));
    size_t test_sz              = randomizer() % TEST_SZ_RANGE;
    size_t clear_chance         = size_t{1} << 6;

    // std::cout << "buf_sz > " << buf_sz << std::endl;
    // std::cout << "tree_height > " << tree_height << std::endl;

    auto actual_allocator       = std::make_shared<global_allocator::SegmentAllocator<std::integral_constant<size_t, 4u>>>(tree_height);
    auto test_allocator         = std::make_shared<TestAllocator>(buf_sz);
    auto buf_vec                = std::vector<std::shared_ptr<void>>();

    for (size_t i = 0u; i < test_sz; ++i)
    {
        buf_vec.push_back(test_one(test_allocator, actual_allocator, randomizer() % (buf_sz + 1u)));

        if (randomizer() % clear_chance == 0u || buf_vec.back() == nullptr)
        {
            free_blocks(buf_vec);
        }
    }
}

void sequential_bench()
{

    //what we have found is that the outdegree of 8 and uint32_t is very important for cache fetch and the total population of the tree
    //even if the deviation is only within +/- base_sz, the difference is very crucial for cache fetch
    //such is that we increase the cache hit rate by a lot of time for base trees of lower sizes, and we also decrease the number of steps to the base by 3 folds
    //

    size_t tree_height          = 10u;
    auto actual_allocator       = std::make_shared<global_allocator::SegmentAllocator<std::integral_constant<size_t, 8u>, uint32_t>>(tree_height);
    const size_t TOTAL_BLK_SZ   = size_t{1} << 20;
    size_t sum                  = 0u;
    auto then                   = std::chrono::high_resolution_clock::now().time_since_epoch();

    for (size_t i = 0u; i < TOTAL_BLK_SZ; ++i)
    {
        auto blk = actual_allocator->malloc(1u);

        if (!blk.has_value())
        {
            throw std::runtime_error("empty blk");
        }

        sum += blk->second;
    }

    auto now                    = std::chrono::high_resolution_clock::now().time_since_epoch();
    size_t tick                 = std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count();

    std::cout << "SEQUENTIAL_BENCH, " << TOTAL_BLK_SZ << "<block size>" << sum << "<sum>" << tree_height << "<height>" << tick << "<milliseconds>" << std::endl;
}

void jagged_bench()
{
    size_t tree_height          = 10u;
    auto actual_allocator       = std::make_shared<global_allocator::SegmentAllocator<std::integral_constant<size_t, 8u>, uint32_t>>(tree_height);
    const size_t TOTAL_BLK_SZ   = size_t{1} << 20;
    size_t sum                  = 0u;
    auto blk_vec                = std::vector<std::pair<size_t, size_t>>();

    for (size_t i = 0u; i < TOTAL_BLK_SZ; ++i)
    {
        auto blk = actual_allocator->malloc(1u);

        if (!blk.has_value())
        {
            throw std::runtime_error("empty blk");
        }

        sum += blk->second;
        blk_vec.push_back(blk.value());
    }

    std::shuffle(blk_vec.begin(), blk_vec.end(), std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

    for (size_t i = 0u; i < TOTAL_BLK_SZ / 2; ++i)
    {
        actual_allocator->free(blk_vec[i]);
    }

    auto then                   = std::chrono::high_resolution_clock::now().time_since_epoch();

    for (size_t i = 0u; i < TOTAL_BLK_SZ / 2; ++i)
    {
        auto blk = actual_allocator->malloc(1u);

        if (!blk.has_value())
        {
            throw std::runtime_error("empty blk");
        }

        sum += blk->second;
        blk_vec.push_back(blk.value());
    }

    auto now                    = std::chrono::high_resolution_clock::now().time_since_epoch();
    size_t tick                 = std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count();

    std::cout << "JAGGED_BENCH, " << TOTAL_BLK_SZ << "<block size>" << sum << "<sum>" << tree_height << "<height>" << tick << "<milliseconds>" << std::endl;
}

void run_test()
{
    const size_t TEST_SZ  = size_t{1} << 20;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        run_one_test();

        if (i % 128 == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }

    sequential_bench();
    jagged_bench();
}

auto sum() -> size_t
{
    static auto randomizer          = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    const size_t ARRAY_SIZE_RANGE   = 1024u;
    size_t array_size               = randomizer() % ARRAY_SIZE_RANGE;
    size_t total                    = 0u;

    std::vector<size_t, global_allocator::GlobalAllocator<size_t>> sum_vec{};

    for (size_t i = 0u; i < array_size; ++i)
    {
        sum_vec.push_back(i);
    }

    for (size_t e: sum_vec)
    {
        total += e;
    }

    return total;
}

void test_memory()
{
    std::cout << "TESTING MEMORY" << std::endl;

    const size_t TEST_SZ    = size_t{1} << 30;
    size_t total            = 0u;
    auto then               = std::chrono::high_resolution_clock::now().time_since_epoch();

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        total += sum();

        if (i % (size_t{1} << 20) == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }

    auto now                = std::chrono::high_resolution_clock::now().time_since_epoch();
    size_t tick             = std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count();

    std::cout << "MEMORY BENCH, " << TEST_SZ << "<test_sz>" << total << "<total>" << tick << "<milliseconds>" << std::endl;
}

int main()
{
    run_test();
    test_memory();
}
#define STRONG_MEMORY_ORDERING_FLAG true

#include <memory>
#include <memory_management/immutable_multiplatform_memory_x.h>
#include <iostream>
#include <functional>
#include <utility>
#include <bit>
#include <random>

class Allocator: public virtual immutable_multiplatform_memory_x::MemoryAllocatorInterface
{
    public:

        auto allocate_from_view(std::string_view buffer_view) -> std::shared_ptr<void>
        {
            auto rs = std::make_unique<char[]>(buffer_view.size());
            std::memcpy(rs.get(), buffer_view.data(), buffer_view.size());

            return rs;
        }
};

//most people wont pass this introduction class, would you?
//after this course by me, everyone would know how to code accurately 100% 

//one thing we have learnt the hard way about error codes is that we'd have to solve the error code in a generic manner
//and we'd have to resolve the problem at the function call, not at the caller, essentially we can composite the function and solve it gnerically

class MemoryTester
{
    private:

        struct MemoryBucket
        {
            std::shared_ptr<void> mem_ptr;
            size_t mem_ptr_sz;
        };

        using randomizer_t = decltype(std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{static_cast<uint32_t>(0u)}));

        std::shared_ptr<immutable_multiplatform_memory_x::ImmutableMemoryCacheInterface> mem_cache;
        size_t window_sz;
        size_t operation_sz;
        std::vector<MemoryBucket> bucket_vec;
        randomizer_t randomizer;

    public:

        // static inline constexpr size_t DEFAULT_WINDOW_SZ = size_t{1} << 10;

        MemoryTester(std::shared_ptr<immutable_multiplatform_memory_x::ImmutableMemoryCacheInterface> mem_cache,
                     size_t window_sz,
                     size_t operation_sz): mem_cache(std::move(mem_cache)),
                                           window_sz(window_sz),
                                           operation_sz(operation_sz),
                                           bucket_vec(),
                                           randomizer(std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count())})){}

        void operator()()
        {
            for (size_t i = 0u; i < this->operation_sz; ++i)
            {
                if (this->bucket_vec.size() == this->window_sz)
                {
                    this->evict_one_random_memory_reference();
                }

                if (!this->bucket_vec.empty())
                {
                    size_t idx = this->randomizer() % this->bucket_vec.size();

                    if (randomizer() % 2 == 0u)
                    {
                        std::optional<immutable_multiplatform_memory_x::MemoryReference> mem_reference;

                        this->mem_cache->acquire_memory(&this->bucket_vec[idx].mem_ptr, 1u, &mem_reference);

                        if (!mem_reference.has_value())
                        {
                            continue;
                        }

                        if (mem_reference->ptr_mem_sz != this->bucket_vec[idx].mem_ptr_sz)
                        {
                            std::cout << "mayday, memory size mismatched" << std::endl;;
                            std::abort();
                        }

                        if (std::memcmp(this->bucket_vec[idx].mem_ptr.get(), mem_reference->device_ptr, mem_reference->ptr_mem_sz) != 0)
                        {
                            std::cout << "mayday, semantic representation mismatched" << std::endl;
                            std::abort();
                        }

                        this->mem_cache->release_memory(&mem_reference.value(), 1u);
                    }
                    else
                    {
                        immutable_multiplatform_memory_x::MemoryReference mem_reference;
                        immutable_multiplatform_memory_x::CacheAndAcquireArgument arg
                        {
                            .immutable_reference    = this->bucket_vec[idx].mem_ptr,
                            .host_memory            = std::string_view(static_cast<char *>(this->bucket_vec[idx].mem_ptr.get()), this->bucket_vec[idx].mem_ptr_sz)
                        };

                        this->mem_cache->cache_and_acquire_memory(&arg, 1u, &mem_reference);

                        if (mem_reference.ptr_mem_sz != this->bucket_vec[idx].mem_ptr_sz)
                        {
                            std::cout << "mayday, memory size mismatched" << std::endl;
                            std::abort();
                        }

                        if (std::memcmp(this->bucket_vec[idx].mem_ptr.get(), mem_reference.device_ptr, mem_reference.ptr_mem_sz) != 0)
                        {
                            std::cout << "mayday, semantic representation mismatched" << std::endl;
                            std::abort();
                        }

                        this->mem_cache->release_memory(&mem_reference, 1u);
                    }
                }

                MemoryBucket bucket = this->get_random_memory_bucket();

                {
                    immutable_multiplatform_memory_x::MemoryReference mem_reference;
                    immutable_multiplatform_memory_x::CacheAndAcquireArgument arg
                    {
                        .immutable_reference    = bucket.mem_ptr,
                        .host_memory            = std::string_view(static_cast<char *>(bucket.mem_ptr.get()), bucket.mem_ptr_sz)
                    };

                    this->mem_cache->cache_and_acquire_memory(&arg, 1u, &mem_reference);
                    this->mem_cache->release_memory(&mem_reference, 1u);
                }
            }
        }

    private:

        void evict_one_random_memory_reference() noexcept
        {
            if (this->bucket_vec.empty())
            {
                std::cout << "mayday, evicting empty container" << std::endl;
                std::abort();
            }

            size_t idx = this->randomizer() % this->bucket_vec.size();

            std::swap(this->bucket_vec.back(), this->bucket_vec[idx]);
            this->bucket_vec.pop_back();
        }

        auto get_random_memory_bucket() -> MemoryBucket
        {
            const size_t MEMORY_SZ_RANGE = size_t{1} << 6;

            size_t memory_sz            = this->randomizer() % MEMORY_SZ_RANGE + 1u;
            std::unique_ptr<char[]> mem = std::make_unique<char[]>(memory_sz);

            for (size_t i = 0u; i < memory_sz; ++i)
            {
                mem[i] = std::bit_cast<char>(static_cast<uint8_t>(this->randomizer() & std::numeric_limits<uint8_t>::max()));
            }

            return MemoryBucket
            {
                .mem_ptr    = std::shared_ptr<void>(std::move(mem)),
                .mem_ptr_sz = memory_sz
            };
        }
};

auto randomize_uint(size_t first, size_t incl_last) -> size_t
{
    static auto randomizer = std::bind(std::uniform_int_distribution<size_t>(), std::mt19937_64{static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count())});
    size_t last = incl_last + 1u;

    if (first >= last)
    {
        std::cout << "mayday, bad randomization, first >= last" << std::endl;
        std::abort();
    }

    size_t sz       = last - first;
    size_t offset   = randomizer() % sz;

    return first + offset;
}

void run_one_immutable_multiplatform_memory_test()
{
    const size_t MIN_MEMORY_LIFETIME_IN_MILLISECONDS    = 1u;
    const size_t MAX_MEMORY_LIFETIME_IN_MILLISECONDS    = size_t{1} << 10;
    const size_t MIN_CRON_DUR_IN_MILLISECONDS           = size_t{1} << 10;
    const size_t MAX_CRON_DUR_IN_MILLISECONDS           = size_t{1} << 14;
    const size_t MIN_AUTOEVICT_BYTE_SZ                  = 0u;
    const size_t MAX_AUTOEVICT_BYTE_SZ                  = size_t{1} << 10;
    const size_t MIN_CONCURRENT_SZ                      = 1u;
    const size_t MAX_CONCURRENT_SZ                      = size_t{1} << 4;
    const size_t MIN_WINDOW_SZ                          = 1u;
    const size_t MAX_WINDOW_SZ                          = size_t{1} << 6;
    const size_t MIN_OPERATION_SZ                       = 0u;
    const size_t MAX_OPERATION_SZ                       = size_t{1} << 6;

    std::shared_ptr<immutable_multiplatform_memory_x::ImmutableMemoryCacheInterface> memcache = std::make_shared<immutable_multiplatform_memory_x::ImmutableMemoryCache>(std::make_unique<Allocator>(), randomize_uint(MIN_AUTOEVICT_BYTE_SZ, MAX_AUTOEVICT_BYTE_SZ));

    size_t concurrent_worker    = randomize_uint(MIN_CONCURRENT_SZ, MAX_CONCURRENT_SZ);
    size_t window_sz            = randomize_uint(MIN_WINDOW_SZ, MAX_WINDOW_SZ);
    size_t operation_sz         = randomize_uint(MIN_OPERATION_SZ, MAX_OPERATION_SZ);

    std::vector<std::unique_ptr<std::thread>> thr_vec{};

    for (size_t i = 0u; i < concurrent_worker; ++i)
    {
        auto runner = [=]
        {
            MemoryTester(memcache, window_sz, operation_sz)();
        };

        thr_vec.push_back(std::make_unique<std::thread>(runner));
    }

    for (const auto& thr: thr_vec)
    {
        thr->join();
    }
}

void run_immutable_multiplatform_memory_test()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 10;

    std::cout << "__BEGIN_IMMUTABLE_MULTIPLATFORM_MEMORY_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        run_one_immutable_multiplatform_memory_test();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }

    std::cout << "__END_IMMUTABLE_MULTIPLATFORM_MEMORY_TEST__" << std::endl;
}

int main()
{
    run_immutable_multiplatform_memory_test();
}
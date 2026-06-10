#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <iostream>
#include <memory_sync/memory_synchronizer_factory.h>
#include <random>
#include <functional>
#include <algorithm>
#include <memory>
// #include <concurrency_utility/concurrency_utility.h>
#include <stl_extension/stdx.h>
#include <cstring>

using namespace memory_sync;

auto randomize_int(size_t first, size_t last) -> size_t
{
    if (first >= last)
    {
        std::abort();
    }

    static auto randomizer = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

    return first + randomizer() % (last - first);
}

template <class T>
using TaskPromise   = concurrency_detachable_task::DetachableTaskHandleInterface<T>;

template <class T>
class ValueAsTaskPromise: public virtual TaskPromise<T>
{
    private:

        bool was_wait_invoked;
        T value;

    public:

        ValueAsTaskPromise(T value): value(std::move(value)),
                                        was_wait_invoked(false){}

        auto is_completed() noexcept -> bool
        {
            return true;
        }

        void interrupt() noexcept
        {
            (void) this->value;
        }

        auto wait() -> T
        {
            if (std::exchange(this->was_wait_invoked, true))
            {
                throw std::invalid_argument("bad wait, second wait");
            }

            return std::move(this->value);
        }

        void detach() noexcept
        {
            (void) this->value;
        }
};

template <class T>
auto value_to_task_promise(T&& value) -> std::unique_ptr<TaskPromise<std::decay_t<T>>>
{
    return std::make_unique<ValueAsTaskPromise<std::decay_t<T>>>(std::forward<T>(value));
}

template <class T>
auto to_shared_promise(std::unique_ptr<TaskPromise<T>>&& promise) -> std::shared_ptr<TaskPromise<T>>
{
    return promise;
}

class MemoryReadable: public virtual MemoryReadableInterface
{
    private:

        std::string * buf;
    
    public:

        MemoryReadable(std::string * buf): buf(buf){}

        void read(void * dst, size_t offset, size_t sz)
        {
            size_t first    = offset;
            size_t last     = first + sz;

            if (last > this->buf->size())
            {
                throw std::invalid_argument("bad region access");
            }

            std::memcpy(dst, std::next(buf->data(), offset), sz);
        }
};

class MemoryWritable: public virtual AsyncMemoryWritableInterface
{
    private:

        std::string * buf;
    
    public:

        MemoryWritable(std::string * buf): buf(buf){}

        auto write(size_t offset, size_t sz, const void * src) -> AsyncMemoryWritableInterface::Promise
        {
            size_t first    = offset;
            size_t last     = first + sz;

            if (last > this->buf->size())
            {
                throw std::invalid_argument("bad region access");
            }

            std::memcpy(std::next(this->buf->data(), offset), src, sz);

            return to_shared_promise(value_to_task_promise(stdx::fancy_void{}));
        }
};

void run_one_test()
{
    const size_t BUFFER_SZ_RANGE    = size_t{1} << 4;
    const size_t TAINT_SZ_RANGE     = size_t{1} << 4;
    size_t buffer_sz                = randomize_int(0u, BUFFER_SZ_RANGE);

    std::string host_buffer(buffer_sz, ' ');
    std::string other_buffer(buffer_sz, ' ');

    std::shared_ptr<MemoryReadableInterface> memory_readable        = std::make_shared<MemoryReadable>(&host_buffer);
    std::shared_ptr<AsyncMemoryWritableInterface> memory_writable   = std::make_shared<MemoryWritable>(&other_buffer);
    std::unique_ptr<MemorySynchronizerInterface> synchronizer       = MemorySynchronizerFactory::get_host_memory_synchronizer(memory_readable,
                                                                                                                              {memory_writable},
                                                                                                                              buffer_sz);

    for (size_t i = 0u; i < TAINT_SZ_RANGE; ++i)
    {
        size_t first    = randomize_int(0u, buffer_sz + 1);
        size_t rem_sz   = buffer_sz - first;
        size_t range    = randomize_int(0u, rem_sz + 1);

        for (size_t j = 0u; j < range; ++j)
        {
            host_buffer[first + j] = std::bit_cast<char>(static_cast<uint8_t>(randomize_int(0, 256)));
        }

        synchronizer->taint_memory({first, range});

        if (randomize_int(0, 16) == 0u)
        {
            synchronizer->sync();

            if (host_buffer != other_buffer)
            {
                std::cout << "mayday, mismatched synchronization\n";
                std::abort();
            }
        }
    }
}

void run_test()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 10;

    std::cout << "__BEGIN_MEMORY_SYNC_TEST__\n";

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        run_one_test();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << "\n";
        }
    }

    std::cout << "__END_MEMORY_SYNC_TEST__\n";
}

int main()
{
    run_test();
}
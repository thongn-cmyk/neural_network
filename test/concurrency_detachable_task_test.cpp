#define DEBUG_MODE_FLAG true
#define STRONG_MEMORY_ORDERING_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <concurrency_detachable_task/detachable_task_launcher.h>
#include <iostream>
// #include <concurrency_base/concurrency_base.h>
// #include <main_service/main_service.h>

void initialize_concurrency()
{
    using namespace concurrency_base;

    std::vector<WorkerInformation> worker_vec{};

    for (size_t i = 0u; i < 1; ++i)
    {
        worker_vec.push_back(
        {
            .cpu_id = std::nullopt,
            .daemon = RESOURCE_DISPOSER_DAEMON
        });
    }

    for (size_t i = 0u; i < 10; ++i)
    {
        worker_vec.push_back({
            .cpu_id = std::nullopt,
            .daemon = COMMON_ONFLY_POOL
        });
    }

    for (size_t i = 0u; i < 1; ++i)
    {
        worker_vec.push_back({
            .cpu_id = std::nullopt,
            .daemon = COROUTINE_DAEMON
        });
    }

    concurrency_base::init({worker_vec});
}

void initialize_resource()
{
    std::cout << "initializing concurrency...\n";
    initialize_concurrency();

    // std::cout << "initializing affined randomizer...\n";
    // affined_randomizer::init();

    // std::cout << "initializing stack allocator...\n";
    // allocation_base::stack_allocator::init();

    // std::cout << "initializing global allocator...\n";
    // allocation_base::global_allocator::init();

    std::cout << "initializing resource disposer...\n";
    resource_disposer::init();

    std::filesystem::path tmp_file = std::filesystem::temp_directory_path() / "concurrency_task_test.txt";

    std::cout << "initializing log file...\n";
    logging_subsystem::init(tmp_file);

    std::cout << "initializing main service...\n";
    main_service::init();
}

class CalculationTask: public virtual concurrency_task::TaskInterface<size_t>
{
    private:

        size_t sum_iteration_sz;

    public:

        CalculationTask(size_t sum_iteration_sz): sum_iteration_sz(sum_iteration_sz){}

        auto run(common_exception::CancellationTokenInterface& cancellation_token) -> size_t
        {
            size_t total = 0u;

            for (size_t i = 0u; i < sum_iteration_sz; ++i)
            {
                total += i;
            }

            return total;
        }
};

void run_calculation_test()
{
    const size_t SUM_ITERATION_SZ = size_t{1} << 8;

    common_exception::CancellationToken cancellation_token{};

    size_t expected_result  = CalculationTask(SUM_ITERATION_SZ).run(cancellation_token);
    auto task               = concurrency_detachable_task::DetachableTaskLauncher{}.launch(std::static_pointer_cast<concurrency_task::TaskInterface<size_t>>(std::make_shared<CalculationTask>(SUM_ITERATION_SZ)));
    size_t actual_result    = task->wait();

    if (expected_result != actual_result)
    {
        std::cout << "mayday, task result mismatched\n";
        std::abort();
    }

    if (!task->is_completed())
    {
        std::cout << "mayday, is completed test failed\n";
        std::abort();
    }
}

class InterruptionTask: public virtual concurrency_task::TaskInterface<size_t>
{
    public:

        auto run(common_exception::CancellationTokenInterface& cancellation_token) -> size_t
        {
            while (true)
            {
                if (cancellation_token.is_canceled())
                {
                    common_exception::throw_exception(common_exception::OPERATION_CANCELED_ERROR);
                }
            }
        }
};

void run_interruption_test()
{
    auto task = concurrency_detachable_task::DetachableTaskLauncher{}.launch(std::static_pointer_cast<concurrency_task::TaskInterface<size_t>>(std::make_shared<InterruptionTask>()));

    try
    {
        task->interrupt();
        task->wait();
    }
    catch (common_exception::operation_canceled_error& err)
    {
        if (!task->is_completed())
        {
            std::cout << "mayday, is completed test failed\n";
            std::abort();
        }

        return;
    }

    std::cout << "mayday, interruption test failed\n";
    std::abort();
}

void run_is_completed_test()
{
    const size_t SUM_ITERATION_SZ = size_t{1} << 8;

    common_exception::CancellationToken cancellation_token{};

    size_t expected_result  = CalculationTask(SUM_ITERATION_SZ).run(cancellation_token);
    auto task               = concurrency_detachable_task::DetachableTaskLauncher{}.launch(std::static_pointer_cast<concurrency_task::TaskInterface<size_t>>(std::make_shared<CalculationTask>(SUM_ITERATION_SZ)));

    while (!task->is_completed()){}

    size_t actual_result    = task->wait();

    if (expected_result != actual_result)
    {
        std::cout << "mayday, task result mismatched\n";
        std::abort();
    }
}

void run_detach_test()
{
    const size_t SUM_ITERATION_SZ = size_t{1} << 8;

    common_exception::CancellationToken cancellation_token{};

    size_t expected_result  = CalculationTask(SUM_ITERATION_SZ).run(cancellation_token);
    auto task               = concurrency_detachable_task::DetachableTaskLauncher{}.launch(std::static_pointer_cast<concurrency_task::TaskInterface<size_t>>(std::make_shared<CalculationTask>(SUM_ITERATION_SZ)));
    size_t actual_result    = task->wait();

    if (expected_result != actual_result)
    {
        std::cout << "mayday, task result mismatched\n";
        std::abort();
    }

    if (!task->is_completed())
    {
        std::cout << "mayday, is completed test failed\n";
        std::abort();
    }

    task->detach();
}

void run_loop_test()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 10;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        run_detach_test();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << "\n";
        }
    }
}

class TestWorker: public virtual concurrency_base::WorkerInterface
{
    public:

        auto run_one_epoch() noexcept -> bool
        {
            std::cout << "running calculation test...\n";
            run_calculation_test();

            std::cout << "running interruption test...\n";
            run_interruption_test();

            std::cout << "running is completed test...\n";
            run_is_completed_test();

            std::cout << "running loop test...\n";
            run_loop_test();

            std::cout << "__END_CONCURRENCY_TASK_TEST__\n";

            // common_exception::throw_exception(common_exception::OPERATION_GRACEFUL_TERMINATION_ERROR);
            std::abort();

            return true;
        }
};

void run_test()
{
    std::cout << "__BEGIN_CONCURRENCY_TASK_TEST__\n";

    std::cout << "initializing resource...\n";
    initialize_resource();

    std::cout << "registering daemon...\n";
    auto handle = concurrency_base::daemon_saferegister(concurrency_base::COROUTINE_DAEMON, std::make_unique<TestWorker>());

    if (!handle.has_value())
    {
        std::cout << "mayday, cannot register Test daemon\n";
        std::abort();
    }

    std::cout << "subscribing main...\n";
    main_service::main_subscribe();
}

int main()
{
    run_test();
}
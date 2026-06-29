#ifndef __COROUTINE_SUBSYSTEM_COROUTINEABLE_X_H__
#define __COROUTINE_SUBSYSTEM_COROUTINEABLE_X_H__

#include <stdint.h>
#include <stdlib.h>
#include "topic_code.h"
#include "implementation/coroutine_implementation.h"
#include <stl_extension/stdx.h>
#include <global_config/coroutine_x_config.h>
#include <stdexcept>
#include <exception>
#include "coroutine_waitable_interface.h"

namespace coroutine_x
{
    using namespace coroutine_x::implementation;

    struct Signature{};
    struct Signature1{};
    struct Signature2{};

    using NetworkLauncherSingleton  = stdx::singleton_container<std::shared_ptr<LauncherInterface>, Signature>;
    using FileIOLauncherSingleton   = stdx::singleton_container<std::shared_ptr<LauncherInterface>, Signature1>;
    using ComputeLauncherSingleton  = stdx::singleton_container<std::shared_ptr<LauncherInterface>, Signature2>;

    void init()
    {
        using namespace global_config::coroutine_x_config;

        stdx::memtransaction_guard tx_grd;

        if (HAS_NETWORK_COROUTINE_MACHINE)
        {
            NetworkLauncherSingleton::get() = LauncherFactory::get_normal_launcher();
        }

        if (HAS_FILEIO_COROUTINE_MACHINE)
        {
            FileIOLauncherSingleton::get()  = LauncherFactory::get_normal_launcher();
        }

        if (HAS_COMPUTE_COROUTINE_MACHINE)
        {
            ComputeLauncherSingleton::get() = LauncherFactory::get_normal_launcher();
        }
    }

    void deinit() noexcept
    {
        stdx::memtransaction_guard tx_grd;

        NetworkLauncherSingleton::get() = nullptr;
        FileIOLauncherSingleton::get()  = nullptr;
        ComputeLauncherSingleton::get() = nullptr;
    }

    void run_detached(std::shared_ptr<CoroutineableInterface> coroutineable,
                      uint8_t coroutine_topic)
    {
        if (coroutineable == nullptr)
        {
            throw std::invalid_argument("bad coroutineable, null");
        }

        switch (coroutine_topic)
        {
            case NETWORK_COROUTINE:
            {
                if (NetworkLauncherSingleton::get() == nullptr)
                {
                    throw std::runtime_error("network coroutine launcher is not initialized");
                }

                NetworkLauncherSingleton::get()->add(std::move(coroutineable));
                break;
            }
            case FILEIO_COROUTINE:
            {
                if (FileIOLauncherSingleton::get() == nullptr)
                {
                    throw std::runtime_error("fileio coroutine launcher is not initialized");
                }

                FileIOLauncherSingleton::get()->add(std::move(coroutineable));
                break;
            }
            case COMPUTE_COROUTINE:
            {
                if (ComputeLauncherSingleton::get() == nullptr)
                {
                    throw std::runtime_error("compute coroutine launcher is not initialized");
                }

                ComputeLauncherSingleton::get()->add(std::move(coroutineable));
                break;
            }
            default:
            {
                throw std::invalid_argument("bad coroutine topic, enumeration out of range");
            }
        }
    }

    auto run_promise(std::shared_ptr<CoroutineableInterface> coroutineable,
                    uint8_t coroutine_topic) -> std::unique_ptr<CoroutineWaitableInterface>
    {
        if (coroutineable == nullptr)
        {
            throw std::invalid_argument("bad coroutineable, null");
        }

        std::shared_ptr<std::atomic<bool>> complete_status              = std::make_shared<std::atomic<bool>>(false);
        std::shared_ptr<CoroutineableInterface> promoted_coroutineable  = std::make_shared<WaitingCoroutineableWrapper>(coroutineable, complete_status);

        run_detached(std::move(promoted_coroutineable), coroutine_topic);

        try
        {
            return std::make_unique<CoroutineWaiter>(complete_status);
        }
        catch (...)
        {
            std::abort();
        }
    }
}
#endif
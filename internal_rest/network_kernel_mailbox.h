#ifndef __NETWORK_KERNEL_PRODUCER_H__
#define __NETWORK_KERNEL_PRODUCER_H__

//define HEADER_CONTROL 11

#include <stdint.h>
#include <stddef.h>
#include <array>
#include <string>
#include <memory>
#include "network_std_container.h"
#include <chrono>
#include <vector>
#include <optional>
// #include "network_kernel_mailbox_impl1.h"
// #include "network_kernel_mailbox_impl1_x.h" 

namespace dg_sock::network_kernel_mailbox
{
    using Address           = uint8_t;
    using channel_t         = uint8_t;

    struct Config{};

    struct Signature{};

    // using SingletonObject = sdtxx::singleton<Signature, std::shared_ptr<dg_sock::network_kernel_mailbox_impl1_x::channel::MailboxInterface>>;

    void init(const Config& config)
    {
        // SingletonObject::get() = dg_sock::network_kernel_mailbox_impl1_x::channel::SolutionBuilder{}.set_config(config).build();
        // std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    void deinit() noexcept
    {
        // std::atomic_thread_fence(std::memory_order_seq_cst);
        // SingletonObject::get() = nullptr;
    }

    //be very careful about lifetime of packets here, because the lifetime of recv and send should be unbounded as it is referenced
    //but internally for mailbox_impl1_x and mailbox_impl1, we have patched all the memory holes there could be

    //as we already discussed last time, synchronization of packet transfer from REST_Controller guarantees the destination pool to be of consistent and stable size
    //allowing us to have a 100% transfer rate and success rate across compute nodes in AWS and Google Cloud architecture if we partition the bandwidth by using capacity correctly

    //our job is to anticipate for the worst case scenerio of the mayhem, such is that we compromise memory but not crash the system at the mailbox site
    //and that we know that our global pool has to be referenced by the rest request client or rest request handler, whose lifetime is clear and never in a fault state

    //truth be told, I've seen too many systems that are not fair, and does not hold unique reference of the process, this is usually very bad
    //doing thing this way, we can make sure that maybe we aren't coverign 100% of other use cases, but in our use case, we'd be 100%

    void send(uint32_t channel,
              Address * addr_arr, dg_sock::string * content_arr, size_t sz,
              exception_t * exception_arr) noexcept
    {
        // if constexpr(DEBUG_MODE_FLAG)
        // {
        //     if (sz > SingletonObject::get()->max_consume_size())
        //     {
        //         dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
        //         std::abort();
        //     }
        // }

        // dg_sock::network_stack_allocation::NoExceptAllocation<MailBoxArgument[]> mailbox_arr(sz);

        // for (size_t i = 0u; i < sz; ++i)
        // {
        //     mailbox_arr[i].to           = addr_arr[i];
        //     mailbox_arr[i].content      = content_arr[i].data();
        //     mailbox_arr[i].content_sz   = content_arr[i].size();
        // }

        // SingletonObject::get()->send(channel, mailbox_arr.get(), sz, exception_arr);
    }

    void recv(uint32_t channel,
              dg_sock::string * output_arr, size_t output_arr_sz, size_t output_arr_cap) noexcept
    {
        // SingletonObject::get()->recv(channel, output_arr, output_arr_sz, output_arr_cap);
    }

    auto max_consume_size() noexcept -> size_t
    {
        // return SingletonObject::get()->max_consume_size();

        return {};
    }
}

#endif
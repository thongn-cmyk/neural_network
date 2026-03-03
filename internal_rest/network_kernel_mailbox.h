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
#include "network_log.h"
#include "network_kernel_mailbox_impl1_channel_x.h"

// #include "network_kernel_mailbox_impl1.h"
// #include "network_kernel_mailbox_impl1_x.h" 

namespace dg_sock::network_kernel_mailbox
{
    using Address           = dg_sock::network_kernel_mailbox_impl1::model::Address;

    struct Config{};

    struct Signature{};

    using SingletonObject   = stdxx::singleton<Signature, std::shared_ptr<dg_sock::network_kernel_mailbox_impl1_channel_x::ChannelMailboxInterface>>;
    using MailBoxArgument   = dg_sock::network_kernel_mailbox_impl1::model::MailBoxArgument;

    struct InternalRecvWrapper: virtual dg_sock::network_kernel_mailbox_impl1_channel_x::OutputContainableInterface
    {
        private:

            dg_sock::string * reference;

        public:

            InternalRecvWrapper() = default;

            InternalRecvWrapper(dg_sock::string * reference): reference(reference){}

            void resize(size_t sz)
            {
                stdxx::safe_ptr_access(this->reference)->resize(sz);
            }

            auto data() noexcept -> std::add_pointer_t<char>
            {
                return stdxx::safe_ptr_access(this->reference)->data();
            }
    };

    void init(const Config& config)
    {

    }

    void deinit() noexcept
    {
        SingletonObject::get() = nullptr;
    }

    void send(uint32_t channel,
              Address * addr_arr, dg_sock::string * content_arr, size_t sz,
              exception_t * exception_arr) noexcept
    {
        dg_sock::network_stack_allocation::NoExceptAllocation<MailBoxArgument[]> mailbox_arr(sz);

        for (size_t i = 0u; i < sz; ++i)
        {
            mailbox_arr[i] = MailBoxArgument
            {
                .to         = addr_arr[i],
                .content    = static_cast<const void *>(content_arr[i].data()),
                .content_sz = content_arr[i].size()
            };
        }

        SingletonObject::get()->send(channel, mailbox_arr.get(), sz, exception_arr);
    }

    void recv(uint32_t channel,
              dg_sock::string * output_arr, size_t& output_arr_sz, size_t output_arr_cap) noexcept
    {
        dg_sock::network_stack_allocation::NoExceptAllocation<InternalRecvWrapper[]> recv_container_arr(output_arr_cap);
        dg_sock::network_stack_allocation::NoExceptAllocation<std::add_pointer_t<dg_sock::network_kernel_mailbox_impl1_channel_x::OutputContainableInterface>[]> virtual_recv_container_arr(output_arr_cap);
        dg_sock::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(output_arr_cap);

        for (size_t i = 0u; i < output_arr_cap; ++i)
        {
            recv_container_arr[i]           = InternalRecvWrapper(std::next(output_arr, i));
            virtual_recv_container_arr[i]   = &recv_container_arr[i];
        }

        SingletonObject::get()->recv(channel,
                                     virtual_recv_container_arr.get(), output_arr_sz, output_arr_cap,
                                     exception_arr.get());

        if constexpr(DEBUG_MODE_FLAG)
        {
            for (size_t i = 0u; i < output_arr_sz; ++i)
            {
                if (dg_sock::network_exception::is_failed(exception_arr[i]))
                {
                    dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                    std::abort();
                }
            }
        }
    }

    auto max_consume_size() noexcept -> size_t
    {
        return SingletonObject::get()->max_consume_size();
    }
}

#endif
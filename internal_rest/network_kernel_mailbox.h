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
#include "network_kernel_mailbox_impl1.h"
#include "network_kernel_mailbox_impl1_flash_stream_x.h"

namespace dg_sock::network_kernel_mailbox
{
    using Address           = dg_sock::network_kernel_mailbox_impl1::model::Address;

    struct Config
    {
        dg_sock::network_kernel_mailbox_impl1::Config base_config;
        dg_sock::network_kernel_mailbox_impl1_flash_stream_x::Config stream_config;
        dg_sock::network_kernel_mailbox_impl1_channel_x::Config channel_config;
    };

    struct Signature{};

    using SingletonObject   = stdxx::singleton<Signature, std::shared_ptr<dg_sock::network_kernel_mailbox_impl1_channel_x::ChannelMailboxInterface>>;
    using MailBoxArgument   = dg_sock::network_kernel_mailbox_impl1::model::MailBoxArgument;

    void init(const Config& config)
    {
        stdxx::memtransaction_guard tx_grd;

        std::shared_ptr<dg_sock::network_kernel_mailbox_impl1::core::MailboxInterface> base = dg_sock::network_kernel_mailbox_impl1::ConfigMaker::make(config.base_config);
        dg_sock::network_kernel_mailbox_impl1_flash_stream_x::Config stream_config = config.stream_config;
        stream_config.base = base;
        std::shared_ptr<dg_sock::network_kernel_mailbox_impl1_flash_stream_x::DynamicMailboxInterface> stream_base = dg_sock::network_kernel_mailbox_impl1_flash_stream_x::spawn(stream_config);

        SingletonObject::get() = dg_sock::network_kernel_mailbox_impl1_channel_x::SolutionBuilder{}.load_config(config.channel_config)
                                                                                                   .set_base(stream_base)
                                                                                                   .build();
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
        SingletonObject::get()->recv(channel,
                                     output_arr, output_arr_sz, output_arr_cap);
    }

    auto max_consume_size() noexcept -> size_t
    {
        return SingletonObject::get()->max_consume_size();
    }
}

#endif
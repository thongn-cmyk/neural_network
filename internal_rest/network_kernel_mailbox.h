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
#include "network_kernel_mailbox_impl1.h"
#include "network_kernel_mailbox_impl1_x.h" 

namespace dg_sock::network_kernel_mailbox_channel{

    using radix_t = uint8_t; 

    enum mailbox_channel: radix_t{
        CHANNEL_HEARTBEAT       = 0u,
        CHANNEL_EXTMEMCOMMIT    = 1u,
        CHANNEL_REST            = 2u
    };
} 

namespace dg_sock::network_kernel_mailbox{

    using MailBoxArgument   = dg_sock::network_kernel_mailbox_impl1::model::MailBoxArgument; 
    using Address           = dg_sock::network_kernel_mailbox_impl1::model::Address;
    using channel_t         = network_kernel_mailbox_channel::radix_t;

    struct Config{
        size_t outbound_worker_count;
        size_t inbound_worker_count;
        size_t retransmission_worker_count;
        // dg_sock::vector<ip_t> host_ips;
        dg_sock::vector<uint16_t> host_ports;
        std::chrono::nanoseconds retransmission_delay;
        size_t retranmission_count;
        std::optional<size_t> inbound_exhaustion_control_sz;
        std::optional<size_t> outbound_exhaustion_control_sz;
        bool is_meterlog_enabled;
        bool is_heartbeat_enabled;
        bool is_recovery_on_failure_enabled;
    };

    inline std::unique_ptr<dg_sock::network_kernel_mailbox_impl1::core::MailboxInterface> mailbox;

    void init(){

    }

    void send(...) noexcept{

        // mailbox->send(std::move(addr), std::move(msg));
    }

    auto recv(...) noexcept -> std::optional<dg_sock::string>{ //optional string because string.empty() does not mean that it is not a packet
        
        // return mailbox->recv();

        return {};
    }

    auto max_consume_size() noexcept -> size_t{

        return {};
    }
}

#endif
#ifndef __NETWORK_SOCK_STATUS_CONTROLLER_H__
#define __NETWORK_SOCK_STATUS_CONTROLLER_H__

#include "network_exception.h"
#include <stdint.h>
#include <stdlib.h>
#include <chrono>

namespace dg_sock::network_sock_traffic_status_controller::types
{
    using traffic_status_t = uint8_t;    
}

namespace dg_sock::network_sock_traffic_status_controller::constants
{
    using namespace dg_sock::network_sock_traffic_status_controller::types;

    enum traffic_status : traffic_status_t
    {
        ease = 0,
        normal = 1,
        congested = 2
    };
}

namespace dg_sock::network_sock_traffic_status_controller::interface
{
    using namespace dg_sock::network_sock_traffic_status_controller::types;

    class TrafficStatusControllerInterface
    {
        public:

            virtual ~TrafficStatusControllerInterface() noexcept = default;
            virtual auto set_inbound_congested_for(std::chrono::nanoseconds dur) noexcept -> exception_t = 0;
            virtual auto set_outbound_congested_for(std::chrono::nanoseconds dur) noexcept -> exception_t = 0;
            virtual auto add_inbound(size_t inbound_byte_sz) noexcept -> exception_t = 0;
            virtual auto add_outbound(size_t outbound_byte_sz) noexcept -> exception_t = 0;
            virtual auto get_inbound_status() noexcept -> traffic_status_t = 0;
            virtual auto get_outbound_status() noexcept -> traffic_status_t = 0; 
    };
}

namespace dg_sock::network_sock_traffic_status_controller::impl
{
    struct ComponentFactory
    {
        static auto make(...) -> std::unique_ptr<interface::TrafficStatusControllerInterface>
        {
            return {};
        }
    };
}

namespace dg_sock::network_sock_traffic_status_controller
{
    using namespace dg_sock::network_sock_traffic_status_controller::types; 

    struct Config
    {
        uint64_t ease_inbound_byte_threshold;
        uint64_t normal_inbound_byte_threshold;
        uint64_t congested_inbound_byte_threshold;

        uint64_t ease_outbound_byte_threshold;
        uint64_t normal_outbound_byte_threshold;
        uint64_t congested_outbound_byte_threshold; 

        std::chrono::nanoseconds threshold_interval;  
        std::chrono::nanoseconds least_update_interval;
    };

    inline dg_sock::network_sock_traffic_status_controller::interface::TrafficStatusControllerInterface * volatile status_controller_instance; 

    extern void init(Config config)
    {
        stdxx::memtransaction_guard transaction_guard;
        auto status_controller_instance_up  = dg_sock::network_sock_traffic_status_controller::impl::ComponentFactory::make(config.ease_inbound_byte_threshold,
                                                                                                                       config.normal_inbound_byte_threshold,
                                                                                                                       config.congested_inbound_byte_threshold,
                                                                                                                       config.ease_outbound_byte_threshold,
                                                                                                                       config.normal_outbound_byte_threshold,
                                                                                                                       config.congested_outbound_byte_threshold,
                                                                                                                       config.threshold_interval,
                                                                                                                       config.least_update_interval);

        status_controller_instance          = status_controller_instance_up.get();
        status_controller_instance_up.release();
    }

    extern void deinit() noexcept
    {
        stdxx::memtransaction_guard transaction_guard;
        delete status_controller_instance;
    }

    extern auto set_inbound_congested_for(std::chrono::nanoseconds dur) noexcept -> exception_t
    {
        return status_controller_instance->set_inbound_congested_for(dur);
    }

    extern auto set_outbound_congested_for(std::chrono::nanoseconds dur) noexcept -> exception_t
    {
        return status_controller_instance->set_outbound_congested_for(dur);
    }

    extern auto add_inbound(size_t inbound_byte_sz) noexcept -> exception_t
    {
        return status_controller_instance->add_inbound(inbound_byte_sz);
    }

    extern auto add_outbound(size_t outbound_byte_sz) noexcept -> exception_t
    {
        return status_controller_instance->add_outbound(outbound_byte_sz);
    }

    extern auto get_inbound_status() noexcept -> traffic_status_t
    {
        return status_controller_instance->get_inbound_status();
    }

    extern auto get_outbound_status() noexcept -> traffic_status_t
    {
        return status_controller_instance->get_outbound_status();
    }
}

#endif
#ifndef __DATA_LOADER_SOURCE_LOADER_WAIT_LOADER_MODEL_H__
#define __DATA_LOADER_SOURCE_LOADER_WAIT_LOADER_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <serializer/compact_serializer.h>
#include <data_loader/transaction_broker/model.h>
#include <string>

namespace data_loader::source_loader::wait_loader
{
    struct WaitLoaderConfig
    {
        uint64_t tx_sz;
        data_loader::transaction_broker::ExternalSourceTransactionBrokerConfig broker_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(tx_sz, broker_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(tx_sz, broker_config);
        }
    };

    struct ExternalWaitLoaderConfig
    {
        std::string config_bytestream;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(config_bytestream);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(config_bytestream);
        }
    };

    auto to_external_wait_loader_config(const WaitLoaderConfig& config) -> ExternalWaitLoaderConfig
    {
        return ExternalWaitLoaderConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_wait_loader_config(const ExternalWaitLoaderConfig& config) -> WaitLoaderConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<WaitLoaderConfig>(config.config_bytestream);
    } 
}

#endif
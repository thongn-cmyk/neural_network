#ifndef __DATA_LOADER_TRANSACTION_BROKER_MODEL_H__
#define __DATA_LOADER_TRANSACTION_BROKER_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <data_loader/source/generic_source/model.h>
#include <data_loader/retryer_device/generic_device/model.h>
#include <string>
#include <serializer/compact_serializer.h>

namespace data_loader::transaction_broker
{
    struct SourceTransactionBrokerConfig
    {
        data_loader::source::generic_source::ExternalGenericReaderConfig source_config;
        data_loader::retryer_device::generic_device::ExternalGenericRetryConfig retry_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(source_config, retry_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(source_config, retry_config);
        }
    };

    struct ExternalSourceTransactionBrokerConfig
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

    auto to_external_source_transaction_broker_config(const SourceTransactionBrokerConfig& config) -> ExternalSourceTransactionBrokerConfig
    {
        return ExternalSourceTransactionBrokerConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_source_transaction_broker_config(const ExternalSourceTransactionBrokerConfig& config) -> SourceTransactionBrokerConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<SourceTransactionBrokerConfig>(config.config_bytestream);
    }
}

#endif
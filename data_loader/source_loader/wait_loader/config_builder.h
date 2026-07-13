#ifndef __DATA_LOADER_SOURCE_LOADER_WAIT_LOADER_CONFIG_BUILDER_H__
#define __DATA_LOADER_SOURCE_LOADER_WAIT_LOADER_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include <data_loader/transaction_broker/config_builder.h>
#include <stl_extension/stdx.h>
#include "model.h"

namespace data_loader::source_loader::wait_loader
{
    class WaitLoaderConfigBuilder
    {
        private:

            using self  = WaitLoaderConfigBuilder;

            uint64_t tx_sz;
            data_loader::transaction_broker::SourceTransactionBrokerConfigBuilder tx_broker_config_builder;

            static inline constexpr uint64_t MIN_TX_SZ          = 1u;
            static inline constexpr uint64_t DEFAULT_TX_SZ      = 1u; 

        public:

            WaitLoaderConfigBuilder(): tx_sz(DEFAULT_TX_SZ),
                                       tx_broker_config_builder(){}

            auto set_transaction_size(size_t tx_sz) -> self&
            {
                if (tx_sz < MIN_TX_SZ)
                {
                    throw std::invalid_argument("bad transaction size, > 0 required");
                }

                this->tx_sz = stdx::throw_integer_cast<uint64_t>(tx_sz);

                return *this;
            }

            auto get_transaction_broker_config_builder() -> data_loader::transaction_broker::SourceTransactionBrokerConfigBuilder& 
            {
                return this->tx_broker_config_builder;
            }

            auto build() -> ExternalWaitLoaderConfig
            {
                return this->get_external_wait_loader_config();
            }

        private:

            auto get_internal_wait_loader_config() -> WaitLoaderConfig
            {
                return
                {
                    .tx_sz          = this->tx_sz,
                    .broker_config  = this->tx_broker_config_builder.build()
                };
            }

            auto get_external_wait_loader_config() -> ExternalWaitLoaderConfig
            {
                return to_external_wait_loader_config
                (
                    this->get_internal_wait_loader_config()
                );
            }
    };
}

#endif
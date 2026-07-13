#ifndef __DATA_LOADER_TRANSACTION_BROKER_CONFIG_BUILDER_H__
#define __DATA_LOADER_TRANSACTION_BROKER_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include "model.h"

#include <data_loader/source/generic_source/config_builder.h>
#include <data_loader/source/generic_source/model.h>

#include <data_loader/retryer_device/generic_device/config_builder.h>
#include <data_loader/retryer_device/generic_device/model.h> 

namespace data_loader::transaction_broker
{
    class SourceTransactionBrokerConfigBuilder
    {
        private:

            using ReaderConfigBuilder           = data_loader::source::generic_source::GenericReaderConfigBuilder;
            using RetryerMachineConfigBuilder   = data_loader::retryer_device::generic_device::GenericRetryerMachineConfigBuilder;

            std::unique_ptr<ReaderConfigBuilder> reader_config_builder;
            std::unique_ptr<RetryerMachineConfigBuilder> retryer_machine_config_builder;

        public:

            SourceTransactionBrokerConfigBuilder(): reader_config_builder(std::make_unique<ReaderConfigBuilder>()),
                                                    retryer_machine_config_builder(std::make_unique<RetryerMachineConfigBuilder>()){}

            auto get_reader_config_builder() -> ReaderConfigBuilder&
            {
                return *this->reader_config_builder;
            }

            auto get_retryer_machine_config_builder() -> RetryerMachineConfigBuilder&
            {
                return *this->retryer_machine_config_builder;
            }

            auto build() -> ExternalSourceTransactionBrokerConfig
            {
                return this->get_external_source_transaction_broker_config();
            }

        private:

            auto get_internal_source_transaction_broker_config() -> SourceTransactionBrokerConfig
            {
                return
                {
                    .source_config  = this->reader_config_builder->build(),
                    .retry_config   = this->retryer_machine_config_builder->build()
                };
            }

            auto get_external_source_transaction_broker_config() -> ExternalSourceTransactionBrokerConfig
            {
                return to_external_source_transaction_broker_config
                (
                    this->get_internal_source_transaction_broker_config()
                );
            }
    };
}

#endif
#ifndef __DATA_LOADER_TRANSACTION_BROKER_TRANSACTION_BROKER_H__
#define __DATA_LOADER_TRANSACTION_BROKER_TRANSACTION_BROKER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <data_loader/source/generic_source/generic_source.h>
#include <data_loader/retryer_device/retryer_device_interface.h>
#include <data_loader/retryer_device/generic_device/generic_device.h>
#include "transaction_broker_interface.h"
#include "model.h"

namespace data_loader::transaction_broker
{
    class SourceTransactionBroker: public virtual TransactionBrokerInterface
    {
        private:

            std::unique_ptr<data_loader::source::SourceLoaderInterface> base;
            std::unique_ptr<data_loader::retryer_device::RetryerMachineInterface> retryer_machine;

        public:

            SourceTransactionBroker(const SourceTransactionBrokerConfig& config): base(std::make_unique<data_loader::source::generic_source::GenericReader>(config.source_config)),
                                                                                  retryer_machine(std::make_unique<data_loader::retryer_device::generic_device::GenericRetryerMachine>(config.retry_config)){}

            SourceTransactionBroker(const ExternalSourceTransactionBrokerConfig& config): SourceTransactionBroker(to_internal_source_transaction_broker_config(config)){}

            auto get(size_t tx_hint_sz,
                     common_exception::CancellationTokenInterface& cancellation_token) -> std::optional<std::vector<std::string>>
            {
                std::optional<std::vector<std::string>> rs{};

                InternalRunnable runnable(tx_hint_sz, this->base.get(), &rs);
                this->retryer_machine->run(runnable, cancellation_token);

                return rs;
            }

        private:

            class InternalRunnable: public virtual data_loader::retryer_device::RunnableInterface
            {
                private:

                    size_t tx_hint_sz;
                    data_loader::source::SourceLoaderInterface * src;
                    std::optional<std::vector<std::string>> * rs;

                public:

                    InternalRunnable(size_t tx_hint_sz,
                                     data_loader::source::SourceLoaderInterface * src,
                                     std::optional<std::vector<std::string>> * rs): tx_hint_sz(tx_hint_sz),
                                                                                    src(src),
                                                                                    rs(rs){}

                    void run(common_exception::CancellationTokenInterface& cancellation_token)
                    {
                        *this->rs = this->src->get(this->tx_hint_sz);
                    }
            };
    };
}

#endif
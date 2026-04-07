#ifndef __TRANSACTION_REQUESTOR_H__
#define __TRANSACTION_REQUESTOR_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <data_loader/source/generic_source.h>
#include <data_loader/retryer_device/retryer_device_interface.h>
#include "transaction_broker_interface.h"

namespace data_loader::source_loader::broker
{
    struct SourceTransactionBrokerConfig
    {
        data_loader::generic_source::GenericReaderConfig source_config;
        data_loader::retryer_device::generic_device::GenericRetryConfig retry_config;

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

    class SourceTransactionBroker: public virtual TransactionBrokerInterface
    {
        private:

            std::unique_ptr<data_loader::SourceLoaderInterface> base;
            std::unique_ptr<data_loader::retryer_device::RetryerMachineInterface> retryer_machine;

        public:

            SourceTransactionBroker(SourceTransactionBrokerConfig config): base(std::make_unique<data_loader::generic_source::GenericReader>(config.source_config)),
                                                                           retryer_machine(std::make_unique<data_loader::retryer_device::generic_device::GenericRetryerMachine>(config.retry_config)){}

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
                    data_loader::SourceLoaderInterface * src;
                    std::optional<std::vector<std::string>> * rs;

                public:

                    InternalRunnable(size_t tx_hint_sz,
                                     data_loader::SourceLoaderInterface * src,
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
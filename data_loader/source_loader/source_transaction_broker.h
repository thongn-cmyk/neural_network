#ifndef __TRANSACTION_REQUESTOR_H__
#define __TRANSACTION_REQUESTOR_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <data_loader/source/generic_source.h>

namespace data_loader::source_loader::broker
{
    struct retryer_machine_zero_ran_error: std::invalid_argument
    {
        retryer_machine_zero_ran_error(): std::invalid_argument("run process was not invoked by retryer machine"){}
    };

    class SourceTransactionBroker: public virtual TransactionBrokerInterface
    {
        private:

            std::unique_ptr<data_loader::SourceLoaderInterface> base;

        public:

            SourceTransactionBroker(data_loader::generic_source::Configuration config): base(std::make_unique<data_loader::generic_source::GenericReader>(config)){}

            auto get(size_t tx_hint_sz,
                     data_loader::retryer_device::RetryerMachineInterface& retryer_machine,
                     common_exception::CancellationTokenInterface& cancellation_token) -> std::optional<std::vector<std::string>>
            {
                bool was_ran = false;
                std::optional<std::vector<std::string>> rs{};

                InternalRunnable runnable(tx_hint_sz, this->base.get(), &was_ran, &rs);
                retryer_machine.run(runnable, cancellation_token);

                if (!was_ran)
                {
                    throw retryer_machine_zero_ran_error{};
                }

                return rs;
            }

        private:

            class InternalRunnable: public virtual data_loader::retryer_device::RunnableInterface
            {
                private:

                    size_t tx_hint_sz;
                    data_loader::SourceLoaderInterface * src;
                    bool * was_ran;
                    std::optional<std::vector<std::string>> * rs;

                public:

                    InternalRunnable(size_t tx_hint_sz,
                                     data_loader::SourceLoaderInterface * src,
                                     bool * was_ran,
                                     std::optional<std::vector<std::string>> * rs): tx_hint_sz(tx_hint_sz),
                                                                                    src(src),
                                                                                    was_ran(was_ran),
                                                                                    rs(rs){}

                    void run(common_exception::CancellationTokenInterface& cancellation_token)
                    {
                        *this->rs       = this->src->get(this->tx_hint_sz);
                        *this->was_ran  = true;
                    }
            };
    };
}

#endif
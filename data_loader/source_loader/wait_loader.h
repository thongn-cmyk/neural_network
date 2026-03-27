#ifndef __DATA_LOADER_WAIT_LOADER_H__
#define __DATA_LOADER_WAIT_LOADER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <data_loader/source/generic_source.h>
#include <data_loader/retryer_device/generic_device.h>

namespace data_loader::source_loader::wait_loader
{
    struct Configuration
    {
        uint64_t tx_hint_sz;
        data_loader::generic_source::Configuration source_config;
        data_loader::retryer_device::Configuration retry_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(tx_hint_sz, source_config, retry_config);            
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(tx_hint_sz, source_config, retry_config);
        }
    };

    class WaitLoader: public virtual data_loader::source_loader::UserSpaceSourceLoaderInterface
    {
        private:

            std::unique_ptr<data_loader::source_loader::broker::TransactionBrokerInterface> broker;
            std::unique_ptr<data_loader::retryer_device::RetryerMachineInterface> retryer_machine;
            std::deque<std::string> prefetched_token_vec;
            size_t tx_hint_sz;
            bool was_completed;
            bool was_corrupted;

        public:

            WaitLoader(Configuration config): broker(std::make_unique<data_loader::source_loader::broker::SourceTransactionBroker>(config.source_config)),
                                              retryer_machine(std::make_unique<data_loader::retryer_device::GenericRetryDevice>(config.retry_config)),
                                              prefetched_token_vec(),
                                              tx_hint_sz(stdx::safe_non_zero_access(config.tx_hint_sz)),
                                              was_completed(false),
                                              was_corrupted(false){}

            auto get(common_exception::CancellationTokenInterface& cancellation_token) -> std::optional<std::string>
            {
                if (this->was_corrupted)
                {
                    throw loader_corrupted_error{};
                }

                if (this->was_completed)
                {
                    return std::nullopt;
                }

                if (this->prefetched_token_vec.empty())
                {
                    this->refill_prefetched_token_vec(cancellation_token);
                }

                if (this->prefetched_token_vec.empty())
                {
                    this->was_completed = true;

                    return std::nullopt;
                }

                std::string rs = std::move(this->prefetched_token_vec.front());
                this->prefetched_token_vec.pop_front();

                return rs;
            }

        private:

            void refill_prefetched_token_vec(common_exception::CancellationTokenInterface& cancellation_token)
            {
                while (true)
                {
                    std::optional<std::vector<std::string>> rs = this->broker->get(this->tx_hint_sz,
                                                                                   *this->retryer_machine,
                                                                                   cancellation_token);

                    if (!rs.has_value())
                    {
                        return;
                    }

                    if (rs->empty())
                    {
                        continue;
                    }

                    try
                    {
                        std::copy(std::make_move_iterator(rs->begin()),
                                  std::make_move_iterator(rs->end()),
                                  std::back_inserter(this->prefetched_token_vec));
                    }
                    catch (...)
                    {
                        this->was_corrupted = true;
                        throw;
                    }

                    return;
                }
            }
    };
}

#endif
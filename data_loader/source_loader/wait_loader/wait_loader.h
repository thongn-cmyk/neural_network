#ifndef __DATA_LOADER_SOURCE_LOADER_WAIT_LOADER_WAIT_LOADER_H__
#define __DATA_LOADER_SOURCE_LOADER_WAIT_LOADER_WAIT_LOADER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <data_loader/source/generic_source/generic_source.h>
#include <data_loader/retryer_device/generic_device/generic_device.h>
#include "local_exception.h"
#include "model.h"
#include <deque>
#include <data_loader/transaction_broker/transaction_broker.h>
#include <data_loader/source_loader/userspace_source_loader_interface.h>

namespace data_loader::source_loader::wait_loader
{
    class WaitLoader: public virtual data_loader::source_loader::UserSpaceSourceLoaderInterface
    {
        private:

            std::unique_ptr<data_loader::transaction_broker::TransactionBrokerInterface> broker;
            std::deque<std::string> prefetched_token_vec;
            size_t tx_sz;
            bool was_completed;
            bool was_corrupted;

        public:

            WaitLoader(const WaitLoaderConfig& config): broker(std::make_unique<data_loader::transaction_broker::SourceTransactionBroker>(config.broker_config)),
                                                        prefetched_token_vec(),
                                                        tx_sz(stdx::safe_non_zero_access(config.tx_sz)),
                                                        was_completed(false),
                                                        was_corrupted(false){}

            WaitLoader(const ExternalWaitLoaderConfig& config): WaitLoader(to_internal_wait_loader_config(config)){}

            auto get(common_exception::CancellationTokenInterface& cancellation_token) -> std::optional<std::string>
            {
                if (this->was_corrupted)
                {
                    throw corrupted_loader_error{};
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

            auto is_ready() -> bool
            {
                return true;
            }

        private:

            void refill_prefetched_token_vec(common_exception::CancellationTokenInterface& cancellation_token)
            {
                try
                {
                    while (true)
                    {
                        std::optional<std::vector<std::string>> rs = this->broker->get(this->tx_sz, 
                                                                                       cancellation_token);

                        if (!rs.has_value())
                        {
                            return;
                        }

                        if (rs->empty())
                        {
                            continue;
                        }

                        std::copy(std::make_move_iterator(rs->begin()),
                                  std::make_move_iterator(rs->end()),
                                  std::back_inserter(this->prefetched_token_vec));

                        return;
                    }
                }
                catch (...)
                {
                    this->was_corrupted = true;
                    throw;
                }
            }
    };
}

#endif
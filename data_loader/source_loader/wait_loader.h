#ifndef __DATA_LOADER_WAIT_LOADER_H__
#define __DATA_LOADER_WAIT_LOADER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <data_loader/source/generic_source.h>
#include <data_loader/retryer_device/generic_device.h>
#include <data_loader/exception_base.h>
#include <deque>
#include "source_transaction_broker.h"
#include "userspace_source_loader_interface.h"

namespace data_loader::source_loader::wait_loader
{
    using namespace data_loader::exception_base;

    struct corrupted_loader_error: runtime_error_base
    {
        corrupted_loader_error(): runtime_error_base("bad loader, loader is in corrupted state"){}
    };

    struct WaitLoaderConfig
    {
        uint64_t tx_sz;
        data_loader::source_loader::broker::SourceTransactionBrokerConfig broker_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(tx_sz,
                      broker_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(tx_sz,
                      broker_config);
        }
    };

    class WaitLoader: public virtual data_loader::source_loader::UserSpaceSourceLoaderInterface
    {
        private:

            std::unique_ptr<data_loader::source_loader::broker::TransactionBrokerInterface> broker;
            std::deque<std::string> prefetched_token_vec;
            size_t tx_sz;
            bool was_completed;
            bool was_corrupted;

        public:

            WaitLoader(const WaitLoaderConfig& config): broker(std::make_unique<data_loader::source_loader::broker::SourceTransactionBroker>(config.broker_config)),
                                                        prefetched_token_vec(),
                                                        tx_sz(stdx::safe_non_zero_access(config.tx_sz)),
                                                        was_completed(false),
                                                        was_corrupted(false){}

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
#ifndef __DATA_LOADER_NORMAL_RETRYER_DEVICE_H__
#define __DATA_LOADER_NORMAL_RETRYER_DEVICE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include "retryer_device_interface.h"

#include <data_loader/source_loader/generic_loader.h>
#include <data_loader/exception_base.h>
#include <fire_bandwidth_control/generic_firer.h>

#include <optional>
#include <chrono>
#include <string>
#include <vector>
#include <unordered_set>
#include <thread>
#include <cmath>
#include <exception>

namespace data_loader::retryer_device::normal_device
{
    using namespace data_loader::exception_base;

    struct RetryConfig
    {
        std::chrono::nanoseconds base_wait_time;
        uint32_t exponential_base;
        uint32_t max_retry_count;
        std::optional<std::vector<std::string>> retryable_exception_vec;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(base_wait_time,
                      exponential_base,
                      max_retry_count,
                      retryable_exception_vec);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(base_wait_time,
                      exponential_base,
                      max_retry_count,
                      retryable_exception_vec);
        }
    };

    class RetryerMachine: public virtual RetryerMachineInterface
    {
        private:

            std::chrono::nanoseconds base_wait_time;
            size_t exponential_base;
            size_t retry_idx;
            size_t max_retry_count;
            std::optional<std::unordered_set<std::string>> retryable_exception_vec;
        
            static inline constexpr size_t MIN_EXPONENTIAL_BASE             = 2;
            static inline constexpr size_t MAX_EXPONENTIAL_BASE             = 10;
            static inline constexpr size_t MIN_RETRY_COUNT                  = 0u;
            static inline constexpr size_t MAX_RETRY_COUNT                  = 10;

            static inline const std::chrono::nanoseconds MIN_BASE_WAIT_TIME = std::chrono::nanoseconds(0);
            static inline const std::chrono::nanoseconds MAX_BASE_WAIT_TIME = std::chrono::nanoseconds::max();

            static inline const std::chrono::nanoseconds MIN_BREAK_TIME     = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(10));
            static inline const std::chrono::nanoseconds MAX_BREAK_TIME     = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1));

        public:

            RetryerMachine(const RetryConfig& retry_config)
            {
                if (std::clamp(retry_config.base_wait_time, MIN_BASE_WAIT_TIME, MAX_BASE_WAIT_TIME) != retry_config.base_wait_time)
                {
                    throw invalid_argument_base("bad base wait time, base wait time is not in range, [0, max]");
                }   
                
                if (std::clamp(static_cast<size_t>(retry_config.exponential_base), MIN_EXPONENTIAL_BASE, MAX_EXPONENTIAL_BASE) != retry_config.exponential_base)
                {
                    throw invalid_argument_base("bad exponential base, exponential base is not in range [2, 10]");
                }

                if (std::clamp(static_cast<size_t>(retry_config.max_retry_count), MIN_RETRY_COUNT, MAX_RETRY_COUNT) != retry_config.max_retry_count)
                {
                    throw invalid_argument_base("bad max retry count, max retry count is not in range [0, 10]");
                }

                this->base_wait_time            = retry_config.base_wait_time;
                this->exponential_base          = retry_config.exponential_base;
                this->max_retry_count           = retry_config.max_retry_count;
                this->retryable_exception_vec   = std::nullopt;

                if (retry_config.retryable_exception_vec.has_value())
                {
                    this->retryable_exception_vec = std::unordered_set<std::string>(retry_config.retryable_exception_vec->begin(), retry_config.retryable_exception_vec->end());
                }
            }

            void run(RunnableInterface& runnable,
                     common_exception::CancellationTokenInterface& cancellation_token)
            {
                std::exception_ptr leftover_exception = nullptr;

                for (size_t i = 0u; i < this->max_retry_count + 1u; ++i)
                {
                    try
                    {
                        runnable.run(cancellation_token);
                    }
                    catch (data_loader::exception_base::retryable_error& err)
                    {
                        if (!this->is_user_defined_retryable_error(std::current_exception()))
                        {
                            throw;
                        }

                        leftover_exception = std::current_exception();
                        this->sleep_at_index(i, cancellation_token);

                        continue;
                    }
                    catch (std::exception& e)
                    {
                        throw;
                    }

                    return;
                }

                std::rethrow_exception(leftover_exception);
            }
        
        private:

            auto is_user_defined_retryable_error(std::exception_ptr err) -> bool
            {
                if (!this->retryable_exception_vec.has_value())
                {
                    return true;
                }

                std::string_view err_id = data_loader::exception_base::get_exception_id(err);

                if (this->retryable_exception_vec->contains(std::string(err_id)))
                {
                    return true;
                }

                return false;
            }

            auto get_sleep_duration_at_index(size_t i) -> std::chrono::nanoseconds
            {
                return this->base_wait_time * static_cast<size_t>(std::pow(this->exponential_base, i));
            }

            auto get_break_duration(std::chrono::nanoseconds sleep_dur) -> std::chrono::nanoseconds
            {
                return std::clamp(sleep_dur, MIN_BREAK_TIME, MAX_BREAK_TIME);
            }

            void sleep_at_index(size_t i,
                                common_exception::CancellationTokenInterface& cancellation_token)
            {
                std::chrono::nanoseconds sleep_dur  = this->get_sleep_duration_at_index(i);
                std::chrono::nanoseconds break_dur  = this->get_break_duration(sleep_dur);

                std::chrono::time_point<std::chrono::steady_clock> since = std::chrono::steady_clock::now();

                while (true)
                {
                    if (cancellation_token.is_canceled())
                    {
                        common_exception::throw_exception(common_exception::OPERATION_CANCELED_ERROR);
                    }

                    std::chrono::time_point<std::chrono::steady_clock> now  = std::chrono::steady_clock::now();
                    std::chrono::nanoseconds lapsed                         = std::chrono::duration_cast<std::chrono::nanoseconds>(now - since);

                    if (lapsed >= sleep_dur)
                    {
                        break;
                    }

                    std::this_thread::sleep_for(break_dur);
                }
            }
    };
}

#endif

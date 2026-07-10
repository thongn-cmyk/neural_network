#ifndef __DATA_LOADER_RETRYER_DEVICE_NORMAL_DEVICE_CONFIG_BUILDER_H__
#define __DATA_LOADER_RETRYER_DEVICE_NORMAL_DEVICE_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include <chrono>
#include <string>
#include <stl_extension/stdx.h>
#include <variant>
#include "model.h"
#include <string_view>

namespace data_loader::retryer_device::normal_device
{
    class RetryerMachineConfigBuilder
    {
        private:

            std::chrono::nanoseconds base_retry_dur;
            std::chrono::nanoseconds max_retry_dur;
            double exp_base;
            uint64_t retry_count;
            std::optional<std::vector<std::string>> retryable_exception_vec;

            static inline const std::chrono::nanoseconds DEFAULT_BASE_RETRY_DUR = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1));
            static inline const std::chrono::nanoseconds DEFAULT_MAX_RETRY_DUR  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(4));
            static inline constexpr double DEFAULT_EXP_BASE                     = 2;
            static inline constexpr size_t DEFAULT_RETRY_COUNT                  = 4;

            using self  = RetryerMachineConfigBuilder;

        public:

            RetryerMachineConfigBuilder(): base_retry_dur(DEFAULT_BASE_RETRY_DUR),
                                           max_retry_dur(DEFAULT_MAX_RETRY_DUR),
                                           exp_base(DEFAULT_EXP_BASE),
                                           retry_count(DEFAULT_RETRY_COUNT),
                                           retryable_exception_vec(std::nullopt){}

            auto set_base_retry_duration(std::chrono::nanoseconds base_retry_dur) -> self&
            {
                this->base_retry_dur = base_retry_dur;

                return *this;
            }

            auto set_max_retry_duration(std::chrono::nanoseconds max_retry_dur) -> self&
            {
                this->max_retry_dur = max_retry_dur;

                return *this;
            }

            auto set_exponential_base(double exp_base) -> self&
            {
                this->exp_base  = exp_base;

                return *this;
            }

            auto set_retry_count(size_t retry_count) -> self&
            {
                this->retry_count   = stdx::throw_integer_cast<uint64_t>(retry_count);

                return *this;
            }

            auto add_retryable_exception(const std::string& retryable_exception) -> self&
            {
                if (!this->retryable_exception_vec.has_value())
                {
                    this->retryable_exception_vec   = std::vector<std::string>();
                }

                this->retryable_exception_vec->push_back(retryable_exception);

                return *this;
            }

            auto build() -> data_loader::retryer_device::normal_device::ExternalRetryConfig
            {
                return this->get_external_exponential_retry_config();
            }

        private:

            auto get_internal_exponential_retry_config() -> data_loader::retryer_device::normal_device::RetryConfig
            {
                return
                {
                    .base_wait_time             = this->base_retry_dur,
                    .max_wait_time              = this->max_retry_dur,
                    .exponential_base           = this->exp_base,
                    .max_retry_count            = this->retry_count,
                    .retryable_exception_vec    = this->retryable_exception_vec
                };
            }

            auto get_external_exponential_retry_config() -> data_loader::retryer_device::normal_device::ExternalRetryConfig
            {
                return data_loader::retryer_device::normal_device::to_external_retry_config
                (
                    this->get_internal_exponential_retry_config()
                );
            }
    };
}

#endif
#ifndef __DATA_LOADER_CONFIG_BUILDER_S3_LOADER_CONFIG_BUILDER_H__
#define __DATA_LOADER_CONFIG_BUILDER_S3_LOADER_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include <data_loader/source_loader/multisource_loader/config_builder.h>
#include <stl_extension/stdx.h>
#include <chrono>
#include <string>
#include <optional>

namespace data_loader::config_builder
{
    class S3LoaderConfigBuilder
    {
        private:

            using self = S3LoaderConfigBuilder;

            data_loader::source_loader::multisource_loader::MultisourceLoaderConfigBuilder config_builder;

        public:

            auto set_region(const std::string& region) -> self&
            {
                this->get_s3_loader_config_builder()
                    .set_region(region);

                return *this;
            }

            auto set_credential(const std::string& access_key_id,
                                const std::string& secret_key) -> self&
            {
                this->get_s3_loader_config_builder()
                    .set_credential(access_key_id, secret_key);

                return *this;
            }

            auto set_credential(const std::string& access_key_id,
                                const std::string& secret_key,
                                const std::string& session_token) -> self&
            {
                this->get_s3_loader_config_builder()
                    .set_credential(access_key_id, secret_key, session_token);

                return *this;
            }

            auto set_infinite_retry() -> self&
            {
                this->config_builder.get(0u)
                                    .as_wait_loader()
                                    .get_transaction_broker_config_builder()
                                    .get_retryer_machine_config_builder()
                                    .as_infinite_retry_machine();

                return *this;
            }

            auto set_exponential_retry(std::chrono::nanoseconds base_retry_dur,
                                       std::chrono::nanoseconds max_retry_dur,
                                       double exp_base,
                                       size_t retry_count) -> self&
            {
                this->config_builder.get(0u)
                                    .as_wait_loader()
                                    .get_transaction_broker_config_builder()
                                    .get_retryer_machine_config_builder()
                                    .as_exponential_retry_machine()
                                    .set_base_retry_duration(base_retry_dur)
                                    .set_max_retry_duration(max_retry_dur)
                                    .set_exponential_base(exp_base)
                                    .set_retry_count(retry_count);

                return *this;
            }

            auto set_file_pointer(const std::string& bucket_name,
                                  const std::string& object_key) -> self&
            {
                this->get_s3_loader_config_builder()
                    .set_file_pointer(bucket_name, object_key);

                return *this;
            }

            auto set_token_unit_size(size_t sz) -> self&
            {
                this->get_s3_loader_config_builder()
                    .set_token_unit_size(sz);

                return *this;
            }

            auto set_token_max_unit_size(size_t sz) -> self&
            {
                this->get_s3_loader_config_builder()
                    .set_token_max_unit_size(sz);

                return *this;
            }

            auto set_token_size_per_batch(size_t sz) -> self&
            {
                this->config_builder.get(0u)
                                    .as_wait_loader()
                                    .set_transaction_size(sz);

                return *this;
            }

            auto set_download_size(size_t sz) -> self&
            {
                this->get_s3_loader_config_builder()
                    .set_download_size(sz);

                return *this;
            }

            auto set_token_delimitor(char c) -> self&
            {
                this->get_s3_loader_config_builder()
                    .set_token_delimitor(c);

                return *this;
            }

            auto set_token_eor(char c) -> self&
            {
                this->get_s3_loader_config_builder()
                    .set_token_eor(c);

                return *this;
            }

            auto build() -> data_loader::source_loader::multisource_loader::ExternalMultisourceLoaderConfig
            {
                return this->config_builder.build();
            }

        private:
            
            auto get_s3_loader_config_builder() -> data_loader::source::s3_source::S3LoaderConfigBuilder&
            {
                return this->config_builder.get(0u)
                                           .as_wait_loader()
                                           .get_transaction_broker_config_builder()
                                           .get_reader_config_builder()
                                           .as_s3_loader();
            }
    };
}

#endif
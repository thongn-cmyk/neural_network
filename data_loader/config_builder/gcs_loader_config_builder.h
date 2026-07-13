#ifndef __DATA_LOADER_CONFIG_BUILDER_GCS_LOADER_CONFIG_BUILDER_H__
#define __DATA_LOADER_CONFIG_BUILDER_GCS_LOADER_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include <data_loader/source_loader/multisource_loader/config_builder.h>
#include <stl_extension/stdx.h>
#include <chrono>
#include <string>
#include <optional>

namespace data_loader::config_builder
{
    class GCSLoaderConfigBuilder
    {
        private:

            using self  = GCSLoaderConfigBuilder;

            data_loader::source_loader::multisource_loader::MultisourceLoaderConfigBuilder config_builder;

        public:

            auto set_endpoint(const std::string& endpoint) -> self&
            {
                this->get_gcs_loader_config_builder()
                    .set_endpoint(endpoint);

                return *this;
            }

            auto set_project_id(const std::string& project_id) -> self&
            {
                this->get_gcs_loader_config_builder()
                    .set_project_id(project_id);

                return *this;
            }

            auto set_application_name(const std::string& application_name) -> self&
            {
                this->get_gcs_loader_config_builder()
                    .set_application_name(application_name);

                return *this;
            }

            auto set_environment(const std::string& environment) -> self&
            {
                this->get_gcs_loader_config_builder()
                    .set_environment(environment);

                return *this;
            }

            auto set_credential_by_access_token(const std::string& access_token,
                                                std::chrono::seconds token_lifetime) -> self&
            {
                this->get_gcs_loader_config_builder()
                    .set_credential_by_access_token(access_token, token_lifetime);

                return *this;
            }

            auto set_credential_by_jsonized_service_account(const std::string& json_content) -> self&
            {
                this->get_gcs_loader_config_builder()
                    .set_credential_by_jsonized_service_account(json_content);

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
                this->get_gcs_loader_config_builder()
                    .set_file_pointer(bucket_name, object_key);

                return *this;
            }

            auto set_token_unit_size(size_t sz) -> self&
            {
                this->get_gcs_loader_config_builder()
                    .set_token_unit_size(sz);

                return *this;
            }

            auto set_token_max_unit_size(size_t sz) -> self&
            {
                this->get_gcs_loader_config_builder()
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
                this->get_gcs_loader_config_builder()
                    .set_download_size(sz);

                return *this;
            }

            auto set_token_delimitor(char c) -> self&
            {
                this->get_gcs_loader_config_builder()
                    .set_token_delimitor(c);

                return *this;
            }

            auto set_token_eor(char c) -> self&
            {
                this->get_gcs_loader_config_builder()
                    .set_token_eor(c);

                return *this;
            }

            auto build() -> data_loader::source_loader::multisource_loader::ExternalMultisourceLoaderConfig
            {
                return this->config_builder.build();
            }
        
        private:

            auto get_gcs_loader_config_builder() -> data_loader::source::gcs_source::GCSLoaderConfigBuilder&
            {
                return this->config_builder.get(0u)
                                           .as_wait_loader()
                                           .get_transaction_broker_config_builder()
                                           .get_reader_config_builder()
                                           .as_gcs_loader();
            }            
    };
}

#endif
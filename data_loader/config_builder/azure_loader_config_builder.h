#ifndef __DATA_LOADER_CONFIG_BUILDER_AZURE_LOADER_CONFIG_BUILDER_H__
#define __DATA_LOADER_CONFIG_BUILDER_AZURE_LOADER_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include <data_loader/source_loader/multisource_loader/model.h>
#include <stl_extension/stdx.h>
#include <chrono>
#include <string>
#include <optional>

namespace data_loader::config_builder
{
    class AzureLoaderConfigBuilder
    {
        private:
            
            using self  = AzureLoaderConfigBuilder;

            data_loader::source_loader::multisource_loader::MultisourceLoaderConfigBuilder config_builder;
        
        public:

            auto set_credential_by_connection_string(const std::string& connection_str) -> self&
            {
                this->get_azure_loader_config_builder()
                    .set_credential_by_connection_string(connection_str);

                return *this;
            }

            auto set_credential_by_shared_key(const std::string& account_name,
                                              const std::string& account_key) -> self&
            {
                this->get_azure_loader_config_builder()
                    .set_credential_by_shared_key(account_name, account_key);

                return *this;
            }

            auto set_credential_by_sas_token(const std::string& sas_token) -> self&
            {
                this->get_azure_loader_config_builder()
                    .set_credential_by_sas_token(sas_token);

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

            auto set_blob_uri(const std::string& blob_uri) -> self&
            {
                this->get_azure_loader_config_builder()
                    .set_blob_uri(blob_uri);

                return *this;
            }

            auto set_file_pointer(const std::string& container_name,
                                  const std::string& blob_name) -> self&
            {
                this->get_azure_loader_config_builder()
                    .set_file_pointer(container_name, blob_name);

                return *this;
            }

            auto set_token_unit_size(size_t sz) -> self&
            {
                this->get_azure_loader_config_builder()
                    .set_token_unit_size(sz);

                return *this;
            }

            auto set_token_max_unit_size(size_t sz) -> self&
            {
                this->get_azure_loader_config_builder()
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
                this->get_azure_loader_config_builder()
                    .set_download_size(sz);

                return *this;
            }

            auto set_token_delimitor(char c) -> self&
            {
                this->get_azure_loader_config_builder()
                    .set_token_delimitor(c);

                return *this;
            }

            auto set_token_eor(char c) -> self&
            {
                this->get_azure_loader_config_builder()
                    .set_token_eor(c);

                return *this;
            }

            auto build() -> data_loader::source_loader::multisource_loader::ExternalMultisourceLoaderConfig
            {
                return this->config_builder.build();
            }
        
        private:

            auto get_azure_loader_config_builder() -> data_loader::source::azure_source::AzureLoaderConfigBuilder&
            {
                return this->config_builder.get(0u)
                                           .as_wait_loader()
                                           .get_transaction_broker_config_builder()
                                           .get_reader_config_builder()
                                           .as_azure_loader();
            }
    };
}

#endif
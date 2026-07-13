#ifndef __DATA_LOADER_SOURCE_AZURE_SOURCE_CONFIG_BUILDER_H__
#define __DATA_LOADER_SOURCE_AZURE_SOURCE_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include "model.h"
#include <stl_extension/stdx.h>
#include <chrono>
#include <string>
#include <optional>
#include <data_loader/stream_reader/config_builder.h>
#include <variant>

namespace data_loader::source::azure_source
{
    class AzureLoaderConfigBuilder
    {
        private:

            using self = AzureLoaderConfigBuilder;
            using DelimitedStreamReaderConfigBuilder    = data_loader::stream_reader::DelimitedStreamReaderConfigBuilder;

            GenericAuthConfig auth_config;

            std::optional<std::string> blob_uri;
            std::optional<std::string> container_name;
            std::optional<std::string> blob_name;

            std::optional<uint64_t> token_unit_sz;
            std::optional<uint64_t> download_sz;

            std::unique_ptr<DelimitedStreamReaderConfigBuilder> delimited_stream_reader_config_builder;

        public:

            AzureLoaderConfigBuilder(): auth_config(),
                                        blob_uri(),
                                        container_name(),
                                        blob_name(),
                                        token_unit_sz(),
                                        download_sz(),
                                        delimited_stream_reader_config_builder(std::make_unique<DelimitedStreamReaderConfigBuilder>()){}

            auto set_credential_by_connection_string(const std::string& connection_str) -> self&
            {
                this->auth_config.auth_config   = ConnectionStringAuthConfig
                {
                    .connection_str = connection_str
                };

                return *this;
            }

            auto set_credential_by_shared_key(const std::string& account_name,
                                              const std::string& account_key) -> self&
            {
                this->auth_config.auth_config   = SharedKeyAuthConfig
                {
                    .account_name   = account_name,
                    .account_key    = account_key
                }; 

                return *this;
            }

            auto set_credential_by_sas_token(const std::string& sas_token) -> self&
            {
                this->auth_config.auth_config   = SASTokenAuthConfig
                {
                    .sas_token  = sas_token
                };

                return *this;
            }

            auto set_blob_uri(const std::string& blob_uri) -> self&
            {
                this->blob_uri  = blob_uri;

                return *this;
            }

            auto set_file_pointer(const std::string& container_name,
                                  const std::string& blob_name) -> self&
            {
                this->container_name    = container_name;
                this->blob_name         = blob_name;

                return *this;
            }

            auto set_token_unit_size(size_t sz) -> self&
            {
                this->token_unit_sz = stdx::throw_integer_cast<uint64_t>(sz);

                return *this;
            }

            auto set_token_max_unit_size(size_t sz) -> self&
            {
                this->delimited_stream_reader_config_builder->set_max_size_per_token(sz);

                return *this;
            }

            auto set_download_size(size_t sz) -> self&
            {
                this->download_sz   = stdx::throw_integer_cast<uint64_t>(sz);

                return *this;
            }

            auto set_token_delimitor(char c) -> self&
            {
                this->delimited_stream_reader_config_builder->set_token_delimitor(c);

                return *this;
            }

            auto set_token_eor(char c) -> self&
            {
                this->delimited_stream_reader_config_builder->set_token_eor(c);

                return *this;
            }

            auto build() -> ExternalAzureLoaderConfig
            {
                return this->get_external_azure_loader_config();
            }

        private:

            auto get_service_url() -> std::string
            {
                if (!this->blob_uri.has_value())
                {
                    throw std::invalid_argument("bad blob uri, not set");
                }

                if (!this->container_name.has_value())
                {
                    throw std::invalid_argument("bad container name, not set");
                }

                return this->blob_uri.value() + "/" + this->container_name.value();
            }

            auto get_internal_secured_azure_client_config() -> SecuredAzureClientConfig
            {
                if (std::holds_alternative<stdx::reflectible_monostate>(this->auth_config.auth_config))
                {
                    throw std::invalid_argument("bad auth config, not set");
                }

                if (this->auth_config.auth_config.valueless_by_exception())
                {
                    throw std::invalid_argument("bad auth config, valueless");
                }

                return
                {
                    .service_ep_url = this->get_service_url(),
                    .auth_config    = this->auth_config
                };
            }

            auto get_external_secured_azure_client_config() -> ExternalSecuredAzureClientConfig
            {
                return to_external_secured_azure_client_config(this->get_internal_secured_azure_client_config());
            }

            auto get_internal_azure_loader_config() -> AzureLoaderConfig
            {
                if (!this->container_name.has_value())
                {
                    throw std::invalid_argument("bad container name, not set");
                }

                if (!this->blob_name.has_value())
                {
                    throw std::invalid_argument("bad blob name, not set");
                }

                return
                {
                    .delim_config               = this->delimited_stream_reader_config_builder->build(),
                    .service_client_config      = this->get_external_secured_azure_client_config(),
                    .container_name             = this->container_name.value(),
                    .blob_name                  = this->blob_name.value(),
                    .read_ahead_buffer_sz_hint  = this->download_sz,
                    .unit_byte_sz_hint          = this->token_unit_sz
                };
            }

            auto get_external_azure_loader_config() -> ExternalAzureLoaderConfig
            {
                return to_external_azure_loader_config(this->get_internal_azure_loader_config());
            }
    };
}

#endif
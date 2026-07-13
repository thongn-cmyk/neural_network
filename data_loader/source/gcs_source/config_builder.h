#ifndef __DATA_LOADER_SOURCE_GCS_SOURCE_CONFIG_BUILDER_H__
#define __DATA_LOADER_SOURCE_GCS_SOURCE_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include "model.h"
#include <stl_extension/stdx.h>
#include <chrono>
#include <string>
#include <optional>
#include <data_loader/stream_reader/config_builder.h>

namespace data_loader::source::gcs_source
{
    class GCSLoaderConfigBuilder
    {
        private:

            using self                                  = GCSLoaderConfigBuilder;
            using DelimitedStreamReaderConfigBuilder    = data_loader::stream_reader::DelimitedStreamReaderConfigBuilder;

            std::optional<std::string> bucket_name;
            std::optional<std::string> object_key;

            std::optional<uint64_t> token_unit_sz;
            std::optional<uint64_t> download_sz;

            std::unique_ptr<DelimitedStreamReaderConfigBuilder> delimited_stream_reader_config_builder;
            data_loader::source::gcs_source::SecuredGCSClientConfig gcs_client_config;

        public:

            GCSLoaderConfigBuilder(): bucket_name(),
                                      object_key(),
                                      token_unit_sz(),
                                      download_sz(),
                                      delimited_stream_reader_config_builder(std::make_unique<DelimitedStreamReaderConfigBuilder>()),
                                      gcs_client_config(){}

            auto set_endpoint(const std::string& endpoint) -> self&
            {
                this->gcs_client_config.endpoint_config.endpoint = endpoint;

                return *this;
            }

            auto set_file_pointer(const std::string& bucket_name,
                                  const std::string& object_key) -> self&
            {
                this->bucket_name   = bucket_name;
                this->object_key    = object_key;

                return *this;
            }

            auto set_project_id(const std::string& project_id) -> self&
            {
                this->gcs_client_config.project_id = project_id;

                return *this;
            }

            auto set_application_name(const std::string& application_name) -> self&
            {
                this->gcs_client_config.application_name = application_name;

                return *this;
            }

            auto set_environment(const std::string& environment) -> self&
            {
                this->gcs_client_config.environment = environment;

                return *this;
            }

            auto set_credential_by_access_token(const std::string& access_token,
                                                std::chrono::seconds token_lifetime) -> self&
            {
                this->gcs_client_config.credential = data_loader::source::gcs_source::GenericCredential
                {
                    .credential = data_loader::source::gcs_source::AccessTokenCredential
                    {
                        .access_token   = access_token,
                        .token_lifetime = token_lifetime
                    }
                };

                return *this;
            }

            auto set_credential_by_jsonized_service_account(const std::string& json_content) -> self&
            {
                this->gcs_client_config.credential = data_loader::source::gcs_source::GenericCredential
                {
                    .credential = data_loader::source::gcs_source::ServiceAccountJsonCredential
                    {
                        .json_content   = json_content
                    }
                };

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

            auto build() -> ExternalGCSLoaderConfig
            {
                return this->get_external_gcs_loader_config();
            }
        
        private:

            auto get_internal_gcs_loader_config() -> data_loader::source::gcs_source::GCSLoaderConfig
            {
                if (!this->bucket_name.has_value())
                {
                    throw std::invalid_argument("bad resource pointer, bucket name not set");
                }

                if (!this->object_key.has_value())
                {
                    throw std::invalid_argument("bad resource pointer, object key not set");
                }

                return
                {
                    .delim_config               = this->delimited_stream_reader_config_builder->build(),
                    .gcs_client_config          = to_external_secured_gcs_client_config(this->gcs_client_config),
                    .bucket_name                = this->bucket_name.value(),
                    .object_key                 = this->object_key.value(),
                    .read_ahead_buffer_sz_hint  = this->download_sz,
                    .unit_byte_sz_hint          = this->token_unit_sz
                };
            }

            auto get_external_gcs_loader_config() -> data_loader::source::gcs_source::ExternalGCSLoaderConfig
            {
                return data_loader::source::gcs_source::to_external_gcs_loader_config
                (
                    this->get_internal_gcs_loader_config()
                );
            }
    };
}

#endif
#ifndef __DATA_LOADER_SOURCE_S3_SOURCE_CONFIG_BUILDER_H__
#define __DATA_LOADER_SOURCE_S3_SOURCE_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include "model.h"
#include <stl_extension/stdx.h>
#include <chrono>
#include <string>
#include <optional>
#include <data_loader/stream_reader/config_builder.h>

namespace data_loader::source::s3_source
{
    class S3LoaderConfigBuilder
    {
        private:

            using self                                  = S3LoaderConfigBuilder;
            using DelimitedStreamReaderConfigBuilder    = data_loader::stream_reader::DelimitedStreamReaderConfigBuilder;
 
            GenericCredential cred;

            std::optional<std::string> region;
            std::optional<std::string> bucket_name;
            std::optional<std::string> object_key;

            std::optional<uint64_t> token_unit_sz;
            std::optional<uint64_t> download_sz;

            std::unique_ptr<DelimitedStreamReaderConfigBuilder> delimited_stream_reader_config_builder;

        public:

            S3LoaderConfigBuilder(): cred(),
                                     region(),
                                     bucket_name(),
                                     object_key(),
                                     token_unit_sz(),
                                     download_sz(),
                                     delimited_stream_reader_config_builder(std::make_unique<DelimitedStreamReaderConfigBuilder>()){}

            auto set_region(const std::string& region) -> self&
            {
                this->region = region;

                return *this;
            }

            auto set_credential(const std::string& access_key_id,
                                const std::string& secret_key) -> self&
            {
                this->cred.credential   = Credential_0
                {
                    .access_key_id  = access_key_id,
                    .secret_key     = secret_key
                };

                return *this;
            }

            auto set_credential(const std::string& access_key_id,
                                const std::string& secret_key,
                                const std::string& session_token) -> self&
            {
                this->cred.credential   = Credential_1
                {
                    .access_key_id  = access_key_id,
                    .secret_key     = secret_key,
                    .session_token  = session_token
                };

                return *this;
            }

            auto set_file_pointer(const std::string& bucket_name,
                                  const std::string& object_key) -> self&
            {
                this->bucket_name   = bucket_name;
                this->object_key    = object_key;

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
                this->download_sz = stdx::throw_integer_cast<uint64_t>(sz);

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

            auto build() -> ExternalS3LoaderConfig
            {
                return this->get_external_s3_loader_config();
            }

        private:

            auto get_s3_client_configuration() -> data_loader::source::s3_source::S3ClientConfiguration_2
            {
                return
                {
                    .region = this->region
                };
            }

            auto get_valid_s3_generic_credential() -> data_loader::source::s3_source::GenericCredential
            {
                if (std::holds_alternative<stdx::reflectible_monostate>(this->cred.credential))
                {
                    throw std::invalid_argument("bad credential, not set");
                }

                if (this->cred.credential.valueless_by_exception())
                {
                    throw std::invalid_argument("bad credential, bad variant");
                }

                return this->cred;
            }

            auto get_internal_secured_s3_client_config() -> data_loader::source::s3_source::SecuredS3ClientConfiguration
            {
                return
                {
                    .client_config  = this->get_s3_client_configuration(),
                    .credential     = this->get_valid_s3_generic_credential()
                };
            }

            auto get_external_secured_s3_client_config() -> data_loader::source::s3_source::ExternalSecuredS3ClientConfiguration
            {
                return data_loader::source::s3_source::to_external_secured_s3_client_configuration
                (
                    this->get_internal_secured_s3_client_config()
                );
            }

            auto get_internal_s3_loader_config() -> data_loader::source::s3_source::S3LoaderConfig
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
                    .s3_client_config           = this->get_external_secured_s3_client_config(),
                    .bucket_name                = this->bucket_name.value(),
                    .object_key                 = this->object_key.value(),
                    .read_ahead_buffer_sz_hint  = this->download_sz,
                    .unit_byte_sz_hint          = this->token_unit_sz
                };
            }

            auto get_external_s3_loader_config() -> data_loader::source::s3_source::ExternalS3LoaderConfig
            {
                return data_loader::source::s3_source::to_external_s3_loader_config
                (
                    this->get_internal_s3_loader_config()
                );
            }
    };
}

#endif
#ifndef __DATA_LOADER_CONFIG_BUILDER_S3_SOURCE_CONFIG_BUILDER_H__
#define __DATA_LOADER_CONFIG_BUILDER_S3_SOURCE_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
// #include <data_loader/retryer_device/generic_device/model.h>
// #include <data_loader/source/generic_source/model.h>
#include <data_loader/source_loader/multisource_loader/model.h>
#include <stl_extension/stdx.h>
#include <chrono>
#include <string>
#include <optional>

namespace data_loader::config_builder
{
    class S3SourceLoaderConfigBuilder //?
    {
        private:

            struct CredentialOption0
            {
                std::string access_key_id;
                std::string secret_key;
            };

            struct CredentialOption1
            {
                std::string access_key_id;
                std::string secret_key;
                std::string session_token;
            };

            struct ResourcePointer
            {
                std::string bucket_name;
                std::string object_key;
            };

            std::optional<std::string> region;
            std::variant<stdx::reflectible_monostate, CredentialOption0, CredentialOption1> cred;

            data_loader::retryer_device::generic_device::GenericRetryerMachineConfigBuilder retryer_machine_config_builder;
            data_loader::stream_reader::DelimitedStreamReaderConfigBuilder delimited_stream_reader_config_builder;

            std::optional<ResourcePointer> resource_pointer;
            std::optional<uint64_t> token_unit_sz;
            std::optional<uint64_t> token_sz_per_batch;

            static inline constexpr std::optional<uint64_t> DEFAULT_TOKEN_UNIT_SZ           = uint64_t{1} << 10;
            static inline constexpr std::optional<uint64_t> DEFAULT_TOKEN_SZ_PER_BATCH      = uint64_t{1} << 10;

        public:

            S3SourceLoaderConfigBuilder(): region(std::nullopt),
                                           cred(),
                                           retryer_machine_config_builder(),
                                           delimited_stream_reader_config_builder(),
                                           resource_pointer(std::nullopt),
                                           token_unit_sz(DEFAULT_TOKEN_UNIT_SZ)
                                           token_sz_per_batch(DEFAULT_TOKEN_SZ_PER_BATCH){}

            auto set_region(const std::string& region) -> S3SourceLoaderConfigBuilder&
            {
                this->region = region;

                return *this;
            }

            auto set_credential(const std::string& access_key_id,
                                const std::string& secret_key) -> S3SourceLoaderConfigBuilder&
            {
                this->cred  = CredentialOption0
                {
                    .access_key_id  = access_key_id,
                    .secret_key     = secret_key
                };

                return *this;
            }

            auto set_credential(const std::string& access_key_id,
                                const std::string& secret_key,
                                const std::string& session_token) -> S3SourceLoaderConfigBuilder&
            {
                this->cred  = CredentialOption1
                {
                    .access_key_id  = access_key_id,
                    .secret_key     = secret_key,
                    .session_token  = session_token
                };

                return *this;
            }

            auto set_infinite_retry() -> S3SourceLoaderConfigBuilder&
            {
                this->retryer_machine_config_builder.as_infinite_retry_machine();

                return *this;
            }

            auto set_exponential_retry(std::chrono::nanoseconds base_retry_dur,
                                       std::chrono::nanoseconds max_retry_dur,
                                       double exp_base,
                                       size_t retry_count) -> S3SourceLoaderConfigBuilder&
            {
                this->retryer_machine_config_builder.as_exponential_retry_machine().set_base_retry_duration(base_retry_dur)
                                                                                   .set_max_retry_duration(max_retry_dur)
                                                                                   .set_exponential_base(exp_base)
                                                                                   .set_retry_count(retry_count);

                return *this;
            }

            auto set_file_pointer(const std::string& bucket_name,
                                  const std::string& object_key) -> S3SourceLoaderConfigBuilder&
            {
                this->resource_pointer  = ResourcePointer
                {
                    .bucket_name    = bucket_name,
                    .object_key     = object_key
                };

                return *this;
            }

            auto set_token_unit_size(size_t sz) -> S3SourceLoaderConfigBuilder&
            {
                this->token_unit_sz = stdx::throw_integer_cast<uint64_t>(sz);

                return *this;
            }

            auto set_token_max_unit_size(size_t sz) -> S3SourceLoaderConfigBuilder&
            {
                this->delimited_stream_reader_config_builder.set_max_size_per_token(stdx::throw_integer_cast<uint64_t>(sz));

                return *this;
            }

            auto set_token_size_per_batch(size_t sz) -> S3SourceLoaderConfigBuilder&
            {
                this->token_sz_per_batch    = stdx::throw_integer_cast<uint64_t>(sz);

                return *this;
            }

            auto set_token_delimitor(char c) -> S3SourceLoaderConfigBuilder&
            {
                this->delimited_stream_reader_config_builder.set_token_delimitor(c);

                return *this;
            }

            auto set_token_eor(char c) -> S3SourceLoaderConfigBuilder&
            {
                this->delimited_stream_reader_config_builder.set_token_eor(c);

                return *this;
            }

            auto build() -> data_loader::source_loader::multisource_loader::ExternalMultisourceLoaderConfig
            {
                return data_loader::source_loader::multisource_loader::to_external_multisource_loader_config
                (
                    this->get_internal_multisource_loader_config()
                );
            }

        private:

            // auto get_s3_client_configuration() -> data_loader::source::s3_source::S3ClientConfiguration_2
            // {
            //     return
            //     {
            //         .region = this->region
            //     };
            // }

            // auto get_s3_generic_credential() -> data_loader::source::s3_source::GenericCredential
            // {
            //     if (std::holds_alternative<CredentialOption0>(this->cred))
            //     {
            //         auto& obj_reference = std::get<CredentialOption0>(this->cred);

            //         return
            //         {
            //             .credential = data_loader::source::s3_source::Credential_0
            //             {
            //                 .access_key_id  = obj_reference.access_key_id,
            //                 .secret_key     = obj_reference.secret_key
            //             }
            //         };
            //     }
            //     else if (std::holds_alternative<CredentialOption1>(this->cred))
            //     {
            //         auto& obj_reference = std::get<CredentialOption1>(this->cred);

            //         return
            //         {
            //             .credential = data_loader::source::s3_source::Credential_1
            //             {
            //                 .access_key_id  = obj_reference.access_key_id,
            //                 .secret_key     = obj_reference.secret_key,
            //                 .session_token  = obj_reference.session_token
            //             }
            //         };
            //     }
            //     else
            //     {
            //         throw std::invalid_argument("bad credential option, enumeration out of range");
            //     }
            // }

            // auto get_internal_secured_s3_client_config() -> data_loader::source::s3_source::SecuredS3ClientConfiguration
            // {
            //     return
            //     {
            //         .client_config  = this->get_s3_client_configuration(),
            //         .credential     = this->get_s3_generic_credential()
            //     };
            // }

            // auto get_external_secured_s3_client_config() -> data_loader::source::s3_source::ExternalSecuredS3ClientConfiguration
            // {
            //     return data_loader::source::s3_source::to_external_secured_s3_client_configuration
            //     (
            //         this->get_internal_secured_s3_client_config()
            //     );
            // }

            // auto get_internal_s3_loader_config() -> data_loader::source::s3_source::S3LoaderConfig
            // {
            //     if (!this->resource_pointer.has_value())
            //     {
            //         throw std::invalid_argument("bad resource pointer, not set");
            //     }

            //     return
            //     {
            //         .delim_config               = this->delimited_stream_reader_config_builder.build(),
            //         .s3_client_config           = this->get_external_secured_s3_client_config(),
            //         .bucket_name                = this->resource_pointer->bucket_name,
            //         .object_key                 = this->resource_pointer->object_key,
            //         .read_ahead_buffer_sz_hint  = std::nullopt,
            //         .unit_byte_sz_hint          = this->token_unit_sz
            //     };
            // }

            // auto get_external_s3_loader_config() -> data_loader::source::s3_source::ExternalS3LoaderConfig
            // {
            //     return data_loader::source::s3_source::to_external_s3_loader_config
            //     (
            //         this->get_internal_s3_loader_config()
            //     );
            // }

            auto get_internal_generic_reader_config() -> data_loader::source::generic_source::GenericReaderConfig
            {
                return
                {
                    .source = this->get_external_s3_loader_config()
                };
            }

            auto get_external_generic_reader_config() -> data_loader::source::generic_source::ExternalGenericReaderConfig
            {
                return data_loader::source::generic_source::to_external_generic_reader_config
                (
                    this->get_internal_generic_reader_config()
                );
            }

            auto get_internal_source_transaction_broker_config() -> data_loader::transaction_broker::SourceTransactionBrokerConfig
            {
                return
                {
                    .source_config  = this->get_external_generic_reader_config(),
                    .retry_config   = this->retryer_machine_config_builder.build()
                };
            }

            auto get_external_source_transaction_broker_config() -> data_loader::transaction_broker::ExternalSourceTransactionBrokerConfig
            {
                return data_loader::transaction_broker::to_external_source_transaction_broker_config
                (
                    this->get_internal_source_transaction_broker_config()
                );
            }

            auto get_internal_wait_loader_config() -> data_loader::source_loader::wait_loader::WaitLoaderConfig
            {
                if (!this->token_sz_per_batch.has_value())
                {
                    throw std::invalid_argument("bad token size per batch, not set")
                }

                if (this->token_sz_per_batch.value() == 0u)
                {
                    throw std::invalid_argument("bad token size per batch, 0");
                }

                return
                {
                    .tx_sz          = this->token_sz_per_batch.value(),
                    .broker_config  = this->get_external_source_transaction_broker_config()
                };
            }

            auto get_external_wait_loader_config() -> data_loader::source_loader::wait_loader::ExternalWaitLoaderConfig
            {
                return data_loader::source_loader::wait_loader::to_external_wait_loader_config
                (
                    this->get_internal_wait_loader_config()
                );
            }

            auto get_internal_generic_loader_config() -> data_loader::source_loader::generic_loader::GenericLoaderConfig
            {
                return
                {
                    .config = get_external_wait_loader_config()
                };
            }

            auto get_external_generic_loader_config() -> data_loader::source_loader::generic_loader::ExternalGenericLoaderConfig
            {
                return data_loader::source_loader::generic_loader::to_external_generic_loader_config
                (
                    this->get_internal_generic_loader_config()
                );
            }

            auto get_internal_multisource_loader_config() -> data_loader::source_loader::multisource_loader::MultisourceLoaderConfig
            {
                return
                {
                    .config_vec = {get_external_generic_loader_config()}
                };
            }
    };
}

#endif
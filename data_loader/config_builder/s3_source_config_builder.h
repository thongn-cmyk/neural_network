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
    class S3SourceConfigBuilder
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

            struct RetryOptionInfinite{};

            struct RetryOptionExponential
            {
                std::chrono::nanoseconds base_retry_dur;
                std::chrono::nanoseconds max_retry_dur;
                double exp_base;
                size_t retry_count;
            };

            struct ResourcePointer
            {
                std::string bucket_name;
                std::string object_key;
            };

            std::optional<std::string> region;
            std::variant<stdx::reflectible_monostate, CredentialOption0, CredentialOption1> cred;
            std::variant<stdx::reflectible_monostate, RetryOptionInfinite, RetryOptionExponential> retry_option;
            std::optional<ResourcePointer> resource_pointer;
            std::optional<size_t> token_unit_sz;
            std::optional<size_t> token_max_unit_sz;
            std::optional<size_t> token_sz_per_batch;
            std::optional<char> token_delim;
            std::optional<char> token_eor;

            static inline constexpr std::optional<size_t> DEFAULT_TOKEN_UNIT_SZ         = size_t{1} << 10;
            static inline constexpr std::optional<size_t> DEFAULT_TOKEN_MAX_UNIT_SZ     = std::nullopt;
            static inline constexpr std::optional<size_t> DEFAULT_TOKEN_SZ_PER_BATCH    = size_t{1} << 10;
            static inline constexpr std::optional<char> DEFAULT_TOKEN_DELIM             = ',';
            static inline constexpr std::optional<char> DEFAULT_TOKEN_EOR               = '\0';

            static inline constexpr RetryOptionInfinite DEFAULT_RETRY_OPTION            = {};

        public:

            S3SourceConfigBuilder(): region(std::nullopt),
                                     cred(),
                                     retry_option(DEFAULT_RETRY_OPTION),
                                     token_unit_sz(DEFAULT_TOKEN_UNIT_SZ)
                                     token_max_unit_sz(DEFAULT_TOKEN_MAX_UNIT_SZ),
                                     token_sz_per_batch(DEFAULT_TOKEN_SZ_PER_BATCH),
                                     token_delim(DEFAULT_TOKEN_DELIM),
                                     token_eor(DEFAULT_TOKEN_EOR){}

            auto set_region(const std::string& region) -> S3SourceConfigBuilder&
            {
                this->region = region;

                return *this;
            }

            auto set_credential(const std::string& access_key_id,
                                const std::string& secret_key) -> S3SourceConfigBuilder&
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
                                const std::string& session_token) -> S3SourceConfigBuilder&
            {
                this->cred  = CredentialOption1
                {
                    .access_key_id  = access_key_id,
                    .secret_key     = secret_key,
                    .session_token  = session_token
                };

                return *this;
            }

            auto set_infinite_retry() -> S3SourceConfigBuilder&
            {
                this->retry_option  = RetryOptionInfinite{};

                return *this;
            }

            auto set_exponential_retry(std::chrono::nanoseconds base_retry_dur,
                                       std::chrono::nanoseconds max_retry_dur,
                                       double exp_base,
                                       size_t retry_count) -> S3SourceConfigBuilder&
            {
                this->retry_option  = RetryOptionExponential
                {
                    .base_retry_dur = base_retry_dur,
                    .max_retry_dur  = max_retry_dur,
                    .exp_base       = exp_base,
                    .retry_count    = retry_count
                };

                return *this;
            }

            auto set_file_pointer(const std::string& bucket_name,
                                  const std::string& object_key) -> S3SourceConfigBuilder&
            {
                this->resource_pointer  = ResourcePointer
                {
                    .bucket_name    = bucket_name,
                    .object_key     = object_key
                };

                return *this;
            }

            auto set_token_unit_size(size_t sz) -> S3SourceConfigBuilder&
            {
                this->token_unit_sz = sz;

                return *this;
            }

            auto set_token_max_unit_size(size_t sz) -> S3SourceConfigBuilder&
            {
                this->token_max_unit_sz = sz;

                return *this;
            }

            auto set_token_size_per_batch(size_t sz) -> S3SourceConfigBuilder&
            {
                this->token_sz_per_batch    = sz;

                return *this;
            }

            auto set_token_delimitor(char c) -> S3SourceConfigBuilder&
            {
                this->token_delim   = c;

                return *this;
            }

            auto set_token_eor(char c) -> S3SourceConfigBuilder&
            {
                this->token_eor = c;

                return *this;
            }

            auto build() -> data_loader::source_loader::multisource_loader::ExternalMultisourceLoaderConfig
            {
                return {};
            }
    };
}

#endif
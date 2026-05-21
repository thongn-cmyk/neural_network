#ifndef __DATA_LOADER_SOURCE_GCS_SOURCE_CLIENT_BUILDER_H__
#define __DATA_LOADER_SOURCE_GCS_SOURCE_CLIENT_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include "model.h"
#include <string>
#include <cstring>
#include <optional>
#include <chrono>
#include <algorithm>
#include <functional>
#include <memory>
#include <stl_extension/stdx.h>
#include <cstdlib>

namespace data_loader::gcs_source
{
    namespace gc    = ::google::cloud;
    namespace gcs   = ::google::cloud::storage;

    class GCSClientBuilder
    {
        private:

            std::optional<SecuredGCSClientConfig> config;

        public:

            auto set(const SecuredGCSClientConfig& config) -> GCSClientBuilder&
            {
                this->config = config;

                return *this;
            }

            auto build() -> std::unique_ptr<gcs::Client>
            {
                return std::make_unique<gcs::Client>(this->get_options());
            }

        private:

            auto get_options() -> gcs::Options
            {
                gcs::Options rs{};

                this->set_endpoint_options(rs);
                this->set_tls_options(rs);
                this->set_credential_options(rs);
                this->set_encryption_options(rs);
                this->set_upload_options(rs);
                this->set_download_options(rs);
                this->set_client_primitives_options(rs);

                return rs;
            }

            static void fix_endpoint_prefix(std::string& ep, uint8_t ep_scheme)
            {
                auto get_endpoint_without_prefix = [](const std::string& ep_arg)
                {
                    if (ep_arg.starts_with("http://"))
                    {
                        return std::string(std::next(ep_arg.begin(), std::string("http://").size()), ep_arg.end());
                    }

                    if (ep_arg.starts_with("https://"))
                    {
                        return std::string(std::next(ep_arg.begin(), std::string("https://").size()), ep_arg.end());
                    }

                    return ep_arg;
                };

                switch (ep_scheme)
                {
                    case ENDPOINT_SCHEME_K_HTTP:
                    {
                        ep = "http://" + get_endpoint_without_prefix(ep);
                        break;
                    }
                    case ENDPOINT_SCHEME_K_HTTPS:
                    {
                        ep = "https://" + get_endpoint_without_prefix(ep);
                        break;
                    }
                    default:
                    {
                        throw std::invalid_argument("bad endpoint scheme option, enumeration value out of range");
                    }
                }
            }

            void set_endpoint_options(gcs::Options& options)
            {
                if (!this->config.has_value())
                {
                    return;
                }

                if (this->config->endpoint_config.endpoint.has_value())
                {
                    std::string ep = this->config->endpoint_config.endpoint.value();

                    if (this->config->endpoint_config.endpoint_scheme.has_value())
                    {
                        fix_endpoint_prefix(ep, this->config->endpoint_config.endpoint_scheme.value());
                    }

                    options.template set<gcs::RestEndpointOption>(ep);
                }
            }

            void set_tls_options(gcs::Options& options)
            {
                if (!this->config.has_value())
                {
                    return;
                }

                if (this->config->tls_config.ca_file_path.has_value())
                {
                    options.template set<google::cloud::CARootsFilePathOption>(this->config->tls_config.ca_file_path.value());
                }

                if (this->config->tls_config.has_mutual_tls.has_value())
                {
                    if (this->config->tls_config.has_mutual_tls.value())
                    {
                        uint32_t override_env = 1;
                        setenv("GOOGLE_API_USE_CLIENT_CERTIFICATE", "true", override_env);
                    }
                }
            }

            void set_credential_options_helper(gcs::Options& options,
                                               const stdx::reflectible_monostate& cred)
            {
                (void) options;
                (void) cred;
            }

            void set_credential_options_helper(gcs::Options& options,
                                               const ServiceAccountFileCredential& cred)
            {
                auto gg_cred    = gc::CreateServiceAccountCredentialsFromJsonFilePath(cred.json_path);

                options.template set<google::cloud::UnifiedCredentialsOption>(gg_cred);
            }

            void set_credential_options_helper(gcs::Options& options,
                                               const ServiceAccountJsonCredential& cred)
            {
                auto gg_cred    = gc::CreateServiceAccountCredentials(cred.json_content);

                options.template set<google::cloud::UnifiedCredentialsOption>(gg_cred);
            }

            void set_credential_options_helper(gcs::Options& options,
                                               const AccessTokenCredential& cred)
            {
                auto expiry     = std::chrono::system_clock::now() + cred.token_lifetime;
                auto gg_cred    = gc::CreateAccessTokenCredentials(cred.access_token, expiry);

                options.template set<google::cloud::UnifiedCredentialsOption>(gg_cred);
            }


            void set_credential_options_helper(gcs::Options& options,
                                               const ExternalAccountCredential& cred)
            {
                auto gg_cred    = gc::CreateExternalAccountCredentialsFromJsonFilePath(cred.credential_config_file);

                options.template set<google::cloud::UnifiedCredentialsOption>(gg_cred);
            }

            void set_credential_options(gcs::Options& options)
            {
                if (!this->config.has_value())
                {
                    return;
                }

                auto visitor = [&](const auto& e)
                {
                    set_credential_options_helper(options, e);
                };

                std::visit(visitor, this->config->credential.credential);
            }

            void set_encryption_options(gcs::Options& options)
            {
                if (!this->config.has_value())
                {
                    return;
                }

                if (!this->config->encryption_config.has_value())
                {
                    return;
                }

                switch (this->config->encryption_config->encryption_type)
                {
                    case ENCRYPTION_TYPE_K_NONE:
                    case ENCRYPTION_TYPE_K_GOOGLE_MANAGED:
                    {
                        break;
                    }
                    case ENCRYPTION_TYPE_K_CUSTOMER_MANAGED_KMS:
                    {
                        options.template set<gcs::DestinationKmsKeyOption>(this->config->encryption_config->kms_key_name);
                        break;
                    }
                    case ENCRYPTION_TYPE_K_CUSTOMER_SUPPLIED:
                    {
                        auto encryption_key = gcs::EncryptionKey::FromBase64Key
                        (
                            this->config->encryption_config->customer_supplied_key;
                        );

                        options.template set<gcs::EncryptionKey>(encryption_key);
                        break;
                    }
                    default:
                    {
                        throw std::invalid_argument("bad encryption option, enumeration value out of range");
                    }
                }
            }

            void set_upload_options(gcs::Options& options)
            {
                if (!this->config.has_value())
                {
                    return;
                }

                if (this->config->upload_config.upload_buffer_sz.has_value())
                {
                    const size_t MIN_BUFFER_SZ  = size_t{1} << 10;
                    const size_t MAX_BUFFER_SZ  = size_t{1} << 30;

                    size_t usr_upload_buffer_sz = this->config->upload_config.upload_buffer_sz.value();
                    size_t upload_buffer_sz     = std::clamp(usr_upload_buffer_sz, MIN_BUFFER_SZ, MAX_BUFFER_SZ);

                    options.template set<gcs::UploadBufferSizeOption>(upload_buffer_sz);
                }

                if (this->config->upload_config.enable_checksum_validation.has_value())
                {
                    bool has_chksum = this->config->upload_config.enable_checksum_validation.value();

                    if (has_chksum)
                    {
                        options.template set<gcs::DisableCrc32cValidationOption>(false);
                    }
                }
            }

            void set_download_options(gcs::Options& options)
            {
                if (!this->config.has_value())
                {
                    return;
                }

                if (this->config->download_config.enable_checksum_validation.has_value())
                {
                    bool has_chksum = this->config->download_config.enable_checksum_validation.value();

                    if (has_chksum)
                    {
                        options.template set<gcs::DisableCrc32cValidationOption>(false);
                    }
                }
            }

            void set_client_primitives_options(gcs::Options& options)
            {
                if (!this->config.has_value())
                {
                    return;
                }

                if (this->config->project_id.has_value())
                {
                    options.template set<google::cloud::storage::ProjectIdOption>(this->config->project_id.value());
                }

                std::vector<std::string> user_agent_config_vec{};

                if (this->config->application_name.has_value())
                {
                    user_agent_config_vec.push_back(this->config->application_name.value());
                }

                if (this->config->environment.has_value())
                {
                    user_agent_config_vec.push_back(this->config->environment.value());
                }

                if (!user_agent_config_vec.empty())
                {
                    options.template set<google::cloud::UserAgentProductsOption>(user_agent_config_vec);
                }

                if (this->config->enable_crc_32c.has_value())
                {
                    if (this->config->enable_crc_32c.value())
                    {
                        options.template set<gcs::DisableCrc32cValidationOption>(false);
                    }                
                }

                if (this->config->enable_md5_validation.has_value())
                {
                    if (this->config->enable_md5_validation.value())
                    {
                        options.template set<gcs::EnableMD5ValidationOption>(true);
                    }
                }

                if (this->config->enable_connection_pooling.has_value())
                {
                    size_t pool_sz  = static_cast<size_t>(this->config->enable_connection_pooling.value()) * 16;
                    options.template set<gcs::ConnectionPoolSizeOption>(pool_sz);
                }
            }
    };
}

#endif
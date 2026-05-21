#ifndef __DATA_LOADER_SOURCE_AZURE_SOURCE_CLIENT_BUILDER_H__
#define __DATA_LOADER_SOURCE_AZURE_SOURCE_CLIENT_BUILDER_H__

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

namespace data_loader::azure_source
{
    namespace as    = Azure::Storage;
    namespace asb   = Azure::Storage::Blobs;
    namespace acc   = Azure::Core::Credentials;

    class AzureServiceClientBuilder
    {
        private:

            std::optional<SecuredAzureClientConfig> config;
        
        public:

            auto set(const SecuredAzureClientConfig& config) -> AzureServiceClientBuilder&
            {
                this->config = config;

                return *this;
            }

            auto build() -> std::unique_ptr<asb::BlobServiceClient>
            {
                if (!this->config.has_value())
                {
                    throw std::invalid_argument("bad Azure config, null")
                }

                if (std::holds_alternative<ConnectionStringAuthConfig>(this->config->auth_config.auth_config))
                {
                    return std::make_unique<asb::BlobServiceClient>(this->get_connection_string(),
                                                                    this->get_sdk_options());
                }

                if (std::holds_alternative<SharedKeyAuthConfig>(this->config->auth_config.auth_config))
                {
                    return std::make_unique<asb::BlobServiceClient>(this->get_service_ep_url(),
                                                                    this->get_storage_shared_key_credential(),
                                                                    this->get_sdk_options());
                }

                if (std::holds_alternative<SASTokenAuthConfig>(this->config->auth_config.auth_config))
                {
                    return std::make_unique<asb::BlobServiceClient>(this->get_sas_service_ep_url(),
                                                                    this->get_sdk_options());
                }

                if (std::holds_alternative<ManagedIdentityAuthConfig>(this->config->auth_config.auth_config))
                {
                    return std::make_unique<asb::BlobServiceClient>(this->get_service_ep_url(),
                                                                    this->get_managed_identity_token_credential(),
                                                                    this->get_sdk_options());
                }

                if (std::holds_altenative<ServicePrincipalSecretAuthConfig>(this->auth_config.auth_config))
                {
                    return std::make_unique<asb::BlobServiceClient>(this->get_service_ep_url(),
                                                                    this->get_service_principal_token_credential(),
                                                                    this->get_sdk_options());
                }

                throw std::invalid_argument("bad Azure config, invalid authentication option");
            }

        private:

            auto get_connection_string()
            {
                if (!this->config.has_value())
                {
                    throw std::invalid_argument("bad Azure config, null")
                }

                if (!std::holds_alternative<ConnectionStringAuthConfig>(this->config->auth_config.auth_config))
                {
                    throw std::invalid_argument("bad Azure authentication type, connection string expected");
                }

                return std::get<ConnectionStringAuthConfig>(this->config->auth_config.auth_config).connection_str;
            }

            auto get_storage_shared_key_credential() -> std::shared_ptr<as::StorageSharedKeyCredential>
            {
                if (!this->config.has_value())
                {
                    throw std::invalid_argument("bad Azure config, null");
                }

                if (!std::holds_alternative<SharedKeyAuthConfig>(this->config->auth_config.auth_config))
                {
                    throw std::invalid_argument("bad Azure authentication type, shared key expected");
                }

                const auto& shared_key = std::get<SharedKeyAuthConfig>(this->config->auth_config.auth_config);

                return std::make_shared<as::StorageSharedKeyCredential>(shared_key.account_name,
                                                                        shared_key.account_key);
            }

            auto get_azure_sas_credential() -> std::string
            {
                if (!this->config.has_value())
                {
                    throw std::invalid_argument("bad Azure config, null");
                }

                if (!std::holds_alternative<SASTokenAuthConfig>(this->config->auth_config.auth_config))
                {
                    throw std::invalid_argument("bad Azure authentication type, SAS token expected");
                }

                return std::get<SASTokenAuthConfig>(this->config->auth_config.auth_config).sas_token;
            }

            auto get_service_ep_url() -> std::string
            {
                if (!this->config.has_value())
                {
                    throw std::invalid_argument("bad Azure config, null");
                }

                return this->config->service_ep_url;
            }

            auto get_sas_service_ep_url() -> std::string
            {
                return this->get_service_ep_url() + this->get_azure_sas_credential();
            }

            auto get_managed_identity_token_credential() -> std::shared_ptr<acc::TokenCredential>
            {
                if (!this->config.has_value())
                {
                    throw std::invalid_argument("bad Azure config, null");
                }

                if (!std::holds_alternative<ManagedIdentityAuthConfig>(this->config->auth_config.auth_config))
                {
                    throw std::invalid_argument("bad Azure authentication type, ManagedIdentity expected");
                }

                Azure::Identity::ManagedIdentityCredentialOptions options{};
                options.ClientId    = std::get<ManagedIdentityAuthConfig>(this->config->auth_config.auth_config).managed_identity_client_id;

                return std::make_shared<Azure::Identity::ManagedIdentityCredential>(options);
            }

            auto get_service_principal_token_credential() -> std::shared_ptr<acc::TokenCredential>
            {
                if (!this->config.has_value())
                {
                    throw std::invalid_argument("bad Azure config, null");
                }

                if (!std::holds_alternative<ServicePrincipalSecretAuthConfig>(this->config->auth_config.auth_config))
                {
                    throw std::invalid_argument("bad Azure authentication type, ServicePrincipleSecret expected");
                }

                const auto& cred = std::get<ServicePrincipalSecretAuthConfig>(this->config->auth_config.auth_config);

                return std::make_shared<Azure::Identity::ClientSecretCredential>
                (
                    cred.tenant_id,
                    cred.client_id,
                    cred.client_secret
                );
            }

            auto get_sdk_options() -> asb::BlobClientOptions
            {
                asb::BlobClientOptions rs{};
                Azure::Core::Http::CurlTransportOptions curl_transport_options{};

                if (!this->config.has_value())
                {
                    return rs;
                }

                if (this->config->transport_config.http_proxy.has_value())
                {
                    curl_transport_options.Proxy  = this->config->transport_config.http_proxy.value();
                }

                if (this->config->transport_config.ca_file_path.has_value())
                {
                    curl_transport_options.SslOptions.CaInfo    = this->config->transport_config.ca_file_path.value();
                }

                if (this->config->telemetry_config.application_id.has_value())
                {
                    rs.Telemetry.ApplicationId = this->config->telemetry_config.application_id.value();
                }

                rs.Transport.Transport = std::make_shared<Azure::Core::Http::CurlTransport>(curl_transport_options);

                return rs;
            }
    };
}

#endif
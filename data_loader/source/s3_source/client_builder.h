#ifndef __DATA_LOADER_SOURCE_S3_SOURCE_CLIENT_BUILDER_H__
#define __DATA_LOADER_SOURCE_S3_SOURCE_CLIENT_BUILDER_H__

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
#include <aws/s3/S3Client.h>

namespace data_loader::s3_source
{
    class S3ClientBuilder
    {
        private:

            std::optional<S3ClientConfiguration> config;

        public:

            auto set(const S3ClientConfiguration& config) -> S3ClientBuilder&
            {
                this->config = config;

                return *this;
            }

            auto get() -> std::unique_ptr<Aws::S3::S3Client>
            {
                return std::make_unique<Aws::S3::S3Client>(this->get_credential(),
                                                           this->get_client_configuration());
            }

        private:

            auto get_credential() -> Aws::Auth::Credentials
            {
                if (!this->config.has_value())
                {
                    return Aws::Auth::AWSCredentials();
                }

                auto to_aws = [](const std::string &s) -> Aws::String { return Aws::String(s.c_str()); };

                const GenericCredential& gen = this->config->credential;

                if (std::holds_alternative<Credential_0>(gen.credential))
                {
                    const Credential_0 &c = std::get<Credential_0>(gen.credential);
                    return Aws::Auth::AWSCredentials(to_aws(c.access_key_id), to_aws(c.secret_key));
                }
                else if (std::holds_alternative<Credential_1>(gen.credential))
                {
                    const Credential_1 &c = std::get<Credential_1>(gen.credential);
                    return Aws::Auth::AWSCredentials(to_aws(c.access_key_id), to_aws(c.secret_key), to_aws(c.session_token));
                }
                else
                {
                    return Aws::Auth::AWSCredentials();
                }
            }

            auto get_client_configuration() -> Aws::Client::ClientConfiguration
            {
                Aws::Client::ClientConfiguration cc{};

                if (!this->config.has_value())
                {
                    return cc;
                }

                if (!this->config->client_config.has_value())
                {
                    return cc;
                }

                const ClientConfiguration_2 &c  = this->config->client_config.value();

                cc.region                       = Aws::String(c.region.c_str());
                cc.endpointOverride             = Aws::String(c.endpoint_override.c_str());

                if (c.scheme == "http")
                {
                    cc.scheme = Aws::Http::Scheme::HTTP;
                }
                else if (c.scheme == "https")
                {
                    cc.scheme = Aws::Http::Scheme::HTTPS;
                }
                else
                {
                    throw std::invalid_argument("bad ClientConfiguration's scheme, {http, https} required");
                }

                cc.followRedirects  = c.follow_redirects;
                cc.proxyHost        = Aws::String(c.proxy_host.c_str());
                cc.proxyPort        = static_cast<unsigned short>(c.proxy_port);
                cc.proxyUserName    = Aws::String(c.proxy_user_name.c_str());
                cc.proxyPassword    = Aws::String(c.proxy_password.c_str());
                cc.verifySSL        = c.verify_ssl;
                cc.caFile           = Aws::String(c.ca_file.c_str());
                cc.caPath           = Aws::String(c.ca_path.c_str());

                if (c.max_connections != 0u)
                {
                    cc.maxConnections = static_cast<size_t>(c.max_connections);
                }
                else
                {
                    throw std::invalid_argument("bad ClientConfiguration's max connections, 0");
                }

                // AWS SDK uses milliseconds fields for timeouts

                cc.requestTimeoutMs             = static_cast<unsigned>(c.request_timeout.count());
                cc.connectTimeoutMs             = static_cast<unsigned>(c.connect_timeout.count());
                cc.maxRetries                   = static_cast<unsigned>(c.max_retries);
                cc.enableClockSkewAdjustment    = c.enable_clock_skew_adjustment;
                cc.userAgent                    = Aws::String(c.user_agent.c_str());

                return cc;
            }
    };
}

#endif
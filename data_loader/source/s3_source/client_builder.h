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
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>

namespace data_loader::source::s3_source
{
    class S3ClientBuilder
    {
        private:

            std::optional<SecuredS3ClientConfiguration> config;

        public:

            auto set(const SecuredS3ClientConfiguration& config) -> S3ClientBuilder&
            {
                this->config = config;

                return *this;
            }

            auto build() -> std::unique_ptr<Aws::S3::S3Client>
            {
                return std::make_unique<Aws::S3::S3Client>(this->get_credential(),
                                                           this->get_endpoint_provider(),
                                                           this->get_client_configuration());
            }

        private:

            auto get_credential() -> Aws::Auth::AWSCredentials
            {
                if (!this->config.has_value())
                {
                    throw std::invalid_argument("bad credential, null config");
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
                    throw std::invalid_argument("bad credential, invalid credential option");
                }
            }

            auto get_endpoint_provider() -> std::shared_ptr<Aws::S3::Endpoint::S3EndpointProvider>
            {
                return Aws::MakeShared<Aws::S3::Endpoint::S3EndpointProvider>("s3_client_builder");
            }

            auto get_client_configuration() -> Aws::S3::S3ClientConfiguration
            {
                Aws::S3::S3ClientConfiguration cc{};

                if (!this->config.has_value())
                {
                    return cc;
                }

                if (!this->config->client_config.has_value())
                {
                    return cc;
                }

                const S3ClientConfiguration_2 &c  = this->config->client_config.value();

                if (c.region.has_value())
                {
                    cc.region   = Aws::String(c.region->c_str());
                }

                if (c.endpoint_override.has_value())
                {
                    cc.endpointOverride    = Aws::String(c.endpoint_override->c_str());
                }

                if (c.scheme.has_value())
                {
                    if (c.scheme.value() == "http")
                    {
                        cc.scheme = Aws::Http::Scheme::HTTP;
                    }
                    else if (c.scheme.value() == "https")
                    {
                        cc.scheme = Aws::Http::Scheme::HTTPS;
                    }
                    else
                    {
                        throw std::invalid_argument("bad S3ClientConfiguration, scheme c {http, https} required");
                    }
                }

                if (c.proxy_host.has_value())
                {
                    cc.proxyHost        = Aws::String(c.proxy_host->c_str());
                }

                if (c.proxy_port.has_value())
                {
                    cc.proxyPort        = static_cast<unsigned short>(c.proxy_port.value());
                }

                if (c.proxy_user_name.has_value())
                {
                    cc.proxyUserName    = Aws::String(c.proxy_user_name->c_str());
                }

                if (c.proxy_password.has_value())
                {
                    cc.proxyPassword    = Aws::String(c.proxy_password->c_str());
                }

                if (c.verify_ssl.has_value())
                {
                    cc.verifySSL        = c.verify_ssl.value();
                }

                if (c.ca_file.has_value())
                {
                    cc.caFile           = Aws::String(c.ca_file->c_str());
                }

                if (c.ca_path.has_value())
                {
                    cc.caPath           = Aws::String(c.ca_path->c_str());
                }

                if (c.max_connections.has_value())
                {
                    if (c.max_connections.value() != 0u)
                    {
                        cc.maxConnections = static_cast<size_t>(c.max_connections.value());
                    }
                    else
                    {
                        throw std::invalid_argument("bad S3ClientConfiguration, max connections 0");
                    }
                }

                // AWS SDK uses milliseconds fields for timeouts

                const size_t REQUEST_TIMEOUT_IN_MILLISECONDS_MIN    = 10000ULL;
                const size_t REQUEST_TIMEOUT_IN_MILLISECONDS_MAX    = 120000ULL;

                const size_t CONNECT_TIMEOUT_IN_MILLISECONDS_MIN    = 1000ULL;
                const size_t CONNECT_TIMEOUT_IN_MILLISECONDS_MAX    = 5000ULL;

                if (c.request_timeout.has_value())
                {
                    cc.requestTimeoutMs             = std::clamp(static_cast<size_t>(c.request_timeout->count()),
                                                                 REQUEST_TIMEOUT_IN_MILLISECONDS_MIN,
                                                                 REQUEST_TIMEOUT_IN_MILLISECONDS_MAX);
                }

                if (c.connect_timeout.has_value())
                {
                    cc.connectTimeoutMs             = std::clamp(static_cast<size_t>(c.connect_timeout->count()),
                                                                 CONNECT_TIMEOUT_IN_MILLISECONDS_MIN,
                                                                 CONNECT_TIMEOUT_IN_MILLISECONDS_MAX);
                }

                if (c.enable_clock_skew_adjustment.has_value())
                {
                    cc.enableClockSkewAdjustment    = c.enable_clock_skew_adjustment.value();
                }

                if (c.user_agent.has_value())
                {
                    cc.userAgent                    = Aws::String(c.user_agent->c_str());
                }

                if (c.use_virtual_addressing.has_value())
                {
                    cc.useVirtualAddressing         = c.use_virtual_addressing.value();
                }

                if (c.payload_signing_policy.has_value())
                {
                    switch (c.payload_signing_policy.value())
                    {
                        case PAYLOAD_SIGNING_POLICY_REQUEST_DEPENDENT:
                        {
                            cc.payloadSigningPolicy = Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::RequestDependent;
                            break;
                        }
                        case PAYLOAD_SIGNING_POLICY_ALWAYS:
                        {
                            cc.payloadSigningPolicy = Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Always;
                            break;
                        }
                        case PAYLOAD_SIGNING_POLICY_NEVER:
                        {
                            cc.payloadSigningPolicy = Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never;
                            break;
                        }
                        default:
                        {
                            throw std::invalid_argument("bad S3ClientConfiguration, payloadSigningPolicy enumeration out of range");
                        }
                    }
                }

                if (c.regional_endpoint_option.has_value())
                {
                    switch (c.regional_endpoint_option.value())
                    {
                        case US_EAST_1_REGIONAL_ENDPOINT_OPTION_NOT_SET:
                        {
                            cc.useUSEast1RegionalEndPointOption = Aws::S3::US_EAST_1_REGIONAL_ENDPOINT_OPTION::NOT_SET;
                            break;
                        }
                        case US_EAST_1_REGIONAL_ENDPOINT_OPTION_LEGACY:
                        {
                            cc.useUSEast1RegionalEndPointOption = Aws::S3::US_EAST_1_REGIONAL_ENDPOINT_OPTION::LEGACY;
                            break;
                        }
                        case US_EAST_1_REGIONAL_ENDPOINT_OPTION_REGIONAL:
                        {
                            cc.useUSEast1RegionalEndPointOption = Aws::S3::US_EAST_1_REGIONAL_ENDPOINT_OPTION::REGIONAL;
                            break;
                        }
                        default:
                        {
                            throw std::invalid_argument("bad S3ClientConfiguration, regionalEndpointOption's enumeration out of range");
                        }
                    }
                }

                if (c.disable_multi_region_access_points.has_value())
                {
                    cc.disableMultiRegionAccessPoints   = c.disable_multi_region_access_points.value();
                }

                if (c.use_arn_region.has_value())
                {
                    cc.useArnRegion                     = c.use_arn_region.value();
                }

                if (c.disable_s3_express_auth.has_value())
                {
                    cc.disableS3ExpressAuth             = c.disable_s3_express_auth.value();
                }

                if (c.enable_host_prefix_injection.has_value())
                {
                    cc.enableHostPrefixInjection        = c.enable_host_prefix_injection.value();
                }

                if (c.use_fips.has_value())
                {
                    cc.useFIPS                          = c.use_fips.value();
                }

                return cc;
            }
    };
}

#endif
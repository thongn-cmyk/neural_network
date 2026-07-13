#ifndef __DATA_LOADER_SOURCE_AZURE_SOURCE_MODEL_H__
#define __DATA_LOADER_SOURCE_AZURE_SOURCE_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <variant>
#include <stl_extension/stdx.h>
#include <data_loader/stream_reader/model.h>
#include <serializer/compact_serializer.h>

namespace data_loader::source::azure_source
{
    static inline constexpr uint8_t AZURE_AUTH_TYPE_CONNECTION_STRING           = 0u;
    static inline constexpr uint8_t AZURE_AUTH_TYPE_SHARED_KEY                  = 1u;
    static inline constexpr uint8_t AZURE_AUTH_TYPE_SAS_TOKEN                   = 2u;
    static inline constexpr uint8_t AZURE_AUTH_TYPE_MANAGED_IDENTITY            = 3u;
    static inline constexpr uint8_t AZURE_AUTH_TYPE_SERVICE_PRINCIPAL_SECRET    = 4u;

    struct ConnectionStringAuthConfig
    {
        std::string connection_str;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(connection_str);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(connection_str);
        }
    };

    struct SharedKeyAuthConfig
    {
        std::string account_name;
        std::string account_key;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(account_name, account_key);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(account_name, account_key);
        }
    };

    struct SASTokenAuthConfig
    {
        std::string sas_token;
        
        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(sas_token);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(sas_token);
        }
    };

    struct GenericAuthConfig
    {
        std::variant<stdx::reflectible_monostate,
                     ConnectionStringAuthConfig,
                     SharedKeyAuthConfig,
                     SASTokenAuthConfig> auth_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(auth_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(auth_config);
        }
    };

    struct TransportConfig
    {
        std::optional<std::string> http_proxy;
        std::optional<std::string> ca_file_path;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(http_proxy,
                      ca_file_path);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(http_proxy,
                      ca_file_path);
        }
    };

    struct TelemetryConfig
    {
        std::optional<std::string> application_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(application_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(application_id);
        }
    };

    struct SecuredAzureClientConfig
    {
        std::string service_ep_url;

        GenericAuthConfig auth_config;
        TransportConfig transport_config;
        TelemetryConfig telemetry_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(service_ep_url,
                      auth_config,
                      transport_config,
                      telemetry_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(service_ep_url,
                      auth_config,
                      transport_config,
                      telemetry_config);
        }
    };

    struct ExternalSecuredAzureClientConfig
    {
        std::string config_bytestream;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(config_bytestream);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(config_bytestream);
        }
    };

    struct AzureLoaderConfig
    {
        data_loader::stream_reader::ExternalDelimitedStreamReaderConfig delim_config;
        ExternalSecuredAzureClientConfig service_client_config;
        std::string container_name;
        std::string blob_name;  
        std::optional<uint64_t> read_ahead_buffer_sz_hint;
        std::optional<uint64_t> unit_byte_sz_hint;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(delim_config,
                      service_client_config,
                      container_name,
                      blob_name,
                      read_ahead_buffer_sz_hint,
                      unit_byte_sz_hint);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(delim_config,
                      service_client_config,
                      container_name,
                      blob_name,
                      read_ahead_buffer_sz_hint,
                      unit_byte_sz_hint);
        }
    };

    struct ExternalAzureLoaderConfig
    {
        std::string config_bytestream;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(config_bytestream);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(config_bytestream);
        }
    };

    auto to_internal_secured_azure_client_config(const ExternalSecuredAzureClientConfig& config) -> SecuredAzureClientConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<SecuredAzureClientConfig>(config.config_bytestream);
    }

    auto to_external_secured_azure_client_config(const SecuredAzureClientConfig& config) -> ExternalSecuredAzureClientConfig
    {
        return
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_external_azure_loader_config(const AzureLoaderConfig& config) -> ExternalAzureLoaderConfig
    {
        return ExternalAzureLoaderConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_azure_loader_config(const ExternalAzureLoaderConfig& config) -> AzureLoaderConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<AzureLoaderConfig>(config.config_bytestream);
    }
}

#endif
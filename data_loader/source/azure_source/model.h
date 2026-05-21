#ifndef __DATA_LOADER_SOURCE_AZURE_SOURCE_MODEL_H__
#define __DATA_LOADER_SOURCE_AZURE_SOURCE_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <variant>

namespace data_loader::azure_source
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

    struct ManagedIdentityAuthConfig
    {
        std::string managed_identity_client_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(managed_identity_client_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(managed_identity_client_id);
        }
    };

    struct ServicePrincipalSecretAuthConfig
    {
        std::string tenant_id;
        std::string client_id;
        std::string client_secret;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(tenant_id, client_id, client_secret);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(tenant_id, client_id, client_secret);
        }
    };

    struct GenericAuthConfig
    {
        std::variant<stdx::reflectible_monostate,
                     ConnectionStringAuthConfig,
                     SharedKeyAuthConfig,
                     SASTokenAuthConfig,
                     ManagedIdentityAuthConfig,
                     ServicePrincipalSecretAuthConfig> auth_config;

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
}

#endif
#ifndef __DATA_LOADER_SOURCE_S3_SOURCE_MODEL_H__
#define __DATA_LOADER_SOURCE_S3_SOURCE_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
// #include <aws/s3/S3Client.h>
// #include <aws/core/client/ClientConfiguration.h>
#include <variants>
#include <string>
#include <cstring>
#include <optional>
#include <chrono>
#include <stl_extension/stdx.h>

namespace data_loader::s3_source
{
    struct ClientConfiguration_2
    {
        // Basic network / endpoint options
        std::string region;
        std::string endpoint_override; // endpoint override (hostname[:port])
        std::string scheme; // "http" or "https"
        bool follow_redirects;

        // Proxy settings
        std::string proxy_host; //= "";
        uint16_t proxy_port;  //= 0;
        std::string proxy_user_name; // = "";
        std::string proxy_password; // = "";

        // TLS / CA
        bool verify_ssl; // = true;
        std::string ca_file; // = "";   // path to CA file
        std::string ca_path; // = "";   // path to CA directory

        // Timeouts and networking

        size_t max_connections; // = 50;
        std::chrono::milliseconds request_timeout; // = std::chrono::milliseconds{0}; // 0 => infinite
        std::chrono::milliseconds connect_timeout; // = std::chrono::milliseconds{3000};

        // Retry / performance
        size_t max_retries; // = 3;
        bool enable_clock_skew_adjustment; // = true;

        // User agent / application identity
        std::string user_agent; // = "";

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(region,
                      endpoint_override,
                      scheme,
                      follow_redirects,
                      proxy_host,
                      proxy_port,
                      proxy_user_name,
                      proxy_password,
                      verify_ssl,
                      ca_file,
                      ca_path,
                      max_connections,
                      request_timeout,
                      connect_timeout,
                      max_retries,
                      enable_clock_skew_adjustment,
                      user_agent);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(region,
                      endpoint_override,
                      scheme,
                      follow_redirects,
                      proxy_host,
                      proxy_port,
                      proxy_user_name,
                      proxy_password,
                      verify_ssl,
                      ca_file,
                      ca_path,
                      max_connections,
                      request_timeout,
                      connect_timeout,
                      max_retries,
                      enable_clock_skew_adjustment,
                      user_agent);
        }
    };

    struct Credential_0
    {
        std::string access_key_id;
        std::string secret_key;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(access_key_id, secret_key);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(access_key_id, secret_key);
        }
    };

    struct Credential_1
    {
        std::string access_key_id;
        std::string secret_key;
        std::string session_token;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(access_key_id, secret_key, session_token);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(access_key_id, secret_key, session_token);
        }
    };

    struct GenericCredential
    {
        std::variant<stdx::reflectible_monostate,
                     Credential_0,
                     Credential_1> credential;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(credential);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(credential);
        }
    };

    struct S3ClientConfiguration
    {
        std::optional<ClientConfiguration_2> client_config;
        GenericCredential credential;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_config, credential);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_config, credential);
        }
    };

    struct ExternalS3ClientConfiguration
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
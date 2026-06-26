#ifndef __DATA_LOADER_SOURCE_S3_SOURCE_MODEL_H__
#define __DATA_LOADER_SOURCE_S3_SOURCE_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
// #include <aws/s3/S3Client.h>
// #include <aws/core/client/ClientConfiguration.h>
#include <variant>
#include <string>
#include <cstring>
#include <optional>
#include <chrono>
#include <stl_extension/stdx.h>
#include <data_loader/stream_reader/model.h>
#include <serializer/compact_serializer.h>

namespace data_loader::source::s3_source
{
    static inline constexpr uint8_t PAYLOAD_SIGNING_POLICY_REQUEST_DEPENDENT    = 0u;
    static inline constexpr uint8_t PAYLOAD_SIGNING_POLICY_ALWAYS               = 1u;
    static inline constexpr uint8_t PAYLOAD_SIGNING_POLICY_NEVER                = 2u;

    static inline constexpr uint8_t US_EAST_1_REGIONAL_ENDPOINT_OPTION_NOT_SET  = 0u;
    static inline constexpr uint8_t US_EAST_1_REGIONAL_ENDPOINT_OPTION_LEGACY   = 1u;
    static inline constexpr uint8_t US_EAST_1_REGIONAL_ENDPOINT_OPTION_REGIONAL = 2u;

    struct S3ClientConfiguration_2
    {
        // Basic network / endpoint options
        std::optional<std::string> region;
        std::optional<std::string> endpoint_override; // endpoint override (hostname[:port])
        std::optional<std::string> scheme; // "http" or "https"

        // Proxy settings
        std::optional<std::string> proxy_host; //= "";
        std::optional<uint16_t> proxy_port;  //= 0;
        std::optional<std::string> proxy_user_name; // = "";
        std::optional<std::string> proxy_password; // = "";

        // TLS / CA
        std::optional<bool> verify_ssl; // = true;
        std::optional<std::string> ca_file; // = "";   // path to CA file
        std::optional<std::string> ca_path; // = "";   // path to CA directory

        // Timeouts and networking

        std::optional<uint32_t> max_connections; // = 50;
        std::optional<std::chrono::milliseconds> request_timeout; // = std::chrono::milliseconds{0}; // 0 => infinite
        std::optional<std::chrono::milliseconds> connect_timeout; // = std::chrono::milliseconds{3000};

        // Retry / performance
        std::optional<bool> enable_clock_skew_adjustment; // = true;

        // User agent / application identity
        std::optional<std::string> user_agent; // = "";

        std::optional<bool> use_virtual_addressing; // = true
        std::optional<uint8_t> payload_signing_policy; // = PayloadSigningPolicy::RequestDependent
        std::optional<uint8_t> regional_endpoint_option; // = US_EAST_1_REGIONAL_ENDPOINT_OPTION::NOT_SET

        std::optional<bool> disable_multi_region_access_points; // = false
        std::optional<bool> use_arn_region; // = false
        std::optional<bool> disable_s3_express_auth; // = false
        std::optional<bool> enable_host_prefix_injection; // = false
        std::optional<bool> use_fips; // = false

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(region,
                      endpoint_override,
                      scheme,
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
                      enable_clock_skew_adjustment,
                      user_agent,
                      use_virtual_addressing,
                      payload_signing_policy,
                      regional_endpoint_option,
                      disable_multi_region_access_points,
                      use_arn_region,
                      disable_s3_express_auth,
                      enable_host_prefix_injection,
                      use_fips);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(region,
                      endpoint_override,
                      scheme,
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
                      enable_clock_skew_adjustment,
                      user_agent,
                      use_virtual_addressing,
                      payload_signing_policy,
                      regional_endpoint_option,
                      disable_multi_region_access_points,
                      use_arn_region,
                      disable_s3_express_auth,
                      enable_host_prefix_injection,
                      use_fips);
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

    struct SecuredS3ClientConfiguration
    {
        std::optional<S3ClientConfiguration_2> client_config;
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

    struct ExternalSecuredS3ClientConfiguration
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

    auto to_internal_secured_s3_client_configuration(const ExternalSecuredS3ClientConfiguration& config) -> SecuredS3ClientConfiguration
    {
        return dg::network_compact_serializer::dgstd_deserialize<SecuredS3ClientConfiguration>(config.config_bytestream);
    }

    auto to_external_secured_s3_client_configuration(const SecuredS3ClientConfiguration& config) -> ExternalSecuredS3ClientConfiguration
    {
        return
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    struct S3LoaderConfig
    {
        data_loader::stream_reader::ExternalDelimitedStreamReaderConfig delim_config;
        ExternalSecuredS3ClientConfiguration s3_client_config;
        std::string bucket_name;
        std::string object_key;
        std::optional<uint64_t> read_ahead_buffer_sz_hint;
        std::optional<uint64_t> unit_byte_sz_hint;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(delim_config,
                      s3_client_config,
                      bucket_name,
                      object_key,
                      read_ahead_buffer_sz_hint,
                      unit_byte_sz_hint);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(delim_config,
                      s3_client_config,
                      bucket_name,
                      object_key,
                      read_ahead_buffer_sz_hint,
                      unit_byte_sz_hint);
        }
    };

    struct ExternalS3LoaderConfig
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

    auto to_external_s3_loader_config(const S3LoaderConfig& config) -> ExternalS3LoaderConfig
    {
        return ExternalS3LoaderConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_s3_loader_config(const ExternalS3LoaderConfig& config) -> S3LoaderConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<S3LoaderConfig>(config.config_bytestream);
    }
}

#endif
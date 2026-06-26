#ifndef __DATA_LOADER_SOURCE_GCS_SOURCE_MODEL_H__
#define __DATA_LOADER_SOURCE_GCS_SOURCE_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <optional>
#include <variant>
#include <stl_extension/stdx.h>
#include <chrono>
#include <vector>
#include <data_loader/stream_reader/model.h>
#include <serializer/compact_serializer.h>

namespace data_loader::source::gcs_source
{
    static inline constexpr uint8_t CREDENTIAL_TYPE_K_DEFAULT                       = 0u;
    static inline constexpr uint8_t CREDENTIAL_TYPE_K_SERVICE_ACCOUNT_FILE          = 1u;
    static inline constexpr uint8_t CREDENTIAL_TYPE_K_SERVICE_ACCOUNT_JSON          = 2u;
    static inline constexpr uint8_t CREDENTIAL_TYPE_K_ACCESS_TOKEN                  = 3u;
    static inline constexpr uint8_t CREDENTIAL_TYPE_K_IMPERSONATE_SERVICE_ACCOUNT   = 4u;
    static inline constexpr uint8_t CREDENTIAL_TYPE_K_EXTERNAL_ACCOUNT              = 5u;
    static inline constexpr uint8_t CREDENTIAL_TYPE_K_ANONYMOUS                     = 6u;

    static inline constexpr uint8_t ENDPOINT_SCHEME_K_HTTP                          = 0u;
    static inline constexpr uint8_t ENDPOINT_SCHEME_K_HTTPS                         = 1u;

    struct EndpointConfig
    {
        std::optional<uint8_t> endpoint_scheme;
        std::optional<std::string> endpoint;
        std::optional<bool> use_virtual_hosted_style;
        std::optional<bool> enable_dns_caching;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(endpoint_scheme,
                      endpoint,
                      use_virtual_hosted_style,
                      enable_dns_caching);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(endpoint_scheme,
                      endpoint,
                      use_virtual_hosted_style,
                      enable_dns_caching);
        }
    };

    struct ServiceAccountFileCredential
    {
        std::string json_path;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(json_path);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(json_path);
        }
    };

    struct ServiceAccountJsonCredential
    {
        std::string json_content;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(json_content);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(json_content);
        }
    };

    struct AccessTokenCredential
    {
        std::string access_token;
        std::chrono::seconds token_lifetime;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(access_token,
                      token_lifetime);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(access_token,
                      token_lifetime);
        }
    };

    struct ExternalAccountCredential
    {
        std::string credential_config_file;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(credential_config_file);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(credential_config_file);
        }
    };

    struct GenericCredential
    {
        std::variant<stdx::reflectible_monostate,
                     ServiceAccountFileCredential,
                     ServiceAccountJsonCredential,
                     AccessTokenCredential,
                     ExternalAccountCredential> credential;

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

    struct UploadConfig
    {
        std::optional<uint64_t> upload_buffer_sz;
        std::optional<bool> enable_checksum_validation;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(upload_buffer_sz,
                      enable_checksum_validation);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(upload_buffer_sz,
                      enable_checksum_validation);
        }
    };

    struct DownloadConfig
    {
        std::optional<bool> enable_checksum_validation;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(enable_checksum_validation);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(enable_checksum_validation);
        }
    };

    struct TLSConfig
    {
        std::optional<std::string> ca_file_path;
        std::optional<bool> has_mutual_tls;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(ca_file_path, has_mutual_tls);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(ca_file_path, has_mutual_tls);
        }
    };

    struct SecuredGCSClientConfig
    {
        //client_resource_pointer

        std::optional<std::string> project_id;
        std::optional<std::string> application_name;
        std::optional<std::string> environment;

        EndpointConfig endpoint_config;
        TLSConfig tls_config;

        //client feature toggles

        std::optional<bool> enable_crc_32c;
        std::optional<bool> enable_md5_validation;
        std::optional<bool> enable_connection_pooling;

        //client features

        GenericCredential credential;
        UploadConfig upload_config;
        DownloadConfig download_config;
    
        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(project_id,
                      application_name,
                      environment,

                      endpoint_config,
                      tls_config,

                      enable_crc_32c,
                      enable_md5_validation,
                      enable_connection_pooling,

                      credential,
                      upload_config,
                      download_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(project_id,
                      application_name,
                      environment,

                      endpoint_config,
                      tls_config,

                      enable_crc_32c,
                      enable_md5_validation,
                      enable_connection_pooling,

                      credential,
                      upload_config,
                      download_config);
        }
    };

    struct ExternalSecuredGCSClientConfig
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

    auto to_internal_secured_gcs_client_config(const ExternalSecuredGCSClientConfig& config) -> SecuredGCSClientConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<SecuredGCSClientConfig>(config.config_bytestream);
    }

    auto to_external_secured_gcs_client_config(const SecuredGCSClientConfig& config) -> ExternalSecuredGCSClientConfig
    {
        return
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    struct GCSLoaderConfig
    {
        data_loader::stream_reader::ExternalDelimitedStreamReaderConfig delim_config;
        ExternalSecuredGCSClientConfig gcs_client_config;
        std::string bucket_name;
        std::string object_key;
        std::optional<uint64_t> read_ahead_buffer_sz_hint;
        std::optional<uint64_t> unit_byte_sz_hint;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(delim_config,
                      gcs_client_config,
                      bucket_name,
                      object_key,
                      read_ahead_buffer_sz_hint,
                      unit_byte_sz_hint);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(delim_config,
                      gcs_client_config,
                      bucket_name,
                      object_key,
                      read_ahead_buffer_sz_hint,
                      unit_byte_sz_hint);
        }
    };

    struct ExternalGCSLoaderConfig
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

    auto to_external_gcs_loader_config(const GCSLoaderConfig& config) -> ExternalGCSLoaderConfig
    {
        return ExternalGCSLoaderConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_gcs_loader_config(const ExternalGCSLoaderConfig& config) -> GCSLoaderConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<GCSLoaderConfig>(config.config_bytestream);
    }
}

#endif
#ifndef __DATA_LOADER_SOURCE_GCS_SOURCE_MODEL_H__
#define __DATA_LOADER_SOURCE_GCS_SOURCE_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <optional>

namespace data_loader::gcs_source
{   
    struct GCSClientConfig
    {
        std::optional<std::string> application_name;
        std::optional<std::string> environment;
        std::optional<std::string> deployment_region;

        std::optional<std::string> endpoint;
        std::optional<std::string> universe_domain;

        std::optional<bool> use_https;
        std::optional<bool> use_grpc;
        std::optional<bool> enable_direct_path;
        std::optional<bool> enable_dual_stack;
        std::optional<bool> enable_ipv6;

        std::optional<std::chrono::milliseconds> connect_timeout;
        std::optional<std::chrono::milliseconds> read_timeout;
        std::optional<std::chrono::milliseconds> write_timeout;
        std::optional<std::chrono::milliseconds> request_timeout;
        std::optional<std::chrono::milliseconds> upload_chunk_timeout;
        std::optional<std::chrono::milliseconds> download_chunk_timeout;

        std::optional<uint32_t> grpc_channel_count;
        std::optional<uint32_t> grpc_max_receive_message_size_mb;
        std::optional<uint32_t> grpc_max_send_message_size_mb;
        std::optional<uint32_t> max_http_connections;
        std::optional<bool> enable_connection_pooling;
        std::optional<bool> enable_keepalive;
        std::optional<uint32_t> keepalive_interval_seconds;

        std::optional<bool> enable_crc32c;
        std::optional<bool> enable_md5;
        std::optional<bool> verify_download_checksums;
        std::optional<bool> verify_upload_checksums;

        
    };

    struct SecuredGCSClientConfig
    {

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
}

#endif
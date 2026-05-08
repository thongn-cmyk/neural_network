#ifndef __DATA_LOADER_SOURCE_GCS_SOURCE_GCS_SOURCE_H__
#define __DATA_LOADER_SOURCE_GCS_SOURCE_GCS_SOURCE_H__

#include <google/cloud/storage/client.h>
#include <array>
#include <iostream>
#include <string>
#include <cstring>
#include <stdint.h>
#include <stdlib.h>

namespace data_loader::gcs_source
{
    struct GCSLoaderConfig
    {
        data_loader::stream_reader::ExternalDelimitedStreamReaderConfig delim_config;
        data_loader::gcs_source::SerializableGCSClientConfiguration gcs_client_configuration;
        std::string bucket_name;
        std::string object_key;
        std::optional<uint64_t> read_ahead_buffer_sz_hint;
        std::optional<uint64_t> unit_byte_sz_hint;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(delim_config,
                      gcs_client_configuration,
                      bucket_name,
                      object_key,
                      read_ahead_buffer_sz_hint,
                      unit_byte_sz_hint);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(delim_config,
                      gcs_client_configuration,
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

    class GCSLoader: public virtual data_loader::SourceLoaderInterface
    {
        private:

            struct GCSObjectConnectionString
            {
                std::string bucket_name;
                std::string object_key;
            };

            struct BufferPointer
            {
                size_t offset;
                size_t sz;
            };

            std::unique_ptr<data_loader::stream_reader::DelimitedStreamReaderInterface> delim_stream_reader;
            std::unique_ptr<> gcs_file_object;
            size_t tx_unit_sz;
            bool was_completed;
            bool is_bad_state;
            size_t soft_read_error_sz;
            GCSObjectConnectionString gcs_connection_string;
            gcs::ClientConfiguration client_config;
            std::optional<BufferPointer> buf_pointer;
        
            static inline constexpr size_t SOFT_READ_ERROR_THRESHOLD    = size_t{1} << 3;
        
        public:
            
            static inline constexpr size_t MAX_READ_SZ                  = size_t{1} << 20;
            static inline constexpr size_t MIN_BUFFER_SZ                = size_t{1} << 10;
            static inline constexpr size_t MAX_BUFFER_SZ                = size_t{1} << 20;

            GCSLoader(const GCSLoaderConfig& config)
            {
                this->delim_stream_reader   = std::make_unique<data_loader::stream_reader::DelimitedStreamReader>(config.delim_config);
                this->gcs_file_object       = nullptr;
                this->tx_unit_sz            = 1u;

                if (config.unit_byte_sz_hint.has_value())
                {
                    this->tx_unit_sz    = std::max(this->tx_unit_sz, static_cast<size_t>(config.unit_byte_sz_hint.value()));
                }

                if (config.read_ahead_buffer_sz_hint.has_value())
                {

                }
            }

            GCSLoader(const ExternalGCSLoaderConfig& config): GCSLoader(to_internal_gcs_loader_config(config)){}

            auto get(size_t tx_hint_sz) -> std::optional<std::vector<std::string>>
            {
                if (this->is_bad_state)
                {
                    throw bad_state_error{};
                }

                if (this->was_completed)
                {
                    return std::nullopt;
                }

                size_t tx_byte_sz   = tx_hint_sz * this->tx_unit_sz;
                tx_byte_sz          = std::min(tx_byte_sz, MAX_READ_SZ);

                if (tx_byte_sz == 0u)
                {
                    return std::vector<std::string>();
                }

                std::string buf(tx_byte_sz, ' ');
                intmax_t read_bytes;

                if (this->gcs_file_object == nullptr)
                {

                }

            }
    };
}

#endif
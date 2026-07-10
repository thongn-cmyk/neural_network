#ifndef __DATA_LOADER_SOURCE_FILE_SOURCE_MODEL_H__
#define __DATA_LOADER_SOURCE_FILE_SOURCE_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <optional>
#include <string>
#include <data_loader/stream_reader/model.h>
#include <serializer/compact_serializer.h>

namespace data_loader::source::file_source
{
    struct FileLoaderConfig
    {
        data_loader::stream_reader::ExternalDelimitedStreamReaderConfig delim_config;
        std::string local_file_path;
        std::optional<uint64_t> read_ahead_buffer_sz_hint;
        std::optional<uint64_t> unit_byte_sz_hint;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(delim_config,
                      local_file_path,
                      read_ahead_buffer_sz_hint,
                      unit_byte_sz_hint);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(delim_config,
                      local_file_path,
                      read_ahead_buffer_sz_hint,
                      unit_byte_sz_hint);
        }
    };

    struct ExternalFileLoaderConfig
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

    auto to_external_file_loader_config(const FileLoaderConfig& config) -> ExternalFileLoaderConfig
    {
        return ExternalFileLoaderConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_file_loader_config(const ExternalFileLoaderConfig& config) -> FileLoaderConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<FileLoaderConfig>(config.config_bytestream);
    }
}

#endif
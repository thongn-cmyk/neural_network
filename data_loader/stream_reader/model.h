#ifndef __DATA_LOADER_STREAM_READER_MODEL_H__
#define __DATA_LOADER_STREAM_READER_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <optional>
#include <serializer/compact_serializer.h>

namespace data_loader::stream_reader
{
    struct DelimitedStreamReaderConfig
    {
        char delim_char;
        char eor_char;
        std::optional<uint64_t> max_token_sz;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(delim_char,
                      eor_char,
                      max_token_sz);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(delim_char,
                      eor_char,
                      max_token_sz);
        }
    };

    struct ExternalDelimitedStreamReaderConfig
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

    auto to_external_delimited_stream_reader_config(const DelimitedStreamReaderConfig& config) -> ExternalDelimitedStreamReaderConfig
    {
        return ExternalDelimitedStreamReaderConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_delimited_stream_reader_config(const ExternalDelimitedStreamReaderConfig& config) -> DelimitedStreamReaderConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<DelimitedStreamReaderConfig>(config.config_bytestream);
    }
}

#endif
#ifndef __DATA_LOADER_DELIMITED_STREAM_READER_H__
#define __DATA_LOADER_DELIMITED_STREAM_READER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include "delimited_stream_reader_interface.h"
#include <data_loader/exception_base.h>
#include <serializer/compact_serializer.h>

namespace data_loader::stream_reader
{
    using namespace data_loader::exception_base;

    struct token_overflow_error: runtime_error_base
    {
        token_overflow_error(): runtime_error_base("max token size reached", "token_overflow_error"){}
    };

    struct DelimitedStreamReaderConfig
    {
        char delim_char;
        char eor_char;
        std::optional<uint64_t> max_token_sz;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(delim_char, max_token_sz);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(delim_char, max_token_sz);
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

    class DelimitedStreamReader: public virtual DelimitedStreamReaderInterface
    {
        private:

            std::string stream;
            char delim_char;
            char eor_char;
            std::optional<uint64_t> max_token_sz;

        public:

            DelimitedStreamReader(const DelimitedStreamReaderConfig& config): stream(),
                                                                              delim_char(config.delim_char),
                                                                              eor_char(config.eor_char),
                                                                              max_token_sz(config.max_token_sz){}

            DelimitedStreamReader(const ExternalDelimitedStreamReaderConfig& config): DelimitedStreamReader(to_internal_delimited_stream_reader_config(config)){}

            auto put(std::string_view stream_arg) -> std::vector<std::string>
            {
                size_t first                = 0u;
                std::vector<std::string> rs = {};

                for (size_t i = 0u; i < stream_arg.size(); ++i)
                {
                    if (stream_arg[i] == this->delim_char || stream_arg[i] == this->eor_char)
                    {
                        rs.push_back(this->get_delimited_stream(std::string_view(std::next(stream_arg.data(), first),
                                                                                 std::next(stream_arg.data(), i))));

                        first = i + 1u;
                    }
                }

                this->stream.insert(this->stream.end(),
                                    std::next(stream_arg.data(), first),
                                    std::next(stream_arg.data(), stream_arg.size()));

                if (this->max_token_sz.has_value())
                {
                    if (this->stream.size() > this->max_token_sz.value())
                    {
                        throw token_overflow_error{};
                    }
                }

                return rs;
            }
        
        private:

            auto get_delimited_stream(std::string_view app_str) -> std::string
            {
                std::string rs = {};

                if (!this->stream.empty())
                {
                    rs = std::move(this->stream);
                    this->stream = {};
                }

                rs.insert(rs.end(), app_str.begin(), app_str.end());

                return rs;
            }
    };
}

#endif
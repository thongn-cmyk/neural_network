#ifndef __DATA_LOADER_DELIMITED_STREAM_READER_H__
#define __DATA_LOADER_DELIMITED_STREAM_READER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include "stream_delimitor_interface.h"

namespace data_loader::stream_reader
{
    struct token_overflow_error: std::runtime_error
    {
        token_overflow_error(): std::runtime_error("max token size reached"){}
    };

    struct Configuration
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

    class DelimitedStreamReader: public virtual DelimitedStreamReaderInterface
    {
        private:

            std::string stream;
            char delim_char;
            char eor_char;
            std::optional<uint64_t> max_token_sz;

        public:

            DelimitedStreamReader(Configuration config): stream(),
                                                         delim_char(config.delim_char),
                                                         eor_char(config.eor_char),
                                                         max_token_sz(config.max_token_sz){}

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
#ifndef __FILE_SOURCE_H__
#define __FILE_SOURCE_H__

#include "../source_interface.h"
#include "../../delimitor/stream_delimitor.h"

namespace datasource::file_source
{
    struct bad_state_error: std::runtime_error
    {
        bad_state_error(): std::runtime_error("stream delimitor is in incorrect state"){}
    };

    struct Configuration
    {
        datasource::delimitor::stream_delimitor::Configuration delim_config;
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

    class FileLoader: public virtual SourceInterface
    {
        private:

            std::unique_ptr<datasource::delimitor::stream_delimitor::StreamDelimitorInterface> delim_streamer;
            std::fstream f_stream;
            std::optional<std::vector<char>> buf;
            size_t tx_unit_sz;
            bool was_completed;
            bool is_bad_state;

        public:

            static inline constexpr size_t MAX_READ_SZ      = size_t{1} << 20;
            static inline constexpr size_t MIN_BUFFER_SZ    = size_t{1} << 10;
            static inline constexpr size_t MAX_BUFFER_SZ    = size_t{1} << 20;

            FileLoader(Configuration config)
            {
                this->delim_streamer    = std::make_unique<datasource::delimitor::stream_delimitor::DelimitedStreamLoader>(config.delim_config);
                this->f_stream          = std::fstream(config.local_file_path, std::ios::in | std::ios::binary);
                this->tx_unit_sz        = 1u;

                if (config.unit_byte_sz_hint.has_value())
                {
                    this->tx_unit_sz = std::max(this->tx_unit_sz, config.unit_byte_sz_hint.value());
                }

                if (config.read_ahead_buffer_sz_hint.has_value())
                {
                    this->buf = std::vector<char>(std::clamp(config.read_ahead_buffer_sz_hint.value(), MIN_BUFFER_SZ, MAX_BUFFER_SZ), ' ');
                    this->f_stream.rdbuf()->pubsetbuf(this->buf->data(), this->buf->size());
                }

                this->was_completed     = false;
                this->is_bad_state      = false;
            }

            ~FileLoader() noexcept
            {
                this->f_stream.close();
                this->buf = std::nullopt;
            }

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

                auto buf            = std::string(tx_byte_sz, ' ');

                this->f_stream.read(buf.data(), tx_byte_sz);
                buf.resize(this->f_stream.gcount());

                if (buf.size() == 0u)
                {
                    this->was_completed = true;
                    return std::nullopt;
                }

                try
                {
                    return this->delim_streamer->put(buf);
                }
                catch (...)
                {
                    this->is_bad_state = true;
                    throw;
                }
            }
    };
}

#endif
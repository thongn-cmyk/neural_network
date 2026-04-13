#ifndef __DATA_LOADER_FILE_SOURCE_H__
#define __DATA_LOADER_FILE_SOURCE_H__

#include <stdint.h>
#include <stdlib.h>
#include <optional>
#include <string>
#include <memory>
#include <fstream>
#include <data_loader/source/source_loader_interface.h>
#include <data_loader/stream_reader/delimited_stream_reader_interface.h>
#include <data_loader/stream_reader/delimited_stream_reader.h>
#include <utility>
#include <algorithm>
#include <data_loader/source/source_exception.h>
#include <serializer/compact_serializer.h>

namespace data_loader::file_source
{
    using namespace data_loader::source_exception;

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

    class FileLoader: public virtual data_loader::SourceLoaderInterface
    {
        private:

            std::unique_ptr<data_loader::stream_reader::DelimitedStreamReaderInterface> delim_streamer;
            std::ifstream f_stream;
            std::unique_ptr<char[]> buf;
            size_t tx_unit_sz;
            bool was_completed;
            bool is_bad_state;
            size_t expected_sz;
            size_t total_read_bytes;

        public:

            static inline constexpr size_t MAX_READ_SZ      = size_t{1} << 20;
            static inline constexpr size_t MIN_BUFFER_SZ    = size_t{1} << 10;
            static inline constexpr size_t MAX_BUFFER_SZ    = size_t{1} << 20;

            FileLoader(const FileLoaderConfig& config): delim_streamer(std::make_unique<data_loader::stream_reader::DelimitedStreamReader>(config.delim_config)),
                                                        f_stream(config.local_file_path, std::ios::binary),
                                                        buf(nullptr),
                                                        tx_unit_sz(1u),
                                                        was_completed(false),
                                                        is_bad_state(false)
            {
                if (!this->f_stream.is_open())
                {
                    throw bad_resource_pointer_error("bad file open, unable to access file");
                }

                if (config.unit_byte_sz_hint.has_value())
                {
                    this->tx_unit_sz = std::max(this->tx_unit_sz, static_cast<size_t>(config.unit_byte_sz_hint.value()));
                }

                if (config.read_ahead_buffer_sz_hint.has_value())
                {
                    size_t buf_sz   = std::clamp(static_cast<size_t>(config.read_ahead_buffer_sz_hint.value()), MIN_BUFFER_SZ, MAX_BUFFER_SZ);
                    this->buf       = std::make_unique<char[]>(buf_sz);

                    this->f_stream.rdbuf()->pubsetbuf(this->buf.get(), buf_sz);
                }

                this->expected_sz       = this->get_initial_file_size(this->f_stream);
                this->total_read_bytes  = 0u;
            }

            FileLoader(const ExternalFileLoaderConfig& config): FileLoader(to_internal_file_loader_config(config)){}

            ~FileLoader() noexcept
            {
                this->f_stream.close();
                this->buf = nullptr;
            }

            auto get(size_t tx_hint_sz) -> std::optional<std::vector<std::string>>
            {
                if (this->is_bad_state)
                {
                    throw bad_state_error("bad file loader, file loader is in a bad state");
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
                intmax_t read_bytes;

                try
                {
                    this->f_stream.read(buf.data(), tx_byte_sz);
                    read_bytes = this->f_stream.gcount();

                    if (read_bytes < 0)
                    {
                        throw hard_file_read_error("file read went wrong, negative read byte");
                    }
                }
                catch (...)
                {
                    this->is_bad_state = true;
                    throw;
                }

                buf.resize(read_bytes);
                this->total_read_bytes += read_bytes;

                if (buf.size() == 0u)
                {
                    if (this->total_read_bytes != this->expected_sz)
                    {
                        this->is_bad_state = true;
                        throw hard_file_read_error("file read went wrong, mismatched size");
                    }

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
        
        private:

            static auto get_initial_file_size(std::ifstream& f_stream) -> size_t
            {
                f_stream.exceptions(std::ios::badbit | std::ios::failbit);

                f_stream.seekg(0, std::ios::end);
                std::streampos rs = f_stream.tellg();
                f_stream.seekg(0, std::ios::beg);

                f_stream.exceptions(std::ios::goodbit);

                if (rs < 0)
                {
                    throw std::runtime_error("bad file operation");
                }

                return rs;
            }
    };
}

#endif
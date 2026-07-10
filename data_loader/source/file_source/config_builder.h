#ifndef __DATA_LOADER_SOURCE_FILE_SOURCE_CONFIG_BUILDER_H__
#define __DATA_LOADER_SOURCE_FILE_SOURCE_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include "model.h"
#include <stl_extension/stdx.h>
#include <chrono>
#include <string>
#include <optional>
#include <data_loader/stream_reader/config_builder.h>
#include <variant>

namespace data_loader::source::file_source
{
    class FileLoaderConfigBuilder
    {
        private:

            using self  = FileLoaderConfigBuilder;

            std::optional<std::string> local_file_path;
            std::optional<uint64_t> token_unit_sz;
            std::optional<uint64_t> download_sz;

            data_loader::stream_reader::DelimitedStreamReaderConfigBuilder delimited_stream_reader_config_builder;

        public:

            auto set_local_file_path(const std::string& local_file_path) -> self&
            {
                this->local_file_path   = local_file_path;

                return *this;
            }

            auto set_token_unit_size(size_t sz) -> self&
            {
                this->token_unit_sz = stdx::throw_integer_cast<uint64_t>(sz);

                return *this;
            }

            auto set_token_max_unit_size(size_t sz) -> self&
            {
                this->delimited_stream_reader_config_builder.set_max_size_per_token(sz);

                return *this;
            }

            auto set_download_size(size_t sz) -> self&
            {
                this->download_sz   = stdx::throw_integer_cast<uint64_t>(sz);

                return *this;
            }

            auto set_token_delimitor(char c) -> self&
            {
                this->delimited_stream_reader_config_builder.set_token_delimitor(c);

                return *this;
            }

            auto set_token_eor(char c) -> self&
            {
                this->delimited_stream_reader_config_builder.set_token_eor(c);

                return *this;
            }

            auto build() -> ExternalFileLoaderConfig
            {
                return this->get_external_file_loader_config();
            }

        private:

            auto get_internal_file_loader_config() -> FileLoaderConfig
            {
                if (!this->local_file_path.has_value())
                {
                    throw std::invalid_argument("bad local file path, not set");
                }

                return
                {
                    .delim_config               = this->delimited_stream_reader_config_builder.build(),
                    .local_file_path            = this->local_file_path.value(),
                    .read_ahead_buffer_sz_hint  = this->download_sz,
                    .unit_byte_sz_hint          = this->token_unit_sz
                };
            }

            auto get_external_file_loader_config() -> ExternalFileLoaderConfig
            {
                return to_external_file_loader_config(this->get_internal_file_loader_config());
            }
    };
}

#endif
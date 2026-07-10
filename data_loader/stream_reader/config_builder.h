#ifndef __DATA_LOADER_STREAM_READER_CONFIG_BUILDER_H__
#define __DATA_LOADER_STREAM_READER_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <optional>
#include <string>
#include "model.h"

namespace data_loader::stream_reader
{
    class DelimitedStreamReaderConfigBuilder
    {
        private:

            char token_delim;
            char token_eor;
            std::optional<uint64_t> token_max_unit_sz;

            static inline constexpr char DEFAULT_TOKEN_DELIM    = ',';
            static inline constexpr char DEFAULT_TOKEN_EOR      = '\0';

            using self = DelimitedStreamReaderConfigBuilder;

        public:

            DelimitedStreamReaderConfigBuilder(): token_delim(DEFAULT_TOKEN_DELIM),
                                                  token_eor(DEFAULT_TOKEN_EOR),
                                                  token_max_unit_sz(std::nullopt){}


            auto set_token_delimitor(char c) -> self&
            {
                this->token_delim   = c;

                return *this;
            }

            auto set_token_eor(char c) -> self&
            {
                this->token_eor = c;

                return *this;
            }

            auto set_max_size_per_token(size_t sz) -> self&
            {
                this->token_max_unit_sz = stdx::throw_integer_cast<uint64_t>(sz);

                return *this;
            }

            auto build() -> data_loader::stream_reader::ExternalDelimitedStreamReaderConfig
            {
                return this->get_external_delimited_stream_reader_config();
            }

        private:

            auto get_internal_delimited_stream_reader_config() -> data_loader::stream_reader::DelimitedStreamReaderConfig
            {
                return
                {
                    .delim_char     = this->token_delim,
                    .eor_char       = this->token_eor,
                    .max_token_sz   = this->token_max_unit_sz
                };
            }

            auto get_external_delimited_stream_reader_config() -> data_loader::stream_reader::ExternalDelimitedStreamReaderConfig
            {
                return data_loader::stream_reader::to_external_delimited_stream_reader_config
                (
                    this->get_internal_delimited_stream_reader_config()
                );
            }
    };
}

#endif
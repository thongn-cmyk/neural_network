#ifndef __DATA_LOADER_DELIMITED_STREAM_READER_INTERFACE_H__
#define __DATA_LOADER_DELIMITED_STREAM_READER_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <string_view>

namespace data_loader::stream_reader
{
    class DelimitedStreamReaderInterface
    {
        public:

            virtual ~DelimitedStreamReaderInterface() noexcept = default;
            virtual auto put(std::string_view stream) -> std::vector<std::string> = 0;
    };
}

#endif
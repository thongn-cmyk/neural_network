#ifndef __STREAM_DELIMITOR_INTERFACE_H__
#define __STREAM_DELIMITOR_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <string_view>

namespace datasource::delimitor::stream_delimitor
{
    class DelimitedStreamReaderInterface
    {
        public:

            virtual ~DelimitedStreamReaderInterface() noexcept = default;
            virtual auto put(std::string_view stream) -> std::vector<std::string> = 0;
    };
}

#endif
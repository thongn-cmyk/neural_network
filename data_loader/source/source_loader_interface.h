#ifndef __DATA_LOADER_SOURCE_INTERFACE_H__
#define __DATA_LOADER_SOURCE_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <optional>

namespace data_loader
{
    class SourceLoaderInterface
    {
        public:

            virtual ~SourceLoaderInterface() noexcept = default;
            virtual auto get(size_t tx_hint_sz) -> std::optional<std::vector<std::string>> = 0;
    };
}

#endif
#ifndef __SOURCE_INTERFACE_H__
#define __SOURCE_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <optional>

namespace datasource
{
    class SourceInterface
    {
        public:

            virtual ~SourceInterface() noexcept = default;
            virtual auto get(size_t tx_hint_sz) -> std::optional<std::vector<std::string>> = 0;
    };
}

#endif
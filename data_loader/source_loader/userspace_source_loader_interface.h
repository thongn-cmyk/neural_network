#ifndef __USER_SPACE_SOURCE_LOADER_INTERFACE_H__
#define __USER_SPACE_SOURCE_LOADER_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include <common_exception/cancellation_token.h>
#include <optional>
#include <string>

namespace data_loader::source_loader
{
    class UserSpaceSourceLoaderInterface
    {
        public:

            virtual ~UserSpaceSourceLoaderInterface() noexcept = default;

            virtual auto get(common_exception::CancellationTokenInterface& cancellation_token) -> std::optional<std::string> = 0;
            virtual auto is_ready() -> bool = 0;
    };
}

#endif
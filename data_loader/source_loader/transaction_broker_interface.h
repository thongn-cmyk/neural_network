#ifndef __DATA_LOADER_TRANSACTION_REQUESTOR_INTERFACE_H__
#define __DATA_LOADER_TRANSACTION_REQUESTOR_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>

namespace data_loader::source_loader::broker
{
    class TransactionBrokerInterface
    {
        public:

            virtual ~TransactionBrokerInterface() noexcept = default;

            virtual auto get(size_t tx_hint_sz,
                             common_exception::CancellationTokenInterface& cancellation_token) -> std::optional<std::vector<std::string>> = 0;
    };

    
}

#endif
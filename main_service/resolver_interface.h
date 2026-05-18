#ifndef __MAIN_SERVICE_RESOLVER_INTERFACE_H__
#define __MAIN_SERVICE_RESOLVER_INTERFACE_H__

#include <memory>
#include "resolvable_interface.h"

namespace main_service::main_broker
{
    class ResolverInterface
    {
        public:

            virtual ~ResolverInterface() noexcept = default;

            virtual void resolve(std::shared_ptr<Resolvable> resolvable) = 0;
    };
}

#endif
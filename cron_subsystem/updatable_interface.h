#ifndef __CRON_SUBSYSTEM_UPDATABLE_INTERFACE_H__
#define __CRON_SUBSYSTEM_UPDATABLE_INTERFACE_H__

#include <stdint.h>
#include <stdlib.h>

namespace cron_subsystem
{
    class UpdatableInterface
    {
        public:

            virtual ~UpdatableInterface() noexcept = default;

            virtual void update() = 0;
    };
}

#endif
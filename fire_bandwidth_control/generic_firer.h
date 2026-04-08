#ifndef __FIRE_BANDWIDTH_CONTROL_GENERIC_FIRER_H__
#define __FIRE_BANDWIDTH_CONTROL_GENERIC_FIRER_H__

#include <stdint.h>
#include <stdlib.h>
#include <stl_extension/stdx.h>
#include "temporal_firer.h"
#include <variant>
#include "fireable_firer_interface.h"
#include "fireable_interface.h"
#include <common_exception/common_exception.h>
#include <memory>

namespace fire_bandwidth_control::generic_firer
{
    using namespace fire_bandwidth_control::interface;

    struct GenericFirerConfig
    {
        std::variant<stdx::reflectible_monostate, fire_bandwidth_control::temporal_firer::TemporalFirerConfig> config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(config);
        }
    };

    class GenericFirer: public virtual FireableFirerInterface
    {
        private:

            std::unique_ptr<FireableFirerInterface> base;

        public:

            GenericFirer(const GenericFirerConfig& config)
            {
                if (std::holds_alternative<fire_bandwidth_control::temporal_firer::TemporalFirerConfig>(config.config))
                {
                    this->base = std::make_unique<fire_bandwidth_control::temporal_firer::TemporalFirer>(std::get<fire_bandwidth_control::temporal_firer::TemporalFirerConfig>(config.config));
                }
                else
                {
                    throw std::invalid_argument("bad firer config, dispatch code not found");
                }
            }

            void run(FireableInterface& fireable,
                     common_exception::CancellationTokenInterface& cancellation_token)
            {
                this->base->run(fireable, cancellation_token);
            }
    };
}

#endif
#ifndef __FIRE_BANDWIDTH_CONTROL_FREE_FIRER_H__
#define __FIRE_BANDWIDTH_CONTROL_FREE_FIRER_H__

#include <stdint.h>
#include <stdlib.h>
#include "fireable_interface.h"
#include "fireable_firer_interface.h"
#include <chrono>
#include <common_exception/common_exception.h>
#include <common_exception/cancellation_token.h>
#include <functional>
#include <serializer/compact_serializer.h>

namespace fire_bandwidth_control::free_firer
{
    using namespace fire_bandwidth_control::interface;

    struct FreeFirerConfig
    {
        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            (void) reflector;
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            (void) reflector;
        }
    };

    struct ExternalFreeFirerConfig
    {
        std::string config_bytestream;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(config_bytestream);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(config_bytestream);
        }
    };

    auto to_external_free_firer_config(const FreeFirerConfig& config) -> ExternalFreeFirerConfig
    {
        return ExternalFreeFirerConfig
        {
            .config_bytestream  = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_free_firer_config(const ExternalFreeFirerConfig& config) -> FreeFirerConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<FreeFirerConfig>(config.config_bytestream);
    }

    class FreeFirer: public virtual FireableFirerInterface
    {
        public:

            FreeFirer() = default;
            FreeFirer(const FreeFirerConfig&): FreeFirer(){}
            FreeFirer(const ExternalFreeFirerConfig& config): FreeFirer(to_internal_free_firer_config(config)){}

            void run(FireableInterface& fireable,
                     common_exception::CancellationTokenInterface& cancellation_token)
            {
                while (true)
                {
                    if (!fireable.fire_one(cancellation_token))
                    {
                        return;
                    }
                }
            }
    };
}

#endif
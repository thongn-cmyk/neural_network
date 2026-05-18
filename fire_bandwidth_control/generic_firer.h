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
        std::variant<stdx::reflectible_monostate, fire_bandwidth_control::temporal_firer::ExternalTemporalFirerConfig> config;

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

    struct ExternalGenericFirerConfig
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

    auto to_external_generic_firer_config(const GenericFirerConfig& config) -> ExternalGenericFirerConfig
    {
        return ExternalGenericFirerConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_generic_firer_config(const ExternalGenericFirerConfig& config) -> GenericFirerConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<GenericFirerConfig>(config.config_bytestream);
    }

    class GenericFirer: public virtual FireableFirerInterface
    {
        private:

            std::unique_ptr<FireableFirerInterface> base;

        public:

            GenericFirer(const GenericFirerConfig& config)
            {
                if (std::holds_alternative<fire_bandwidth_control::temporal_firer::ExternalTemporalFirerConfig>(config.config))
                {
                    this->base = std::make_unique<fire_bandwidth_control::temporal_firer::TemporalFirer>(std::get<fire_bandwidth_control::temporal_firer::ExternalTemporalFirerConfig>(config.config));
                }
                else
                {
                    throw std::invalid_argument("bad firer config, dispatch code not found");
                }
            }

            GenericFirer(const ExternalGenericFirerConfig& config): GenericFirer(to_internal_generic_firer_config(config)){}

            void run(FireableInterface& fireable,
                     common_exception::CancellationTokenInterface& cancellation_token)
            {
                this->base->run(fireable, cancellation_token);
            }
    };
}

#endif
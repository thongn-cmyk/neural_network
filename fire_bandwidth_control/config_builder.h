#ifndef __FIRE_BANDWIDTH_CONTROL_CONFIG_BUILDER_H__
#define __FIRE_BANDWIDTH_CONTROL_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include "generic_firer.h"

namespace fire_bandwidth_control::config_builder
{
    class FreeFirerBuilder
    {
        public:

            auto build() -> fire_bandwidth_control::generic_firer::ExternalGenericFirerConfig
            {
                return this->get_external_generic_firer_config();
            }
        
        private:

            auto get_internal_free_firer_config() -> fire_bandwidth_control::free_firer::FreeFirerConfig
            {
                return {};
            }

            auto get_external_free_firer_config() -> fire_bandwidth_control::free_firer::ExternalFreeFirerConfig
            {
                return fire_bandwidth_control::free_firer::to_external_free_firer_config(this->get_internal_free_firer_config());
            }

            auto get_internal_generic_firer_config() -> fire_bandwidth_control::generic_firer::GenericFirerConfig
            {
                return
                {
                    .config = this->get_external_free_firer_config()
                };
            }

            auto get_external_generic_firer_config() -> fire_bandwidth_control::generic_firer::ExternalGenericFirerConfig
            {
                return fire_bandwidth_control::generic_firer::to_external_generic_firer_config(this->get_internal_generic_firer_config());
            }
    };
}

#endif
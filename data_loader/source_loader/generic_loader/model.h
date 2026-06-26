#ifndef __DATA_LOADER_SOURCE_LOADER_GENERIC_LOADER_MODEL_H__
#define __DATA_LOADER_SOURCE_LOADER_GENERIC_LOADER_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <variant>
#include <data_loader/source_loader/detach_loader/model.h>
#include <data_loader/source_loader/wait_loader/model.h>
#include <stl_extension/stdx.h>
#include <serializer/compact_serializer.h>
#include <string>

namespace data_loader::source_loader::generic_loader
{
    struct GenericLoaderConfig
    {
        std::variant<stdx::reflectible_monostate,
                     data_loader::source_loader::detach_loader::ExternalDetachLoaderConfig,
                     data_loader::source_loader::wait_loader::ExternalWaitLoaderConfig> config;

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

    struct ExternalGenericLoaderConfig
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

    auto to_external_generic_loader_config(const GenericLoaderConfig& config) -> ExternalGenericLoaderConfig
    {
        return ExternalGenericLoaderConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_generic_loader_config(const ExternalGenericLoaderConfig& config) -> GenericLoaderConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<GenericLoaderConfig>(config.config_bytestream);
    }
}

#endif
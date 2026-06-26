#ifndef __DATA_LOADER_SOURCE_LOADER_MULTISOURCE_LOADER_MODEL_H__
#define __DATA_LOADER_SOURCE_LOADER_MULTISOURCE_LOADER_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <data_loader/source_loader/generic_loader/model.h>
#include <string>
#include <serializer/compact_serializer.h>
#include <vector>

namespace data_loader::source_loader::multisource_loader
{
    struct MultisourceLoaderConfig
    {
        std::vector<data_loader::source_loader::generic_loader::ExternalGenericLoaderConfig> config_vec;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(config_vec);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(config_vec);
        }
    };

    struct ExternalMultisourceLoaderConfig
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

    auto to_external_multisource_loader_config(const MultisourceLoaderConfig& config) -> ExternalMultisourceLoaderConfig
    {
        return ExternalMultisourceLoaderConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_multisource_loader_config(const ExternalMultisourceLoaderConfig& config) -> MultisourceLoaderConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<MultisourceLoaderConfig>(config.config_bytestream);
    }
}

#endif
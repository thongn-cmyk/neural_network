#ifndef __DATA_LOADER_SOURCE_GENERIC_SOURCE_MODEL_H__
#define __DATA_LOADER_SOURCE_GENERIC_SOURCE_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <stl_extension/stdx.h>
#include <data_loader/source/azure_source/model.h>
#include <data_loader/source/file_source/model.h>
#include <data_loader/source/gcs_source/model.h>
#include <data_loader/source/kafka_broker_source/model.h>
#include <data_loader/source/s3_source/model.h>
#include <variant>
#include <string>
#include <serializer/compact_serializer.h>

namespace data_loader::source::generic_source
{
    struct GenericReaderConfig
    {
        std::variant<stdx::reflectible_monostate,
                     data_loader::source::azure_source::ExternalAzureLoaderConfig,
                     data_loader::source::file_source::ExternalFileLoaderConfig,
                     data_loader::source::gcs_source::ExternalGCSLoaderConfig,
                     data_loader::source::kafka_broker_source::ExternalKafkaBrokerConfig,
                     data_loader::source::s3_source::ExternalS3LoaderConfig> source;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(source);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(source);
        }
    };

    struct ExternalGenericReaderConfig
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

    auto to_external_generic_reader_config(const GenericReaderConfig& config) -> ExternalGenericReaderConfig
    {
        return ExternalGenericReaderConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_generic_reader_config(const ExternalGenericReaderConfig& config) -> GenericReaderConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<GenericReaderConfig>(config.config_bytestream);
    }
}

#endif
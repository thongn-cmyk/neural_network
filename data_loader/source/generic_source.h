#ifndef __DATA_LOADER_GENERIC_SOURCE_H__
#define __DATA_LOADER_GENERIC_SOURCE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <stl_extension/stdx.h>

#include "file_source/file_source.h"
#include "kafka_broker_source/kafka_broker_source.h"
// #include "s3_source/s3_source.h"
#include <data_loader/exception_base.h>
#include <serializer/compact_serializer.h>

namespace data_loader::generic_source
{
    using namespace data_loader::exception_base;

    struct GenericReaderConfig
    {
        std::variant<stdx::reflectible_monostate,
                     data_loader::file_source::ExternalFileLoaderConfig,
                    //  data_loader::s3_source::ExternalS3LoaderConfig,
                     data_loader::kafka_broker_source::ExternalKafkaBrokerConfig> source;

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

    class GenericReader: public virtual data_loader::SourceLoaderInterface
    {
        private:

            std::unique_ptr<data_loader::SourceLoaderInterface> base;

        public:

            GenericReader(const GenericReaderConfig& config)
            {
                if (std::holds_alternative<data_loader::file_source::ExternalFileLoaderConfig>(config.source))
                {
                    this->base  = std::make_unique<data_loader::file_source::FileLoader>(std::get<data_loader::file_source::ExternalFileLoaderConfig>(config.source));
                }
                // else if (std::holds_alternative<data_loader::s3_source::ExternalS3LoaderConfig>(config.source))
                // {
                //     this->base  = std::make_unique<data_loader::s3_source::S3Loader>(std::get<data_loader::s3_source::ExternalS3LoaderConfig>(config.source));
                // }
                // else if (std::holds_alternative<data_loader::kafka_broker_source::Configuration>(config.source))
                // {
                //     this->base = std::make_unique<data_loader::kafka_broker_source::KafkaBrokerLoader>(std::get<data_loader::kafka_broker_source::Configuration>(config.source));
                // }
                else
                {
                    throw invalid_argument_base("bad configuration, polymorphic state is not defined");
                }
            }

            GenericReader(const ExternalGenericReaderConfig& config): GenericReader(to_internal_generic_reader_config(config)){}

            auto get(size_t tx_hint_sz) -> std::optional<std::vector<std::string>>
            {
                return this->base->get(tx_hint_sz);
            }
    };
}

#endif
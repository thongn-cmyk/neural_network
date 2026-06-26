#ifndef __DATA_LOADER_SOURCE_GENERIC_SOURCE_GENERIC_SOURCE_H__
#define __DATA_LOADER_SOURCE_GENERIC_SOURCE_GENERIC_SOURCE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <stl_extension/stdx.h>

#include <data_loader/source/file_source/file_source.h>
#include <data_loader/source/kafka_broker_source/kafka_broker_source.h>
//#include <data_loader/source/s3_source/s3_source.h>

#include <data_loader/exception_base.h>

#include "model.h"

namespace data_loader::source::generic_source
{
    using namespace data_loader::exception_base;

    class GenericReader: public virtual data_loader::source::SourceLoaderInterface
    {
        private:

            std::unique_ptr<data_loader::source::SourceLoaderInterface> base;

        public:

            GenericReader(const GenericReaderConfig& config)
            {
                if (std::holds_alternative<data_loader::source::file_source::ExternalFileLoaderConfig>(config.source))
                {
                    this->base  = std::make_unique<data_loader::source::file_source::FileLoader>(std::get<data_loader::source::file_source::ExternalFileLoaderConfig>(config.source));
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
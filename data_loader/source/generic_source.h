#ifndef __DATA_LOADER_GENERIC_SOURCE_H__
#define __DATA_LOADER_GENERIC_SOURCE_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include "file_source/file_source.h"
#include "kafka_broker_source/kafka_broker_source.h"
#include "s3_source/s3_source.h"
#include <stl_extension/stdx.h>

namespace data_loader::generic_source
{
    struct Configuration
    {
        std::variant<stdx::reflectible_monostate,
                     data_loader::file_source::Configuration,
                     data_loader::kafka_broker_source::Configuration,
                     data_loader::s3_source::Configuration> source;

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

    class GenericReader: public virtual data_loader::SourceLoaderInterface
    {
        private:

            std::unique_ptr<data_loader::SourceLoaderInterface> base;

        public:

            GenericReader(Configuration config)
            {
                if (std::holds_alternative<data_loader::file_source::Configuration>(config.source))
                {
                    this->base = std::make_unique<data_loader::file_source::FileLoader>(std::get<data_loader::file_source::Configuration>(config.source));
                }
                else if (std::holds_alternative<data_loader::s3_source::Configuration>(config.source))
                {
                    this->base = std::make_unique<data_loader::s3_source::S3Loader>(std::get<data_loader::s3_source::Configuration>(config.source));
                }
                else if (std::holds_alternative<data_loader::kafka_broker_source::Configuration>(config.source))
                {
                    this->base = std::make_unique<data_loader::kafka_broker_source::KafkaBrokerLoader>(std::get<data_loader::kafka_broker_source::Configuration>(config.source));
                }
                else
                {
                    throw std::invalid_argument("bad configuration, polymorphic state is not defined");
                }
            }

            auto get(size_t tx_hint_sz) -> std::optional<std::vector<std::string>>
            {
                return this->base->get();
            }
    };
}

#endif
#ifndef __DATA_LOADER_SOURCE_GENERIC_SOURCE_CONFIG_BUILDER_H__
#define __DATA_LOADER_SOURCE_GENERIC_SOURCE_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <stl_extension/stdx.h>
#include <data_loader/source/azure_source/config_builder.h>
#include <data_loader/source/file_source/config_builder.h>
#include <data_loader/source/gcs_source/config_builder.h>
#include <data_loader/source/s3_source/config_builder.h>
#include <variant>
#include "model.h"

namespace data_loader::source::generic_source
{
    class GenericReaderConfigBuilder
    {
        private:

            using AzureLoaderConfigBuilder  = data_loader::source::azure_source::AzureLoaderConfigBuilder;
            using FileLoaderConfigBuilder   = data_loader::source::file_source::FileLoaderConfigBuilder;
            using GCSLoaderConfigBuilder    = data_loader::source::gcs_source::GCSLoaderConfigBuilder;
            using S3LoaderConfigBuilder     = data_loader::source::s3_source::S3LoaderConfigBuilder;

            std::variant<std::monostate,
                         std::unique_ptr<AzureLoaderConfigBuilder>,
                         std::unique_ptr<FileLoaderConfigBuilder>,
                         std::unique_ptr<GCSLoaderConfigBuilder>,
                         std::unique_ptr<S3LoaderConfigBuilder>> generic_builder;

        public:

            auto as_azure_loader() -> AzureLoaderConfigBuilder&
            {
                if (!std::holds_alternative<std::unique_ptr<AzureLoaderConfigBuilder>>(this->generic_builder))
                {
                    this->generic_builder   = std::make_unique<AzureLoaderConfigBuilder>();
                }

                return *std::get<std::unique_ptr<AzureLoaderConfigBuilder>>(this->generic_builder);
            }

            auto as_file_loader() -> FileLoaderConfigBuilder&
            {
                if (!std::holds_alternative<std::unique_ptr<FileLoaderConfigBuilder>>(this->generic_builder))
                {
                    this->generic_builder   = std::make_unique<FileLoaderConfigBuilder>();
                }

                return *std::get<std::unique_ptr<FileLoaderConfigBuilder>>(this->generic_builder);
            }

            auto as_gcs_loader() -> GCSLoaderConfigBuilder&
            {
                if (!std::holds_alternative<std::unique_ptr<GCSLoaderConfigBuilder>>(this->generic_builder))
                {
                    this->generic_builder   = std::make_unique<GCSLoaderConfigBuilder>();
                }

                return *std::get<std::unique_ptr<GCSLoaderConfigBuilder>>(this->generic_builder);
            }

            auto as_s3_loader() -> S3LoaderConfigBuilder&
            {
                if (!std::holds_alternative<std::unique_ptr<S3LoaderConfigBuilder>>(this->generic_builder))
                {
                    this->generic_builder   = std::make_unique<S3LoaderConfigBuilder>();
                }

                return *std::get<std::unique_ptr<S3LoaderConfigBuilder>>(this->generic_builder);
            }

            auto build() -> ExternalGenericReaderConfig 
            {
                return this->get_external_generic_reader_config();
            }

        private:

            auto get_internal_generic_reader_config() -> GenericReaderConfig
            {
                if (std::holds_alternative<std::monostate>(this->generic_builder))
                {
                    throw std::invalid_argument("bad builder option, not set");
                }

                if (this->generic_builder.valueless_by_exception())
                {
                    throw std::invalid_argument("bad builder option, corrupted variant");
                }

                GenericReaderConfig rs  = {};
                auto callback           = [&rs](auto& builder)
                {
                    if constexpr(std::is_same_v<std::monostate, std::decay_t<decltype(builder)>>)
                    {
                        std::abort();
                    }
                    else
                    {
                        rs.source   = builder->build();
                    }
                };

                std::visit(callback,
                           this->generic_builder);

                return rs;
            }

            auto get_external_generic_reader_config() -> ExternalGenericReaderConfig
            {
                return to_external_generic_reader_config
                (
                    this->get_internal_generic_reader_config()
                );
            }
    };
}

#endif
#ifndef __DATA_LOADER_SOURCE_LOADER_MULTISOURCE_LOADER_CONFIG_BUILDER_H__
#define __DATA_LOADER_SOURCE_LOADER_MULTISOURCE_LOADER_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include "model.h"
#include <data_loader/source_loader/generic_loader/config_builder.h>
#include <unordered_map>

namespace data_loader::source_loader::multisource_loader
{
    class MultisourceLoaderConfigBuilder
    {
        private:

            using GenericLoaderConfigBuilder   = data_loader::source_loader::generic_loader::GenericLoaderConfigBuilder;

            std::unordered_map<size_t, GenericLoaderConfigBuilder> config_builder_map;

        public:

            auto get(size_t idx) -> GenericLoaderConfigBuilder&
            {
                return this->config_builder_map[idx];
            }

            auto build() -> ExternalMultisourceLoaderConfig
            {
                return this->get_external_multisource_loader_config();
            }

        private:

            auto get_internal_multisource_loader_config() -> MultisourceLoaderConfig
            {
                std::vector<data_loader::source_loader::generic_loader::ExternalGenericLoaderConfig> config_vec{};

                for (auto& [idx, builder]: this->config_builder_map)
                {
                    config_vec.push_back(builder.build());
                }

                return
                {
                    .config_vec = std::move(config_vec)
                };
            }

            auto get_external_multisource_loader_config() -> ExternalMultisourceLoaderConfig
            {
                return to_external_multisource_loader_config
                (
                    this->get_internal_multisource_loader_config()
                );
            }
    };
}

#endif
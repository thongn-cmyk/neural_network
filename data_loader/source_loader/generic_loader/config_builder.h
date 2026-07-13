#ifndef __DATA_LOADER_SOURCE_LOADER_GENERIC_LOADER_CONFIG_BUILDER_H__
#define __DATA_LOADER_SOURCE_LOADER_GENERIC_LOADER_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include <variant>
#include <stl_extension/stdx.h>
#include "model.h"
#include <data_loader/source_loader/wait_loader/config_builder.h>

namespace data_loader::source_loader::generic_loader
{
    class GenericLoaderConfigBuilder
    {
        private:

            using WaitLoaderConfigBuilder   = data_loader::source_loader::wait_loader::WaitLoaderConfigBuilder;

            std::variant<std::monostate,
                         std::unique_ptr<WaitLoaderConfigBuilder>> loader_config_builder;

        public:

            auto as_wait_loader() -> WaitLoaderConfigBuilder&
            {
                if (!std::holds_alternative<std::unique_ptr<WaitLoaderConfigBuilder>>(this->loader_config_builder))
                {
                    this->loader_config_builder = std::make_unique<WaitLoaderConfigBuilder>();
                }

                return *std::get<std::unique_ptr<WaitLoaderConfigBuilder>>(this->loader_config_builder);
            }

            auto build() -> ExternalGenericLoaderConfig
            {
                return this->get_external_generic_loader_config();
            }

        private:
            
            auto get_internal_generic_loader_config() -> GenericLoaderConfig
            {
                if (std::holds_alternative<std::monostate>(this->loader_config_builder))
                {
                    throw std::invalid_argument("bad loader option, not set");
                }

                if (this->loader_config_builder.valueless_by_exception())
                {
                    throw std::invalid_argument("bad loader option, bad variant");
                }

                GenericLoaderConfig rs{};

                auto callback = [&rs](auto& builder)
                {
                    if constexpr(std::is_same_v<std::monostate, std::decay_t<decltype(builder)>>)
                    {
                        std::abort();
                    }
                    else
                    {
                        rs.config   = builder->build();
                    }
                };

                std::visit(callback,
                           this->loader_config_builder);

                return rs;
            }

            auto get_external_generic_loader_config() -> ExternalGenericLoaderConfig
            {
                return to_external_generic_loader_config(this->get_internal_generic_loader_config());
            }
    };
}

#endif
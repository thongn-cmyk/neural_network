#ifndef __DATA_LOADER_GENERIC_SOURCE_LOADER_H__
#define __DATA_LOADER_GENERIC_SOURCE_LOADER_H__

#include "detach_loader.h"
#include "wait_loader.h"
#include <variant>
#include <stl_extension/stdx.h>
#include <data_loader/exception_base.h>

namespace data_loader::source_loader::generic_loader
{
    using namespace data_loader::exception_base;

    struct GenericLoaderConfig
    {
        std::variant<stdx::reflectible_monostate,
                     data_loader::source_loader::detach_loader::DetachLoaderConfig,
                     data_loader::source_loader::wait_loader::WaitLoaderConfig> config;

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

    class GenericLoader: public virtual data_loader::source_loader::UserSpaceSourceLoaderInterface
    {
        private:

            std::unique_ptr<data_loader::source_loader::UserSpaceSourceLoaderInterface> base;

        public:

            GenericLoader(const GenericLoaderConfig& config)
            {
                if (std::holds_alternative<data_loader::source_loader::wait_loader::WaitLoaderConfig>(config.config))
                {
                    this->base = std::make_unique<data_loader::source_loader::wait_loader::WaitLoader>(std::get<data_loader::source_loader::wait_loader::WaitLoaderConfig>(config.config));
                }
                else
                {
                    throw invalid_argument_base("bad data loader config, dispatch code not found");
                }
            }

            auto get(common_exception::CancellationTokenInterface& cancellation_token) -> std::optional<std::string>
            {
                return this->base->get(cancellation_token);
            }

            auto is_ready() -> bool
            {
                return this->base->is_ready();
            }
    };
}

#endif
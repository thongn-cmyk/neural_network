#ifndef __DATA_LOADER_GENERIC_SOURCE_LOADER_H__
#define __DATA_LOADER_GENERIC_SOURCE_LOADER_H__

#include <memory>
#include <data_loader/source_loader/detach_loader/detach_loader.h>
#include <data_loader/source_loader/wait_loader/wait_loader.h>
#include <variant>
#include <stl_extension/stdx.h>
#include <data_loader/exception_base.h>
#include "model.h"

namespace data_loader::source_loader::generic_loader
{
    using namespace data_loader::exception_base;

    class GenericLoader: public virtual data_loader::source_loader::UserSpaceSourceLoaderInterface
    {
        private:

            std::unique_ptr<data_loader::source_loader::UserSpaceSourceLoaderInterface> base;

        public:

            GenericLoader(const GenericLoaderConfig& config)
            {
                if (std::holds_alternative<data_loader::source_loader::wait_loader::ExternalWaitLoaderConfig>(config.config))
                {
                    this->base = std::make_unique<data_loader::source_loader::wait_loader::WaitLoader>(std::get<data_loader::source_loader::wait_loader::ExternalWaitLoaderConfig>(config.config));
                }
                else
                {
                    throw invalid_argument_base("bad data loader config, dispatch code not found");
                }
            }

            GenericLoader(const ExternalGenericLoaderConfig& config): GenericLoader(to_internal_generic_loader_config(config)){}

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
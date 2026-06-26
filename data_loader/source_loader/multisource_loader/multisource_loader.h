#ifndef __DATA_LOADER_MULTISOURCE_SOURCE_LOADER_H__
#define __DATA_LOADER_MULTISOURCE_SOURCE_LOADER_H__

#include <vector>
#include <memory>
#include <stdint.h>
#include <stdlib.h>
#include <data_loader/source_loader/generic_loader/generic_loader.h>
#include <vector>
#include <data_loader/source_loader/userspace_source_loader_interface.h>
#include <common_exception/cancellation_token.h>
#include "model.h"

namespace data_loader::source_loader::multisource_loader
{
    class MultisourceLoader: public virtual data_loader::source_loader::UserSpaceSourceLoaderInterface
    {
        private:

            std::vector<std::unique_ptr<data_loader::source_loader::UserSpaceSourceLoaderInterface>> base_vec;
            size_t offset;

        public:

            MultisourceLoader(const MultisourceLoaderConfig& config): base_vec(),
                                                                      offset(0u)
            {
                for (const auto& sub_config: config.config_vec)
                {
                    this->base_vec.push_back(std::make_unique<data_loader::source_loader::generic_loader::GenericLoader>(sub_config));
                }
            }

            MultisourceLoader(const ExternalMultisourceLoaderConfig& config): MultisourceLoader(to_internal_multisource_loader_config(config)){}

            auto get(common_exception::CancellationTokenInterface& cancellation_token) -> std::optional<std::string>
            {
                while (true)
                {
                    if (this->offset == this->base_vec.size())
                    {
                        return std::nullopt;
                    }

                    auto rs = this->base_vec[this->offset]->get(cancellation_token);

                    if (!rs.has_value())
                    {
                        this->offset += 1;
                        continue;
                    }

                    return rs;
                }
            }

            auto is_ready() -> bool
            {
                if (this->offset == this->base_vec.size())
                {
                    return true;
                }

                return this->base_vec[this->offset]->is_ready();
            }
    };
}

#endif
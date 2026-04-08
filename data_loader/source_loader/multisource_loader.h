#ifndef __DATA_LOADER_MULTISOURCE_SOURCE_LOADER_H__
#define __DATA_LOADER_MULTISOURCE_SOURCE_LOADER_H__

#include <stdint.h>
#include <stdlib.h>
#include "generic_loader.h"
#include <vector>
#include "userspace_source_loader_interface.h"
#include <common_exception/cancellation_token.h>

namespace data_loader::source_loader::multisource_loader
{
    struct MultisourceLoaderConfig
    {
        std::vector<data_loader::source_loader::generic_loader::GenericLoaderConfig> config_vec;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(config_vec);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(config_vec);
        }
    };

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
    };
}

#endif
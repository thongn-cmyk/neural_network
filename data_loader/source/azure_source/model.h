#ifndef __DATA_LOADER_SOURCE_AZURE_SOURCE_MODEL_H__
#define __DATA_LOADER_SOURCE_AZURE_SOURCE_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>

namespace data_loader::azure_source
{
    struct SecuredAzureClientConfig
    {

    };

    struct ExternalSecuredAzureClientConfig
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
}

#endif
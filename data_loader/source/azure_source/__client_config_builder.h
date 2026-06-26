#ifndef __DATA_LOADER_SOURCE_AZURE_SOURCE_CLIENT_CONFIG_BUILDER_H__
#define __DATA_LOADER_SOURCE_AZURE_SOURCE_CLIENT_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include "model.h"
#include <serializer/compact_serializer.h>

namespace data_loader::azure_source
{
    auto to_internal_secured_azure_client_config(const ExternalSecuredAzureClientConfig& config) -> SecuredAzureClientConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<SecuredAzureClientConfig>(config.config_bytestream);
    }

    auto to_external_secured_azure_client_config(const SecuredAzureClientConfig& config) -> ExternalSecuredAzureClientConfig
    {
        return
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }
}

#endif
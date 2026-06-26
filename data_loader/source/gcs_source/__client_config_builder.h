#ifndef __DATA_LOADER_SOURCE_GCS_SOURCE_CLIENT_CONFIG_BUILDER_H__
#define __DATA_LOADER_SOURCE_GCS_SOURCE_CLIENT_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include "model.h"
#include <serializer/compact_serializer.h>

namespace data_loader::gcs_source
{
    auto to_internal_secured_gcs_client_config(const ExternalSecuredGCSClientConfig& config) -> SecuredGCSClientConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<SecuredGCSClientConfig>(config.config_bytestream);
    }

    auto to_external_secured_gcs_client_config(const SecuredGCSClientConfig& config) -> ExternalSecuredGCSClientConfig
    {
        return
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }
}

#endif
#ifndef __DATA_LOADER_SOURCE_S3_SOURCE_CLIENT_CONFIG_BUILDER_H__
#define __DATA_LOADER_SOURCE_S3_SOURCE_CLIENT_CONFIG_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include "model.h"
#include <serializer/compact_serializer.h>

namespace data_loader::s3_source
{
    auto to_internal_secured_s3_client_configuration(const ExternalSecuredS3ClientConfiguration& config) -> SecuredS3ClientConfiguration
    {
        return dg::network_compact_serializer::dgstd_deserialize<SecuredS3ClientConfiguration>(config.config_bytestream);
    }

    auto to_external_secured_s3_client_configuration(const SecuredS3ClientConfiguration& config) -> ExternalSecuredS3ClientConfiguration
    {
        return
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }
}

#endif
#ifndef __DATA_LOADER_S3_SERIALIZABLE_H__
#define __DATA_LOADER_S3_SERIALIZABLE_H__

#include <stdint.h>
#include <stdlib.h>
// #include <aws/s3/S3Client.h>
// #include <aws/core/client/ClientConfiguration.h>

namespace data_loader::s3_source
{
    struct SerializableS3ClientConfiguration
    {
        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            (void) reflector;
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            (void) reflector;
        }
    };

    // auto to_legacy_s3_client_config(const SerializableS3ClientConfiguration& config) -> Aws::Client::ClientConfiguration
    // {
    //     return Aws::Client::ClientConfiguration();
    // }

    // auto to_serializable_s3_client_config(const Aws::Client::ClientConfiguration& config) -> SerializableS3ClientConfiguration
    // {
    //     return {};
    // }
}

#endif
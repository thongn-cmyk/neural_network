#ifndef __DEVIATION_PROJECTION_INGESTION_AID_MODEL_H__
#define __DEVIATION_PROJECTION_INGESTION_AID_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <internal_rest/network_rest_frame.h>
#include <connectivity_subsystem/connectivity_subsystem.h>

namespace deviation_projection_ingestion_aid
{
    template <class T>
    using Promise   = dg_sock::network_rest_frame::client::Promise<T>;

    using Remote    = dg_sock::network_rest_frame::model::Remote;
    using Url       = dg_sock::network_rest_frame::model::Url;

    //migrations
    struct ExternalMultisourceLoaderConfig
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

    //migrations
    struct ExternalGenericFirerConfig
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

    struct ClientRemote
    {
        Remote remote;
        uint64_t client_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(remote, client_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(remote, client_id);
        }
    };

    struct PiecewiseArgument
    {
        Remote worker_remote;
        ClientRemote client_remote;
        ExternalMultisourceLoaderConfig data_loader_config;
        ExternalGenericFirerConfig firer_config;
        connectivity_subsystem::MasterConfiguration connection_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(worker_remote,
                      client_remote,
                      data_loader_config,
                      firer_config,
                      connection_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(worker_remote,
                      client_remote,
                      data_loader_config,
                      firer_config,
                      connection_config);
        }
    };
}

#endif
#ifndef __DEVIATION_PROJECTION_INGESTION_AID_MODEL_H__
#define __DEVIATION_PROJECTION_INGESTION_AID_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <internal_rest/network_rest_frame.h>
#include <deviation_projection_client/model.h>
#include <data_loader/source_loader/multisource_loader.h>
#include <fire_bandwidth_control/generic_firer.h>
#include <connectivity_subsystem/connectivity_subsystem.h>

namespace deviation_projection_ingestion_aid
{
    template <class T>
    using Promise       = dg_sock::network_rest_frame::client::Promise<T>;

    using Remote        = dg_sock::network_rest_frame::model::Remote;
    using Url           = dg_sock::network_rest_frame::model::Url;
    using ClientRemote  = deviation_projection_client::ClientRemote;

    struct PiecewiseArgument
    {
        Remote worker_remote;
        ClientRemote client_remote;
        data_loader::source_loader::multisource_loader::ExternalMultisourceLoaderConfig data_loader_config;
        fire_bandwidth_control::generic_firer::ExternalGenericFirerConfig firer_config;
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
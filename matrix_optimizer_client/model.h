#ifndef __MATRIX_OPTIMIZER_CLIENT_MODEL_H__
#define __MATRIX_OPTIMIZER_CLIENT_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <expected>
#include "local_exception.h"
#include <string>
#include <connectivity_subsystem/connectivity_subsystem.h>
#include <internal_rest/network_rest_frame.h>

namespace matrix_optimizer_client
{
    template <class T>
    using Promise       = dg_sock::network_rest_frame::client::Promise<T>;

    using Remote        = dg_sock::network_rest_frame::model::Remote;
    using Url           = dg_sock::network_rest_frame::model::Url;
    using ClientRequest = dg_sock::network_rest_frame::model::ClientRequest;

    //it seems I introduce complexity that I'm unsure if I'm willing to handle the complexity
    //I think it's fine to add an extra automap for every external resource, it's just some extra linking steps, it's complicated to include everything

    //migrations
    struct ExternalGenericMatrixResourceAsDeviationCalculatorWrapperConfig
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
    struct ExternalGenericOptimizerEngineConfig
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
    struct ExternalGenericMatrixResource
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

    struct GetVersionRequest
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

    struct GetVersionResponse
    {
        std::expected<std::string, matrix_optimizer_client::local_exception_t> response;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(response, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(response, err_verbal_description);
        }
    };

    struct OpenClientRequest
    {
        connectivity_subsystem::SlaveConfiguration connection_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(connection_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(connection_config);
        }
    };

    struct OpenClientResponse
    {
        std::expected<uint64_t, matrix_optimizer_client::local_exception_t> result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct CloseClientRequest
    {
        uint64_t client_box_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id);
        }
    };

    struct CloseClientResponse
    {
        matrix_optimizer_client::local_exception_t result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct PullWorkOrder
    {
        Remote worker_remote;
        Remote dst_remote;

        ExternalMultisourceLoaderConfig data_loader_config;
        ExternalGenericFirerConfig firer_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(worker_remote,
                      dst_remote,
                      data_loader_config,
                      firer_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(worker_remote,
                      dst_remote,
                      data_loader_config,
                      firer_config);
        }
    };

    struct RunWorkOrder
    {
        ExternalGenericMatrixResource matrix;
        std::vector<PullWorkOrder> pull_work_order_vec;
        ExternalGenericMatrixResourceAsDeviationCalculatorWrapperConfig deviation_config;
        ExternalGenericOptimizerEngineConfig optimizer_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(matrix,
                      pull_work_order_vec,
                      deviation_config,
                      optimizer_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(matrix,
                      pull_work_order_vec,
                      deviation_config,
                      optimizer_config);
        }
    };

    struct RunRequest
    {
        uint64_t client_box_id;
        RunWorkOrder run_work_order;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id, run_work_order);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id, run_work_order);
        }
    };

    struct RunResponse
    {
        matrix_optimizer_client::local_exception_t result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct InterruptRequest
    {
        uint64_t client_box_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id);
        }
    };

    struct InterruptResponse
    {
        matrix_optimizer_client::local_exception_t result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct IsCompletedRequest
    {
        uint64_t client_box_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id);
        }
    };

    struct IsCompletedResponse
    {
        std::expected<bool, matrix_optimizer_client::local_exception_t> result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct GetResultRequest
    {
        uint64_t client_box_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id);
        }
    };

    struct GetResultResponse
    {
        std::expected<ExternalGenericMatrixResource, matrix_optimizer_client::local_exception_t> result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
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
}

#endif
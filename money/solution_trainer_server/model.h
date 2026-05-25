#ifndef __MONEY_SOLUTION_SOLUTION_TRAINER_SERVER_MODEL_H__
#define __MONEY_SOLUTION_SOLUTION_TRAINER_SERVER_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include "local_exception.h"
#include <expected>
#include <optional>
#include <string>
#include <connectivity_subsystem/connectivity_subsystem.h>
#include <data_loader/source_loader/multisource_loader.h>
#include <matrix/generic_matrix_factory.h>
#include <internal_rest/network_rest_frame.h>
#include <fire_bandwidth_control/generic_firer.h>

namespace stock_solution_trainer_server::model
{
    using Remote = dg_sock::network_rest_frame::model::Remote;

    static inline constexpr uint8_t OPTIMIZATION_FLAG_LOW       = 0u;
    static inline constexpr uint8_t OPTIMIZATION_FLAG_MEDIUM    = 1u;
    static inline constexpr uint8_t OPTIMIZATION_FLAG_HIGH      = 2u;

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
        std::expected<std::string, local_exception_t> response;
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
        std::expected<uint64_t, local_exception_t> result;
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
        local_exception_t result;
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

    struct StockDataSource
    {
        data_loader::source_loader::multisource_loader::ExternalMultisourceLoaderConfig data_loader_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(data_loader_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(data_loader_config);
        }
    };

    struct StockDataSink
    {
        Remote sink_remote;
        fire_bandwidth_control::generic_firer::ExternalGenericFirerConfig firer_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(sink_remote, firer_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(sink_remote, firer_config);
        }
    };

    struct RunWorkOrder
    {
        StockDataSource data_source;
        std::vector<StockDataSink> data_sink_vec;
        uint8_t optimization_flag;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(data_source,
                      data_sink_vec,
                      optimization_flag);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(data_source,
                      data_sink_vec,
                      optimization_flag);
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
        local_exception_t result;
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
        local_exception_t result;
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
        std::expected<bool, local_exception_t> result;
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

    //migrations
    struct ExternalSolutionData
    {
        std::string solution_bytestream;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(solution_bytestream);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(solution_bytestream);
        }
    };

    struct GetResultResponse
    {
        std::expected<ExternalSolutionData, local_exception_t> result;
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
}

#endif
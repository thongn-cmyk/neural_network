#ifndef __MONEY_SOLUTION_SOLUTION_TRAINER_CLIENT_MODEL_H__
#define __MONEY_SOLUTION_SOLUTION_TRAINER_CLIENT_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include "local_exception.h"
#include <expected>
#include <optional>
#include <string>
#include <connectivity_subsystem/connectivity_subsystem.h>
#include <data_loader/source_loader/multisource_loader/model.h>
#include <internal_rest/network_rest_frame.h>
#include <fire_bandwidth_control/generic_firer.h>

namespace stock_solution_trainer_client
{
    template <class T>
    using Promise       = dg_sock::network_rest_frame::client::Promise<T>;

    using Remote        = dg_sock::network_rest_frame::model::Remote;
    using Url           = dg_sock::network_rest_frame::model::Url;
    using ClientRequest = dg_sock::network_rest_frame::model::ClientRequest;

    static inline constexpr uint8_t OPTIMIZATION_FLAG_O1    = 0u;
    static inline constexpr uint8_t OPTIMIZATION_FLAG_O2    = 1u;
    static inline constexpr uint8_t OPTIMIZATION_FLAG_O3    = 2u;

    //migrations
    struct TickerData
    {
        std::string ticker_name;
        std::string feature_name;
        double feature_value;
        std::chrono::time_point<std::chrono::utc_clock> timestamp;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(ticker_name,
                      feature_name,
                      feature_value,
                      timestamp);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(ticker_name,
                      feature_name,
                      feature_value,
                      timestamp);
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

    struct ComputeSink
    {
        Remote sink_remote;
        std::optional<fire_bandwidth_control::generic_firer::ExternalGenericFirerConfig> firer_config;

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

    struct TrainingWindowInfo
    {
        std::chrono::time_point<std::chrono::utc_clock> from_timepoint;
        std::chrono::time_point<std::chrono::utc_clock> to_timepoint;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(from_timepoint,
                      to_timepoint);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(from_timepoint,
                      to_timepoint);
        }
    };

    struct RunWorkOrder
    {
        StockDataSource data_source;
        std::vector<ComputeSink> compute_sink_vec;
        TrainingWindowInfo training_window;

        uint8_t optimization_flag;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(data_source,
                      compute_sink_vec,
                      training_window,
                      optimization_flag);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(data_source,
                      compute_sink_vec,
                      training_window,
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
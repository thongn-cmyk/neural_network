#ifndef __DG_DEVIATION_PROJECTION_SERVER_MODEL_H__
#define __DG_DEVIATION_PROJECTION_SERVER_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include "local_exception.h"
#include <connectivity_subsystem/connectivity_subsystem.h>
#include <expected>
#include <deviation_projector/matrix_resource_as_deviation_projector/generic_matrix_resource_as_deviation_projector.h>
#include <vector>

namespace deviation_projection_server
{
    using namespace float_def;

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
        std::expected<std::string, deviation_projection_server::local_exception_t> response;
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
        std::expected<uint64_t, deviation_projection_server::local_exception_t> result;
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
        uint64_t client_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id);
        }
    };

    struct CloseClientResponse
    {
        deviation_projection_server::local_exception_t result;
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

    struct AddTrainingDataRequest
    {
        uint64_t client_id;
        std::string token;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id, token);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id, token);
        }
    };

    struct AddTrainingDataResponse
    {
        deviation_projection_server::local_exception_t result;
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

    struct ClearTrainingDataRequest
    {
        uint64_t client_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id);
        }
    };

    struct ClearTrainingDataResponse
    {
        deviation_projection_server::local_exception_t result;
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

    struct SetMatrixResourceRequest
    {
        uint64_t client_id;
        std::vector<deviation_projector::matrix_resource_as_deviation_projector::ExternalGenericMatrixResourceAsDeviationCalculatorResource> matrix_resource_vec;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id, matrix_resource_vec);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id, matrix_resource_vec);
        }
    };

    struct SetMatrixResourceResponse
    {
        deviation_projection_server::local_exception_t result;
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

    struct GetDeviationRequest
    {
        uint64_t client_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id);
        }
    };

    struct GetDeviationResponse
    {
        std::expected<std::vector<mdc_float_t>, deviation_projection_server::local_exception_t> result;
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

    struct SetAndGetDeviationRequest
    {
        uint64_t client_id;
        std::vector<deviation_projector::matrix_resource_as_deviation_projector::ExternalGenericMatrixResourceAsDeviationCalculatorResource> matrix_resource_vec;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id, matrix_resource_vec);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id, matrix_resource_vec);
        }
    };

    struct SetAndGetDeviationResponse
    {
        std::expected<std::vector<mdc_float_t>, deviation_projection_server::local_exception_t> result;
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
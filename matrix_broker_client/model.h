#ifndef __MATRIX_BROKER_CLIENT_MODEL_H__
#define __MATRIX_BROKER_CLIENT_MODEL_H__

#include <internal_rest/network_rest_frame.h>
#include <stdint.h>
#include <stdlib.h>
#include "local_exception.h"
#include <variant>
#include <vector>
#include <stl_extension/stdx.h>
#include <matrix/generic_matrix_factory.h>
#include <string>
#include <expected>

namespace matrix_broker_client
{
    using matrix_entropy_t  = uint8_t;

    template <class T>
    using Promise           = dg_sock::network_rest_frame::client::Promise<T>;

    using Remote            = dg_sock::network_rest_frame::model::Remote;
    using Url               = dg_sock::network_rest_frame::model::Url;
    using ClientRequest     = dg_sock::network_rest_frame::model::ClientRequest;

    static inline constexpr matrix_entropy_t MATRIX_ENTROPY_LOW     = 0u;
    static inline constexpr matrix_entropy_t MATRIX_ENTROPY_MID     = 1u;
    static inline constexpr matrix_entropy_t MATRIX_ENTROPY_HIGH    = 2u;

    struct FixedProjectionArgument
    {
        std::vector<uint64_t> inp_matrix_shape;
        std::vector<uint64_t> out_matrix_shape;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(inp_matrix_shape, out_matrix_shape);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(inp_matrix_shape, out_matrix_shape);
        }
    };

    struct ProjectionArgument
    {
        std::variant<stdx::reflectible_monostate, FixedProjectionArgument> projection_argument;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(projection_argument);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(projection_argument);
        }
    };

    struct ClientMatrixResult
    {
        ProjectionArgument projection_argument;
        generic_matrix_factory::ExternalGenericMatrixResource matrix_resource;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(projection_argument, matrix_resource);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(projection_argument, matrix_resource);
        }
    };

    struct BrokeMatrixRequest
    {
        std::string generator_id;
        matrix_entropy_t matrix_entropy;
        uint64_t flat_matrix_sz;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(generator_id, matrix_entropy, flat_matrix_sz);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(generator_id, matrix_entropy, flat_matrix_sz);
        }
    };

    struct BrokeMatrixResponse
    {
        std::expected<ClientMatrixResult, local_exception_t> result;
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
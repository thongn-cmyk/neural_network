#ifndef __MATRIX_MATRIX_SERILIZER_H__
#define __MATRIX_MATRIX_SERILIZER_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <matrix/tensor_model.h>
#include <memory>
#include <serializer/compact_serializer.h>
#include "tensor_factory.h"
#include <stl_extension/stdx.h>

namespace matrix_serializer
{
    struct GenericMatrix
    {
        std::string payload;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(payload);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(payload);
        }
    };

    using Serializable = std::pair<std::vector<uint64_t>, std::vector<tensor_std_float_t>>;  

    auto serialize(const std::shared_ptr<tensor_model::Matrix>& matrix) -> GenericMatrix
    {
        std::vector<size_t> shape{};
        std::vector<tensor_std_float_t> logit_vec{}:

        tensor_factory::flatten(matrix, logit_vec)
        tensor_factory::get_shape(matrix, shape);

        auto serializable   = Serializable(stdx::to_castable_vector_initializer(std::move(shape)),
                                           std::move(logit_vec));

        return GenericMatrix
        {
            .payload = dg::network_compact_serializer::dgstd_serialize<std::string>(serializable)
        };
    }

    auto deserialize(const GenericMatrix& matrix) -> std::shared_ptr<tensor_model::Matrix>
    {
        Serializable serializable                   = dg::network_compact_serializer::dgstd_deserialize<Serializable>(matrix.payload);

        std::vector<size_t> shape                   = stdx::to_castable_vector_initializer(std::move(serializable.first));
        std::vector<tensor_std_float_t> logit_vec   = std::move(serializable.second);

        return tensor_factory::make_matrix_from_flat_vec(shape, logit_vec);
    }
}

#endif
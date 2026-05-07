#ifndef __DEVIATION_PROJECTOR_TRAINING_TOKEN_FACTORY_H__
#define __DEVIATION_PROJECTOR_TRAINING_TOKEN_FACTORY_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <serializer/compact_serializer.h>
#include <matrix/tensor_model.h>
#include <matrix/tensor_factory.h>

namespace deviation_projector::training_token_factory
{
    using tensor_std_float_t = tensor_model::tensor_std_float_t;

    struct MatrixSerializable
    {
        std::vector<tensor_std_float_t> logit_vec;
        std::vector<uint64_t> shape;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(logit_vec, shape);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(logit_vec, shape);
        }
    };

    struct TrainingToken
    {
        MatrixSerializable inp;
        MatrixSerializable out;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(inp, out);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(inp, out);
        }
    };

    auto encode_training_token(const std::shared_ptr<tensor_model::Matrix>& inp,
                               const std::shared_ptr<tensor_model::Matrix>& out) -> std::string
    {

        MatrixSerializable serializable_inp;
        MatrixSerializable serializable_out;

        {
            std::vector<tensor_std_float_t> rs{};
            std::vector<size_t> shape{};

            tensor_factory::flatten(inp, rs);
            tensor_factory::get_shape(inp, shape);

            serializable_inp = MatrixSerializable
            {
                .logit_vec  = std::move(rs),
                .shape      = stdx::to_castable_vector_initializer(shape)
            };
        }

        {
            std::vector<tensor_std_float_t> rs{};
            std::vector<size_t> shape{};

            tensor_factory::flatten(out, rs);
            tensor_factory::get_shape(out, shape);

            serializable_out = MatrixSerializable
            {
                .logit_vec  = std::move(rs),
                .shape      = stdx::to_castable_vector_initializer(shape)
            };
        }

        TrainingToken training_token
        {
            .inp = std::move(serializable_inp),
            .out = std::move(serializable_out)
        };

        return dg::network_compact_serializer::dgstd_serialize<std::string>(training_token);
    }

    auto decode_training_token(const std::string& data) -> std::pair<std::shared_ptr<tensor_model::Matrix>,
                                                                     std::shared_ptr<tensor_model::Matrix>>
    {
        TrainingToken training_token = dg::network_compact_serializer::dgstd_deserialize<TrainingToken>(data);

        return {tensor_factory::make_matrix_from_flat_vec(std::vector<size_t>(stdx::to_castable_vector_initializer(training_token.inp.shape)),
                                                          training_token.inp.logit_vec),
                tensor_factory::make_matrix_from_flat_vec(std::vector<size_t>(training_token.out.shape),
                                                          training_token.out.logit_vec)};
    }
}

#endif
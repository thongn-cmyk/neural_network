#ifndef __DEVIATION_PROJECTOR_GENERIC_HOST_WRAPPER_H__
#define __DEVIATION_PROJECTOR_GENERIC_HOST_WRAPPER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <serializer/compact_serializer.h>
#include <matrix/generic_matrix_factory.h>
#include <matrix/tensor_model.h>
#include <matrix/tensor_matrix_operation.h>
#include <deviation_projector/matrix_deviation_calculator_factory.h>
#include <deviation_projector/generic_matrix_deviation_calculator_interface.h>
#include <stl_extension/stdx.h>

namespace deviation_projector::host_wrapper
{
    static inline constexpr uint32_t SERIALIZATION_SECRET = 3871831494UL;
    using tensor_std_float_t = tensor_model::tensor_std_float_t;

    struct GenericResource
    {
        deviation_projector::NoTransformDeviationCalculatorResource deviation_resource;
        generic_matrix_factory::GenericMatrixResource matrix_resource;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(deviation_resource, matrix_resource);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(deviation_resource, matrix_resource);
        }
    };

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

    auto serialize_matrix(const std::shared_ptr<tensor_model::Matrix>& matrix) -> std::string
    {
        std::vector<tensor_std_float_t> rs{};
        std::vector<size_t> shape{};

        tensor_matrix_operation::flatten(matrix, rs);
        tensor_matrix_operation::get_shape(matrix, shape);

        auto serializable = MatrixSerializable
        {
            .logit_vec  = std::move(rs),
            .shape      = stdx::to_castable_vector_initializer(shape)
        };

        return dg::network_compact_serializer::dgstd_serialize<std::string>(serializable, SERIALIZATION_SECRET);
    }

    auto deserialize_matrix(const std::string& data) -> std::shared_ptr<tensor_model::Matrix>
    {
        MatrixSerializable rs = dg::network_compact_serializer::dgstd_deserialize<MatrixSerializable>(data, SERIALIZATION_SECRET);

        return tensor_matrix_operation::make_matrix_from_flat_vec(std::vector<size_t>(stdx::to_castable_vector_initializer(rs.shape)),
                                                                  rs.logit_vec);
    }

    class DeviationCalculator: public virtual deviation_projector::GenericMatrixDeviationCalculatorInterface
    {
        private:

            std::unique_ptr<deviation_projector::MatrixDeviationCalculatorInterface> deviation_calculator;
            std::unique_ptr<the_matrix::MatrixInterface> matrix;

        public:

            DeviationCalculator(std::unique_ptr<deviation_projector::MatrixDeviationCalculatorInterface>&& deviation_calculator,
                                std::unique_ptr<the_matrix::MatrixInterface>&& matrix)
            {
                if (deviation_calculator == nullptr)
                {
                    throw std::invalid_argument("bad deviation calculator, null");
                }

                if (matrix == nullptr)
                {
                    throw std::invalid_argument("bad matrix, null");
                }

                this->deviation_calculator  = std::move(deviation_calculator);
                this->matrix                = std::move(matrix);
            }

            auto get_deviation(const std::vector<std::pair<std::shared_ptr<std::string>, std::shared_ptr<std::string>>>& arg) -> mdc_float_t
            {
                std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>> tensor_pair_vec{};
                std::vector<std::shared_ptr<tensor_model::Matrix>> projection_vec{};
                std::vector<std::shared_ptr<tensor_model::Matrix>> projected_vec{};

                for (const auto& [lhs, rhs]: arg)
                {
                    if (lhs == nullptr)
                    {
                        throw std::invalid_argument("bad tensor, null");
                    }

                    if (rhs == nullptr)
                    {
                        throw std::invalid_argument("bad tensor, null");
                    }
                }

                for (const auto& [lhs, _] : arg)
                {
                    projection_vec.push_back(deserialize_matrix(*lhs));
                }

                projected_vec = this->matrix->project(projection_vec);

                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (projected_vec.size() != projection_vec.size())
                    {
                        std::abort();
                    }
                }

                for (size_t i = 0u; i < projected_vec.size(); ++i)
                {
                    tensor_pair_vec.push_back({projection_vec[i], projected_vec[i]});
                }

                return this->deviation_calculator->get_deviation(tensor_pair_vec);
            }
    };

    class GenericMatrixDeviationCalculatorResourceLoader
    {
        public:

            auto load(const GenericResource& resource) -> std::unique_ptr<deviation_projector::GenericMatrixDeviationCalculatorInterface>
            {
                return std::make_unique<DeviationCalculator>(deviation_projector::NoTransformDeviationCalculatorLoader{}.load(resource.deviation_resource),
                                                             generic_matrix_factory::GenericMatrixLoader{}.load_resource(resource.matrix_resource));
            }
    };
}

#endif
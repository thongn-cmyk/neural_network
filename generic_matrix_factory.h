#ifndef __GENERIC_MATRIX_FACTORY_H__
#define __GENERIC_MATRIX_FACTORY_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include "float_def.h"
#include <functional>
#include <algorithm>
#include "compact_serializer.h"
#include "the_host_matrix.h"
#include "tensor_model.h"
#include <variant>
#include "stdx.h"

namespace generic_matrix_factory
{
    static inline const std::string VERSION_CONTROL = "v1.0";

    struct version_exception: std::invalid_argument
    {
        version_exception(): invalid_argument("bad version exception"){}
    };

    struct bad_format_exception: std::invalid_argument
    {
        bad_format_exception(): invalid_argument("bad format exception"){}
    };

    struct incompatible_exception: std::invalid_argument
    {
        incompatible_exception(): invalid_argument("incompatible exception"){}
    };

    struct TheHostMatrixResource
    {
        uint8_t entropy_option;
        uint8_t compute_option;
        uint64_t vector_sz;
        std::vector<tensor_model::tensor_std_float_t> logit_vec;
        std::string background_semantic;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(entropy_option, compute_option, vector_sz, logit_vec, background_semantic);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(entropy_option, compute_option, vector_sz, logit_vec, background_semantic);
        }
    };

    struct TheCudaMatrixResource
    {
        uint8_t entropy_option;
        uint8_t compute_option;
        uint64_t vector_sz;
        std::vector<tensor_model::tensor_std_float_t> logit_vec;
        std::string background_semantic;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(entropy_option, compute_option, vector_sz, logit_vec, background_semantic);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(entropy_option, compute_option, vector_sz, logit_vec, background_semantic);
        }
    };

    struct TheCudaIfPossibleMatrixResource
    {
        TheHostMatrixResource host_resource;
        TheCudaMatrixResource cuda_resource;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(host_resource, cuda_resource);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(host_resource, cuda_resource);
        }
    };

    struct GenericMatrixResource
    {
        std::string version_control;
        std::variant<stdx::reflectible_monostate, TheHostMatrixResource, TheCudaMatrixResource, TheCudaIfPossibleMatrixResource> resource;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(version_control, resource);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(version_control, resource);
        }
    };

    class TheHostMatrixLoader
    {
        public:

            static inline constexpr uint8_t LOW_COMPUTE     = the_host_matrix::TheHostMatrixFactory::LOW_COMPUTE;
            static inline constexpr uint8_t MID_COMPUTE     = the_host_matrix::TheHostMatrixFactory::MID_COMPUTE;
            static inline constexpr uint8_t HIGH_COMPUTE    = the_host_matrix::TheHostMatrixFactory::HIGH_COMPUTE;

            static inline constexpr uint8_t LOW_ENTROPY     = the_host_matrix::TheHostMatrixFactory::LOW_ENTROPY;
            static inline constexpr uint8_t MID_ENTROPY     = the_host_matrix::TheHostMatrixFactory::MID_ENTROPY;
            static inline constexpr uint8_t HIGH_ENTROPY    = the_host_matrix::TheHostMatrixFactory::HIGH_ENTROPY;

        private:

            struct BaseConfiguration
            {
                uint8_t entropy_option;
                uint8_t compute_option;
                uint64_t vector_sz;
            };

            BaseConfiguration base_configuration;

            static auto get_matrix_factory_from_base_configuration(BaseConfiguration configuration) -> the_host_matrix::TheHostMatrixFactory
            {
                return the_host_matrix::TheHostMatrixFactory{}.set_entropy(configuration.entropy_option)
                                                              .set_compute(configuration.compute_option)
                                                              .set_vector_size(configuration.vector_sz);
            }

            static auto get_matrix_from_base_configuration(BaseConfiguration configuration) -> std::unique_ptr<the_matrix::MatrixInterface>
            {
                return get_matrix_factory_from_base_configuration(configuration).get();
            }

        public:

            auto set_entropy(uint8_t entropy_option) -> TheHostMatrixLoader&
            {
                this->base_configuration.entropy_option = entropy_option;

                return *this;
            }

            auto set_compute(uint8_t compute_option) -> TheHostMatrixLoader&
            {
                this->base_configuration.compute_option = compute_option;

                return *this;
            }

            auto set_vector_size(size_t sz) -> TheHostMatrixLoader&
            {
                this->base_configuration.vector_sz = sz;

                return *this;
            }

            auto get_matrix_shape() -> std::vector<size_t>
            {
                return get_matrix_factory_from_base_configuration(this->base_configuration).get_matrix_shape();
            }

            auto get_matrix() -> std::unique_ptr<the_matrix::MatrixInterface>
            {
                return std::make_unique<InternalMatrix>(this->base_configuration,
                                                        get_matrix_from_base_configuration(this->base_configuration));
            }

            auto unload(the_matrix::MatrixInterface& matrix) -> TheHostMatrixResource
            {
                InternalMatrix * internal_matrix = dynamic_cast<InternalMatrix *>(&matrix);

                if (internal_matrix == nullptr)
                {
                    throw std::invalid_argument("invalid matrix, matrix is not from loader");
                }

                return TheHostMatrixResource
                {
                    .entropy_option         = internal_matrix->get_configuration().entropy_option,
                    .compute_option         = internal_matrix->get_configuration().compute_option,
                    .vector_sz              = internal_matrix->get_configuration().vector_sz,
                    .logit_vec              = internal_matrix->get_coefficient_vector(),
                    .background_semantic    = get_matrix_factory_from_base_configuration(internal_matrix->get_configuration()).get_background_semantic()
                };
            }

            auto load_from_resource(const TheHostMatrixResource& resource) -> std::unique_ptr<the_matrix::MatrixInterface>
            {
                BaseConfiguration config
                {
                    .entropy_option = resource.entropy_option,
                    .compute_option = resource.compute_option,
                    .vector_sz      = resource.vector_sz
                };

                if (get_matrix_factory_from_base_configuration(config).get_background_semantic() != resource.background_semantic)
                {
                    throw incompatible_exception();
                }

                std::unique_ptr<the_matrix::MatrixInterface> matrix = get_matrix_from_base_configuration(config);

                return std::make_unique<InternalMatrix>(std::move(config),
                                                        std::move(matrix));
            }

        private:

            class InternalMatrix: public virtual the_matrix::MatrixInterface
            {
                private:

                    TheHostMatrixLoader::BaseConfiguration base_config;
                    std::shared_ptr<the_matrix::MatrixInterface> base_matrix;
                
                public:

                    InternalMatrix(TheHostMatrixLoader::BaseConfiguration base_config,
                                   std::shared_ptr<the_matrix::MatrixInterface> base_matrix) noexcept: base_config(std::move(base_config)),
                                                                                                       base_matrix(std::move(base_matrix)){}

                    auto get_configuration() const noexcept -> const TheHostMatrixLoader::BaseConfiguration&
                    {
                        return this->base_config;
                    }

                    auto get_coefficient_vector() -> std::vector<tensor_model::tensor_std_float_t>
                    {
                        return this->base_matrix->get_coefficient_vector();
                    }

                    void set_coefficient_vector(const std::vector<tensor_model::tensor_std_float_t>& coeff_vec)
                    {
                        this->base_matrix->set_coefficient_vector(coeff_vec);
                    }

                    auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
                    {
                        return this->base_matrix->project(matrix_vec);
                    }

                    auto clone() -> std::shared_ptr<the_matrix::MatrixInterface>
                    {
                        return std::make_shared<InternalMatrix>(this->base_config,
                                                                this->base_matrix->clone());
                    }
            };
    };

    class TheCudaMatrixLoader
    {
        public:

            auto load_from_resource(const TheCudaMatrixResource& resource) -> std::unique_ptr<the_matrix::MatrixInterface>
            {
                return {};
            }

            auto unload(the_matrix::MatrixInterface& matrix) -> TheCudaMatrixResource
            {
                return {};
            }
    };

    class TheHostCudaMatrixLoader
    {
        public:

            auto load_from_resource(const TheCudaIfPossibleMatrixResource& resource) -> std::unique_ptr<the_matrix::MatrixInterface>
            {
                return {};
            }

            auto unload(the_matrix::MatrixInterface& matrix) -> TheCudaIfPossibleMatrixResource
            {
                return {};
            }
    };

    class GenericMatrixLoader
    {
        private:

            static inline constexpr uint8_t THE_HOST_MATRIX_VARIANT             = 0u;
            static inline constexpr uint8_t THE_CUDA_MATRIX_VARIANT             = 1u;
            static inline constexpr uint8_t THE_CUDA_IF_POSSIBLE_MATRIX_VARIANT = 2u;

        public:

            auto virtualize_resource(const TheHostMatrixResource& resource) -> GenericMatrixResource
            {
                return GenericMatrixResource
                {
                    .version_control    = VERSION_CONTROL,
                    .resource           = resource
                };
            }

            auto virtualize_resource(const TheCudaMatrixResource& resource) -> GenericMatrixResource
            {
                return GenericMatrixResource
                {
                    .version_control    = VERSION_CONTROL,
                    .resource           = resource
                };
            }

            auto virtualize_resource(const TheCudaIfPossibleMatrixResource& resource) -> GenericMatrixResource
            {
                return GenericMatrixResource
                {
                    .version_control    = VERSION_CONTROL,
                    .resource           = resource
                };
            }

            auto load_resource(const GenericMatrixResource& resource) -> std::unique_ptr<the_matrix::MatrixInterface>
            {
                if (resource.version_control != VERSION_CONTROL)
                {
                    throw version_exception{};
                }

                if (resource.resource.valueless_by_exception())
                {
                    throw bad_format_exception{};
                }

                if (std::holds_alternative<TheHostMatrixResource>(resource.resource))
                {
                    return std::make_unique<InternalFancyMatrix>(THE_HOST_MATRIX_VARIANT,
                                                                 TheHostMatrixLoader{}.load_from_resource(std::get<TheHostMatrixResource>(resource.resource)));
                }
                else if (std::holds_alternative<TheCudaMatrixResource>(resource.resource))
                {
                    return std::make_unique<InternalFancyMatrix>(THE_CUDA_MATRIX_VARIANT,
                                                                 TheCudaMatrixLoader{}.load_from_resource(std::get<TheCudaMatrixResource>(resource.resource)));
                }
                else if (std::holds_alternative<TheCudaIfPossibleMatrixResource>(resource.resource))
                {
                    return std::make_unique<InternalFancyMatrix>(THE_CUDA_IF_POSSIBLE_MATRIX_VARIANT,
                                                                 TheCudaIfPossibleMatrixLoader{}.load_from_resource(std::get<TheCudaIfPossibleMatrixResource>(resource.resource)));
                }
                else
                {
                    throw bad_format_exception();
                }
            }

            auto unload(the_matrix::MatrixInterface& matrix) -> GenericMatrixResource
            {
                InternalFancyMatrix * fancy_matrix = dynamic_cast<InternalFancyMatrix *>(&matrix);

                if (fancy_matrix == nullptr)
                {
                    throw std::invalid_argument("invalid matrix, matrix is not from loader");
                }

                switch (fancy_matrix->get_variant())
                {
                    case THE_HOST_MATRIX_VARIANT:
                    {
                        return this->virtualize_resource(TheHostMatrixLoader{}.unload(*fancy_matrix->get_base()));
                    }
                    case THE_CUDA_MATRIX_VARIANT:
                    {
                        return this->virtualize_resource(TheCudaMatrixLoader{}.unload(*fancy_matrix->get_base()));
                    }
                    case THE_CUDA_IF_POSSIBLE_MATRIX_VARIANT:
                    {
                        return this->virtualize_resource(TheCudaIfPossibleMatrixLoader{}.unload(*fancy_matrix->get_base()));
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }
        
        private:
            
            class InternalFancyMatrix: public virtual the_matrix::MatrixInterface
            {
                private:

                    uint8_t matrix_variant;
                    std::shared_ptr<the_matrix::MatrixInterface> base_matrix;
                
                public:

                    InternalFancyMatrix(uint8_t matrix_variant,
                                        std::shared_ptr<the_matrix::MatrixInterface> base_matrix) noexcept: matrix_variant(matrix_variant),
                                                                                                            base_matrix(std::move(base_matrix)){}

                    auto get_variant() const noexcept -> uint8_t
                    {
                        return this->matrix_variant;
                    }

                    auto get_base() const noexcept -> const std::shared_ptr<the_matrix::MatrixInterface>&
                    {
                        return this->base_matrix;
                    }

                    auto get_coefficient_vector() -> std::vector<tensor_model::tensor_std_float_t>
                    {
                        return this->base_matrix->get_coefficient_vector();
                    }

                    auto set_coefficient_vector(const std::vector<tensor_model::tensor_std_float_t>& coeff_vec)
                    {
                        this->base_matrix->set_coefficient_vector(coeff_vec);
                    }

                    auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
                    {
                        return this->base_matrix->project(matrix_vec);
                    }

                    auto clone() -> std::shared_ptr<the_matrix::MatrixInterface>
                    {
                        return std::make_shared<InternalFancyMatrix>(this->matrix_variant,
                                                                     this->base_matrix->clone());
                    }
            };
    };
}

#endif
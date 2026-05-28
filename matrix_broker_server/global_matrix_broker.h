#ifndef __MATRIX_BROKER_SERVER_GLOBAL_MATRIX_BROKER_H__
#define __MATRIX_BROKER_SERVER_GLOBAL_MATRIX_BROKER_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix/generic_matrix_factory.h>
#include "model.h"
#include <string>
#include <string_view>
#include <memory>
#include <mutex_extension/fair_mutex.h>
#include <stl_extension/stdx.h>

namespace matrix_broker_server::global_matrix_broker
{
    //today we'd work on detached requests, I guess that sometimes, we do need detached requests, especially in the case where we can't dedicate 100% resource on a particular order due to "hard-to-implement" or other issues

    class MatrixGeneratorInterface
    {
        public:

            virtual ~MatrixGeneratorInterface() noexcept = default;

            virtual auto generate(matrix_entropy_t matrix_entropy, size_t flat_matrix_sz) -> ClientMatrixResult = 0;
    };

    class MatrixBrokerInterface
    {
        public:
            
            virtual ~MatrixBrokerInterface() noexcept = default;

            virtual void insert_generator(std::string_view generator_id,
                                          const std::shared_ptr<MatrixGeneratorInterface>& matrix_generator) = 0;

            virtual void remove_generator(std::string_view generator_id) noexcept = 0;

            virtual auto broke_matrix(std::string_view generator_id,
                                      matrix_entropy_t matrix_entropy,
                                      size_t flat_matrix_sz) -> ClientMatrixResult = 0;
    };

    class MatrixBroker: public virtual MatrixBrokerInterface
    {
        private:

            std::unordered_map<std::string, std::shared_ptr<MatrixGeneratorInterface>> matrix_generator_map;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
        
        public:

            MatrixBroker(): matrix_generator_map(),
                            mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            void insert_generator(std::string_view generator_id,
                                  const std::shared_ptr<MatrixGeneratorInterface>& matrix_generator)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (matrix_generator == nullptr)
                {
                    throw std::invalid_argument("bad matrix generator, null");
                }

                this->matrix_generator_map.insert(std::make_pair(std::string(generator_id), matrix_generator));
            }

            void remove_generator(std::string_view generator_id) noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                this->matrix_generator_map.erase(std::string(generator_id));
            }

            auto broke_matrix(std::string_view generator_id,
                              matrix_entropy_t matrix_entropy,
                              size_t flat_matrix_sz) -> ClientMatrixResult
            {
                std::shared_ptr<MatrixGeneratorInterface> matrix_generator;

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    auto map_ptr = this->matrix_generator_map.find(std::string(generator_id));

                    if (map_ptr == this->matrix_generator_map.end())
                    {
                        throw std::invalid_argument("bad generator id, generator id not found");
                    }

                    matrix_generator = map_ptr->second;
                }

                return matrix_generator->generate(matrix_entropy, flat_matrix_sz);
            }
    };

    class TaylorHostMatrixGenerator: public virtual MatrixGeneratorInterface
    {
        public:

            auto generate(matrix_entropy_t matrix_entropy, size_t flat_matrix_sz) -> ClientMatrixResult
            {
                using Loader        = generic_matrix_factory::TheHostMatrixLoader;
                using GenericLoader = generic_matrix_factory::GenericMatrixLoader; 
                using Externalizer  = generic_matrix_factory::GenericMatrixExternalizer;

                auto loader = Loader{};

                loader.set_compute(Loader::LOW_COMPUTE)
                      .set_vector_size(flat_matrix_sz);

                switch (matrix_entropy)
                {
                    case MATRIX_ENTROPY_LOW:
                    {
                        loader.set_entropy(Loader::LOW_ENTROPY);
                        break;
                    }
                    case MATRIX_ENTROPY_MID:
                    {
                        loader.set_entropy(Loader::MID_ENTROPY);
                        break;
                    }
                    case MATRIX_ENTROPY_HIGH:
                    {
                        loader.set_entropy(Loader::HIGH_ENTROPY);
                        break;
                    }
                    default:
                    {
                        throw std::invalid_argument("bad matrix entropy option, enumeration out of range");
                    }
                }

                auto matrix = loader.get_matrix();

                return ClientMatrixResult
                {
                    .projection_argument = 
                    {
                        .projection_argument = FixedProjectionArgument
                        {
                            .inp_matrix_shape   = stdx::to_castable_vector_initializer(loader.get_matrix_shape()),
                            .out_matrix_shape   = stdx::to_castable_vector_initializer(loader.get_matrix_shape())
                        }
                    },

                    .matrix_resource = Externalizer{}.to_external(GenericLoader{}.virtualize_resource(loader.unload(*matrix)))
                };
            }
    };

    class TaylorCudaMatrixGenerator: public virtual MatrixGeneratorInterface
    {
        public:

            auto generate(matrix_entropy_t matrix_entropy, size_t flat_matrix_sz) -> ClientMatrixResult
            {
                using Loader        = generic_matrix_factory::TheHostMatrixLoader;
                using GenericLoader = generic_matrix_factory::GenericMatrixLoader;
                using Externalizer  = generic_matrix_factory::GenericMatrixExternalizer;

                auto loader = Loader{};

                loader.set_compute(Loader::LOW_COMPUTE)
                      .set_vector_size(flat_matrix_sz);

                switch (matrix_entropy)
                {
                    case MATRIX_ENTROPY_LOW:
                    {
                        loader.set_entropy(Loader::LOW_ENTROPY);
                        break;
                    }
                    case MATRIX_ENTROPY_MID:
                    {
                        loader.set_entropy(Loader::MID_ENTROPY);
                        break;
                    }
                    case MATRIX_ENTROPY_HIGH:
                    {
                        loader.set_entropy(Loader::HIGH_ENTROPY);
                        break;
                    }
                    default:
                    {
                        throw std::invalid_argument("bad matrix entropy option, enumeration out of range");
                    }
                }

                auto matrix = loader.get_matrix();

                return ClientMatrixResult
                {
                    .projection_argument
                    {
                        .projection_argument = FixedProjectionArgument
                        {
                            .inp_matrix_shape   = stdx::to_castable_vector_initializer(loader.get_matrix_shape()),
                            .out_matrix_shape   = stdx::to_castable_vector_initializer(loader.get_matrix_shape())
                        }
                    },

                    .matrix_resource = Externalizer{}.to_external(GenericLoader{}.virtualize_resource(loader.unload(*matrix))) //laggy
                };
            }
    };

    void register_host_matrix_generator(MatrixBrokerInterface& matrix_broker)
    {
        constexpr std::string_view GENERATOR_ID = "taylor_host_matrix";

        matrix_broker.insert_generator(GENERATOR_ID, std::make_shared<TaylorHostMatrixGenerator>());
    }

    void register_cuda_matrix_generator(MatrixBrokerInterface& matrix_broker)
    {
        constexpr std::string_view GENERATOR_ID = "taylor_cuda_matrix";

        matrix_broker.insert_generator(GENERATOR_ID, std::make_shared<TaylorCudaMatrixGenerator>());
    }

    struct Signature{};

    using SingletonObject = stdx::singleton_container<std::shared_ptr<MatrixBrokerInterface>, Signature>;

    void init()
    {
        stdx::memtransaction_guard tx_grd;

        SingletonObject::get() = std::make_shared<MatrixBroker>();

        register_host_matrix_generator(*SingletonObject::get());
        register_cuda_matrix_generator(*SingletonObject::get());
    }

    void deinit() noexcept
    {
        stdx::memtransaction_guard tx_grd;

        SingletonObject::get() = nullptr;
    }

    auto get_instance() noexcept -> MatrixBrokerInterface *
    {
        if (SingletonObject::get() == nullptr)
        {
            std::abort();
        }

        return SingletonObject::get().get();
    }

    auto broke_matrix(std::string_view generator_id,
                      matrix_entropy_t matrix_entropy,
                      size_t flat_matrix_sz) -> ClientMatrixResult
    {
        return get_instance()->broke_matrix(generator_id,
                                            matrix_entropy,
                                            flat_matrix_sz);
    }
}

#endif
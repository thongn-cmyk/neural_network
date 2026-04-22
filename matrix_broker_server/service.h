#ifndef __MATRIX_BROKER_SERVER_SERVICE_H__
#define __MATRIX_BROKER_SERVER_SERVICE_H__

#include <stdint.h>
#include <stdlib.h>
#include "model.h"
#include "local_exception.h"
#include <string>
#include <string_view>

namespace matrix_broker_server
{
    class MatrixBrokerageServiceInterface
    {
        public:

            virtual ~MatrixBrokerageServiceInterface() noexcept = default;

            virtual auto broke_matrix(std::string_view generator_id,
                                      matrix_entropy_t matrix_entropy,
                                      size_t flat_matrix_sz) -> ClientMatrixResult = 0;
    };

    class MatrixBrokerageService: public virtual MatrixBrokerageServiceInterface
    {
        public:

            auto broke_matrix(std::string_view generator_id,
                              matrix_entropy_t matrix_entropy,
                              size_t flat_matrix_sz) -> ClientMatrixResult
            {
                return matrix_broker_server::global_matrix_broker::broke_matrix(generator_id,
                                                                                matrix_entropy,
                                                                                flat_matrix_sz);
            }
    };
}

#endif
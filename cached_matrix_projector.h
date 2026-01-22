//HEADER_CONTROL 7

#ifndef __CACHED_MATRIX_PROJECTOR_H__
#define __CACHED_MATRIX_PROJECTOR_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <string>
#include <unordered_map>
#include "matrix_projector_interface.h"
#include "tensor_matrix_operation.h"
#include "tensor_model.h"

namespace matrix_projector
{
    class CachedMatrixProjector: public virtual MatrixProjectorInterface
    {
        private:

            std::shared_ptr<MatrixProjectorInterface> base_matrix;
            std::unordered_map<std::string, std::shared_ptr<tensor_model::Matrix>> cache_map;
            size_t cache_map_capacity;

        public:

            CachedMatrixProjector(std::shared_ptr<MatrixProjectorInterface> base_matrix,
                                  size_t cache_map_capacity)
            {
                if (base_matrix == nullptr)
                {
                    throw std::invalid_argument("bad base matrix, null base matrix");
                }

                this->base_matrix           = std::move(base_matrix);
                this->cache_map             = std::unordered_map<std::string, std::shared_ptr<tensor_model::Matrix>>();
                this->cache_map_capacity    = cache_map_capacity;
            }

            auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>& matrix_vec) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
            {
                std::vector<std::pair<size_t, std::shared_ptr<tensor_model::Matrix>>> out_of_cache_matrix_vec{};
                std::vector<std::shared_ptr<tensor_model::Matrix>> rs_vec(matrix_vec.size());

                for (size_t i = 0u; i < matrix_vec.size(); ++i)
                {
                    const auto& matrix              = matrix_vec[i];
                    std::string serialized_matrix   = tensor_matrix_operation::matrix_to_unique_representation(matrix);

                    if (auto map_ptr = this->cache_map.find(serialized_matrix); map_ptr != this->cache_map.end())
                    {
                        rs_vec[i] = map_ptr->second;
                        continue;
                    }

                    out_of_cache_matrix_vec.push_back({i, matrix});
                }

                std::vector<std::shared_ptr<tensor_model::Matrix>> projecting_vec{};
                std::transform(out_of_cache_matrix_vec.begin(), out_of_cache_matrix_vec.end(), std::back_inserter(projecting_vec), [](const auto& e){return e.second;});
                std::vector<std::shared_ptr<tensor_model::Matrix>> projected_vec = this->base_matrix->project(projecting_vec);

                if (projecting_vec.size() != projected_vec.size())
                {
                    throw std::runtime_error("bad projection, incompatible size");
                }

                for (size_t i = 0u; i < projecting_vec.size(); ++i)
                {
                    const auto& [idx, projecting_matrix]    = out_of_cache_matrix_vec[i];
                    const auto& projected_matrix            = projected_vec[i];
                    std::string serialized_matrix           = tensor_matrix_operation::matrix_to_unique_representation(projecting_matrix);
                    rs_vec[idx]                             = projected_matrix;

                    if (this->cache_map_capacity != 0u)
                    {
                        if (this->cache_map.size() == this->cache_map_capacity)
                        {
                            this->cache_map.clear();
                        }

                        this->cache_map.insert({serialized_matrix, projected_matrix});
                    }
                }

                return rs_vec;
            }

            void clear_cache() noexcept
            {
                this->cache_map.clear();
            }
    };
}

#endif
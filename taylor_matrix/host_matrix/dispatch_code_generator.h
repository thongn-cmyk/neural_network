#ifndef __TAYLOR_MATRIX_HOST_MATRIX_DISPATCH_CODE_GENERATOR_H__
#define __TAYLOR_MATRIX_HOST_MATRIX_DISPATCH_CODE_GENERATOR_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix/tensor_model.h>
#include "sip_hasher.h"
#include <numeric>
#include <bit>
#include <optional>
#include <type_traits>
#include <stl_extension/stdx.h>

namespace taylor_matrix::host_matrix::dispatch_code_generator
{
    using namespace tensor_model;

    class DispatchCodeGenerator
    {
        private:

            taylor_matrix::host_matrix::sip_hasher::SipHasher hasher;
            std::optional<size_t> previous_dispatch_code;
            size_t table_sz;
        
        private:

            static constexpr auto get_sip_hasher_key() -> __uint128_t
            {
                uint64_t secret         = 14165881820532578704ULL;
                uint64_t stack_clue     = static_cast<uint64_t>(std::bit_cast<uintptr_t>(&secret));
                __uint128_t master_key  = (static_cast<__uint128_t>(secret) << 64) | stack_clue;

                return master_key;
            }

            template <class Visitor>
            static constexpr void visit_matrix(Matrix * matrix,
                                               Visitor&& visitor)
            {
                stdx::safe_ptr_access(matrix);

                for (size_t i = 0u; i < matrix->being_vec_sz; ++i)
                {
                    for (size_t j = 0u; j < matrix->being_vec[i]->process_group_vec_sz; ++j)
                    {
                        for (size_t k = 0u; k < PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ; ++k)
                        {
                            for (size_t z = 0u; z < PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ; ++z)
                            {
                                visitor(matrix->being_vec[i]->process_group_vec[j]->process_vec[k].logit_vec[z]);
                            }
                        }
                    }
                }
            }

        public:

            constexpr DispatchCodeGenerator(const std::shared_ptr<tensor_model::Matrix>& matrix,
                                            size_t table_sz): hasher(taylor_matrix::host_matrix::sip_hasher::implicit_key_tag{}, get_sip_hasher_key()),
                                                              previous_dispatch_code(std::nullopt),
                                                              table_sz(table_sz)
            {
                if (matrix == nullptr)
                {
                    throw std::invalid_argument("bad matrix, null");
                }

                if (!stdx::is_pow2(table_sz))
                {
                    throw std::invalid_argument("bad table size, not pow 2");
                }

                auto visitor = [&](tensor_std_float_t e)
                {
                    this->hasher.update(e);
                };

                visit_matrix(matrix.get(), visitor);
            }

            constexpr auto get_dispatch_code() -> size_t
            {
                size_t clue                     = this->previous_dispatch_code.value_or(0u);
                this->hasher.update(clue);

                size_t hash_result              = this->hasher.get_hash();
                this->previous_dispatch_code    = hash_result;
                size_t result                   = hash_result % this->table_sz;

                return result;
            }
    };
}

#endif
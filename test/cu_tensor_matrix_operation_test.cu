#include <taylor_matrix/cuda_matrix/tensor_matrix_operation.h>
#include <serializer/dg_buf.h>
#include <string>
#include <cuda_management/scope_allocator.h>
#include <cuda_management/utility.h>
#include <unordered_map>
#include <vector>

template <class FocalSizeVector,
          class SuffixMap,
          class RotationSizeVector,
          class ParameterBoundRatioVector>
__global__ void test(FocalSizeVector focal_sz_vec,
                     SuffixMap suffix_map,
                     RotationSizeVector rotation_sz_vec,
                     ParameterBoundRatioVector param_bound_ratio_vec)
{
    using namespace cuda_management::scope_allocator;
    using namespace taylor_matrix::cuda_matrix::utility;
    
    SplitStackAllocator allocator{};
    size_t sz{};

    taylor_matrix::cuda_matrix::tensor_matrix_operation::matrix_transform({},

                                                                          focal_sz_vec, {},
                                                                          suffix_map,

                                                                          rotation_sz_vec, {},
                                                                          param_bound_ratio_vec, {},

                                                                          to_size_container(std::integral_constant<size_t, 1>{}),
                                                                          {}, sz, {},

                                                                          allocator);
}

int main()
{
    using namespace dg::dgbuf::stl_to_dgbuf;

    using FocalSizeVector           = std::vector<size_t>;
    using SuffixMap                 = std::unordered_map<size_t, std::unordered_map<size_t, std::vector<std::vector<size_t>>>>;
    using RotationSizeVector        = std::vector<size_t>;
    using ParameterBoundRatioVector = std::vector<size_t>;

    std::string bstream{};

    auto focal_sz_vec           = serializer{}.serialize(FocalSizeVector{}, bstream);
    auto suffix_map             = serializer{}.serialize(SuffixMap{}, bstream);
    auto rotation_sz_vec        = serializer{}.serialize(RotationSizeVector{}, bstream);
    auto param_bound_ratio_vec  = serializer{}.serialize(ParameterBoundRatioVector{}, bstream);

    test<<<1, 1, 1>>>(focal_sz_vec, suffix_map, rotation_sz_vec, param_bound_ratio_vec);
}
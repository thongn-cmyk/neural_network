#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include <random>
#include <utility>
#include <functional>
#include <chrono>
#include <iostream>
#include <stl_extension/stdx.h>

struct insufficient_logit_vector_size: std::invalid_argument
{
    insufficient_logit_vector_size(): std::invalid_argument("insufficient logit vec size"){}
};

auto randomize_int(size_t first, size_t last) -> size_t
{
    if (first >= last)
    {
        throw std::invalid_argument("bad interval, first >= last");
    }

    static auto randomizer  = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
    const size_t sz         = last - first;

    return first + randomizer() % sz;
}

auto randomize_range(size_t range_sz) -> size_t
{
    return randomize_int(0u, range_sz);
}

auto get_z_number_within_digits(size_t digit_sz) -> size_t
{
    const size_t first      = 0u;
    const size_t last       = std::pow(10, digit_sz);

    return randomize_int(first, last);
}

auto lpad_string(const std::string& s, char c, size_t pad_sz) -> std::string
{
    size_t new_str_sz   = std::max(s.size(), pad_sz);
    std::string pad     = {};

    for (size_t i = s.size(); i < new_str_sz; ++i)
    {
        pad.push_back(c);
    }

    return pad + s;
}

auto z_number_to_string(size_t sz) -> std::string
{
    return std::to_string(sz);
}

auto to_digit_vec(size_t val) -> std::vector<size_t>
{
    if (val == 0u)
    {
        return std::vector<size_t>{0u};
    }

    std::vector<size_t> rs  = {};

    while (val != 0u)
    {
        rs.push_back(val % 10);
        val /= 10;
    }

    return rs;
}

auto get_handroll_calculation_instruction(size_t lhs,
                                          size_t rhs) -> std::vector<std::string>
{
    if (lhs > rhs)
    {
        return get_handroll_calculation_instruction(rhs, lhs);
    }

    std::vector<size_t> lhs_digit_vec           = to_digit_vec(lhs);
    std::vector<size_t> rhs_digit_vec           = to_digit_vec(rhs);
    size_t tentative_rs_sz                      = std::max(lhs_digit_vec.size(), rhs_digit_vec.size()) + 1u;
    std::vector<size_t> rs_digit_vec            = std::vector<size_t>(tentative_rs_sz, 0u);
    size_t carry                                = 0u;
    std::vector<std::string> instruction_vec    = {};
    size_t common_sz                            = lhs_digit_vec.size();
    size_t upper_sz                             = rhs_digit_vec.size();

    instruction_vec.push_back("cbt");

    for (size_t i = 0u; i < common_sz; ++i)
    {
        size_t total    = carry + lhs_digit_vec[i] + rhs_digit_vec[i];

        std::string ins = z_number_to_string(carry)
                            + "+" + z_number_to_string(lhs_digit_vec[i])
                            + "+" + z_number_to_string(rhs_digit_vec[i]) 
                            + "=" + z_number_to_string(total);

        instruction_vec.push_back(ins);

        carry               = total;
        size_t write_val    = carry % 10;
        size_t rem_val      = carry / 10;

        instruction_vec.push_back("wr");
        instruction_vec.push_back(z_number_to_string(write_val));
        instruction_vec.push_back("rm");
        instruction_vec.push_back(z_number_to_string(rem_val));

        carry               = rem_val;
    }

    instruction_vec.push_back("lbt");

    for (size_t i = common_sz; i < upper_sz; ++i)
    {
        size_t total        = carry + rhs_digit_vec[i];

        std::string ins     = z_number_to_string(carry)
                                + "+" + z_number_to_string(rhs_digit_vec[i])
                                + "=" + z_number_to_string(total);

        instruction_vec.push_back(ins);

        carry               = total;
        size_t write_val    = carry % 10;
        size_t rem_val      = carry / 10;

        instruction_vec.push_back("wr");
        instruction_vec.push_back(z_number_to_string(write_val));
        instruction_vec.push_back("rm");
        instruction_vec.push_back(z_number_to_string(rem_val));

        carry               = rem_val;
    }

    instruction_vec.push_back("fl");

    if (carry != 0u)
    {
        instruction_vec.push_back(z_number_to_string(carry));
    }

    return instruction_vec;
}

struct Equation
{
    std::string eqn;
    std::string instruction;
};

auto join(const std::vector<std::string>& ins_vec, char c) -> std::string
{
    if (ins_vec.empty())
    {
        return {};
    }

    std::string rs = ins_vec.front();

    for (size_t i = 1u; i < ins_vec.size(); ++i)
    {
        rs += c;
        rs += ins_vec[i];
    }

    return rs;
}

auto encode_instruction(const std::vector<std::string>& ins_vec) -> std::string
{
    return join(ins_vec, ',');
}

auto encode_equation(const Equation& eqn) -> std::string
{
    return join({eqn.eqn, eqn.instruction}, '|');
}

auto encode_equation_vector(const std::vector<Equation>& eqn_vec) -> std::string
{
    std::vector<std::string> encoded_eqn_vec = {};

    for (const auto& eqn: eqn_vec)
    {
        encoded_eqn_vec.push_back(encode_equation(eqn));
    }

    return join(encoded_eqn_vec, '#');
}

auto get_addition_equation_string_vector(size_t digit_sz,
                                         size_t calculation_sz) -> std::vector<Equation>
{
    size_t operable_digit_sz    = digit_sz + 1;
    std::vector<Equation> rs    = {};

    for (size_t i = 0u; i < calculation_sz; ++i)
    {
        size_t lhs              = get_z_number_within_digits(digit_sz);
        size_t rhs              = get_z_number_within_digits(digit_sz);
        size_t total            = lhs + rhs;

        std::string lhs_str     = lpad_string(z_number_to_string(lhs), '0', operable_digit_sz);
        std::string rhs_str     = lpad_string(z_number_to_string(rhs), '0', operable_digit_sz);
        std::string total_str   = lpad_string(z_number_to_string(total), '0', operable_digit_sz);

        std::string eqn         = lhs_str + "+" + rhs_str + "=" + total_str;

        rs.push_back
        (
            Equation
            {
                .eqn            = std::move(eqn),
                .instruction    = encode_instruction(get_handroll_calculation_instruction(lhs, rhs))
            }
        );
    }

    return rs;
}

struct Token
{
    std::string context;
    char hex_code;
};

auto numeric_hex_code_to_char_hex_code(uint8_t hex_code)
{
    if (hex_code < 10)
    {
        return '0' + hex_code;
    }
    else if (hex_code < 16)
    {
        return 'a' + (hex_code - 10);
    }
    else
    {
        throw std::invalid_argument("bad hex numeric hex code, not within [0, 16) range");
    }
}

auto hex_decode(uint8_t c) -> std::pair<char, char>
{
    uint8_t lo  = c % 16;
    uint8_t hi  = c / 16;

    return
    {
        numeric_hex_code_to_char_hex_code(lo),
        numeric_hex_code_to_char_hex_code(hi)
    };
}

auto to_hex_vec(const std::string& s) -> std::string
{
    std::string rs{};

    for (char c: s)
    {
        auto [lo, hi] = hex_decode(std::bit_cast<uint8_t>(c));

        rs.push_back(lo);
        rs.push_back(hi);
    }

    return rs;
}

auto tokenize(const std::string& s,
              size_t hex_window_sz,
              size_t token_sz) -> std::vector<Token>
{
    std::vector<Token> tok_vec      = {};
    std::string hex_vec             = to_hex_vec(s);

    if (hex_vec.empty())
    {
        return {};
    }

    size_t prediction_last              = hex_vec.size();

    for (size_t i = 0u; i < token_sz; ++i)
    {
        Token tok                           = {};

        size_t random_first                 = std::min(hex_window_sz, prediction_last);
        size_t random_last                  = prediction_last;
        size_t random_sz                    = random_last - random_first;

        if (random_sz == 0u)
        {
            continue;
        }

        size_t idx                          = random_first + randomize_range(random_sz);
        size_t ctx_window_first             = std::max(static_cast<intmax_t>(idx) - static_cast<intmax_t>(hex_window_sz), intmax_t{0});
        std::string tmp_hex_vec             = {};

        for (size_t j = ctx_window_first; j < idx; ++j)
        {
            tmp_hex_vec.push_back(hex_vec[j]);
        }

        tok_vec.push_back
        (
            Token
            {
                .context    = tmp_hex_vec,
                .hex_code   = hex_vec[idx]
            }
        );
    }

    return tok_vec;
}

auto two_dimensional_project(float x0, float x1,
                            float a, float b, float c) -> float
{
    return x0 * a + x1 * b + c;
}

auto one_dimensional_project(float x,
                             float a, float b) -> float
{
    return a * x + b;
}

template <class SizeContainer>
void two_to_one_project(const float * lhs,
                        const float * rhs,
                        float * output,
                        SizeContainer sz,
                        float interpolation_x_first, float interpolation_x_last, size_t discretization_sz,
                        const float * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap)
{
    const size_t PER_PROJECTION_SZ  = discretization_sz * discretization_sz * 3u;
    const size_t REQUIRED_SZ        = sz.get() * sz.get() * PER_PROJECTION_SZ;
    size_t next_offset              = coeff_arr_offset + REQUIRED_SZ;

    if (next_offset > coeff_arr_cap)
    {
        throw insufficient_logit_vector_size();
    }

    if (discretization_sz == 0u)
    {
        throw std::invalid_argument("bad discretization size, 0");
    }

    std::fill(output, std::next(output, sz.get()), 0);

    float interval      =  (interpolation_x_last - interpolation_x_first) / discretization_sz;
    float inv_interval  =  1 / interval;

    for (size_t i = 0u; i < sz.get(); ++i)
    {
        for (size_t j = 0u; j < sz.get(); ++j)
        {
            size_t flat_slot                        = i * sz.get() + j;
            size_t relative_proj_flat_offset        = flat_slot * PER_PROJECTION_SZ;

            size_t lhs_interpolation_idx            = std::min(std::max(static_cast<intmax_t>((lhs[i] - interpolation_x_first) * inv_interval),
                                                                        intmax_t{0}),
                                                               static_cast<intmax_t>(discretization_sz) - 1);

            size_t rhs_interpolation_idx            = std::min(std::max(static_cast<intmax_t>((rhs[j] - interpolation_x_first) * inv_interval),
                                                                        intmax_t{0}),
                                                               static_cast<intmax_t>(discretization_sz) - 1);

            size_t flat_interpolation_slot          = lhs_interpolation_idx * discretization_sz + rhs_interpolation_idx;
            size_t relative_interpolation_offset    = flat_interpolation_slot * 3u;

            size_t global_offset                    = coeff_arr_offset + relative_proj_flat_offset + relative_interpolation_offset;

            float a                                 = coeff_arr[(global_offset + 0u)];
            float b                                 = coeff_arr[(global_offset + 1u)];
            float c                                 = coeff_arr[(global_offset + 2u)];

            output[i]                               += two_dimensional_project(lhs[i], rhs[j], a, b, c);
        }
    }

    coeff_arr_offset    = next_offset;
}

template <class SizeContainer>
void one_to_one_project(const float * lhs,
                        float * output,
                        SizeContainer sz,
                        float interpolation_x_first, float interpolation_x_last, size_t discretization_sz,
                        const float * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap)
{
    const size_t PER_PROJECTION_SZ  = discretization_sz * 2u;
    const size_t REQUIRED_SZ        = sz.get() *  PER_PROJECTION_SZ;
    size_t next_offset              = coeff_arr_offset + REQUIRED_SZ;

    if (next_offset > coeff_arr_cap)
    {
        throw insufficient_logit_vector_size();
    }

    if (discretization_sz == 0u)
    {
        throw std::invalid_argument("bad discretization size, 0");
    }

    std::fill(output, std::next(output, sz.get()), 0);

    float interval      =  (interpolation_x_last - interpolation_x_first) / discretization_sz;
    float inv_interval  =  1 / interval;

    for (size_t i = 0u; i < sz.get(); ++i)
    {
        size_t flat_slot                        = i;
        size_t relative_proj_flat_offset        = flat_slot * PER_PROJECTION_SZ;

        size_t lhs_interpolation_idx            = std::min(std::max(static_cast<intmax_t>((lhs[i] - interpolation_x_first) * inv_interval),
                                                                    intmax_t{0}),
                                                            static_cast<intmax_t>(discretization_sz) - 1);

        size_t flat_interpolation_slot          = lhs_interpolation_idx;
        size_t relative_interpolation_offset    = flat_interpolation_slot * 2u;

        size_t global_offset                    = coeff_arr_offset + relative_proj_flat_offset + relative_interpolation_offset;

        float a                                 = coeff_arr[(global_offset + 0u)];
        float b                                 = coeff_arr[(global_offset + 1u)];

        output[i]                               += one_dimensional_project(lhs[i], a, b);
    }

    coeff_arr_offset    = next_offset;
}

template <class SizeContainer>
void inplace_one_to_one_project(float * lhs,
                                SizeContainer sz,
                                float interpolation_x_first, float interpolation_x_last, size_t discretization_sz,
                                const float * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap)
{
    const size_t PER_PROJECTION_SZ  = discretization_sz * 2u;
    const size_t REQUIRED_SZ        = sz.get() *  PER_PROJECTION_SZ;
    size_t next_offset              = coeff_arr_offset + REQUIRED_SZ;

    if (next_offset > coeff_arr_cap)
    {
        throw insufficient_logit_vector_size();
    }

    if (discretization_sz == 0u)
    {
        throw std::invalid_argument("bad discretization size, 0");
    }

    float interval      =  (interpolation_x_last - interpolation_x_first) / discretization_sz;
    float inv_interval  =  1 / interval;

    for (size_t i = 0u; i < sz.get(); ++i)
    {
        size_t flat_slot                        = i;
        size_t relative_proj_flat_offset        = flat_slot * PER_PROJECTION_SZ;

        size_t lhs_interpolation_idx            = std::min(std::max(static_cast<intmax_t>((lhs[i] - interpolation_x_first) * inv_interval),
                                                                    intmax_t{0}),
                                                            static_cast<intmax_t>(discretization_sz) - 1);

        size_t flat_interpolation_slot          = lhs_interpolation_idx;
        size_t relative_interpolation_offset    = flat_interpolation_slot * 2u;

        size_t global_offset                    = coeff_arr_offset + relative_proj_flat_offset + relative_interpolation_offset;

        float a                                 = coeff_arr[(global_offset + 0u)];
        float b                                 = coeff_arr[(global_offset + 1u)];

        lhs[i]                                  += one_dimensional_project(lhs[i], a, b);
    }

    coeff_arr_offset    = next_offset;
}

template <class SizeContainer>
void add_to(float * dst,
            const float * src,
            SizeContainer sz)
{
    for (size_t i = 0u; i < sz.get(); ++i)
    {
        dst[i] += src[i];
    }
}

//we are done with BS, it's time to get this done

consteval auto transform_base_unit_size() -> size_t
{
    return 2u;
}

consteval auto transform_base_group_size() -> size_t
{
    return 4u;
}

void rotate(float * matrix_arr, size_t matrix_arr_sz,
            size_t rotation_unit_sz)
{
    if (rotation_unit_sz == 0u)
    {
        throw std::invalid_argument("bad rotation unit size, 0");
    }

    if (matrix_arr_sz % rotation_unit_sz != 0u)
    {
        throw std::invalid_argument("bad rotation array size, not multiples of rotation unit size");
    }

    size_t matrix_element_sz    = matrix_arr_sz / rotation_unit_sz;
    size_t sqrt_sz              = std::sqrt(matrix_element_sz);

    if (sqrt_sz * sqrt_sz != matrix_element_sz)
    {
        throw std::invalid_argument("not square matrix");
    }

    size_t n                    = sqrt_sz;

    for (size_t i = 0u; i < n; ++i)
    {
        for (size_t j = i + 1; j < n; ++j)
        {
            for (size_t z = 0u; z < rotation_unit_sz; ++z)
            {
                size_t lhs_flat_matrix_idx  = i * n + j;
                size_t lhs_idx              = lhs_flat_matrix_idx * rotation_unit_sz + z;
                size_t rhs_flat_matrix_idx  = j * n + i;
                size_t rhs_idx              = rhs_flat_matrix_idx * rotation_unit_sz + z;

                std::swap(matrix_arr[lhs_idx], matrix_arr[rhs_idx]);
            }
        }
    }
}

void transform(float * matrix_arr, size_t matrix_arr_sz,
               size_t rotation_sz,
               float one_d_interpolation_x_first, float one_d_interpolation_x_last, size_t one_d_discretization_sz,
               float two_d_interpolation_x_first, float two_d_interpolation_x_last, size_t two_d_discretization_sz,
               const float * coeff_arr, size_t& coeff_arr_offset, size_t coeff_arr_cap)
{
    constexpr size_t BASE_UNIT_SZ   = transform_base_unit_size();
    constexpr size_t BASE_GROUP_SZ  = transform_base_group_size();
    constexpr size_t BASE_SZ        = BASE_GROUP_SZ * BASE_UNIT_SZ;

    if (matrix_arr_sz == 0u)
    {
        return;
    }

    if (matrix_arr_sz == BASE_SZ * 2)
    {
        std::array<float, BASE_SZ> transformed_lhs{};
        std::array<float, BASE_SZ> transformed_rhs{};

        two_to_one_project
        (
            matrix_arr,
            std::next(matrix_arr, BASE_SZ),
            transformed_lhs.data(),
            stdx::to_size_container(std::integral_constant<size_t, BASE_SZ>()),
            two_d_interpolation_x_first, two_d_interpolation_x_last, two_d_discretization_sz,
            coeff_arr, coeff_arr_offset, coeff_arr_cap
        );

        two_to_one_project
        (
            std::next(matrix_arr, BASE_SZ),
            matrix_arr,
            transformed_rhs.data(),
            stdx::to_size_container(std::integral_constant<size_t, BASE_SZ>()),
            two_d_interpolation_x_first, two_d_interpolation_x_last, two_d_discretization_sz,
            coeff_arr, coeff_arr_offset, coeff_arr_cap
        );

        add_to
        (
            matrix_arr,
            transformed_lhs.data(),
            stdx::to_size_container(std::integral_constant<size_t, BASE_SZ>())
        );

        add_to
        (
            std::next(matrix_arr, BASE_SZ),
            transformed_rhs.data(),
            stdx::to_size_container(std::integral_constant<size_t, BASE_SZ>())
        );

        //

        inplace_one_to_one_project
        (
            matrix_arr,
            stdx::to_size_container(std::integral_constant<size_t, BASE_SZ * 2u>{}),
            one_d_interpolation_x_first, one_d_interpolation_x_last, one_d_discretization_sz,
            coeff_arr, coeff_arr_offset, coeff_arr_cap
        );

        return;
    }

    if (matrix_arr_sz % BASE_SZ != 0u)
    {
        throw std::invalid_argument("bad matrix array size, not multiples of BASE_SZ");
    }

    size_t matrix_element_sz    = matrix_arr_sz / BASE_SZ;
    size_t sqrt_sz              = std::sqrt(matrix_element_sz);

    if (sqrt_sz * sqrt_sz != matrix_element_sz)
    {
        throw std::invalid_argument("not square matrix");
    }

    for (size_t i = 0u; i < rotation_sz; ++i)
    {
        const size_t saved_coeff_arr_offset = coeff_arr_offset;

        for (size_t j = 0u; j < sqrt_sz; ++j)
        {
            coeff_arr_offset            = saved_coeff_arr_offset;

            size_t matrix_arr_first     = j * sqrt_sz * BASE_SZ;
            size_t matrix_arr_last      = (j + 1) * sqrt_sz * BASE_SZ;

            float * nxt_matrix_arr      = std::next(matrix_arr, matrix_arr_first);
            size_t nxt_matrix_arr_sz    = matrix_arr_last - matrix_arr_first;

            transform(nxt_matrix_arr, nxt_matrix_arr_sz,
                      rotation_sz,
                      one_d_interpolation_x_first, one_d_interpolation_x_last, one_d_discretization_sz,
                      two_d_interpolation_x_first, two_d_interpolation_x_last, two_d_discretization_sz,
                      coeff_arr, coeff_arr_offset, coeff_arr_cap);
        }

        rotate(matrix_arr, matrix_arr_sz, BASE_SZ);
    }

    inplace_one_to_one_project
    (
        matrix_arr,
        stdx::to_size_container(matrix_arr_sz),
        one_d_interpolation_x_first, one_d_interpolation_x_last, one_d_discretization_sz,
        coeff_arr, coeff_arr_offset, coeff_arr_cap
    );
}


class SomeMatrix: public virtual the_matrix::MatrixInterface
{
    private:

        std::vector<tensor_std_float_t> coeff_vec;
    
    public:

        SomeMatrix(std::vector<tensor_std_float_t> coeff_vec): coeff_vec(std::move(coeff_vec)){}

        auto get_coefficient_vector() -> std::vector<tensor_std_float_t>
        {
            return this->coeff_vec;
        }

        void set_coefficient_vector(const std::vector<tensor_std_float_t>& arg)
        {
            for (tensor_std_float_t e: arg)
            {
                if (std::isnan(e))
                {
                    throw std::invalid_argument("bad float, NaN");
                }
            }

            this->coeff_vec = arg;
        }

        auto project(const std::vector<std::shared_ptr<tensor_model::Matrix>>&) -> std::vector<std::shared_ptr<tensor_model::Matrix>>
        {
            throw std::invalid_argument("project function not supported");
        }

        auto clone() -> std::shared_ptr<the_matrix::MatrixInterface>
        {
            return std::make_shared<SomeMatrix>(*this);
        }
};

class SomeProjector
{
    private:

        size_t rotation_sz,
        
        float one_d_x_first;
        float one_d_x_last;
        size_t one_d_discretization_sz;

        float two_d_x_first;
        float two_d_x_last;
        size_t two_d_discretization_sz;

    public:

        SomeProjector(size_t rotation_sz,

                      float one_d_x_first,
                      float one_d_x_last,
                      size_t one_d_discretization_sz,
                    
                      float two_d_x_first,
                      float two_d_x_last,
                      size_t two_d_discretization_sz): rotation_sz(rotation_sz),
                                                       one_d_x_first(one_d_x_first),
                                                       one_d_x_last(one_d_x_last)
                                                       one_d_discretization_sz(one_d_discretization_sz),
                                                       two_d_x_first(two_d_x_first),
                                                       two_d_x_last(two_d_x_last),
                                                       two_d_discretization_sz(two_d_discretization_sz){}

        void project(float * x_arr, size_t x_arr_sz,
                     const float * coeff_arr, size_t coeff_arr_cap)
        {
            size_t coeff_arr_offset = 0u;
            
            transform
            (
                x_arr, x_arr_sz,
                this->rotation_sz,
                this->one_d_x_first, this->one_d_x_last, this->one_d_discretization_sz,
                this->two_d_x_first, this->two_d_x_last, this->two_d_discretization_sz,
                coeff_arr, coeff_arr_offset, coeff_arr_cap
            );
        }
};

struct Projection
{
    std::vector<float> x;
    std::vector<float> y;
};

class PointPullMatrixEvaluator: public virtual matrix_evaluator::MatrixEvaluatorInterface
{
    private:

        std::vector<Projection> training_pair_vec;
        std::unique_ptr<SomeProjector> projector;

    public:

        PointPullMatrixEvaluator(std::vector<Projection> training_pair_vec,
                                 std::unique_ptr<SomeProjector> projector): training_pair_vec(std::move(training_pair_vec)),
                                                                            projector(std::move(projector)){}

        auto get_deviation(the_matrix::MatrixInterface& matrix) -> eval_float_t
        {
            std::vector<float> coeff_vec        = stdx::to_castable_vector_initializer(matrix.get_coefficient_vector());
            std::vector<float> expected_y_vec   = {};
            std::vector<float> projected_y_vec  = {};

            for (const Projection& projection: this->training_pair_vec)
            {
                expected_y_vec.push_back(projection.y);

                std::vector<float> actual_y = projection.x;
                
                this->projector->project(actual_y.data(), actual_y.size(),
                                         coeff_vec.data(), coeff_vec.size());

                projected_y_vec.push_back(actual_y);
            }

            return this->parity_distance(projected_y_vec, expected_y_vec);
        }
    
    private:
        
        auto parity_distance(const std::vector<tensor_std_float_t>& lhs_flat_tensor_vec,
                             const std::vector<tensor_std_float_t>& rhs_flat_tensor_vec) -> double
        {
            if (lhs_flat_tensor_vec.size() != rhs_flat_tensor_vec.size())
            {
                std::cout << "mayday, mismatched tensor logit vector\n";
                std::abort();
            }

            double lhs_false_sum    = 0;
            double lhs_true_sum     = 0;

            double rhs_false_sum    = 0;
            double rhs_true_sum     = 0;

            std::vector<tensor_std_float_t> true_vec{};
            std::vector<tensor_std_float_t> false_vec{};

            for (size_t i = 0u; i < rhs_flat_tensor_vec.size(); ++i)
            {
                if (rhs_flat_tensor_vec[i] == 1)
                {
                    rhs_true_sum    += 1;
                    lhs_true_sum    = std::max(lhs_true_sum, static_cast<double>(std::exp(lhs_flat_tensor_vec[i])));
                }
                else
                {
                    rhs_false_sum   += 0;
                    lhs_false_sum   = std::max(lhs_false_sum, static_cast<double>(std::exp(lhs_flat_tensor_vec[i])));
                }
            }

            double lhs_parity       = lhs_true_sum - lhs_false_sum;
            double rhs_parity       = rhs_true_sum - rhs_false_sum;

            if (lhs_parity > 0)
            {
                return 0;
            }

            return std::pow(lhs_parity - rhs_parity, 2); 
        }
};

auto get_optimizer() -> std::unique_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
{
    return std::make_unique<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine>
    (
        matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngineConfig
        {
            .matrix_cache_map_cap                       = randomize_optional_int<uint64_t>(0, size_t{1} << 4),
            .time_machine_cache_map_cap                 = randomize_optional_int<uint64_t>(0, size_t{1} << 4),
            .optimization_epoch_sz                      = 128ULL,
            .optimization_step_sz                       = 4ULL,
            .optimization_loop_sz                       = 2ULL
        }
    );
}

void run_test()
{
    const size_t HEIGHT                 = 4;
    const float DISCRETIZATION_VALUE    = 0.2;
    const size_t SEMANTIC_SZ            = 2;
    const size_t EPOCH_SZ               = size_t{1} << 8;

    std::shared_ptr<NodeContainer> node_container   = make_node_container(make_tree(HEIGHT, DISCRETIZATION_VALUE, SEMANTIC_SZ));
    std::vector<Projection> projection_vec          = make_projection(node_container->root);

    std::unique_ptr<matrix_optimizer_subsystem::CoordinatedSearchOptimizerEngine> optimizer = get_optimizer();
    std::vector<tensor_std_float_t> tensor_vec                                              = std::vector<tensor_std_float_t>(get_binary_unf_interpolated_projection_size(projection_vec.front().x.size(),
                                                                                                                                                                          0,
                                                                                                                                                                          DISCRETIZATION_VALUE * SEMANTIC_SZ,
                                                                                                                                                                          SEMANTIC_SZ),
                                                                                                                              0.f);

    std::cout << "projection vector size > " << projection_vec.size() << "\n";
    std::cout << "coefficient vector size > " << tensor_vec.size() << "\n";

    std::shared_ptr<the_matrix::MatrixInterface> matrix                                     = std::make_unique<SomeMatrix>(std::move(tensor_vec));
    std::unique_ptr<SomeProjector> projector                                                = std::make_unique<SomeProjector>(0,
                                                                                                                              DISCRETIZATION_VALUE * SEMANTIC_SZ,
                                                                                                                              SEMANTIC_SZ);

    std::unique_ptr<PointPullMatrixEvaluator> matrix_evaluator                              = std::make_unique<PointPullMatrixEvaluator>(projection_vec, std::move(projector));
    common_exception::CancellationToken cancellation_token                                  = {};

    {
        double optimized_deviation  = matrix_evaluator->get_deviation(*matrix);

        std::cout << "i > " << -1 << " deviation > " << optimized_deviation << "\n";
    }

    for (size_t i = 0u; i < EPOCH_SZ; ++i)
    {
        matrix                      = optimizer->optimize(*matrix, *matrix_evaluator, cancellation_token);
        double optimized_deviation  = matrix_evaluator->get_deviation(*matrix);

        std::cout << "i > " << i << " deviation > " << optimized_deviation << "\n";
    }
}

int main()
{
    run_test();
}
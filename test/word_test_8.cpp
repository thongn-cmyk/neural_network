//let's move on to the next test of calculator and absolute true entropy transformation
//in this test, we are expecting 100% of infinite number of calculation within the boundaries

//what differs is that in the normal next word prediction scheme, things are hazy, we have different weights for the next word
//what we truly want from the scheme is idea transformation, for the next word is not absolutely true, this is a topic that is hard to explain

//but essentially, we only have one variable (our true North) of deviation to train, the weights of the tokens are to be debated by statistics and AI

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include <random>
#include <utility>
#include <functional>
#include <chrono>
#include <iostream>

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

//it's funny because this backtrack of operations is so deep-rooted that I can't tell my calculator to move a cursor to a certain position
//I'm convinced that this is hard to backtrack without common_knowledge

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

//OK, so our strategy is to train on a massive dataset, and adjust the weights accordingly based on entropy level and training time
//this should reach 100% compression rate, proved that our dynamic transformation scheme successful

//we argued the other day that only exponential interpolation ever makes sense as base projection (it's either line broke or deterministic random, it's concluded)
//so let's use that and merge the interpolation changes into the code base
//it's complicated but we should be fast and efficient first before we scale something else

int main()
{
    size_t calculation_digit_sz = 3u;
    size_t calculation_sz       = 100u;
    size_t window_sz            = 32u;
    size_t token_sz             = 100u;

    std::vector<Token> tok_vec  = tokenize(encode_equation_vector(get_addition_equation_string_vector(calculation_digit_sz, calculation_sz)),
                                           window_sz,
                                           token_sz);

    for (const auto& tok: tok_vec)
    {
        std::cout << "context > " << tok.context << "\n";
        std::cout << "hex_code > " << tok.hex_code << "\n";
        std::cout << "---------------\n";
    }
}
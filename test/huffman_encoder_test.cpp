#define STRONG_MEMORY_ORDERING_FLAG true

#include <serializer/huffman_encoder.h>
#include <functional>
#include <utility>
#include <random>

auto get_random_string() -> std::string
{
    const size_t STR_SZ_RANGE   = size_t{1} << 6;
    static auto randomizer      = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count())});
    size_t str_sz               = randomizer() % STR_SZ_RANGE;

    std::string rs{};

    for (size_t i = 0u; i < str_sz; ++i)
    {
        rs.push_back(std::bit_cast<char>(static_cast<uint8_t>(randomizer() & std::numeric_limits<uint8_t>::max())));
    }

    return rs;
}

void test_one_huffman_encoder()
{
    std::string random_str          = get_random_string();
    std::string other_random_str    = dg::network_huffman_encoder::decode(dg::network_huffman_encoder::encode(random_str));

    if (random_str != other_random_str)
    {
        std::cout << "mayday, huffman encoder value mismatched" << std::endl;
        std::abort();
    }
}

void test_huffman_encoder()
{
    const size_t TEST_SZ    = size_t{1} << 40;
    const size_t COUT_SZ    = size_t{1} << 10;

    std::cout << "__BEGIN_HUFFMAN_ENCODER_TEST__" << std::endl;

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << std::endl;
        }
    }

    std::cout << "__END_HUFFMAN_ENCODER_TEST__" << std::endl;
}

int main()
{
    test_huffman_encoder();
}
#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <data_loader/source_loader/multisource_loader/multisource_loader.h>
#include <data_loader/source_loader/generic_loader/generic_loader.h>
#include <data_loader/source/file_source/file_source.h>
#include <data_loader/hex_encoder/hex_encoder.h>
#include <common_exception/cancellation_token.h>
#include <random>
#include <algorithm>
#include <functional>
#include <utility>
#include <chrono>
#include <iostream>
#include <filesystem>

static inline constexpr char DELIM_CHAR = ',';
static inline constexpr char EOR_CHAR   = '\0';

auto randomize_int(size_t sz) -> size_t
{
    static auto randomizer = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});

    return randomizer() % sz;
}

auto generate_random_string(size_t str_sz) -> std::string
{
    std::string rs(str_sz, ' ');

    for (size_t i = 0u; i < str_sz; ++i)
    {
        rs.push_back(std::bit_cast<char>(static_cast<uint8_t>(randomize_int(256))));
    }

    return rs;
}

auto generate_random_token() -> std::string
{
    size_t STR_SZ_RANGE = size_t{1} << 4;

    return generate_random_string(randomize_int(STR_SZ_RANGE));
}

auto generate_random_token_vec() -> std::vector<std::string>
{
    size_t TOKEN_VEC_SZ_RANGE   = size_t{1} << 4;
    size_t TOKEN_VEC_SZ         = randomize_int(TOKEN_VEC_SZ_RANGE);
    std::vector<std::string> rs = {};

    for (size_t i = 0u; i < TOKEN_VEC_SZ; ++i)
    {
        rs.push_back(generate_random_token());
    }

    return rs;
}

auto hex_encode_token(const std::string& inp) -> std::string
{
    return data_loader::hex_encoder::hex_encode(inp);
}

auto hex_decode_token(const std::string& inp) -> std::string
{
    return data_loader::hex_encoder::hex_decode(inp);
}

auto join(const std::vector<std::string>& data, char c) -> std::string
{
    if (data.empty())
    {
        return {};
    }

    std::string rs = data[0];

    for (size_t i = 1u; i < data.size(); ++i)
    {
        rs += c;
        rs += data[i];
    }

    return rs;
}

auto randomize_delim_config() -> data_loader::stream_reader::DelimitedStreamReaderConfig
{
    return
    {
        .delim_char     = DELIM_CHAR,
        .eor_char       = EOR_CHAR,
        .max_token_sz   = (randomize_int(2) == 0u) ? std::optional<uint64_t>(std::nullopt)
                                                   : std::optional<uint64_t>(std::numeric_limits<uint64_t>::max())
    };
}

auto randomize_external_delim_config() -> data_loader::stream_reader::ExternalDelimitedStreamReaderConfig
{
    return data_loader::stream_reader::to_external_delimited_stream_reader_config(randomize_delim_config());
}

auto randomize_file_loader_config(const std::string& file_path) -> data_loader::source::file_source::FileLoaderConfig
{
    return 
    {
        .delim_config               = randomize_external_delim_config(),
        .local_file_path            = file_path,
        .read_ahead_buffer_sz_hint  = (randomize_int(2) == 0u) ? std::optional<uint64_t>(std::nullopt)
                                                               : std::optional<uint64_t>(randomize_int(size_t{1} << 4)),
        .unit_byte_sz_hint          = (randomize_int(2) == 0u) ? std::optional<uint64_t>(std::nullopt)
                                                               : std::optional<uint64_t>(randomize_int(size_t{1} << 4))
    };
}

auto randomize_external_file_loader_config(const std::string& file_path) -> data_loader::source::file_source::ExternalFileLoaderConfig
{
    return data_loader::source::file_source::to_external_file_loader_config(randomize_file_loader_config(file_path));
}

auto randomize_generic_reader_config(const std::string& file_path) -> data_loader::source::generic_source::GenericReaderConfig
{
    return
    {
        .source = randomize_external_file_loader_config(file_path)
    };
}

auto randomize_external_generic_reader_config(const std::string& file_path) -> data_loader::source::generic_source::ExternalGenericReaderConfig
{
    return data_loader::source::generic_source::to_external_generic_reader_config(randomize_generic_reader_config(file_path));
}

auto randomize_normal_retry_config() -> data_loader::retryer_device::normal_device::RetryConfig
{
    return
    {
        .base_wait_time             = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)),
        .exponential_base           = randomize_int(2) + 2,
        .max_retry_count            = randomize_int(size_t{1} << 2),
        .retryable_exception_vec    = std::nullopt
    };
}

auto randomize_external_normal_retry_config() -> data_loader::retryer_device::normal_device::ExternalRetryConfig
{
    return data_loader::retryer_device::normal_device::to_external_retry_config(randomize_normal_retry_config());
}

auto randomize_generic_retry_config() -> data_loader::retryer_device::generic_device::GenericRetryConfig
{
    return
    {
        .config = randomize_external_normal_retry_config()
    };
}

auto randomize_external_generic_retry_config() -> data_loader::retryer_device::generic_device::ExternalGenericRetryConfig
{
    return data_loader::retryer_device::generic_device::to_external_generic_retry_config(randomize_generic_retry_config());
}

auto randomize_source_transaction_broker_config(const std::string& file_path) -> data_loader::transaction_broker::SourceTransactionBrokerConfig
{
    return
    {
        .source_config  = randomize_external_generic_reader_config(file_path),
        .retry_config   = randomize_external_generic_retry_config()
    };
}

auto randomize_external_source_transaction_broker_config(const std::string& file_path) -> data_loader::transaction_broker::ExternalSourceTransactionBrokerConfig
{
    return data_loader::transaction_broker::to_external_source_transaction_broker_config(randomize_source_transaction_broker_config(file_path));
}

auto randomize_wait_loader_config(const std::string& file_path) -> data_loader::source_loader::wait_loader::WaitLoaderConfig
{
    return
    {
        .tx_sz          = randomize_int(size_t{1} << 4) + 1,
        .broker_config  = randomize_external_source_transaction_broker_config(file_path)
    };
}

auto randomize_external_wait_loader_config(const std::string& file_path) -> data_loader::source_loader::wait_loader::ExternalWaitLoaderConfig
{
    return data_loader::source_loader::wait_loader::to_external_wait_loader_config(randomize_wait_loader_config(file_path));
}

auto randomize_config(const std::string& file_path) -> data_loader::source_loader::generic_loader::GenericLoaderConfig
{
    return
    {
        .config = randomize_external_wait_loader_config(file_path)
    };
}

auto randomize_external_config(const std::string& file_path) -> data_loader::source_loader::generic_loader::ExternalGenericLoaderConfig
{
    return data_loader::source_loader::generic_loader::to_external_generic_loader_config(randomize_config(file_path));
}

auto randomize_multisource_loader_config(const std::string& file_path) -> data_loader::source_loader::multisource_loader::MultisourceLoaderConfig
{
    return 
    {
        .config_vec = {randomize_external_config(file_path)}
    };
}

auto randomize_external_multisource_loader_config(const std::string& file_path) -> data_loader::source_loader::multisource_loader::ExternalMultisourceLoaderConfig
{
    return data_loader::source_loader::multisource_loader::to_external_multisource_loader_config(randomize_multisource_loader_config(file_path));
}

auto join_hex_token(const std::vector<std::string>& token_vec) -> std::string
{
    if (token_vec.empty())
    {
        return "";
    }

    return join(token_vec, DELIM_CHAR) + EOR_CHAR;
}

auto randomize_tx_hint_size() -> uint64_t
{
    return randomize_int(size_t{1} << 4);
}

void run_one_test()
{
    std::vector<std::string> org_vec    = generate_random_token_vec();
    std::vector<std::string> token_vec  = {};

    for (const auto& str: org_vec)
    {
        token_vec.push_back(hex_encode_token(str));
    }

    std::string file_output = join_hex_token(token_vec);
    std::string FILE_PATH   = "data_loader_file_source_test.bin";

    {
        std::ofstream out_file(FILE_PATH, std::ios::binary);
        out_file.write(file_output.data(), file_output.size());
    }

    common_exception::CancellationToken cancellation_token{};
    data_loader::source_loader::multisource_loader::MultisourceLoader loader(randomize_external_multisource_loader_config(FILE_PATH));

    std::vector<std::string> rs{};

    while (true)
    {
        auto incremental_rs = loader.get(cancellation_token);

        if (!incremental_rs.has_value())
        {
            break;
        }
        
        rs.push_back(incremental_rs.value());
    }

    std::vector<std::string> org_vec_2{};

    for (const auto& token: rs)
    {
        org_vec_2.push_back(hex_decode_token(token));
    }

    if (org_vec != org_vec_2)
    {
        std::cout << "mayday, mismatched context" << "<>" << org_vec.size() << "/" << org_vec_2.size() << "\n";
        std::abort();
    }

    std::filesystem::remove(FILE_PATH);
}

void run_test()
{
    const size_t TEST_SZ    = size_t{1} << 20;
    const size_t COUT_SZ    = size_t{1} << 10;

    std::cout << "__BEGIN_DTA_LOADER_FILE_SOURCE_TEST__\n";

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        run_one_test();
    
        if (i % COUT_SZ == 0u)
        {
            std::cout << i  << "/" << TEST_SZ << "\n";
        }
    }

    std::cout << "__END_DATA_LOADER_FILE_SOURCE_TEST__\n";
}

int main()
{
    run_test();    
}
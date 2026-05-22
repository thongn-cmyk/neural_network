#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <data_loader/source/gcs_source/gcs_source.h>
#include <data_loader/source/gcs_source/client_config_builder.h>
#include <data_loader/hex_encoder/hex_encoder.h>
#include <stdint.h>
#include <stdlib.h>
#include <data_loader/source/file_source/file_source.h>

#include <google/cloud/storage/client.h>

#include <iostream>
#include <random>
#include <algorithm>
#include <functional>
#include <utility>
#include <chrono>
#include <filesystem>
#include <cstdlib>

namespace gc    = ::google::cloud;
namespace gcs   = ::google::cloud::storage;

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

// auto randomize_file_loader_config(const std::string& file_path) -> data_loader::file_source::FileLoaderConfig
// {
//     return 
//     {
//         .delim_config               = randomize_external_delim_config(),
//         .local_file_path            = file_path,
//         .read_ahead_buffer_sz_hint  = (randomize_int(2) == 0u) ? std::optional<uint64_t>(std::nullopt)
//                                                                : std::optional<uint64_t>(randomize_int(size_t{1} << 4)),
//         .unit_byte_sz_hint          = (randomize_int(2) == 0u) ? std::optional<uint64_t>(std::nullopt)
//                                                                : std::optional<uint64_t>(randomize_int(size_t{1} << 4))
//     };
// }

// auto randomize_external_file_loader_config(const std::string& file_path) -> data_loader::file_source::ExternalFileLoaderConfig
// {
//     return data_loader::file_source::to_external_file_loader_config(randomize_file_loader_config(file_path));
// }

auto get_bucket_name() -> std::string
{
    return std::string(std::getenv("GCS_BUCKET_NAME"));
}

auto get_endpoint() -> std::string
{
    return std::string(std::getenv("GCS_ENDPOINT"));
}

auto get_access_token() -> std::string
{
    return std::string(std::getenv("GCS_ACCESS_TOKEN"));
}

auto get_gcs_client_credential() -> data_loader::gcs_source::GenericCredential
{
    //environmental variables

    const std::string ACCESS_TOKEN  = get_access_token();

    return
    {
        .credential = data_loader::gcs_source::AccessTokenCredential
        {
            .access_token   = ACCESS_TOKEN,
            .token_lifetime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::hours(1))
        }
    };
}

auto get_secured_gcs_client_config() -> data_loader::gcs_source::SecuredGCSClientConfig
{
    data_loader::gcs_source::SecuredGCSClientConfig rs{};

    rs.endpoint_config.endpoint = get_endpoint();
    rs.credential               = get_gcs_client_credential();

    return rs;
}

auto get_external_secured_gcs_client_config() -> data_loader::gcs_source::ExternalSecuredGCSClientConfig
{
    return data_loader::gcs_source::to_external_secured_gcs_client_config(get_secured_gcs_client_config());
}

auto randomize_gcs_loader_config(const std::string& file_path) -> data_loader::gcs_source::GCSLoaderConfig
{
    return
    {
        .delim_config               = randomize_external_delim_config(),
        .gcs_client_config          = get_external_secured_gcs_client_config(),
        .bucket_name                = get_bucket_name(),
        .object_key                 = file_path,
        .read_ahead_buffer_sz_hint  = (randomize_int(2) == 0u) ? std::optional<uint64_t>(std::nullopt)
                                                               : std::optional<uint64_t>(randomize_int(size_t{1} << 4)),
        .unit_byte_sz_hint          = (randomize_int(2) == 0u) ? std::optional<uint64_t>(std::nullopt)
                                                               : std::optional<uint64_t>(randomize_int(size_t{1} << 4))
    };
}

auto randomize_external_gcs_loader_config(const std::string& file_path) -> data_loader::gcs_source::ExternalGCSLoaderConfig
{
    return data_loader::gcs_source::to_external_gcs_loader_config(randomize_gcs_loader_config(file_path));
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

auto get_gcs_client_config() -> gc::Options
{
    gc::Options options{};
    options.template set<google::cloud::UnifiedCredentialsOption>(gc::MakeAccessTokenCredentials(get_access_token(), std::chrono::system_clock::now() + std::chrono::hours(1)));

    return options;
}

auto get_gcs_client() -> std::unique_ptr<gcs::Client>
{
    return std::make_unique<gcs::Client>(get_gcs_client_config());
}

void create_gcs_file(const std::string& remote_file_name,
                      const std::string& local_file_path)
{
    auto client = get_gcs_client();

    client->UploadFile(local_file_path, get_bucket_name(), remote_file_name);
}

void delete_gcs_file(const std::string& file_name)
{
    auto client = get_gcs_client();

    client->DeleteObject(get_bucket_name(), file_name);
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

    std::cout << "actual sz > " << file_output.size() << "\n";

    {
        create_gcs_file(FILE_PATH, FILE_PATH);
    }

    data_loader::gcs_source::GCSLoader loader(randomize_external_gcs_loader_config(FILE_PATH));
    std::vector<std::string> rs{};

    while (true)
    {
        auto incremental_rs = loader.get(randomize_tx_hint_size());

        if (!incremental_rs.has_value())
        {
            break;
        }

        rs.insert(rs.end(), incremental_rs->begin(), incremental_rs->end());
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

    delete_gcs_file(FILE_PATH);
    std::filesystem::remove(FILE_PATH);
}

void run_test()
{
    const size_t TEST_SZ    = size_t{1} << 10;
    const size_t COUT_SZ    = size_t{1} << 4;

    std::cout << "__BEGIN_GCS_LOADER_TEST__\n";

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        run_one_test();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << "\n";
        }
    }

    std::cout << "__END_GCS_LOADER_TEST__\n";
}

int main()
{
    run_test();
}
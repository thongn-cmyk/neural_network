#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <data_loader/source/s3_source/s3_source.h>
#include <data_loader/hex_encoder/hex_encoder.h>
#include <stdint.h>
#include <stdlib.h>
#include <data_loader/source/file_source/file_source.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <iostream>
#include <random>
#include <algorithm>
#include <functional>
#include <utility>
#include <chrono>
#include <filesystem>
#include <cstdlib>

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

auto get_s3_client_config() -> data_loader::s3_source::S3ClientConfiguration_2
{
    //environmental variables

    const std::string region    = std::string(std::getenv("AWS_REGION"));

    return
    {
        .region = region
    };
}

auto get_s3_client_credential() -> data_loader::s3_source::GenericCredential
{
    //environmental variables

    const std::string ACCESS_KEY_ID = std::string(std::getenv("AWS_ACCESS_KEY_ID"));
    const std::string SECRET_KEY    = std::string(std::getenv("AWS_SECRET_KEY"));

    return
    {
        .credential = data_loader::s3_source::Credential_0
        {
            .access_key_id  = ACCESS_KEY_ID,
            .secret_key     = SECRET_KEY
        }
    };
}

auto get_secured_s3_client_config() -> data_loader::s3_source::SecuredS3ClientConfiguration
{
    return
    {
        .client_config  = get_s3_client_config(),
        .credential     = get_s3_client_credential()
    };
}

auto get_external_secured_s3_client_config() -> data_loader::s3_source::ExternalSecuredS3ClientConfiguration
{
    return data_loader::s3_source::to_external_secured_s3_client_configuration(get_secured_s3_client_config());
}

auto get_bucket_identifier() -> std::string
{
    return std::string(std::getenv("AWS_BUCKET_NAME"));
}

auto randomize_s3_loader_config(const std::string& file_path) -> data_loader::s3_source::S3LoaderConfig
{
    return
    {
        .delim_config               = randomize_external_delim_config(),
        .s3_client_config           = get_external_secured_s3_client_config(),
        .bucket_name                = get_bucket_identifier(),
        .object_key                 = file_path,
        .read_ahead_buffer_sz_hint  = (randomize_int(2) == 0u) ? std::optional<uint64_t>(std::nullopt)
                                                               : std::optional<uint64_t>(randomize_int(size_t{1} << 4)),
        .unit_byte_sz_hint          = (randomize_int(2) == 0u) ? std::optional<uint64_t>(std::nullopt)
                                                               : std::optional<uint64_t>(randomize_int(size_t{1} << 4))
    };
}

auto randomize_external_s3_loader_config(const std::string& file_path) -> data_loader::s3_source::ExternalS3LoaderConfig
{
    return data_loader::s3_source::to_external_s3_loader_config(randomize_s3_loader_config(file_path));
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

auto get_s3_client() -> std::unique_ptr<Aws::S3::S3Client>
{
    return data_loader::s3_source::S3ClientBuilder{}.set(get_secured_s3_client_config()).build();
}

void create_s3(const std::string& remote_file_name,
               const std::string& local_file_path)
{
    auto client = get_s3_client();

    Aws::S3::Model::PutObjectRequest request{};

    request.SetBucket(get_bucket_identifier());
    request.SetKey(remote_file_name);

    auto input_data = Aws::MakeShared<Aws::FStream>("SampleAllocationTag",
                                                    local_file_path.c_str(),
                                                    std::ios_base::in | std::ios_base::binary);

    if (!*input_data)
    {
        std::cout << "mayday, local file open went wrong\n";
        std::abort();
    }

    request.SetBody(input_data);
    auto outcome = client->PutObject(request);

    if (!outcome.IsSuccess())
    {
        std::cout << "mayday, unable to create remote s3 object\n";
        std::abort();
    }
}

void delete_s3(const std::string& file_name)
{
    auto client = get_s3_client();

    Aws::S3::Model::DeleteObjectRequest request{};

    request.SetBucket(get_bucket_identifier());
    request.SetKey(file_name);

    auto outcome = client->DeleteObject(request);

    if (!outcome.IsSuccess())
    {
        std::cout << "s3 client operation went wrong, cannot delete object\n";
        std::abort();
    }
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
        create_s3(FILE_PATH, FILE_PATH);
    }

    data_loader::s3_source::S3Loader loader(randomize_external_s3_loader_config(FILE_PATH));
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

    delete_s3(FILE_PATH);
    std::filesystem::remove(FILE_PATH);
}

void run_test()
{
    const size_t TEST_SZ    = size_t{1} << 10;
    const size_t COUT_SZ    = size_t{1} << 4;

    std::cout << "__BEGIN_S3_LOADER_TEST__\n";

    for (size_t i = 0u; i < TEST_SZ; ++i)
    {
        run_one_test();

        if (i % COUT_SZ == 0u)
        {
            std::cout << i << "/" << TEST_SZ << "\n";
        }
    }

    std::cout << "__END_S3_LOADER_TEST__\n";
}

int main()
{
    Aws::SDKOptions options{};
    
    // 2. Boot up the underlying C-Common Runtime and allocators
    Aws::InitAPI(options);

    run_test();
}
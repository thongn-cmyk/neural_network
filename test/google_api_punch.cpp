#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <google/cloud/storage/client.h>
#include <fstream>
#include <iostream>
#include <data_loader/source/gcs_source/gcs_source.h>

namespace gcs = ::google::cloud::storage;

//I'm telling you that this expat cannot do it in one command

int main(int argc, char* argv[])
{
    if (argc != 4) 
    {
        std::cerr << "Usage:\n"
                  << "  " << argv[0]
                  << " <bucket-name> <object-name> <output-file>\n";

        return 1;
    }

    const std::string bucket_name = argv[1];
    const std::string object_name = argv[2];
    const std::string output_file = argv[3];

    try
    {
        // Create authenticated GCS client
        auto client = gcs::Client();

        // Open local output file
        std::ofstream os(output_file, std::ios::binary);

        if (!os.is_open())
        {
            std::cerr << "Failed to open output file: "
                      << output_file << "\n";

            return 1;
        }

        // Read object from GCS
        auto stream = client.ReadObject(bucket_name, object_name);

        // Stream download directly to disk
        os << stream.rdbuf();

        // Check stream status
        if (!stream.status().ok())
        {
            std::cerr << "Download failed: "
                      << stream.status() << "\n";

            return 1;
        }

        std::cout << "Download completed successfully.\n";
        std::cout << "Bucket : " << bucket_name << "\n";
        std::cout << "Object : " << object_name << "\n";
        std::cout << "Saved  : " << output_file << "\n";

        return 0;
    } catch (std::exception const& ex)
    {
        std::cerr << "Exception: " << ex.what() << "\n";
        return 1;
    }
}
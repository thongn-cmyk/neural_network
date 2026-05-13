#ifndef __DATA_LOADER_SOURCE_AZURE_SOURCE_AZURE_SOURCE_H__
#define __DATA_LOADER_SOURCE_AZURE_SOURCE_AZURE_SOURCE_H__

#include <azure/storage/blobs.hpp>

#include <array>
#include <iostream>
#include <string>
#include <cstring>
#include <stdint.h>
#include <stdlib.h>

namespace data_loader::azure_source
{
    using namespace data_loader::source_exception;

    struct AzureLoaderConfig
    {
        data_loader::stream_reader::ExternalDelimitedStreamReaderConfig delim_config;
        data_loader::azure_source::SerializableAzureClientConfig service_client_config;
        std::string container_name;
        std::string blob_name;
        std::optional<uint64_t> read_ahead_buffer_sz_hint;
        std::optional<uint64_t> unit_byte_sz_hint;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(delim_config,
                      service_client_config,
                      container_name,
                      blob_name,
                      read_ahead_buffer_sz_hint,
                      unit_byte_sz_hint);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(delim_config,
                      service_client_config,
                      container_name,
                      blob_name,
                      read_ahead_buffer_sz_hint,
                      unit_byte_sz_hint);
        }
    };

    struct ExternalAzureLoaderConfig
    {
        std::string config_bytestream;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(config_bytestream);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(config_bytestream);
        }
    };

    auto to_external_azure_loader_config(const AzureLoaderConfig& config) -> ExternalAzureLoaderConfig
    {
        return ExternalAzureLoaderConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_azure_loader_config(const ExternalAzureLoaderConfig& config) -> AzureLoaderConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<AzureLoaderConfig>(config.config_bytestream);
    }

    class AzureLoader: public virtual data_loader::SourceLoaderInterface
    {
        private:

            using DownloadResponse  = Azure::Response<Azure::Storage::Blobs::Models::DownloadBlobResult>;
            using BlobClient        = Azure::Storage::Blobs::BlobClient;
            using BlobServiceClient = Azure::Storage::Blobs::BlobServiceClient;

            struct AzureObjectPointer
            {
                std::string container_name;
                std::string blob_name;
            };

            struct BufferPointer
            {
                size_t offset;
                size_t sz;
            };
        
            std::unique_ptr<data_loader::stream_reader::DelimitedStreamReaderInterface> delim_stream_reader;
            std::unique_ptr<BlobClient> blob_client;
            std::unique_ptr<unsigned char[]> preallocated_buf;
            size_t preallocated_buf_sz;

            size_t tx_unit_sz;
            bool was_completed;
            bool is_bad_state;
            AzureObjectPointer azure_object_pointer;
            data_loader::azure_source::SerializableAzureClientConfig service_client_config;
            std::optional<BufferPointer> buf_pointer;

            static inline constexpr size_t MAX_READ_SZ      = size_t{1} << 20;
            static inline constexpr size_t MIN_BUFFER_SZ    = size_t{1} << 10;
            static inline constexpr size_t MAX_BUFFER_SZ    = size_t{1} << 20;

        public:

            AzureLoader(const AzureLoaderConfig& config)
            {
                this->delim_stream_reader   = std::make_unique<data_loader::stream_reader::DelimitedStreamReader>(config.delim_config);
                this->blob_client           = nullptr;
                this->tx_unit_sz            = 1u;

                if (config.unit_byte_sz_hint.has_value())
                {
                    this->tx_unit_sz = std::max(this->tx_unit_sz, static_cast<size_t>(config.unit_byte_sz_hint.value()));
                }

                if (config.read_ahead_buffer_sz_hint.has_value())
                {
                    size_t buf_sz               = std::clamp(static_cast<size_t>(config.read_ahead_buffer_sz_hint.value()), MIN_BUFFER_SZ, MAX_BUFFER_SZ);
                    this->preallocated_buf      = std::make_unique<unsigned char[]>(buf_sz);
                    this->preallocated_buf_sz   = buf_sz;
                }

                this->was_completed         = false;
                this->is_bad_state          = false;
                this->azure_object_pointer  = AzureObjectPointer
                {
                    .container_name = config.container_name,
                    .blob_name      = config.blob_name
                };

                this->service_client_config = config.service_client_config;
                this->buf_pointer           = std::nullopt;
            }

            AzureLoader(const ExternalAzureLoaderConfig& config): AzureLoader(to_internal_azure_loader_config(config)){}

            auto get(size_t tx_hint_sz) -> std::optional<std::vector<std::string>>
            {
                if (this->is_bad_state)
                {
                    throw bad_state_error{};
                }

                if (this->was_completed)
                {
                    return std::nullopt;
                }

                size_t tx_byte_sz   = tx_hint_sz * this->tx_unit_sz;
                tx_byte_sz          = std::min(tx_byte_sz, MAX_READ_SZ);

                if (tx_byte_sz == 0u)
                {
                    return std::vector<std::string>();
                }

                std::string buf{};

                if (this->blob_client == nullptr)
                {
                    auto tmp_client     = this->get_blob_client();
                    this->buf_pointer   = BufferPointer
                    {
                        .offset = size_t{0u},
                        .sz     = this->get_download_content_length(*tmp_client)
                    };

                    this->blob_client   = std::move(tmp_client);
                }

                if (!this->buf_pointer.has_value())
                {
                    std::abort();
                }

                if (this->buf_pointer->offset == this->buf_pointer->sz)
                {
                    this->was_completed = true;
                    return std::nullopt;
                }

                try
                {
                    buf = this->download_one_chunk();
                }
                catch (...)
                {
                    this->blob_client = this->get_blob_client();
                    throw;
                }

                if (buf.size() == 0u)
                {
                    this->is_bad_state = true;
                    throw other_error("Azure Bucket read went wrong, wrong read byte size");
                }

                try
                {
                    return this->delim_stream_reader->put(buf);
                }
                catch (...)
                {
                    this->is_bad_state = true;
                    throw;
                }
            }

        private:

            void handle_azure_exception(std::exception_ptr ex_ptr)
            {
                try
                {
                    std::rethrow_exception(ex_ptr);
                }
                catch (const Azure::Core::RequestFailedException& e)
                {
                    using Azure::Core::Http::HttpStatusCode;

                    switch (e.StatusCode)
                    {
                        //--------------------------------------------------
                        // 400 Bad Request
                        //--------------------------------------------------
                        case HttpStatusCode::BadRequest:
                        {
                            throw source_invalid_argument("Bad Azure BlobStorage operation, Bad Request");
                        }

                        //--------------------------------------------------
                        // 401 Unauthorized
                        //--------------------------------------------------
                        case HttpStatusCode::Unauthorized:
                        {
                            throw authentication_error("Bad Azure BlobStorage operation, Unauthorized");
                        }

                        //--------------------------------------------------
                        // 403 Forbidden
                        //--------------------------------------------------
                        case HttpStatusCode::Forbidden:
                        {
                            throw source_invalid_argument("Bad Azure BlobStorage operation, Forbidden");
                        }

                        //--------------------------------------------------
                        // 404 Not Found
                        //--------------------------------------------------
                        case HttpStatusCode::NotFound:
                        {
                            throw bad_resource_pointer_error("Bad Azure BlobStorage operation, NotFound");
                        }

                        //--------------------------------------------------
                        // 409 Conflict
                        //--------------------------------------------------
                        case HttpStatusCode::Conflict:
                        {
                            throw source_invalid_argument("Bad Azure BlobStorage operation, Conflict");
                        }

                        //--------------------------------------------------
                        // 412 Precondition Failed
                        //--------------------------------------------------
                        case HttpStatusCode::PreconditionFailed:
                        {
                            throw source_invalid_argument("Bad Azure BlobStorage operation, PreconditionFailed");
                        }

                        //--------------------------------------------------
                        // 416 Requested Range Not Satisfiable
                        //--------------------------------------------------
                        case HttpStatusCode::RequestedRangeNotSatisfiable:
                        {
                            throw source_invalid_argument("Bad Azure BlobStorage operation, RequestedRangeNotSatisfiable");
                        }

                        //--------------------------------------------------
                        // 429 Too Many Requests
                        //--------------------------------------------------
                        case HttpStatusCode::TooManyRequests:
                        {
                            throw connection_error("Bad Azure BlobStorage operation, TooManyRequests");
                        }

                        //--------------------------------------------------
                        // 500 Internal Server Error
                        //--------------------------------------------------
                        case HttpStatusCode::InternalServerError:
                        {
                            throw connection_error("Bad Azure BlobStorage operation, InternalServerError");
                        }

                        //--------------------------------------------------
                        // 502 Bad Gateway
                        //--------------------------------------------------
                        case HttpStatusCode::BadGateway:
                        {
                            throw connection_error("Bad Azure BlobStorage operation, BadGateway");
                        }

                        //--------------------------------------------------
                        // 503 Service Unavailable
                        //--------------------------------------------------
                        case HttpStatusCode::ServiceUnavailable:
                        {
                            throw connection_error("Bad Azure BlobStorage operation, ServiceUnavailable");
                        }

                        //--------------------------------------------------
                        // 504 Gateway Timeout
                        //--------------------------------------------------
                        case HttpStatusCode::GatewayTimeout:
                        {
                            throw connection_error("Bad Azure BlobStorage operation, GatewayTimeout");
                        }

                        //--------------------------------------------------
                        // Default
                        //--------------------------------------------------
                        default:
                        {
                            throw other_error("Bad Azure BlobStorage operation");
                        }
                    }
                }
                catch (...)
                {
                    throw other_error("Bad Azure BlobStorage operation");
                }
            }

            auto get_service_client() -> std::unique_ptr<BlobServiceClient>
            {
                return data_loader::azure_source::get_service_client_from_serializable_config(this->service_client_config);
            }

            auto get_blob_client() -> std::unique_ptr<BlobClient>
            {
                try
                {
                    return std::make_unique<BlobClient>(this->get_service_client()
                                                            ->GetBlobContainerClient(this->azure_object_pointer.container_name)
                                                             .GetBlobClient(this->azure_object_pointer.blob_name));

                }
                catch (...)
                {
                    this->handle_azure_exception(std::current_exception());
                }
            }

            auto get_download_content_length(BlobClient& blob_client) -> size_t
            {
                try
                {
                    return blob_client.GetProperties().Value.BlobSize;
                }
                catch (...)
                {
                    this->handle_azure_exception(std::current_exception());
                }
            }

            auto get_download_options() -> Azure::Storage::Blobs::DownloadBlobToOptions
            {
                if (!this->buf_pointer.has_value())
                {
                    std::abort();
                }

                if (this->buf_pointer->offset > this->buf_pointer->sz)
                {
                    std::abort();
                }

                size_t max_read_sz  = this->buf_pointer->sz - this->buf_pointer->offset;
                size_t read_sz      = std::min(this->preallocated_buf_sz, max_read_sz);

                if (read_sz == 0u)
                {
                    std::abort();
                }

                return Azure::Storage::Blobs::DownloadBlobToOptions
                {
                    .Range
                    {
                        .Offset = static_cast<int64_t>(this->buf_pointer->offset),
                        .Length = static_cast<int64_t>(read_sz)
                    }
                };
            }

            auto get_expected_download_size() -> size_t
            {
                if (!this->buf_pointer.has_value())
                {
                    std::abort();
                }

                if (this->buf_pointer->offset > this->buf_pointer->sz)
                {
                    return 0u;
                }

                size_t max_read_sz  = this->buf_pointer->sz - this->buf_pointer->offset;
                size_t read_sz      = std::min(this->preallocated_buf_sz, max_read_sz);

                return read_sz;
            }

            void increment_read_pointer_by(size_t sz)
            {
                if (!this->buf_pointer.has_value())
                {
                    std::abort();
                }

                this->buf_pointer->offset += sz;
            }

            auto download_one_chunk() -> std::string
            {
                size_t read_bytes{};
                size_t expected_read_bytes{};
                std::string buf{};

                try
                {
                    buf.resize(this->get_expected_download_size());

                    auto rs             = this->blob_client->DownloadTo(buf.data(),
                                                                        this->get_expected_download_size(),
                                                                        this->get_download_options());

                    read_bytes          = rs.Value.Details.Range.Value().Length;
                    expected_read_bytes = this->get_expected_download_size();
                }
                catch (...)
                {
                    this->handle_azure_exception(std::current_exception());
                }

                if (read_bytes != expected_read_bytes)
                {
                    this->is_bad_state = true;
                    throw hard_file_read_error("Hard Azure file read error, range mismatched");
                }

                this->increment_read_pointer_by(read_bytes);

                return buf;
            }
    };
}

#endif
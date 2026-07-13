#ifndef __DATA_LOADER_SOURCE_AZURE_SOURCE_AZURE_SOURCE_H__
#define __DATA_LOADER_SOURCE_AZURE_SOURCE_AZURE_SOURCE_H__

#include <azure/storage/blobs.hpp>
#include <array>
#include <string>
#include <cstring>
#include <stdint.h>
#include <stdlib.h>
#include "model.h"
#include <memory>
#include <data_loader/source/source_loader_interface.h>
#include <data_loader/stream_reader/delimited_stream_reader_interface.h>
#include <data_loader/source/source_exception.h>
#include <data_loader/stream_reader/delimited_stream_reader.h>
#include <algorithm>
#include <functional>
#include <utility>
#include "client_builder.h"
#include "client_config_builder.h"
#include <bit>

namespace data_loader::source::azure_source
{
    using namespace data_loader::source::exception;

    class AzureLoader: public virtual data_loader::source::SourceLoaderInterface
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
            size_t ops_buf_sz;

            size_t tx_unit_sz;
            bool was_completed;
            bool is_bad_state;
            AzureObjectPointer azure_object_pointer;
            data_loader::azure_source::SecuredAzureClientConfig service_client_config;
            std::optional<BufferPointer> buf_pointer;

            static inline constexpr size_t MAX_READ_SZ      = size_t{1} << 30;

            static inline constexpr size_t MIN_BUFFER_SZ    = size_t{1} << 10;
            static inline constexpr size_t MAX_BUFFER_SZ    = size_t{1} << 30;

            static inline constexpr size_t MIN_TX_UNIT_SZ   = size_t{1} << 10;
            static inline constexpr size_t MAX_TX_UNIT_SZ   = size_t{1} << 30;

        public:

            AzureLoader(const AzureLoaderConfig& config)
            {
                this->delim_stream_reader   = std::make_unique<data_loader::stream_reader::DelimitedStreamReader>(config.delim_config);
                this->blob_client           = nullptr;
                this->tx_unit_sz            = MIN_TX_UNIT_SZ;

                if (config.unit_byte_sz_hint.has_value())
                {
                    this->tx_unit_sz = std::clamp(static_cast<size_t>(config.unit_byte_sz_hint.value()),
                                                  MIN_TX_UNIT_SZ,
                                                  MAX_TX_UNIT_SZ);
                }

                size_t buf_sz   = MIN_BUFFER_SZ;

                if (config.read_ahead_buffer_sz_hint.has_value())
                {
                    buf_sz      = std::clamp(static_cast<size_t>(config.read_ahead_buffer_sz_hint.value()),
                                             MIN_BUFFER_SZ,
                                             MAX_BUFFER_SZ);
                }

                this->ops_buf_sz            = buf_sz;

                this->was_completed         = false;
                this->is_bad_state          = false;
                this->azure_object_pointer  = AzureObjectPointer
                {
                    .container_name = config.container_name,
                    .blob_name      = config.blob_name
                };

                this->service_client_config = to_internal_secured_azure_client_config(config.service_client_config);
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
                    buf = this->download_one_chunk(tx_byte_sz);
                }
                catch (...)
                {
                    this->blob_client = this->get_blob_client();
                    throw;
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
                        case HttpStatusCode::RangeNotSatisfiable:
                        {
                            throw source_invalid_argument("Bad Azure BlobStorage operation, RangeNotSatisfiable");
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
                return AzureServiceClientBuilder{}.set(this->service_client_config).build();
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
                    return {};
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
                    return {};
                }
            }

            auto get_download_options(size_t suggested_sz) -> Azure::Storage::Blobs::DownloadBlobToOptions
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
                size_t read_sz      = std::min(std::max(this->ops_buf_sz, suggested_sz), max_read_sz);

                if (read_sz == 0u)
                {
                    std::abort();
                }

                return Azure::Storage::Blobs::DownloadBlobToOptions
                {
                    .Range = Azure::Core::Http::HttpRange
                    {
                        .Offset = static_cast<int64_t>(this->buf_pointer->offset),
                        .Length = static_cast<int64_t>(read_sz)
                    }
                };
            }

            auto get_expected_download_size(size_t suggested_sz) -> size_t
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
                size_t read_sz      = std::min(std::max(this->ops_buf_sz, suggested_sz), max_read_sz);

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

            auto download_one_chunk(size_t suggested_sz) -> std::string
            {
                size_t read_bytes{};
                size_t tentative_read_bytes{};

                using unsigned_char = unsigned char;
                std::unique_ptr<unsigned_char[]> buf;

                try
                {
                    size_t buf_sz           = this->get_expected_download_size(suggested_sz);
                    buf                     = std::make_unique<unsigned_char[]>(buf_sz);

                    auto rs                 = this->blob_client->DownloadTo(buf.get(),
                                                                            buf_sz,
                                                                            this->get_download_options(suggested_sz));

                    read_bytes              = rs.Value.ContentRange.Length.Value();
                    tentative_read_bytes    = buf_sz;
                }
                catch (...)
                {
                    this->handle_azure_exception(std::current_exception());
                }

                if (read_bytes != tentative_read_bytes)
                {
                    throw soft_file_read_error("Bad Azure operation, mismatched read range");
                }

                std::string rs{};
                rs.reserve(read_bytes);

                for (size_t i = 0u; i < read_bytes; ++i)
                {
                    rs.push_back(std::bit_cast<char>(buf[i]));
                }

                this->increment_read_pointer_by(read_bytes);

                return rs;
            }
    };
}

#endif
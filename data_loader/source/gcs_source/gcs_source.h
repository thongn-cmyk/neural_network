#ifndef __DATA_LOADER_SOURCE_GCS_SOURCE_GCS_SOURCE_H__
#define __DATA_LOADER_SOURCE_GCS_SOURCE_GCS_SOURCE_H__

#include <stdint.h>
#include <stdlib.h>
#include <google/cloud/storage/client.h>
#include <array>
#include <iostream>
#include <string>
#include <cstring>
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
#include <google/cloud/status.h>

namespace data_loader::source::gcs_source
{
    namespace gcs   = ::google::cloud::storage;
    namespace gc    = ::google::cloud;

    using namespace data_loader::source::source_exception;

    class GCSLoader: public virtual data_loader::source::SourceLoaderInterface
    {
        private:

            struct GCSObjectPointer
            {
                std::string bucket_name;
                std::string object_key;
            };

            struct BufferPointer
            {
                size_t offset;
                size_t sz;
            };

            std::unique_ptr<data_loader::stream_reader::DelimitedStreamReaderInterface> delim_stream_reader;
            std::unique_ptr<gcs::Client> gcs_client;
            size_t read_buf_sz;

            size_t tx_unit_sz;
            bool was_completed;
            bool is_bad_state;
            GCSObjectPointer gcs_object_pointer;
            data_loader::gcs_source::SecuredGCSClientConfig gcs_client_config;
            std::optional<BufferPointer> buf_pointer;

            static inline constexpr size_t MAX_READ_SZ      = size_t{1} << 24;

            static inline constexpr size_t MIN_BUFFER_SZ    = size_t{1} << 10;
            static inline constexpr size_t MAX_BUFFER_SZ    = size_t{1} << 24;

            static inline constexpr size_t MIN_TX_UNIT_SZ   = size_t{1} << 10;
            static inline constexpr size_t MAX_TX_UNIT_SZ   = size_t{1} << 24;

        public:

            GCSLoader(const GCSLoaderConfig& config)
            {
                this->delim_stream_reader   = std::make_unique<data_loader::stream_reader::DelimitedStreamReader>(config.delim_config);
                this->gcs_client            = nullptr;
                this->tx_unit_sz            = MIN_TX_UNIT_SZ;

                if (config.unit_byte_sz_hint.has_value())
                {
                    this->tx_unit_sz    = std::clamp(static_cast<size_t>(config.unit_byte_sz_hint.value()),
                                                     MIN_TX_UNIT_SZ,
                                                     MAX_TX_UNIT_SZ);
                }

                this->read_buf_sz           = MIN_BUFFER_SZ;

                if (config.read_ahead_buffer_sz_hint.has_value())
                {
                    this->read_buf_sz   = std::clamp(static_cast<size_t>(config.read_ahead_buffer_sz_hint.value()),
                                                     MIN_BUFFER_SZ,
                                                     MAX_BUFFER_SZ);
                }

                this->was_completed         = false;
                this->is_bad_state          = false;
                this->gcs_object_pointer    = GCSObjectPointer
                {
                    .bucket_name    = config.bucket_name,
                    .object_key     = config.object_key
                };

                this->gcs_client_config     = to_internal_secured_gcs_client_config(config.gcs_client_config);
                this->buf_pointer           = std::nullopt;
            }

            GCSLoader(const ExternalGCSLoaderConfig& config): GCSLoader(to_internal_gcs_loader_config(config)){}

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

                if (this->gcs_client == nullptr)
                {
                    auto tmp_client     = this->get_gcs_client();
                    this->buf_pointer   = 
                    {
                        .offset = size_t{0u},
                        .sz     = this->get_download_content_length(*tmp_client, this->gcs_object_pointer)
                    };

                    this->gcs_client    = std::move(tmp_client);
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
                    this->gcs_client    = this->get_gcs_client();
                    throw;
                }

                try
                {
                    return this->delim_stream_reader->put(buf);
                }
                catch (...)
                {
                    this->is_bad_state  = true;
                    throw;
                }
            }

        private:

            void handle_gcs_exception(const google::cloud::Status& s)
            {
                using namespace data_loader::source_exception;

                switch (s.code())
                {
                    case gc::StatusCode::kOk:
                    {
                        break;
                    }
                    case gc::StatusCode::kInvalidArgument:
                    {
                        throw source_invalid_argument("Bad GCS operation, InvalidArgument");
                    }
                    case gc::StatusCode::kUnauthenticated:
                    {
                        throw authentication_error("Bad GCS operation, Unauthenticated");
                    }
                    case gc::StatusCode::kPermissionDenied:
                    {
                        throw authentication_error("Bad GCS operation, PermissionDenied");
                    }
                    case gc::StatusCode::kNotFound:
                    {
                        throw bad_resource_pointer_error("Bad GCS operation, NotFound");
                    }
                    case gc::StatusCode::kFailedPrecondition:
                    case gc::StatusCode::kOutOfRange:
                    {
                        throw source_invalid_argument("Bad GCS operation, Range/Precondition");
                    }
                    case gc::StatusCode::kResourceExhausted:
                    case gc::StatusCode::kUnavailable:
                    case gc::StatusCode::kDeadlineExceeded:
                    case gc::StatusCode::kInternal:
                    case gc::StatusCode::kAborted:
                    {
                        throw connection_error("Bad GCS operation, transient or server error");
                    }
                    default:
                    {
                        throw other_error("Bad GCS operation");
                    }
                }
            }

            auto get_gcs_client() -> std::unique_ptr<gcs::Client>
            {
                return GCSClientBuilder{}.set(this->gcs_client_config).build();
            }

            auto get_download_content_length(gcs::Client& client,
                                             const GCSObjectPointer& obj_pointer) -> size_t
            {
                auto metadata = client.GetObjectMetadata(obj_pointer.bucket_name, obj_pointer.object_key);

                if (!metadata)
                {
                    handle_gcs_exception(metadata.status());
                }

                return metadata->size();
            }

            void increment_read_pointer_by(size_t sz)
            {
                if (!this->buf_pointer.has_value())
                {
                    std::abort();
                }

                this->buf_pointer->offset += sz;
            }

            auto download_one_chunk(size_t requested_sz) -> std::string
            {
                if (!this->buf_pointer.has_value())
                {
                    std::abort();
                }

                if (this->buf_pointer->offset >= this->buf_pointer->sz)
                {
                    std::abort();
                }

                size_t max_read_sz          = this->buf_pointer->sz - this->buf_pointer->offset;
                size_t tentative_read_sz    = std::min(std::max(this->read_buf_sz, requested_sz), max_read_sz);
                auto stream                 = this->gcs_client->ReadObject(this->gcs_object_pointer.bucket_name,
                                                                           this->gcs_object_pointer.object_key,
                                                                           gcs::ReadRange(this->buf_pointer->offset, tentative_read_sz));

                if (!stream)
                {
                    handle_gcs_exception(stream.status());
                }

                std::string rs((std::istreambuf_iterator<char>(stream)),
                                std::istreambuf_iterator<char>());

                if (rs.size() != tentative_read_sz)
                {
                    throw soft_file_read_error("Bad GCS operation, mismatched read range");
                }

                this->increment_read_pointer_by(rs.size());

                return rs;
            }
    };
}

#endif
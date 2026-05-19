#ifndef __DATA_LOADER_S3_SOURCE_H__
#define __DATA_LOADER_S3_SOURCE_H__

#include <stdint.h>
#include <stdlib.h>
#include <optional>
#include <string>
#include <memory>
#include <fstream>
#include <data_loader/source/source_loader_interface.h>
#include <data_loader/stream_reader/delimited_stream_reader_interface.h>
#include <data_loader/source/source_exception.h>
#include <data_loader/stream_reader/delimited_stream_reader.h>
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>
#include "model.h"
#include <algorithm>
#include <functional>
#include <utility>
#include "client_builder.h"
#include "client_config_builder.h"

namespace data_loader::s3_source
{
    using namespace data_loader::source_exception;

    struct S3LoaderConfig
    {
        data_loader::stream_reader::ExternalDelimitedStreamReaderConfig delim_config;
        data_loader::s3_source::ExternalS3ClientConfiguration s3_client_config;
        std::string bucket_name;
        std::string object_key;
        std::optional<uint64_t> read_ahead_buffer_sz_hint;
        std::optional<uint64_t> unit_byte_sz_hint;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(delim_config,
                      s3_client_config,
                      bucket_name,
                      object_key,
                      read_ahead_buffer_sz_hint,
                      unit_byte_sz_hint);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(delim_config,
                      s3_client_config,
                      bucket_name,
                      object_key,
                      read_ahead_buffer_sz_hint,
                      unit_byte_sz_hint);
        }
    };

    struct ExternalS3LoaderConfig
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

    auto to_external_s3_loader_config(const S3LoaderConfig& config) -> ExternalS3LoaderConfig
    {
        return ExternalS3LoaderConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_s3_loader_config(const ExternalS3LoaderConfig& config) -> S3LoaderConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<S3LoaderConfig>(config.config_bytestream);
    }

    //the merits, morals behind data loader is that we have a region <a, b>
    //we want to read each segment once, for segments = <a, b>

    //and we'd have to retry indefinitely to read each segment once, or we'd have to prune by throwing different errors or max retry reached by retryer
    //that's it

    //the assumption that we have is the data being immutable, and the implementation that we have is safely undefined otherwise

    class S3Loader: public virtual data_loader::SourceLoaderInterface
    {
        private:

            struct S3ObjectPointer
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
            std::unique_ptr<Aws::S3::Model::GetObjectOutcome> object_outcome;
            std::unique_ptr<unsigned char[]> buf;
            std::unique_ptr<Aws::Utils::Stream::PreallocatedStreamBuf> buf_reference;
            size_t tx_unit_sz;
            bool was_completed;
            bool is_bad_state;
            size_t soft_read_error_sz;
            S3ObjectPointer s3_object_pointer;
            data_loader::s3_source::S3ClientConfiguration client_config;
            std::optional<BufferPointer> buf_pointer;

            static inline constexpr size_t SOFT_READ_ERROR_THRESHOLD    = size_t{1} << 3;
            static inline constexpr size_t MAX_READ_SZ                  = size_t{1} << 20;
            static inline constexpr size_t MIN_BUFFER_SZ                = size_t{1} << 10;
            static inline constexpr size_t MAX_BUFFER_SZ                = size_t{1} << 20;

        public:

            S3Loader(const S3LoaderConfig& config)
            {
                this->delim_stream_reader   = std::make_unique<data_loader::stream_reader::DelimitedStreamReader>(config.delim_config);
                this->object_outcome        = nullptr;
                this->tx_unit_sz            = 1u;

                if (config.unit_byte_sz_hint.has_value())
                {
                    this->tx_unit_sz = std::max(this->tx_unit_sz, static_cast<size_t>(config.unit_byte_sz_hint.value()));
                }

                if (config.read_ahead_buffer_sz_hint.has_value())
                {
                    size_t buf_sz       = std::clamp(static_cast<size_t>(config.read_ahead_buffer_sz_hint.value()), MIN_BUFFER_SZ, MAX_BUFFER_SZ);
                    this->buf           = std::make_unique<unsigned char[]>(buf_sz);
                    this->buf_reference = std::make_unique<Aws::Utils::Stream::PreallocatedStreamBuf>(this->buf.get(), buf_sz);
                }

                this->was_completed         = false;
                this->is_bad_state          = false;
                this->soft_read_error_sz    = 0u;
                this->s3_object_pointer     = S3ObjectPointer
                {
                    .bucket_name    = config.bucket_name,
                    .object_key     = config.object_key
                };

                this->client_config         = data_loader::s3_source::to_internal_s3_client_configuration(config.s3_client_config);
                this->buf_pointer           = std::nullopt;
            }

            S3Loader(const ExternalS3LoaderConfig& config): S3Loader(to_internal_s3_loader_config(config)){}

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

                std::string buf(tx_byte_sz, ' ');
                intmax_t read_byte_sz;

                if (this->object_outcome == nullptr)
                {
                    this->initialize_object_outcome();
                    this->buf_pointer = BufferPointer
                    {
                        .offset = size_t{0u},
                        .sz     = this->get_outcome_stream_content_length(*this->object_outcome)
                    };
                }

                if (!this->buf_pointer.has_value())
                {
                    this->is_bad_state = true;
                    throw other_error("S3 Bucket read went wrong, failed to initialize read pointer");
                }

                try
                {
                    this->seek_outcome_stream(*this->object_outcome, this->buf_pointer->offset);
                    this->read_outcome_stream(*this->object_outcome, buf.data(), buf.size());

                    read_byte_sz = this->gcount_outcome_stream(*this->object_outcome);
                }
                catch (...)
                {
                    this->revive_object_outcome_on_error(std::current_exception());
                    throw;                    
                }

                if (read_byte_sz < 0)
                {
                    throw std::runtime_error("file read went wrong, negative read bytes");
                }

                buf.resize(read_byte_sz);
                this->buf_pointer->offset += read_byte_sz;

                if (buf.size() == 0u)
                {
                    if (this->buf_pointer->offset != this->buf_pointer->sz)
                    {
                        this->revive_object_outcome_and_throw_size_inconsistency();
                        this->punch_one_soft_read_error();
                    }

                    this->was_completed = true;
                    return std::nullopt;
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

            void seek_outcome_stream(Aws::S3::Model::GetObjectOutcome& obj, size_t pos)
            {
                if (!obj.IsSuccess())
                {
                    throw other_error("S3 Bucket read went wrong, bad objet outcome");
                }

                try
                {
                    obj.GetResult().GetBody().seekg(pos);

                    if (obj.GetResult().GetBody().fail())
                    {
                        throw std::exception();                        
                    }
                }
                catch (...)
                {
                    throw other_error("S3 Bucket read went wrong, bad file seek");
                }
            }

            void read_outcome_stream(Aws::S3::Model::GetObjectOutcome& obj, char * buf, size_t sz)
            {
                if (!obj.IsSuccess())
                {
                    throw other_error("S3 Bucket read went wrong, bad objet outcome");
                }

                try
                {
                    obj.GetResult().GetBody().read(buf, sz);
                }
                catch (...)
                {
                    throw other_error("S3 Bucket read went wrong, bad stream read");
                }
            }

            auto gcount_outcome_stream(Aws::S3::Model::GetObjectOutcome& obj) -> size_t
            {
                if (!obj.IsSuccess())
                {
                    throw other_error("S3 Bucket read went wrong, bad objet outcome");
                }

                try
                {
                    intmax_t result = obj.GetResult().GetBody().gcount();

                    if (result < 0)
                    {
                        throw std::exception();
                    }

                    return result;
                }
                catch (...)
                {
                    throw other_error("S3 Bucket read went wrong, bad file read count");
                }
            }

            auto get_outcome_stream_content_length(Aws::S3::Model::GetObjectOutcome& obj) -> size_t
            {
                if (!obj.IsSuccess())
                {
                    throw other_error("S3 Bucket read went wrong, bad objet outcome");
                }

                try
                {
                    intmax_t result = obj.GetResult().GetContentLength();

                    if (result < 0)
                    {
                        throw std::exception();
                    }

                    return result;
                }
                catch (...)
                {
                    throw other_error("S3 Bucket read went wrong, bad file content length read");
                }
            }

            void revive_object_outcome_and_throw_size_inconsistency()
            {
                if (!this->buf_pointer.has_value())
                {
                    std::abort();
                }

                this->initialize_object_outcome();
                size_t expected_sz = this->get_outcome_stream_content_length(*this->object_outcome);

                if (expected_sz != this->buf_pointer->sz)
                {
                    this->is_bad_state = true;
                    throw hard_file_read_error("file read went wrong, size was mutated");
                }
            }

            void punch_one_soft_read_error()
            {
                if (this->soft_read_error_sz == SOFT_READ_ERROR_THRESHOLD)
                {
                    this->is_bad_state = true;
                    throw hard_file_read_error("hard S3 read error, max retry count reached");
                }

                this->soft_read_error_sz += 1;

                throw soft_file_read_error("soft S3 read error");
            }

            auto get_s3_client() -> std::unique_ptr<Aws::S3::S3Client>
            {
                return S3ClientBuilder{}.set(this->client_config).get();
            }

            void initialize_object_outcome()
            {
                Aws::S3::Model::GetObjectRequest objectRequest{};

                objectRequest.SetBucket(this->s3_object_pointer.bucket_name);
                objectRequest.SetKey(this->s3_object_pointer.object_key);

                if (this->buf != nullptr)
                {
                    objectRequest.SetResponseStreamFactory([&]
                    {
                        return Aws::New<Aws::IOStream>("PreallocatedStream", this->buf_reference.get());
                    });
                }

                auto tmp = std::make_unique<Aws::S3::Model::GetObjectOutcome>(this->get_s3_client()->GetObject(objectRequest));

                if (!tmp->IsSuccess())
                {
                    const auto& err = tmp->GetError();

                    if (err.GetErrorType() == Aws::S3::S3Errors::NO_SUCH_KEY)
                    {
                        throw authentication_error("No such key for AWS S3 bucket");
                    }
                    else if (err.GetErrorType() == Aws::S3::S3Errors::NO_SUCH_BUCKET)
                    {
                        throw bad_resource_pointer_error("No such AWS S3 bucket");
                    }
                    else if (err.GetErrorType() == Aws::S3::S3Errors::ACCESS_DENIED)
                    {
                        throw authentication_error("Access denied for AWS S3 bucket");
                    }
                    else if (err.GetErrorType() == Aws::S3::S3Errors::NETWORK_CONNECTION)
                    {
                        throw connection_error("Connection error for AWS S3 bucket");
                    }
                    else
                    {
                        throw other_error("Something went wrong with AWS S3 bucket reference object instantiation");
                    }
                }

                this->object_outcome        = std::move(tmp);
            }

            auto is_revivable_error(std::exception_ptr exception) -> bool
            {
                return exception != nullptr;
            }

            void revive_object_outcome_on_error(std::exception_ptr exception)
            {
                if (this->is_revivable_error(exception))
                {
                    this->initialize_object_outcome();
                }
            }
    };
}

#endif
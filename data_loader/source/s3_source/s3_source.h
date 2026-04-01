#ifndef __DATA_LOADER_S3_SOURCE_H__
#define __DATA_LOADER_S3_SOURCE_H__

#include <data_loader/source/source_interface.h>
#include <data_loader/stream_reader/delimited_stream_reader_interface.h>
#include <stdint.h>
#include <stdlib.h>
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>

namespace data_loader::s3_source
{
    struct Configuration
    {
        data_loader::stream_reader::Configuration delim_config;
        data_loader::config::SerializableS3ClientConfiguration s3_client_config;
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

    class S3Loader: public virtual data_loader::SourceLoaderInterface
    {
        private:

            struct S3ObjectConnectionString
            {
                std::string bucket_name;
                std::string object_key;
            };

            struct BufferPointer
            {
                size_t offset;
            };

            std::unique_ptr<data_loader::stream_reader::DelimitedStreamReaderInterface> delim_stream_reader;
            std::unique_ptr<Aws::S3::Model::GetObjecOutcome> object_outcome;
            std::unique_ptr<char[]> buf;
            Aws::Utils::Stream::PreallocatedStreamBuf buf_reference;
            size_t tx_unit_sz;
            bool was_completed;
            bool is_bad_state;
            S3ObjectConnectionString s3_connection_string;
            Aws::Client::ClientConfiguration client_config;
            BufferPointer buf_pointer;

        public:

            static inline constexpr size_t MAX_READ_SZ      = size_t{1} << 20;
            static inline constexpr size_t MIN_BUFFER_SZ    = size_t{1} << 10;
            static inline constexpr size_t MAX_BUFFER_SZ    = size_t{1} << 20;

            S3Loader(Configuration config)
            {
                this->delim_streamer    = std::make_unique<data_loader::stream_reader::DelimitedStreamReader>(config.delim_config);
                this->object_outcome    = nullptr;
                this->tx_unit_sz        = 1u;

                if (config.unit_byte_sz_hint.has_value())
                {
                    this->tx_unit_sz = std::max(this->tx_unit_sz, config.unit_byte_sz_hint.value());
                }

                if (config.read_ahead_buffer_sz_hint.has_value())
                {
                    size_t buf_sz       = std::clamp(config.read_ahead_buffer_sz_hint.value(), MIN_BUFFER_SZ, MAX_BUFFER_SZ);
                    this->buf           = std::make_unique<char[]>(buf_sz);
                    this->buf_reference = Aws::Utils::Stream::PreallocatedStreamBuf(this->buf.get(), buf_sz);
                }

                this->was_completed         = false;
                this->is_bad_state          = false;
                this->s3_connection_string  = S3ObjectConnectionString
                {
                    .bucket_name    = config.bucket_name,
                    .object_key     = config.object_key
                };

                this->client_config         = data_loader::config::to_legacy_s3_client_config(config.s3_client_config);
                this->buf_pointer           = BufferPointer
                {
                    .offset = 0u
                };
            }

            ~S3Loader() noexcept
            {
                this->object_outcome = nullptr;
                this->buf = nullptr;
            }

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

                strd::string buf(tx_byte_sz, ' ');
                intmax_t read_bytes;

                if (this->object_outcome == nullptr)
                {
                    this->initialize_object_outcome();
                }

                try
                {
                    auto& oc_stream = this->object_outcome->GetResult().GetBody();

                    oc_stream.seekg(this->buf_pointer.offset);
                    oc_stream.read(buf.data(), buf.size());
    
                    read_bytes = oc_stream.gcount();
                }
                catch (...)
                {
                    this->revive_object_outcome_on_error(std::current_exception());
                    throw;                    
                }

                if (read_bytes < 0)
                {
                    throw std::runtime_error("file read went wrong, negative read bytes");
                }

                buf.resize(read_bytes);
                this->buf_pointer.offset += read_bytes;

                if (buf.size() == 0u)
                {
                    this->was_completed = true;
                    return std::nullopt;
                }

                try
                {
                    return this->delim_streamer->put(buf);
                }
                catch (...)
                {
                    this->is_bad_state = true;
                    throw;
                }
            }
        
        private:

            auto get_s3_client() -> Aws::S3::S3Client
            {
                return Aws::S3::S3Client(this->client_config);
            }

            void initialize_object_outcome()
            {
                Aws::S3::Model::GetObjectRequest objectRequest{};

                objectRequest.SetBucket(this->s3_connection_string.bucket_name);
                objectRequest.SetKey(this->s3_connection_string.object_key);

                if (this->buf.has_value())
                {
                    objectRequest.SetResponseStreamFactory([&]
                    {
                        return Aws::New<Aws::IOStream>("PreallocatedStream", &this->buf_reference);
                    });
                }

                auto tmp = std::make_unique<Aws::S3::Model::GetObjectOutcome>(this->get_s3_client().GetOBject(objectRequest));

                if (!tmp->IsSuccess())
                {
                    throw std::runtime_error("s3 object get went wrong");
                }

                this->object_outcome = std::move(tmp);
            }

            auto is_revivable_error(std::exception_ptr exception) -> bool
            {
                return exception !+ nullptr;
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
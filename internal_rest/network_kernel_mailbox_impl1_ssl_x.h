#ifndef __NETWORK_KERNEL_MAILBOX_IMPL1_SSL_X_H__
#define __NETWORK_KERNEL_MAILBOX_IMPL1_SSL_X_H__

#include <stdint.h>
#include <stdlib.h>
#include <serializer/huffman_encoder.h>
#include <chrono>

namespace dg_sock::network_kernel_mailbox_impl1_ssl_x
{
    struct SSLMessage
    {
        dg_sock::string secret; //I guess how we get the secret is a very hard problem to solve
        dg_sock::string content;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(secret, content);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(secret, content);
        }
    };

    struct StreamMessage
    {
        dg_sock::string secret;
        dg_sock::string pad;
        dg_sock::string content;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(secret, pad, content);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(secret, pad, content);
        }
    };

    struct Mt19937Message
    {
        dg_sock::string content;
        uint64_t salt_sz;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(content, salt_sz);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(content, salt_sz);
        }
    };

    class SecretGeneratorInterface
    {
        public:

            virtual ~SecretGeneratorInterface() noexcept = default;

            virtual auto get_secret(size_t secret_sz) -> dg_sock::string = 0;
    };

    class PadMessageGeneratorInterface
    {
        public:

            virtual ~PadMessageGeneratorInterface() noexcept = default;

            virtual auto get_pad_message() -> dg_sock::string = 0;
    };

    class SecuredMessageEncoderInterface
    {
        public:

            virtual ~SecuredMessageEncoderInterface() noexcept = default;

            virtual auto encode(const dg_sock::string& msg) -> SSLMessage = 0;
            virtual auto decode(const SSLMessage& msg) -> dg_sock::string = 0;
    };

    class StreamMessageEncoderInterface
    {
        public:

            virtual ~StreamMessageEncoderInterface() noexcept = default;

            virtual auto encode(const dg_sock::string& msg) -> dg_sock::string = 0;
            virtual auto decode(const dg_sock::string& msg) -> dg_sock::string = 0;
    };

    template <class Clock>
    class manual_ticking_clock
    {
        private:

            std::optional<std::chrono::time_point<Clock>> clock;

            size_t update_on_tick_sz;
            size_t tick_counter;

        public:

            manual_ticking_clock(size_t update_on_tick_sz): clock(std::nullopt),
                                                            update_on_tick_sz(std::max(size_t{1}, update_on_tick_sz)),
                                                            tick_counter(0u){}

            auto get() -> std::chrono::time_point<Clock>
            {
                if (!this->clock.has_value())
                {
                    this->clock = Clock::now();
                }

                if (this->tick_counter = this->update_on_tick_sz)
                {
                    this->clock = Clock::now();
                    this->tick_counter = 0u;
                }

                this->tick_counter += 1;

                return this->clock.value();
            }
    };

    class ClockBasedSecretGenerator: public virtual SecretGeneratorInterface
    {
        public:

            auto get_secret(size_t secret_sz) -> dg_sock::string
            {
                const size_t CLOCK_TICK_UPDATE_SZ = size_t{1} << 4;

                dg_sock::string rs  = {};
                size_t rev_sz       = secret_sz / sizeof(uint64_t);
                size_t rem_sz       = secret_sz - (rev_sz * sizeof(uint64_t));

                manual_ticking_clock<std::chrono::steady_clock> clock(CLOCK_TICK_UPDATE_SZ);

                for (size_t i = 0u; i < rev_sz; ++i)
                {
                    uint64_t random_0   = dg_sock::network_randomizer::randomize_int<uint64_t>();
                    uint64_t random_1   = clock.get();
                    uint64_t random_2   = std::bit_cast<uint64_t>(&random_1);

                    std::array<char, sizeof(uint64_t)> byte_arr = std::bit_cast<std::array<char, sizeof(uint64_t)>>(dg_sock::network_hash::hash_reflectible(std::make_tuple(random_0, random_1, random_2)));

                    rs.insert(rs.end(), byte_arr.begin(), byte_arr.end());
                }

                {
                    uint64_t random_0   = dg_sock::network_randomizer::randomize_int<uint64_t>();
                    uint64_t random_1   = clock.get();
                    uint64_t random_2   = std::bit_cast<uint64_t>(&random_1);
                    std::array<char, sizeof(uint64_t)> byte_arr = std::bit_cast<std::array<char, sizeof(uint64_t)>>(dg_sock::network_hash::hash_reflectible(std::make_tuple(random_0, random_1, random_2)));

                    rs.insert(rs.end(), byte_arr.begin(), std::next(byte_arr.begin(), rem_sz));
                }

                return rs;
            }
    };

    class PadMessageGenerator: public virtual PadMessageGeneratorInterface
    {
        private:

            ClockBasedSecretGenerator base;

            static inline constexpr size_t PAD_SZ = size_t{1} << 10;

        public:

            auto get_pad_message() -> dg_sock::string
            {
                return this->base.get_secret(PAD_SZ);
            }
    };

    class SymmetricSecuredMessageEncoder: public virtual SecuredMessageEncoderInterface
    {
        private:

            std::unique_ptr<SecretGeneratorInterface> secret_gen;

            const size_t DICTIONARY_RENEW_SZ = size_t{1} << 4;

        public:

            SymmetricSecuredMessageEncoder(std::unique_ptr<SecretGeneratorInterface> secret_gen) noexcept: secret_gen(std::move(secret_gen)){}

            auto encode(const dg_sock::string& msg) -> SSLMessage
            {
                dg_sock::string secret              = this->secret_gen->get_secret(msg.size());
                dg_sock::string high_entropy_msg    = dg::network_huffman_encoder::encode(msg);
                dg_sock::string content             = dg_sock::ud_sym_encoder::Mt19937Encoder(secret, msg.size(), DICTIONARY_RENEW_SZ).encode(high_entropy_msg);
                dg_sock::string new_content         = dg_sock::network_compact_serializer::dgstd_serialize<dg_sock::string>(Mt19937Message
                {
                    .content    = std::move(content),
                    .salt_sz    = static_cast<uint64_t>(msg.size())
                });

                return SSLMessage
                {
                    .secret     = std::move(secret),
                    .content    = std::move(new_content)
                };
            }

            auto decode(const SSLMessage& msg) -> dg_sock::string
            {
                Mt19937Message mt_msg               = dg_sock::network_compact_serializer::dgstd_deserialize<Mt19937Message>(msg.content);
                dg_sock::string high_entropy_msg    = dg_sock::ud_sym_encoder::Mt19937Encoder(msg.secret, mt_msg.salt_sz, DICTIONARY_RENEW_SZ).decode(mt_msg.content);
                dg_sock::string org_msg             = dg::network_huffman_encoder::decode(high_entropy_msg);

                return org_msg;
            }
    };

    class StreamMessageEncoder: public virtual StreamMessageEncoderInterface
    {
        private:

            std::unique_ptr<SecuredMessageEncoderInterface> secured_msg_gen;
            std::unique_ptr<PadMessageGeneratorInterface> pad_msg_gen;

        public:

            StreamMessageEncoder(std::unique_ptr<SecuredMessageEncoderInterface> secured_msg_gen,
                                 std::unique_ptr<PadMessageGeneratorInterface> pad_msg_gen) noexcept: secured_msg_gen(std::move(secured_msg_gen)),
                                                                                                      pad_msg_gen(std::move(pad_msg_gen)){}

            auto encode(const dg_sock::string& msg) -> dg_sock::string
            {
                SSLMessage ssl_msg          = this->secured_msg_gen->encode(msg);
                dg_sock::string pad         = this->pad_msg_gen->get_pad_message();
                StreamMessage stream_msg    = StreamMessage
                {
                    .secret     = std::move(ssl_msg.secret),
                    .pad        = std::move(pad),
                    .content    = std::move(ssl_msg.content)
                };

                return dg_sock::network_compact_serializer::dgstd_serialize<dg_sock::string>(stream_msg);
            }

            auto decode(const dg_sock::string& msg) -> dg_sock::string
            {
                StreamMessage stream_msg    = dg_sock::network_compact_serializer::dgstd_deserialize<StreamMessage>(msg);
                SSLMessage ssl_msg          =
                {
                    .secret     = std::move(stream_msg.secret),
                    .content    = std::move(stream_msg.content)
                };

                return this->secured_msg_gen->decode(ssl_msg);
            }
    };

    class SecuredMailbox: public virtual dg_sock::network_kernel_mailbox_impl1_flash_stream_x::DynamicMailboxInterface
    {
        private:

            std::unique_ptr<dg_sock::network_kernel_mailbox_impl1_flash_stream_x::DynamicMailboxInterface> base;
            std::unique_ptr<StreamMessageEncoderInterface> stream_msg_encoder;

        public:

            SecuredMailbox(std::unique_ptr<dg_sock::network_kernel_mailbox_impl1_flash_stream_x::DynamicMailboxInterface> base,
                           std::unique_ptr<StreamMessageEncoderInterface> stream_msg_encoder): base(std::move(base)),
                                                                                               stream_msg_encoder(std::move(stream_msg_encoder)){}

            void send(MailBoxArgument * data_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                const size_t DEFAULT_FEED_SZ    = size_t{1} << 6;

                auto feed_resolutor             = InternalSendFeedResolutor{};
                feed_resolutor.base             = this->base.get();
                feed_resolutor.encoder          = this->stream_msg_encoder.get();

                size_t trimmed_feed_sz          = std::min(std::min(DEFAULT_FEED_SZ, sz), this->base->max_consume_size());
                size_t allocation_cost          = dg_sock::network_producer_consumer::delvrsrv_allocation_cost(&feed_resolutor, trimmed_feed_sz);
                dg_sock::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(allocation_cost);
                auto feeder                     = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&feed_resolutor, trimmed_feed_sz, feeder_mem.get()));

                for (size_t i = 0u; i < sz; ++i)
                {
                    SendFeedArgument arg
                    {
                        .mailbox_arg_ptr    = std::next(data_arr, i),
                        .exception_ptr      = std::next(exception_arr, i)
                    };

                    dg_sock::network_producer_consumer::delvrsrv_deliver(feeder.get(), arg);
                }
            }

            void recv(dg_sock::string * output_container_arr, size_t& recv_sz, size_t recv_cap) noexcept
            {
                this->base->recv(output_container_arr, recv_sz, recv_cap);
                size_t first_idx = 0u;

                for (size_t i = 0u; i < recv_sz; ++i)
                {
                    try
                    {
                        output_container_arr[first_idx] = this->stream_msg_encoder->decode(output_container_arr[i]);
                        first_idx += 1;
                    }
                    catch (...)
                    {
                        dg_sock::network_log_stackdump::error_fast(dg_sock::network_exception::verbose(dg_sock::network_exception::wrap_std_exception(std::current_exception())));
                        continue;
                    }
                }
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->base->max_consume_size();
            }

            auto max_content_size() const noexcept -> size_t
            {
                return this->base->max_content_size();
            }

        private:

            struct SendFeedArgument
            {
                MailBoxArgument * mailbox_arg_ptr;
                exception_t * exception_ptr;
            };

            struct InternalSendFeedResolutor: dg_sock::network_producer_consumer::ConsumerInterface<SendFeedArgument>
            {
                dg_sock::network_kernel_mailbox_impl1_flash_stream_x::DynamicMailboxInterface * base;
                StreamMessageEncoderInterface * encoder;

                void push(std::move_iterator<SendFeedArgument *> data_arr, size_t data_arr_sz) noexcept
                {
                    dg_sock::network_stack_allocation::NoExceptAllocation<dg_sock::string[]> buf_arr(data_arr_sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<MailBoxArgument[]> new_argument_arr(data_arr_sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<exception_t[]> new_exception_arr(data_arr_sz);

                    SendFeedArgument * base_data_arr = data_arr.base();

                    for (size_t i = 0u; i < data_arr_sz; ++i)
                    {
                        dg_sock::string encoded_buf = this->encoder->encode(dg_sock::string(static_cast<const char *>(base_data_arr[i].mailbox_arg_ptr->content),
                                                                                            base_data_arr[i].mailbox_arg_ptr->content_sz));

                        buf_arr[i]                  = std::move(encoded_buf);
                        new_argument_arr[i]         = MailBoxArgument
                        {
                            .to         = base_data_arr[i].mailbox_arg_ptr->to,
                            .content    = buf_arr[i].data(),
                            .content_sz = buf_arr[i].size()
                        };
                    }

                    this->base->send(std::make_move_iterator(new_argument_arr.get()), data_arr_sz, new_exception_arr.get());

                    for (size_t i = 0u; i < data_arr_sz; ++i)
                    {
                        *base_data_arr[i].exception_ptr = new_exception_arr[i];
                    }
                }
            };
    };
}

#endif
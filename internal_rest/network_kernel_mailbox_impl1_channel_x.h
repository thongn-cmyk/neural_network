#ifndef __NETWORK_KERNEL_MAILBOX_IMPL1_CHANNEL_X_H__
#define __NETWORK_KERNEL_MAILBOX_IMPL1_CHANNEL_X_H__

#include <memory>
#include "network_kernel_mailbox_impl1_flash_stream_x.h"
#include <unordered_map>
#include "network_stack_allocation.h"
#include "network_std_container.h"
#include "network_producer_consumer.h"
#include "network_log.h"

namespace dg_sock::network_kernel_mailbox_impl1_channel_x
{
    using str_buffer        = dg_sock::string;
    using MailBoxArgument   = dg_sock::network_kernel_mailbox_impl1::model::MailBoxArgument;

    static inline constexpr uint32_t SERIALIZATION_SECRET = 1656166028UL;

    struct ChannelMessage
    {
        uint32_t channel;
        str_buffer content;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(channel, content);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(channel, content);
        }
    };

    class OutputContainableInterface
    {
        public:

            virtual ~OutputContainableInterface() noexcept = default;

            virtual void resize(size_t sz) = 0;
            virtual auto data() noexcept -> std::add_pointer_t<char> = 0;
    };

    class ChannelContainerInterface
    {
        public:

            virtual ~ChannelContainerInterface() noexcept = default;

            //the lifetime of the buffer here is clear, we can proceed and make it a global string

            virtual void push(uint32_t channel,
                              std::move_iterator<str_buffer *> str_arr, size_t str_arr_sz, exception_t * exception_arr) noexcept = 0;

            virtual void pop(uint32_t channel,
                             str_buffer * recv_arr, size_t& recv_arr_sz, size_t recv_arr_cap) noexcept = 0;

            virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    class ChannelMailboxInterface
    {
        public:

            virtual ~ChannelMailboxInterface() noexcept = default;

            virtual void send(uint32_t channel,
                              MailBoxArgument * data_arr, size_t sz, exception_t * exception_arr) noexcept = 0;

            virtual void recv(uint32_t channel,
                              std::add_pointer_t<OutputContainableInterface> * output_container_arr,
                              size_t& recv_sz, size_t recv_cap,
                              exception_t * exception_arr) noexcept = 0;

            virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    class StickyChannelContainer: public virtual ChannelContainerInterface
    {
        private:

            struct Consumer
            {
                str_buffer * recv_buffer_arr;

                size_t * recv_buffer_arr_sz;
                size_t recv_buffer_arr_cap;

                std::binary_semaphore * smp;
            };

            dg_sock::unordered_unstable_map<uint32_t, dg_sock::pow2_cyclic_queue<str_buffer>> channel_buffer_map;
            dg_sock::unordered_unstable_map<uint32_t, dg_sock::deque<Consumer>> consumer_buffer_map;

            size_t pow2_max_channel_buffer_sz;

            std::unique_ptr<stdxx::fair_atomic_flag> mtx;
            stdxx::hdi_container<size_t> max_consume_sz;

        public:

            StickyChannelContainer(size_t max_channel_buffer_sz,
                                   size_t max_consume_sz)
            {
                if (max_channel_buffer_sz == 0u)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (max_consume_sz == 0u)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                this->channel_buffer_map            = {};
                this->consumer_buffer_map           = {};
                this->pow2_max_channel_buffer_sz    = stdxx::ceil2(max_channel_buffer_sz);

                this->mtx                           = stdxx::make_unique_fair_atomic_flag();
                this->max_consume_sz.value          = max_consume_sz;
            }

            void push(uint32_t channel,
                      std::move_iterator<str_buffer *> str_arr, size_t str_arr_sz, exception_t * exception_arr) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (str_arr_sz > this->max_consume_size())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INVALID_ARGUMENT));
                        std::abort();
                    }
                }

                str_buffer * base_str_arr   = str_arr.base();
                size_t base_str_arr_sz      = str_arr_sz;

                std::fill(exception_arr, std::next(exception_arr, str_arr_sz), dg_sock::network_exception::SUCCESS);

                {
                    stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                    bool need_release_front = false;

                    while (true)
                    {
                        if (base_str_arr_sz == 0u)
                        {
                            break;
                        }

                        auto map_ptr = this->consumer_buffer_map.find(channel);

                        if (map_ptr == this->consumer_buffer_map.end())
                        {
                            break;
                        }

                        if (map_ptr->second.empty())
                        {
                            break;
                        }

                        Consumer& front_consumer = map_ptr->second.front();

                        if constexpr(DEBUG_MODE_FLAG)
                        {
                            if (*front_consumer.recv_buffer_arr_sz == front_consumer.recv_buffer_arr_cap)
                            {
                                dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                                std::abort();
                            }
                        }

                        front_consumer.recv_buffer_arr[*front_consumer.recv_buffer_arr_sz++] = std::move(base_str_arr[0]);
                        need_release_front = true;

                        if (*front_consumer.recv_buffer_arr_sz == front_consumer.recv_buffer_arr_cap)
                        {
                            front_consumer.smp->release();
                            map_ptr->second.pop_front();

                            need_release_front  = false;
                        }

                        base_str_arr    = std::next(base_str_arr);
                        base_str_arr_sz -= 1u;
                    }

                    if (need_release_front)
                    {
                        auto& container         = this->consumer_buffer_map.at(channel);
                        auto& front_consumer    = container.front();

                        front_consumer.smp->release();
                        container.pop_front();

                        need_release_front      = false;
                    }

                    if (base_str_arr_sz == 0u)
                    {
                        return;
                    }

                    auto map_ptr = this->channel_buffer_map.find(channel);

                    if (map_ptr == this->channel_buffer_map.end())
                    {
                        auto [new_map_ptr, status] = this->channel_buffer_map.insert(std::make_pair(channel, dg_sock::pow2_cyclic_queue<str_buffer>(stdxx::ulog2(this->pow2_max_channel_buffer_sz))));
                        map_ptr = new_map_ptr;
                        dg_sock::network_exception_handler::dg_assert(status);
                    }

                    for (size_t i = 0u; i < base_str_arr_sz; ++i)
                    {
                        if (map_ptr->second.size() == map_ptr->second.capacity())
                        {
                            this->log_full_message_queue_on(channel);
                        }

                        dg_sock::network_exception_handler::nothrow_log(map_ptr->second.push_back(std::move(base_str_arr[i])));
                    }
                }
            }

            void pop(uint32_t channel,
                     str_buffer * recv_arr, size_t& recv_arr_sz, size_t recv_arr_cap) noexcept
            {
                recv_arr_sz = 0u;

                if (recv_arr_cap == 0u)
                {   
                    return;
                }

                std::binary_semaphore smp(0);

                bool need_wait = [&]() noexcept
                {
                    stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                    {
                        auto map_ptr = this->channel_buffer_map.find(channel);

                        if (map_ptr != this->channel_buffer_map.end())
                        {
                            if (!map_ptr->second.empty())
                            {
                                size_t fulfillable_sz   = std::min(recv_arr_cap, map_ptr->second.size());
                                recv_arr_sz             = fulfillable_sz;

                                std::copy(std::make_move_iterator(map_ptr->second.begin()), std::make_move_iterator(std::next(map_ptr->second.begin(), fulfillable_sz)), recv_arr);
                                map_ptr->second.erase_front_range(fulfillable_sz);

                                return false;
                            }
                        }
                    }

                    {
                        auto map_ptr = this->consumer_buffer_map.find(channel);

                        if (map_ptr == this->consumer_buffer_map.end())
                        {
                            auto [new_map_ptr, status] = this->consumer_buffer_map.insert(std::make_pair(channel, dg_sock::deque<Consumer>()));
                            map_ptr = new_map_ptr;
                            dg_sock::network_exception_handler::dg_assert(status);
                        }

                        map_ptr->second.push_back(Consumer
                        {
                            .recv_buffer_arr        = recv_arr,
                            .recv_buffer_arr_sz     = &recv_arr_sz,
                            .recv_buffer_arr_cap    = recv_arr_cap
                        });

                        return true;
                    }
                }();

                if (need_wait)
                {
                    smp.acquire();
                }
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->max_consume_sz.value;
            }

        private:

            void log_full_message_queue_on(uint32_t channel) noexcept
            {
                dg_sock::string msg{};
                std::format_to(std::back_inserter(msg), "Message Queue is full on Channel {}, dropping one head", channel);

                dg_sock::network_log::error_fast(msg.c_str());
            }
    };

    class PreconfiguratedStickyChannelContainer: public virtual ChannelContainerInterface
    {
        private:

            struct Bucket
            {
                std::unique_ptr<StickyChannelContainer> channel_container;
                size_t max_unit_sz;
            };

            dg_sock::unordered_unstable_map<uint32_t, Bucket> sticky_channel_map;
            
            static inline constexpr size_t ONE_CHANNEL_STICKY_CHANNEL_QUEUE_SZ          = 32u;
            static inline constexpr size_t ONE_CHANNEL_STICKY_CHANNEL_QUEUE_CONSUME_SZ  = 8u;

            static inline constexpr size_t THOUSAND_CHANNEL_STICKY_CHANNEL_QUEUE_SZ     = size_t{1} << 8;
            static inline constexpr size_t THOUSAND_CHANNEL_STICKY_CHANNEL_CONSUME_SZ   = size_t{1} << 4;

            static inline constexpr size_t MILLION_CHANNEL_STICKY_CHANNEL_QUEUE_SZ      = size_t{1} << 16;
            static inline constexpr size_t MILLION_CHANNEL_STICKY_CHANNEL_CONSUME_SZ    = size_t{1} << 8;

            static inline constexpr size_t CONSUMPTION_SZ                               = std::min(std::min(ONE_CHANNEL_STICKY_CHANNEL_QUEUE_CONSUME_SZ, THOUSAND_CHANNEL_STICKY_CHANNEL_CONSUME_SZ),
                                                                                                   MILLION_CHANNEL_STICKY_CHANNEL_CONSUME_SZ);

        public:

            using channel_t = uint8_t;

            static inline constexpr uint8_t ONE_CHANNEL_CODEX       = 0u;
            static inline constexpr uint8_t THOUSAND_CHANNEL_CODEX  = 1u;
            static inline constexpr uint8_t MILLION_CHANNEL_CODEX   = 2u;

            PreconfiguratedStickyChannelContainer(const std::unordered_map<uint32_t, channel_t>& channel_choice_map,
                                                  const std::unordered_map<uint32_t, size_t>& channel_msg_cap_map = std::unordered_map<uint32_t, size_t>{}): sticky_channel_map()
            {
                for (const auto& [channel, channel_type]: channel_choice_map)
                {
                    switch (channel_type)
                    {
                        case ONE_CHANNEL_CODEX:
                        {
                            this->sticky_channel_map[channel]   = Bucket
                            {
                                .channel_container  = std::make_unique<StickyChannelContainer>(ONE_CHANNEL_STICKY_CHANNEL_QUEUE_SZ, ONE_CHANNEL_STICKY_CHANNEL_QUEUE_CONSUME_SZ),
                                .max_unit_sz        = this->get_unit_size(channel_msg_cap_map, channel)
                            };

                            break;
                        }
                        case THOUSAND_CHANNEL_CODEX:
                        {
                            this->sticky_channel_map[channel]   = Bucket
                            {
                                .channel_container  = std::make_unique<StickyChannelContainer>(THOUSAND_CHANNEL_STICKY_CHANNEL_QUEUE_SZ, THOUSAND_CHANNEL_STICKY_CHANNEL_CONSUME_SZ),
                                .max_unit_sz        = this->get_unit_size(channel_msg_cap_map, channel)
                            };

                            break;
                        }
                        case MILLION_CHANNEL_CODEX:
                        {
                            this->sticky_channel_map[channel]   = Bucket
                            {
                                .channel_container  = std::make_unique<StickyChannelContainer>(MILLION_CHANNEL_STICKY_CHANNEL_QUEUE_SZ, MILLION_CHANNEL_STICKY_CHANNEL_CONSUME_SZ),
                                .max_unit_sz        = this->get_unit_size(channel_msg_cap_map, channel)
                            };

                            break;
                        }
                        default:
                        {
                            dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                        }
                    }
                }
            }

            void push(uint32_t channel,
                      std::move_iterator<str_buffer *> str_arr, size_t str_arr_sz, exception_t * exception_arr) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (str_arr_sz > this->max_consume_size())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                auto map_ptr = std::as_const(this->sticky_channel_map).find(channel);

                if (map_ptr == this->sticky_channel_map.end())
                {
                    std::fill(exception_arr, std::next(exception_arr, str_arr_sz), dg_sock::network_exception::INVALID_ARGUMENT);
                    return;
                }
                
                dg_sock::network_stack_allocation::NoExceptAllocation<str_buffer[]> thru_str_arr(str_arr_sz);
                dg_sock::network_stack_allocation::NoExceptAllocation<std::add_pointer_t<exception_t>[]> exception_ptr_arr(str_arr_sz);

                size_t thru_str_arr_sz  = 0u;
                auto base_str_arr       = str_arr.base();

                for (size_t i = 0u; i < str_arr_sz; ++i)
                {
                    if (base_str_arr[i].size() > map_ptr->second.max_unit_sz)
                    {
                        exception_arr[i] = dg_sock::network_exception::SOCKET_CHANNEL_MAX_MSG_SIZE_REACHED;
                        continue;
                    }

                    thru_str_arr[thru_str_arr_sz]       = std::move(base_str_arr[i]);
                    exception_ptr_arr[thru_str_arr_sz]  = std::next(exception_arr, i);
                    thru_str_arr_sz                     += 1;
                }

                dg_sock::network_stack_allocation::NoExceptAllocation<exception_t[]> tmp_exception_arr(thru_str_arr_sz);
                map_ptr->second.channel_container->push(channel, std::make_move_iterator(thru_str_arr.get()), thru_str_arr_sz, tmp_exception_arr.get());

                for (size_t i = 0u; i < thru_str_arr_sz; ++i)
                {
                    *exception_ptr_arr[i] = tmp_exception_arr[i];
                }
            }

            void pop(uint32_t channel,
                     str_buffer * recv_arr, size_t& recv_arr_sz, size_t recv_arr_cap) noexcept
            {
                auto map_ptr = std::as_const(this->sticky_channel_map).find(channel);

                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (map_ptr == this->sticky_channel_map.end())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                map_ptr->second.channel_container->pop(channel, recv_arr, recv_arr_sz, recv_arr_cap);
            }

            auto max_consume_size() noexcept -> size_t
            {
                return CONSUMPTION_SZ;
            }
        
        private:

            auto get_unit_size(const std::unordered_map<uint32_t, size_t>& channel_msg_cap_map, uint32_t channel) const noexcept -> size_t
            {
                auto map_ptr = channel_msg_cap_map.find(channel);

                if (map_ptr == channel_msg_cap_map.end())
                {
                    return std::numeric_limits<size_t>::max();
                }

                return map_ptr->second;
            }
    };

    class DistributedChannelContainer: public virtual ChannelContainerInterface
    {
        private:

            std::vector<std::unique_ptr<ChannelContainerInterface>> base_vec;

        public:

            DistributedChannelContainer(std::vector<std::unique_ptr<ChannelContainerInterface>> base_vec): base_vec(std::move(base_vec)){}

            void push(uint32_t channel,
                      std::move_iterator<str_buffer *> str_arr, size_t str_arr_sz, exception_t * exception_arr) noexcept
            {
                size_t channel_container_idx = channel % this->base_vec.size();

                this->base_vec[channel_container_idx]->push(channel, str_arr, str_arr_sz, exception_arr);
            }

            void pop(uint32_t channel,
                     str_buffer * recv_arr, size_t& recv_arr_sz, size_t recv_arr_cap) noexcept
            {
                size_t channel_container_idx = channel % this->base_vec.size();

                this->base_vec[channel_container_idx]->pop(channel, recv_arr, recv_arr_sz, recv_arr_cap);
            }
    };

    class AssorterWorker: public virtual dg_sock::network_concurrency::WorkerInterface
    {
        private:

            std::shared_ptr<ChannelContainerInterface> channel_container;
            std::shared_ptr<dg_sock::network_kernel_mailbox_impl1_flash_stream_x::DynamicMailboxInterface> base_mailbox;
            size_t pull_sz;
            size_t push_sz;
            size_t busy_pull_sz;

        public:

            AssorterWorker(std::shared_ptr<ChannelContainerInterface> channel_container,
                           std::shared_ptr<dg_sock::network_kernel_mailbox_impl1_flash_stream_x::DynamicMailboxInterface> base_mailbox,
                           size_t pull_sz,
                           size_t push_sz,
                           size_t busy_pull_sz) noexcept: channel_container(std::move(channel_container)),
                                                          base_mailbox(std::move(base_mailbox)),
                                                          pull_sz(pull_sz),
                                                          push_sz(push_sz),
                                                          busy_pull_sz(busy_pull_sz){}

            auto run_one_epoch() noexcept -> bool
            {
                dg_sock::network_stack_allocation::NoExceptAllocation<OutputContainer[]> output_container_arr(this->pull_sz);
                dg_sock::network_stack_allocation::NoExceptAllocation<std::add_pointer_t<dg_sock::network_kernel_mailbox_impl1_flash_stream_x::OutputContainableInterface>[]> virtual_output_container_arr(this->pull_sz);
                dg_sock::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(this->pull_sz);

                for (size_t i = 0u; i < this->pull_sz; ++i)
                {
                    virtual_output_container_arr[i] = &output_container_arr[i];
                }

                size_t recv_sz;
                this->base_mailbox->recv(virtual_output_container_arr.get(), recv_sz, this->pull_sz, exception_arr.get());

                auto resolutor  = ChannelFeedResolutor{};
                resolutor.sink  = this->channel_container.get();

                size_t feed_cap = std::min(std::min(recv_sz, this->push_sz), this->channel_container->max_consume_size());
                size_t mem_sz   = dg_sock::network_producer_consumer::delvrsrv_kv_allocation_cost(&resolutor, feed_cap);
                dg_sock::network_stack_allocation::NoExceptRawAllocation<char[]> mem_buf(mem_sz);
                auto feeder     = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&resolutor, feed_cap, mem_buf.get()));

                for (size_t i = 0u; i < recv_sz; ++i)
                {
                    if (dg_sock::network_exception::is_failed(exception_arr[i]))
                    {
                        dg_sock::network_log_stackdump::error_fast(dg_sock::network_exception::verbose(exception_arr[i]));
                        continue;
                    }

                    std::expected<ChannelMessage, exception_t> channel_msg = dg_sock::network_exception::to_cstyle_function(dg_sock::network_compact_serializer::dgstd_deserialize<ChannelMessage, str_buffer>)(output_container_arr[i].get(), SERIALIZATION_SECRET);

                    if (!channel_msg.has_value())
                    {
                        dg_sock::network_log_stackdump::error_fast(dg_sock::network_exception::verbose(channel_msg.error()));
                        continue;
                    }

                    dg_sock::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), channel_msg->channel, std::move(channel_msg->content));
                }

                return recv_sz >= this->busy_pull_sz;
            }

        private:

            struct ChannelFeedResolutor: public virtual dg_sock::network_producer_consumer::KVConsumerInterface<uint32_t, str_buffer>
            {
                ChannelContainerInterface * sink;

                void push(const uint32_t& channel, std::move_iterator<str_buffer *> buf_vec, size_t buf_vec_sz) noexcept
                {
                    dg_sock::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(buf_vec_sz);

                    this->sink->push(channel, buf_vec, buf_vec_sz, exception_arr.get());

                    for (size_t i = 0u; i < buf_vec_sz; ++i)
                    {
                        if (dg_sock::network_exception::is_failed(exception_arr[i]))
                        {
                            dg_sock::network_log_stackdump::error_fast(dg_sock::network_exception::verbose(exception_arr[i]));
                        }
                    }
                }
            };

            class OutputContainer: public virtual dg_sock::network_kernel_mailbox_impl1_flash_stream_x::OutputContainableInterface
            {
                private:

                    str_buffer str_reference;

                public:

                    void resize(size_t sz)
                    {
                        this->str_reference.resize(sz);
                    }

                    auto data() noexcept -> char *
                    {
                        return this->str_reference.data();
                    }

                    auto get() noexcept -> str_buffer&
                    {
                        return this->str_reference;
                    }
            };
    };

    class ChannelMailbox: public virtual ChannelMailboxInterface
    {
        private:

            std::shared_ptr<void> daemon;
            std::shared_ptr<ChannelContainerInterface> channel_container;
            std::shared_ptr<dg_sock::network_kernel_mailbox_impl1_flash_stream_x::DynamicMailboxInterface> base_mailbox;

        public:

            ChannelMailbox(std::shared_ptr<void> daemon,
                           std::shared_ptr<ChannelContainerInterface> channel_container,
                           std::shared_ptr<dg_sock::network_kernel_mailbox_impl1_flash_stream_x::DynamicMailboxInterface> base_mailbox) noexcept: daemon(std::move(daemon)),
                                                                                                                                                  channel_container(std::move(channel_container)),
                                                                                                                                                  base_mailbox(std::move(base_mailbox)){}

            void send(uint32_t channel,
                      MailBoxArgument * data_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                dg_sock::network_stack_allocation::NoExceptAllocation<str_buffer[]> str_buffer_arr(sz);
                dg_sock::network_stack_allocation::NoExceptAllocation<MailBoxArgument[]> new_data_arr(sz);
                dg_sock::network_stack_allocation::NoExceptAllocation<std::add_pointer_t<exception_t>[]> exception_ptr_arr(sz);

                size_t thru_sz = 0u;

                for (size_t i = 0u; i < sz; ++i)
                {
                    std::expected<str_buffer, exception_t> cpy_msg = dg_sock::network_exception::cstyle_initialize<str_buffer>(static_cast<const char *>(data_arr[i].content), //why I hit UB here? if this is not inlined, then I'll be fine, but if it is not from char * or const char *, we'll be undefined, even though char * is convertible to every other pointer, it is not lifetime-initialized by new[] statement
                                                                                                                               std::next(static_cast<const char *>(data_arr[i].content), data_arr[i].content_sz));

                    if (!cpy_msg.has_value())
                    {
                        exception_arr[i] = cpy_msg.error();
                        continue;
                    }

                    ChannelMessage msg = 
                    {
                        .channel    = channel,
                        .content    = std::move(cpy_msg.value())
                    };

                    std::expected<str_buffer, exception_t> actual_msg = dg_sock::network_exception::to_cstyle_function(dg_sock::network_compact_serializer::dgstd_serialize<str_buffer, ChannelMessage>)(msg, SERIALIZATION_SECRET);

                    if (!actual_msg.has_value())
                    {
                        exception_arr[i] = actual_msg.error();
                        continue;
                    }

                    str_buffer_arr[thru_sz]     = std::move(actual_msg.value());
                    new_data_arr[thru_sz]       = MailBoxArgument
                    {
                        .to         = data_arr[i].to,
                        .content    = str_buffer_arr[thru_sz].data(),
                        .content_sz = str_buffer_arr[thru_sz].size()
                    };
                    exception_ptr_arr[thru_sz]  = std::next(exception_arr, i);

                    thru_sz                     += 1;
                }

                dg_sock::network_stack_allocation::NoExceptAllocation<exception_t[]> tmp_exception_arr(thru_sz);
                this->base_mailbox->send(new_data_arr.get(), thru_sz, tmp_exception_arr.get());

                for (size_t i = 0u; i < thru_sz; ++i)
                {
                    *exception_ptr_arr[i] = tmp_exception_arr[i];
                }
            }

            void recv(uint32_t channel,
                      std::add_pointer_t<OutputContainableInterface> * output_container_arr,
                      size_t& recv_sz, size_t recv_cap,
                      exception_t * exception_arr) noexcept
            {
                dg_sock::network_stack_allocation::NoExceptAllocation<str_buffer[]> recv_arr(recv_cap);
                this->channel_container->pop(channel, recv_arr.get(), recv_sz, recv_cap);

                for (size_t i = 0u; i < recv_sz; ++i)
                {
                    try
                    {
                        output_container_arr[i]->resize(recv_arr[i].size());
                        std::copy(recv_arr[i].begin(), recv_arr[i].end(), output_container_arr[i]->data());

                        exception_arr[i] = dg_sock::network_exception::SUCCESS;
                    }
                    catch (...)
                    {
                        exception_arr[i] = dg_sock::network_exception::wrap_std_exception(std::current_exception());
                    }
                }
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->base_mailbox->max_consume_size();
            }
    };

    class ComponentFactory
    {
        public:

            static auto get_sticky_channel_container(size_t max_channel_buffer_sz,
                                                     size_t max_consume_sz) -> std::unique_ptr<ChannelContainerInterface>
            {
                return std::make_unique<StickyChannelContainer>(max_channel_buffer_sz,
                                                                max_consume_sz);
            }

            static auto get_preconfigurated_sticky_channel_container(const std::unordered_map<uint32_t, PreconfiguratedStickyChannelContainer::channel_t>& channel_choice_map,
                                                                     const std::unordered_map<uint32_t, size_t>& channel_msg_cap_map) -> std::unique_ptr<ChannelContainerInterface>
            {
                return std::make_unique<PreconfiguratedStickyChannelContainer>(channel_choice_map,
                                                                               channel_msg_cap_map);
            }

            static auto get_assorter_worker(std::shared_ptr<ChannelContainerInterface> channel_container,
                                            std::shared_ptr<dg_sock::network_kernel_mailbox_impl1_flash_stream_x::DynamicMailboxInterface> base_mailbox,
                                            size_t pull_sz,
                                            size_t push_sz,
                                            size_t busy_pull_sz) -> std::unique_ptr<dg_sock::network_concurrency::WorkerInterface>
            {
                const size_t MIN_PULL_SZ        = 1u;
                const size_t MAX_PULL_SZ        = size_t{1} << 20;
                const size_t MIN_PUSH_SZ        = 1u;
                const size_t MAX_PUSH_SZ        = size_t{1} << 20;
                const size_t MIN_BUSY_PULL_SZ   = 0u;
                const size_t MAX_BUSY_PULL_SZ   = size_t{1} << 20;

                if (channel_container == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (base_mailbox == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(pull_sz, MIN_PULL_SZ, MAX_PULL_SZ) != pull_sz)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(push_sz, MIN_PUSH_SZ, MAX_PUSH_SZ) != push_sz)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(busy_pull_sz, MIN_BUSY_PULL_SZ, MAX_BUSY_PULL_SZ) != busy_pull_sz)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                return std::make_unique<AssorterWorker>(std::move(channel_container),
                                                        std::move(base_mailbox),
                                                        pull_sz,
                                                        push_sz,
                                                        busy_pull_sz);
            }
    };

    class SolutionBuilder
    {
        private:

            std::shared_ptr<dg_sock::network_kernel_mailbox_impl1_flash_stream_x::DynamicMailboxInterface> base;

            std::unordered_map<uint32_t, PreconfiguratedStickyChannelContainer::channel_t> channel_kind_map;
            std::unordered_map<uint32_t, uint64_t> channel_msg_cap_map;

            uint64_t assorter_worker_sz;

            static inline constexpr size_t DEFAULT_ASSORTER_WORKER_SZ   = 1u;
            static inline constexpr size_t DEFAULT_PULL_SZ              = 1u;
            static inline constexpr size_t DEFAULT_PUSH_SZ              = 1u;
            static inline constexpr size_t DEFAULT_BUSY_PULL_SZ         = 0u;

        public:

            SolutionBuilder(): base(nullptr),
                               channel_kind_map(),
                               channel_msg_cap_map(),
                               assorter_worker_sz(DEFAULT_ASSORTER_WORKER_SZ){}

            auto set_base(std::shared_ptr<dg_sock::network_kernel_mailbox_impl1_flash_stream_x::DynamicMailboxInterface> base) -> SolutionBuilder&
            {
                if (base == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                this->base = std::move(base);

                return *this;
            }

            auto add_channel(uint32_t channel, PreconfiguratedStickyChannelContainer::channel_t channel_kind, uint64_t msg_cap) -> SolutionBuilder&
            {
                this->channel_kind_map[channel]     = channel_kind;
                this->channel_msg_cap_map[channel]  = msg_cap;

                return *this;
            }

            auto set_worker_size(size_t sz) -> SolutionBuilder&
            {
                this->assorter_worker_sz = sz;

                return *this;
            }

            auto build() -> std::unique_ptr<ChannelMailboxInterface>
            {
                if (this->base == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                std::shared_ptr<ChannelContainerInterface> channel_container = this->get_channel_container();
                std::shared_ptr<void> daemon = this->get_daemon_worker(channel_container);

                return std::make_unique<ChannelMailbox>(std::move(daemon),
                                                        std::move(channel_container),
                                                        this->base);
            }

            template <class Reflector>
            void dg_reflect(const Reflector& reflector) const
            {
                reflector(channel_kind_map, channel_msg_cap_map, assorter_worker_sz);
            }

            template <class Reflector>
            void dg_reflect(const Reflector& reflector)
            {
                reflector(channel_kind_map, channel_msg_cap_map, assorter_worker_sz);
            }

        private:

            auto get_channel_container() -> std::unique_ptr<ChannelContainerInterface>
            {
                return ComponentFactory::get_preconfigurated_sticky_channel_container(this->channel_kind_map,
                                                                                      {this->channel_msg_cap_map.begin(), this->channel_msg_cap_map.end()});
            }

            auto get_daemon_worker(std::shared_ptr<ChannelContainerInterface> channel) -> std::shared_ptr<void>
            {
                auto worker_handle = dg_sock::network_exception_handler::throw_nolog(dg_sock::network_concurrency::daemon_saferegister(dg_sock::network_concurrency::CHANNEL_DAEMON,
                                                                                                                                       ComponentFactory::get_assorter_worker(channel,
                                                                                                                                                                             this->base,
                                                                                                                                                                             DEFAULT_PULL_SZ,
                                                                                                                                                                             DEFAULT_PUSH_SZ,
                                                                                                                                                                             DEFAULT_BUSY_PULL_SZ)));

                return std::make_shared<decltype(worker_handle)>(std::move(worker_handle));
            }
    };
}

#endif
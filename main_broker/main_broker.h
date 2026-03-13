#ifndef __MAIN_BROKER_H__
#define __MAIN_BROKER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <mutex_extension/fair_mutex.h>
#include <optional>
#include <semaphore>
#include <unordered_map>
#include <deque>
#include <exception>

namespace main_broker
{
    //in this main brokerage system, we'd provide actionables that are polymorphically solvable by main, like thread spawning or recoverable or etc.
    //we'd mainly use this for matrix steering subsystem where we'd be on a dedicated thread to do our synchronization promise mission

    class Resolvable
    {
        public:

            virtual ~Resolvable() noexcept = default;

            virtual auto get_resolvable_id() noexcept -> uint8_t = 0;
    };

    class ResolverInterface
    {
        public:

            virtual ~ResolverInterface() noexcept = default;

            virtual void resolve(std::shared_ptr<Resolvable> resolvable) = 0;
    };
    
    class BrokerageController
    {
        private:

            struct AskWaitArgument
            {
                std::binary_semaphore * smp;
                std::shared_ptr<Resolvable> resolvable;
                std::exception_ptr * exception;
            };

            struct MainWaitArgument
            {
                std::binary_semaphore * smp;
                AskWaitArgument * dst;
            };

            std::deque<AskWaitArgument> resolvable_vec;
            std::unordered_map<uint8_t, std::shared_ptr<ResolverInterface>> resolver_map;
            std::optional<MainWaitArgument> main_arg;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

        public:

            BrokerageController(): resolvable_vec(),
                                   resolver_map(),
                                   main_arg(std::nullopt),
                                   mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            void ask(std::shared_ptr<Resolvable> resolvable)
            {
                if (resolvable == nullptr)
                {
                    throw std::invalid_argument("bad resolvable, null");
                }

                std::binary_semaphore smp(0);
                std::exception_ptr exception = nullptr;

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    if (!this->resolver_map.contains(resolvable->get_resolvable_id()))
                    {
                        throw std::runtime_error("bad resolvable, no resolver found");
                    }

                    AskWaitArgument arg
                    {
                        .smp        = &smp,
                        .resolvable = resolvable,
                        .exception  = &exception
                    };

                    if (this->main_arg.has_value())
                    {
                        *this->main_arg->dst    = std::move(arg);
                        this->main_arg->smp->release();
                        this->main_arg          = std::nullopt;
                    }
                    else
                    {
                        this->resolvable_vec.push_back(std::move(arg));
                    }
                }

                smp.acquire();

                if (exception != nullptr)
                {
                    std::rethrow_exception(exception);
                }
            }

            void add_resolver(uint8_t resolvable_id, std::unique_ptr<ResolverInterface>&& resolver)
            {
                if (resolver == nullptr)
                {
                    throw std::invalid_argument("bad resolver, null");
                }

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    this->resolver_map[resolvable_id] = std::move(resolver);
                }
            }

            void main_subscribe()
            {
                while (true)
                {
                    this->main_wait_for_one();
                }
            }
        
        private:

            void main_wait_for_one()
            {
                std::binary_semaphore smp(0);
                std::shared_ptr<ResolverInterface> resolver{};
                AskWaitArgument ask_arg{};

                MainWaitArgument wait_arg
                {
                    .smp    = &smp,
                    .dst    = &ask_arg
                };

                bool need_wait = [&]
                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    if (!this->resolvable_vec.empty())
                    {
                        ask_arg = std::move(this->resolvable_vec.front());
                        
                        if (auto map_ptr = this->resolver_map.find(ask_arg.resolvable->get_resolvable_id()); map_ptr != this->resolver_map.end())
                        {
                            resolver = map_ptr->second;
                        }
                        else
                        {
                            resolver = nullptr;
                        }

                        this->resolvable_vec.pop_front();
                        return false;
                    }
                    
                    if constexpr(DEBUG_MODE_FLAG)
                    {
                        if (!this->main_arg.has_value())
                        {
                            std::abort();
                        }
                    }

                    this->main_arg = wait_arg;

                    return true;
                }();

                if (need_wait)
                {
                    smp.acquire();
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    if (auto map_ptr = this->resolver_map.find(ask_arg.resolvable->get_resolvable_id()); map_ptr != this->resolver_map.end())
                    {
                        resolver = map_ptr->second;
                    }
                    else
                    {
                        resolver = nullptr;
                    }
                }

                try
                {
                    if (resolver == nullptr)
                    {
                        throw std::runtime_error("bad resolvable, no resolver found");
                    }

                    resolver->resolve(ask_arg.resolvable);
                }
                catch (...)
                {
                    *ask_arg.exception = std::current_exception();
                }

                ask_arg.smp->release();
            }
    };

}

#endif
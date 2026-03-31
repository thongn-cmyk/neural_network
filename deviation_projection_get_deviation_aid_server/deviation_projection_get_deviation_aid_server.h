#ifndef __DEVIATION_PROJECTION_GET_DEVIATION_AID_SERVER_H__
#define __DEVIATION_PROJECTION_GET_DEVIATION_AID_SERVER_H__

#include <stdint.h>
#include <stdlib.h>
#include <concurrency_base/concurrency_base.h>
#include <request_extension/type_based_resolutor_interface.h>
#include <deviation_projection_client/deviation_projection_client.h>

//in this topic, we'd build a tree of deviation aid server, whose base is deviation_projection_client, we'd try to increase the outdegree to make sure that the latency is sound, otherwise, we'd have to do radix 2 just for fast diffraction
//essentially, we'd have to do detached requests, with dedicated threads, and automatic timeout on client side (with cron_subsystem one time subscription)
//detached in the sense of the dedicated thread must make another request to ping on completion

//from the foundation of our AI coding, we already talked that this is session-scoped
//if these things are not session'scoped, we can't quantify and can't be 100% correct about literally anything

//I'm afraid that people underestimate the cuda processing speed and fairness of CPU saturation

namespace deviation_projection_get_deviation_aid_server
{
    static inline constexpr std::string_view DEVIATION_PROJECTION_GET_DEVIATION_AID_SERVER_VERSION_CONTROL = "";

    //what's hard is that we'd have to have connection-based per node, not master node controlling all the connections
    //so it's best to have this reference the clients respectively for set_child_vec
    //we'd have to include the logic at the set_child_vec

    //what I would imagine is that the detached rest would involve the smp id of client, just like how we built the rest_framework, just do it again this time
    //detached rest would take care of the lightweight detachment process on top of the rest stack

    //and we can have a decdicated thread to wait and do etc for us

    class ClientBox
    {
        private:

            std::shared_ptr<detached_rest::RequestContainerInterface> request_container;
            std::shared_ptr<std::thread> task_thr;
            bool was_run_broke;
            bool was_wait_broke;
            bool was_explicitly_destroyed;
            std::shared_ptr<std::atomic<bool>> is_completed_var;
            std::shared_ptr<std::atomic<bool>> interruption_pill;

        public:

            ClientBox(): request_container(detached_rest::ContainerFactory::get_best_container()),
                         task_thr(nullptr),
                         was_run_broke(false),
                         was_wait_broke(false),
                         was_explicitly_destroyed(false),
                         is_completed_var(std::make_shared<std::atomic<bool>>(false)),
                         interruption_pill(std::make_shared<std::atomic<bool>>(false)){}

            ~ClientBox() noexcept
            {
                this->close();
            }

            void set_child_vec()
            {

            }

            void set_accumulation_method()
            {

            }

            void get_deviation()
            {

            }

            void run()
            {

            }

            auto is_completed() -> bool
            {
                if (this->was_explicitly_destroyed)
                {
                    return true;
                }

                return this->is_completed_var->load(std::memory_order_relaxed);
            }

            void interrupt() noexcept
            {

            }

            void wait()
            {

            }

            void close() noexcept
            {

            }
    };

    class ConnectionBoundClientBox: public virtual connection_based_manager::HealthcheckableInterface
    {
        private:
            
            std::unique_ptr<connectivity_subsystem::ConnectionInterface> connection;
            std::unique_ptr<ClientBox> base;
            std::unique_ptr<std::atomic<bool>> was_explicitly_destroyed;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
        
        public:

            ConnectionBoundClientBox(const connectivity_subsystem::SlaveConfiguration& connection_config): connection(std::make_unique<connectivity_subsystem::ThreadSafeSlaveConnection>(connection_config)),
                                                                                                           base(std::make_unique<ClientBox>()),
                                                                                                           was_explicitly_destroyed(std::make_unique<std::atomic<bool>>(false)),
                                                                                                           mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            void set_child_vec()
            {

            }

            void set_accumulation_method()
            {

            }

            void get_deviation()
            {

            }

            void run()
            {

            }

            auto is_completed() -> bool
            {

            }

            void interrupt() noexcept
            {

            }

            void wait()
            {

            }

            void close() noexcept
            {

            }
    };

    class ClientBoxManager
    {
        private:

            std::unique_ptr<connection_based_manager::ManagerInterface> base;
        
        public:

            ClientBoxManager(): base(std::make_unique<connection_based_manager::ClientManager>()){}

            auto open_client_box(const connectivity_subsystem::SlaveConfiguration& connection_config) -> uint64_t
            {
                return this->base->add(std::make_shared<ConnectionBoundClientBox>(connection_config));
            }

            auto get_client_box(uint64_t client_box_id) -> std::shared_ptr<ConnectionBoundClientBox>
            {
                return std::dynamic_pointer_cast<ConnectionBoundClientBox>(this->base->get(client_box_id));
            }

            void close_client_box(uint64_t client_box_id) noexcept
            {
                this->base->close(client_box_id);
            }
    };

    struct GetVersionRequest
    {
        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            (void) reflector;
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            (void) reflector;
        }
    };

    struct GetVersionResponse
    {
        std::expected<std::string, deviation_projection_get_deviation_aid_server::local_exception_t> response;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(response, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(response, err_verbal_description);
        }
    };

    struct OpenClientRequest
    {
        connectivity_subsystem::SlaveConfiguration connection_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(connection_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(connection_config);
        }
    };

    struct OpenClientResponse
    {
        std::expected<uint64_t, deviation_projection_get_deviation_aid_server::local_exception_t> result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct CloseClientRequest
    {
        uint64_t client_box_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id);
        }
    };

    struct CloseClientResponse
    {
        deviation_projection_get_deviation_aid_server::local_exception_t result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };
    
    struct RunRequest
    {

    };

    struct RunResponse
    {

    };

    struct GetDeviationRequest
    {

    };

    struct GetDeviationResponse
    {

    };

    template <class T_In, class T_Out>
    using TypeBasedResolutorInterface = request_extension::resolutor::TypeBasedResolutorInterface<T_In, T_Out>;

    class GetVersionResolver: public virtual TypeBasedResolutorInterface<GetVersionRequest, GetVersionResponse>
    {
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_get_deviation_aid_server/get_version";

            auto handle(const GetVersionRequest& request) -> GetVersionResponse
            {
                (void) request;

                return GetVersionResponse
                {
                    .response = std::string(DEVIATION_PROJECTION_GET_DEVIATION_AID_SERVER_VERSION_CONTROL),
                    .err_verbal_description = ""
                };
            }
    };

    class OpenClientResolver: public virtual TypeBasedResolutorInterface<OpenClientRequest, OpenClientResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;
        
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_get_deviation_aid_server/open_client";

            OpenClientResolver(std::shared_ptr<ClientBoxManager> client_box_manager): client_box_manager(std::move(client_box_manager)){}

            auto handle(const OpenClientRequest& request) -> OpenClientResponse
            {
                try
                {
                    uint64_t client_box_id = this->client_box_manager->open_client_box(request.connection_config);

                    return OpenClientResponse
                    {
                        .result = client_box_id,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return OpenClientResponse
                    {
                        .result = deviation_projection_get_deviation_aid_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = deviation_projection_get_deviation_aid_server::verbose_error_code(deviation_projection_get_deviation_aid_server::to_local_exception_error_code(std::current_exception()))
                    };
                }
            }
    };

    class CloseClientResolver: public virtual TypeBasedResolutorInterface<CloseClientRequest, CloseClientResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;
        
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_get_deviation_aid_server/close_client";

            CloseClientResolver(std::shared_ptr<ClientBoxManager> client_box_manager): client_box_manager(std::move(client_box_manager)){}

            auto handle(const CloseClientRequest& request) -> CloseClientResponse
            {
                this->client_box_manager->close_client_box(request.client_box_id);

                return CloseClientResponse
                {
                    .result = deviation_projection_get_deviation_aid_server::SUCCESS,
                    .err_verbal_description = ""
                };
            }
    };

    class RunResolutor: public virtual TypeBasedResolutorInterface<RunRequest, RunResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_get_deviation_aid_server/run";

            RunResolutor(std::shared_ptr<ClientBoxManager> client_box_manager): client_box_manager(std::move(client_box_manager)){}

            auto handle(const RunRequest& request) -> RunResponse
            {
                return {};
            }
    };

    class GetDeviationResolutor: public virtual TypeBasedResolutorInterface<GetDeviationRequest, GetDeviationResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;
        
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_get_deviation_aid_server/get_deviation";

            GetDeviationResolutor(std::shared_ptr<ClientBoxManager> client_box_manager): client_box_manager(std::move(client_box_manager)){}

            auto handle(const GetDeviationRequest& request) -> GetDeviationResponse
            {
                return {};
            }
    };
}

#endif
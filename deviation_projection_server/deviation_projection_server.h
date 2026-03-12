#ifndef __DEVIATION_PROJECTION_SERVER_H__
#define __DEVIATION_PROJECTION_SERVER_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include "float_def.h"

namespace deviation_projection_server
{
    using namespace float_def;

    struct Remote
    {
        std::string ipv6;
        uint16_t port;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(ipv6, port);
        }

        template <class Reflector>
        void dg_reflect(const Refelctor& reflector)
        {
            reflector(ipv6, port);
        }
    };

    class ClientBox
    {
        private:

            std::vector<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>> training_data;
            std::vector<generic_deviation_projector_factory::GenericDeviationProjectorResource> matrix_resource_vec;

        public:

            ClientBox(): training_data(),
                         matrix_resource_vec(),
                         deviation_calculator(){}

            void add_training_data(const std::shared_ptr<tensor_model::Matrix>& inp, const std::shared_ptr<tensor_model::Matrix>& out)
            {
                if (inp == nullptr)
                {
                    throw std::invalid_argument("bad input matrix, null");
                }

                if (out == nullptr)
                {
                    throw std::invalid_argument("bad output matrix, null");
                }

                this->training_data.push_back({inp, out});
            }

            void clear_training_data() noexcept
            {
                this->training_data.clear();
            }

            void set_matrix_resource(const std::vector<generic_deviation_projector_factory::GenericDeviationProjectorResource>& matrix_resource_vec)
            {
                this->matrix_resource_vec = matrix_resource_vec;
            }

            auto get() -> std::vector<mdc_float_t>
            {
                if (this->deviation_calculator == nullptr)
                {
                    throw std::runtime_error("incomplete setup, missing deviation calculator");
                }

                std::vector<mdc_float_t> rs_vec{};

                for (const auto& matrix_resource: this->matrix_resource_vec)
                {
                    std::unique_ptr<matrix_deviation_calculator::MatrixDeviationCalculatorInterface> deviation_calculator = generic_deviation_projector_factory::GenericDeviationProjectorResourceLoader{}.load_resource(matrix_resource);
                    rs_vec.push_back(deviation_calculator->get_deviation(this->training_data));
                }

                return rs_vec;
            }
    };

    class ConnectionBoundClientBox
    {
        private:

            std::unique_ptr<connectivity_subsystem::ConnectionInterface> connection;
            ClientBox base;
            bool was_explicitly_destroyed;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

        public:

            ConnectionBoundClientBox(const connectivity_subsystem::SlaveConfiguration& connection_config): connection(std::make_unique<connectivity_subsystem::SlaveConnection>(connection_config)),
                                                                                                           base(),
                                                                                                           was_explicitly_destroyed(false),
                                                                                                           mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            void add_training_data(const std::shared_ptr<tensor_model::Matrix>& inp, const std::shared_ptr<tensor_model::Matrix>& out)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed)
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                this->base.add_training_data(inp, out);
            }

            void clear_training_data()
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed)
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                this->base.clear_training_data();
            }

            void set_matrix_resource(const std::vector<generic_deviation_projector_factory::GenericDeviationProjectorResource>& matrix_resource_vec)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed)
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                this->base.set_matrix_resource(matrix_resource_vec);
            }

            auto get() -> std::vector<mdc_float_t>
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed)
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                return this->base.get();
            }

            void close() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (std::exchange(this->was_explicitly_destroyed, true))
                {
                    return;
                }

                this->connection->close();
            }

            auto is_closed() -> bool
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                return this->was_explicitly_destroyed;
            }

            auto is_alive() -> bool
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed)
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                return this->connection->is_alive();
            }
    };

    class ClientManager: public virtual cron_subsystem::UpdatableInterface
    {
        private:

            std::unordered_map<uint64_t, std::shared_ptr<ConnectionBoundClientBox>> client_map;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
            uint64_t id_counter;

        public:

            ClientManager(): client_map(),
                             mtx(fair_mutex::make_unique_fair_atomic_flag()),
                             id_counter(0u){}

            auto open_client_box(const connectivity_subsystem::SlaveConfiguration& connection_config) -> uint64_t
            {
                std::shared_ptr<ConnectionBoundClientBox> obj = std::make_shared<ConnectionBoundClientBox>(connection_config);

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    uint64_t nxt_id             = this->id_counter;
                    this->client_map[nxt_id]    = std::move(obj);
                    this->id_counter            = nxt_id + 1u;

                    return nxt_id;
                }
            }

            auto get_client_box(uint64_t client_box_id) -> std::shared_ptr<ConnectionBoundClientBox>
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (auto map_ptr = this->client_map.find(client_box_id); map_ptr != this->client_map.end())
                {
                    return map_ptr->second;
                }

                return nullptr;
            }

            void close_client_box(uint64_t client_box_id) noexcept
            {
                std::shared_ptr<ConnectionBoundClientBox> tmp{};

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    if (auto map_ptr = this->client_map.find(client_box_id); map_ptr != this->client_map.end())
                    {
                        tmp = std::move(map_ptr->second);
                        this->client_map.erase(map_ptr);
                    }
                    else
                    {
                        tmp = nullptr;
                    }
                }

                if (tmp != nullptr)
                {
                    tmp->close();
                }
            }

            void update()
            {
                std::vector<std::pair<uint64_t, std::shared_ptr<ConnectionBoundClientBox>>> client_vec{};

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
                    std::copy(this->client_map.begin(), this->client_map.end(), std::back_inserter(client_vec));
                }

                std::unordered_set<uint64_t> bad_client_id_set{};

                for (const auto& [client_id, client_instance]: client_vec)
                {
                    bool is_bad_client;

                    try
                    {
                        is_bad_client = client_instance->is_closed() || !client_instance->is_alive();
                    }
                    catch (...)
                    {
                        is_bad_client = true;
                    }

                    if (is_bad_client)
                    {
                        bad_client_id_set.insert(client_id);
                        client_instance->close();
                    }
                }

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
                    decltype(this->client_map) new_client_map{};

                    for (const auto& [client_id, client_instance]: this->client_map)
                    {
                        if (!bad_client_id_set.contains(client_id))
                        {
                            new_client_map.insert({client_id, client_instance});
                        }
                    }

                    this->client_map = std::move(new_client_map);
                }
            }
    };

    class SelfObservedClientManager
    {
        private:

            std::shared_ptr<ClientManager> base;
            std::shared_ptr<void> cron_obj;

        public:

            static inline const std::chrono::nanoseconds CROM_DURATION = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1));

            SelfObservedClientManager()
            {
                this->base      = std::make_shared<ClientManager>();
                this->cron_obj  = cron_subsystem::register_periodic_cronjob(this->base, CRON_DURATION);
            }

            auto open_client_box(const connectivity_subsystem::SlaveConfiguration& connection_config) -> uint64_t
            {
                return this->base->open_client_box(connection_config);
            }

            auto get_client_box(uint64_t client_box_id) -> std::shared_ptr<ConnectionBoundClientBox>
            {
                return this->base->get_client_box(client_box_id);
            }

            void close_client_box(uint64_t client_box_id) noexcept
            {
                this->base->close_client_box(client_box_id);
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
        std::expected<uint64_t, projection_aid_subsystem::exception_t> result;
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
        uint64_t client_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id);
        }
    };

    struct CloseClientResponse
    {
        projection_aid_subsystem::exception_t result;
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

    struct AddTrainingDataRequest
    {
        uint64_t client_id;
        std::vector<tensor_model::tensor_std_float_t> input_logit_vec;
        std::vector<uint64_t> input_logit_vec_shape;
        std::vector<tensor_model::tensor_std_float_t> output_logit_vec;
        std::vector<uint64_t> output_logit_vec_shape;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id,
                      input_logit_vec, input_logit_vec_shape,
                      output_logit_vec, output_logit_vec_shape);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id,
                      input_logit_vec, input_logit_vec_shape,
                      output_logit_vec, output_logit_vec_shape);
        }
    };

    struct AddTrainingDataResponse
    {
        projection_aid_subsystem::exception_t result;
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

    struct ClearTrainingDataRequest
    {
        uint64_t client_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id);
        }
    };

    struct ClearTrainingDataResponse
    {
        projection_aid_subsystem::exception_t result;
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

    struct SetMatrixResourceRequest
    {
        uint64_t client_id;
        std::vector<generic_deviation_projector_factory::GenericDeviationProjectorResource> matrix_resource_vec;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id, matrix_resource_vec);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id, matrix_resource_vec);
        }
    };

    struct SetMatrixResourceResponse
    {
        projection_aid_subsystem::exception_t result;
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

    struct GetDeviationRequest
    {
        uint64_t client_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id);
        }
    };

    struct GetDeviationResponse
    {
        std::expected<mdc_float_t, projection_aid_subsystem::exception_t> result;
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

    //we only enforce one thing, timestamp to do system-calibration, such is that after the timeout + 1 packet request time, we'd be back to calibration

    class RequestTrafficController: public virtual internal_rest_controller::RequestFiltererInterface
    {
        private:

            std::chrono::nanoseconds timeout;

        public:

            RequestTrafficController(std::chrono::nanoseconds timeout): timeout(timeout){}

            auto thru(const internal_rest_controller::Request& request) -> internal_rest_controller::exception_t
            {
                std::chrono::time_point<std::chrono::utc_clock> bar = std::chrono::utc_clock::now() - this->timeout;

                if (request.since < bar)
                {
                    return internal_rest_controller::SUCCESS;                    
                }

                return internal_rest_controller::TIMEOUT;
            }
    };

    class OpenClientResolver: public virtual internal_rest_controller::ResolvableInterface
    {
        private:

            std::shared_ptr<SelfObservedClientManager> client_manager;

        public:

            OpenClientResolver(std::shared_ptr<SelfObservedClientManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto resolve(const internal_rest_controller::Request& request) -> internal_rest_controller::Response
            {
                if (request.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                OpenClientRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<OpenClientRequest>(request.content);
                OpenClientResponse semantic_response;

                try
                {
                    uint64_t client_box_id = this->client_manager->open_client_box(semantic_request.connection_config);
                    
                    semantic_response = OpenClientResponse
                    {
                        .result = client_box_id,
                        .err_verbal_description = ""
                    };
                }
                catch (const std::invalid_argument& e)
                {
                    semantic_response = OpenClientResponse
                    {
                        .result = std::unexpected(INVALID_ARGUMENT_ERROR_CODE),
                        .err_verbal_description = std::string(e.what())
                    };
                }
                catch (const std::exception& e)
                {
                    semantic_response = OpenClientResponse
                    {
                        .result = std::unexpected(INTERNAL_SERVER_ERROR_CODE),
                        .err_verbal_description = std::string(e.what())
                    };
                }
                
                return internal_rest_controller::Response
                {
                    .content = dg::network_compact_serializer::dgstd_serialize<std::string>(semantic_response),
                    .serialization_kind = dg::network_compact_serializer::get_dgstd_serialization_identifier()
                };
            }
    };

    class CloseClientResolver: public virtual internal_rest_controller::ResolvableInterface
    {
        private:

            std::shared_ptr<SelfObservedClientManager> client_manager;

        public:

            CloseClientResolver(std::shared_ptr<SelfObservedClientManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto resolve(const internal_rest_controller::Request& request) -> internal_rest_controller::Response
            {
                if (request.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                CloseClientRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<CloseClientRequest>(request.content);
                this->client_manager->close_client_box(semantic_request.client_id);
                CloseClientResponse semantic_response
                {
                    .result = SUCCESS,
                    .err_verbal_description = ""
                };

                return internal_rest_controller::Response
                {
                    .content = dg::network_compact_serializer::dgstd_serialize<std::string>(semantic_response),
                    .serialization_kind = dg::network_compact_serializer::get_dgstd_serialization_identifier()
                };
            }
    };

    class AddTrainingDataResolver: public virtual internal_rest_controller::ResolvableInterface
    {
        private:

            std::shared_ptr<SelfObservedClientManager> client_manager;

        public:

            AddTrainingDataResolver(std::shared_ptr<SelfObservedClientManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto resolve(const internal_rest_controller::Request& request) -> internal_rest_controller::Response
            {
                if (request.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                AddTrainingDataRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<AddTrainingDataRequest>(request.content);
                AddTrainingDataResponse semantic_response;

                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(semantic_request.client_id);

                if (client_box == nullptr)
                {
                    semantic_response = AddTrainingDataResponse
                    {
                        .result = CLIENT_NOT_FOUND_ERROR_CODE,
                        .err_verbal_description = "client not found"
                    };
                }
                else
                {
                    try
                    {
                        std::shared_ptr<tensor_model::Matrix> inp = tensor_matrix_operation::make_matrix_from_flat_vec(stdx::to_castable_vector_initializer(semantic_request.input_logit_vec_shape),
                                                                                                                       semantic_request.input_logit_vec);

                        std::shared_ptr<tensor_model::Matrix> out = tensor_matrix_operation::make_matrix_from_flat_vec(stdx::to_castable_vector_initializer(semantic_request.output_logit_vec_shape),
                                                                                                                       semantic_request.output_logit_vec);

                        client_box->add_training_data(inp, out);

                        semantic_Response = AddTrainingDataResponse
                        {
                            .result = SUCCESS,
                            .err_verbal_description = ""
                        };
                    }
                    catch (const std::invalid_argument& e)
                    {
                        semantic_response = AddTrainingDataResponse
                        {
                            .result = INVALID_ARGUMENT_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                    catch (const std::exception& e)
                    {
                        semantic_response = AddTrainingDataResponse
                        {
                            .result = RUNTIME_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                }

                return internal_rest_controller::Response
                {
                    .content = dg::network_compact_serializer::dgstd_serialize<std::string>(semantic_response),
                    .serialization_kind = dg::network_compact_serializer::get_dgstd_serialization_identifier()
                };
            }
    };

    class ClearTrainingDataResolver: public virtual internal_rest_controller::ResolvableInterface
    {
        private:

            std::shared_ptr<SelfObservedClientManager> client_manager;
        
        public:

            ClearTrainingDataResolver(std::shared_ptr<SelfObservedClientManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto resolve(const internal_rest_controller::Request& request) -> internal_rest_controller::Response
            {
                if (request.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                ClearTrainingDataRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<ClearTrainingDataRequest>(request.content);
                ClearTrainingDataResponse semantic_response;

                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(semantic_request.client_id);

                if (client_box == nullptr)
                {
                    semantic_response = ClearTrainingDataResponse
                    {
                        .result = CLIENT_NOT_FOUND_ERROR_CODE,
                        .err_verbal_description = "client not found"
                    };
                }
                else
                {
                    try
                    {
                        client_box->clear_training_data();

                        semantic_response = ClearTrainingDataResponse
                        {
                            .result = SUCCESS,
                            .err_verbal_description = ""
                        };
                    }
                    catch (const std::invalid_argument& e)
                    {
                        semantic_response = ClearTrainingDataResponse
                        {
                            .result = INVALID_ARGUMENT_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                    catch (const std::exception& e)
                    {
                        semantic_response = ClearTrainingDataResponse
                        {
                            .result = RUNTIME_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                }

                return internal_rest_controller::Repsonse
                {
                    .content = dg::network_compact_serializer::dgstd_serialize<std::string>(semantic_response),
                    .serialization_kind = dg::network_compact_serializer::get_dgstd_serialization_identifier()
                };
            }
    };

    class SetMatrixResourceResolver: public virtual internal_rest_controller::ResolvableInterface
    {
        private:

            std::shared_ptr<SelfObservedClientManager> client_manager;

        public:

            SetMatrixResourceResolver(std::shared_ptr<SelfObservedClientManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto resolve(const internal_rest_controller::Request& request) -> internal_rest_controller::Response
            {
                if (request.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                SetMatrixResourceRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<SetMatrixResourceRequest>(request.content);
                SetMatrixResourceResponse semantic_response;

                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(semantic_request.client_id);

                if (client_box == nullptr)
                {
                    semantic_response = SetMatrixResourceResponse
                    {
                        .result = CLIENT_NOT_FOUND_ERROR_CODE,
                        .err_verbal_description = "client not found"
                    };
                }
                else
                {
                    try
                    {
                        client_box->set_matrix_resource(semantic_request.matrix_resource_vec);

                        semantic_response = SetMatrixResourceResponse
                        {
                            .result = SUCCESS,
                            .err_verbal_description = ""
                        };
                    }
                    catch (const std::invalid_argument& e)
                    {
                        semantic_response = SetMatrixResourceResponse
                        {
                            .result = INVALID_ARGUMENT_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                    catch (const std::exception& e)
                    {
                        semantic_response = SetMatrixResourceResponse
                        {
                            .result = RUNTIME_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())  
                        };
                    }
                }

                return internal_rest_controller::Response
                {
                    .content = dg::network_compact_serializer::dgstd_serialize<std::string>(semantic_response),
                    .serialization_kind = dg::network_compact_serializer::get_dgstd_serialization_identifier()
                };
            }
    };

    class GetDeviationResolver: public virtual internal_rest_controller::ResolvableInterface
    {
        private:

            std::shared_ptr<SelfObservedClientManager> client_manager;
        
        public:

            GetDeviationResolver(std::shared_ptr<SelfObservedClientManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto resolve(const internal_rest_controller::Request& request) -> internal_rest_controller::Response
            {
                if (request.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                GetDeviationRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<GetDeviationRequest>(request.content);
                GetDeviationResponse semantic_response;

                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(semantic_request.client_id);

                if (client_box == nullptr)
                {
                    semantic_response = GetDeviationResponse
                    {
                        .result = std::unexpected(CLIENT_NOT_FOUND_ERROR_CODE),
                        .err_verbal_description = "client not found"
                    };
                }
                else
                {
                    try
                    {
                        semantic_response = GetDeviationResponse
                        {
                            .result = client_box->get(),
                            .err_verbal_description = ""
                        };
                    }
                    catch (const std::invalid_argument& e)
                    {
                        semantic_response = GetDeviationResponse
                        {
                            .result = std::unexpected(INVALID_ARGUMENT_ERROR_CODE),
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                    catch (const std::exception& e)
                    {
                        semantic_response = GetDeviationResponse
                        {
                            .result = std::unexpected(RUNTIME_ERROR_CODE),
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                }

                return internal_rest_controller::Response
                {
                    .content = dg::network_compact_serializer::dgstd_serialize<std::string>(semantic_response),
                    .serialization_kind = dg::network_compact_serializer::get_dgstd_serialization_identifier()
                };
            }
    };

    static inline const std::chrono::nanoseconds REQUEST_PROCESS_WINDOW = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(1));

    void init()
    {
        std::shared_ptr<SelfObservedClientManager> client_manager = std::make_shared<SelfObservedClientManager>();

        internal_rest_controller::hook(RestConfiguration::get_open_client_url(), std::make_unique<OpenClientResolver>(client_manager));
        internal_rest_controller::bind_filter(RestConfiguration::get_open_client_url(), std::make_unique<RequestTrafficController>(REQUEST_PROCESS_WINDOW));

        internal_rest_controller::hook(RestConfiguration::get_close_client_url(), std::make_unique<CloseClientResolver>(client_manager));
        internal_rest_controller::bind_filter(RestConfiguration::get_close_client_url(), std::make_unique<RequestTrafficController>(REQUEST_PROCESS_WINDOW));

        internal_rest_controller::hook(RestConfiguration::get_add_training_data_url(), std::make_unique<AddTrainingDataResolver>(client_manager));
        internal_rest_controller::bind_filter(RestConfiguration::get_add_training_data_url(), std::make_unique<RequestTrafficController>(REQUEST_PROCESS_WINDOW));

        internal_rest_controller::hook(RestConfiguration::get_set_matrix_resource_url(), std::make_unique<SetMatrixResourceResolver>(client_manager));
        internal_rest_controller::bind_filter(RestConfiguration::get_set_matrix_resource_url(), std::make_unique<RequestTrafficController>(REQUEST_PROCESS_WINDOW));

        internal_rest_controller::hook(RestConfiguration::get_set_deviation_calculator_url(), std::make_unique<SetDeviationCalculatorResolver>(client_manager));
        internal_rest_controller::bind_filter(RestConfiguration::get_set_deviation_calculator_url(), std::make_unique<RequestTrafficController>(REQUEST_PROCESS_WINDOW));

        internal_rest_controller::hook(RestConfiguration::get_get_deviation_url(), std::make_unique<GetDeviationResolver>(client_manager));
        internal_rest_controller::bind_filter(RestConfiguration::get_get_deviation_url(), std::make_unique<RequestTrafficController>(REQUEST_PROCESS_WINDOW));
    }

    void deinit() noexcept
    {
        internal_rest_controller::unhook(RestConfiguration::get_get_deviation_url());
        internal_rest_controller::unhook(RestConfiguration::get_set_deviation_calculator_url());
        internal_rest_controller::unhook(RestConfiguration::get_set_matrix_resource_url());
        internal_rest_controller::unhook(RestConfiguration::get_add_training_data_url());
        internal_rest_controller::unhook(RestConfiguration::get_close_client_url());
        internal_rest_controller::unhook(RestConfiguration::get_open_client_url());
    }
}

#endif
#ifndef __PROJECTION_AID_SUBSYSTEM_H__
#define __PROJECTION_AID_SUBSYSTEM_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include "float_def.h"

namespace projection_aid_subsystem
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
            std::vector<generic_matrix_factory::GenericMatrixResource> matrix_resource_vec;
            std::unique_ptr<matrix_deviation_calculator::MatrixDeviationCalculatorInterface> deviation_calculator;

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

            void set_matrix_resource(const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec)
            {
                this->matrix_resource_vec = matrix_resource_vec;
            }
            
            void set_deviation_calculator(generic_matrix_deviation_calculator_factory::matrix_deviation_calculator_t kind)
            {
                this->deviation_calculator = generic_matrix_deviation_calculator_factory::Factory::get_from_generic_codex(kind);
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
                    std::unique_ptr<the_matrix::MatrixInterface> the_matrix = this->wrap_matrix(generic_matrix_factory::GenericMatrixLoader{}.load_resource(matrix_resource));

                    std::vector<std::shared_ptr<tensor_model::Matrix>> inp_vec{};
                    std::vector<std::shared_ptr<tensor_model::Matrix>> out_vec{};
                    std::vector<std::shared_ptr<tensor_model::Matrix>> expected_vec{};

                    for (const auto& [inp, out]: this->training_data)
                    {
                        inp_vec.push_back(inp);
                        expected_vec.push_back(out);
                    }

                    out_vec = the_matrix->project(inp_vec);

                    rs_vec.push_back(this->deviation_calculator->get_deviation(stdx::zip(out_vec, expected_vec)));
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

            void set_matrix_resource(const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed)
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                this->base.set_matrix_resource(matrix_resource_vec);
            }

            void set_deviation_calculator(generic_matrix_deviation_calculator_factory::matrix_deviation_calculator_t kind)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed)
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                this->base.set_deviation_calculator(kind);
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

    //I was thinking about a relay system, for master to talk to masters, build a heap of communication, essentially to increase bandwidth + reduce latency of the broadcasting matrices (its only 1 or 2 MB)
    //it's complicated how we should build this
    //we'll solve that tomorrow, I think that is two_distinct_responsibilities, but not sure of the reduction process, it's somewhat coupled into this projection aid subssytem, so it's best to just ... solve it all right here

    //I have spent 1 year to tune the REST controller to 10GB/s, it's complicated
    //but it's actually about non-waiting and reactive response
    //the latency is probably ~10-20ms but we'd want to shorten the tree of requests tmr, which we'd discuss in depth
    //I dont really want to talk about who owns what, what is in filesystem or RAM
    //it's a filesystem and a management system that is out of the scope of this project
    //we'd try to fix the session and the connectivity instead of being paranoid about things, worst case, save it and re-ingest the data from files

    //I think it's better to just ... have another subsystem to build a tree of hierarchy where we'd want for the immediate leaf node to manage the leaf node, which means that only the immediate would open the APIClient_2
    //so our model is master_2 -> master_2
    //                master_2 -> master_1 (master_1 holds client_box)
    //it's settled, we'd implement that today

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
        std::vector<generic_matrix_factory::GenericMatrixResource> matrix_resource_vec;

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

    struct SetDeviationCalculatorRequest
    {
        uint64_t client_id;
        generic_matrix_deviation_calculator_factory::matrix_deviation_calculator_t kind;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id, kind);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id, kind);
        }
    };

    struct SetDeviationCalculatorResponse
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

    class SetDeviationCalculatorResolver: public virtual internal_rest_controller::ResolvableInterface
    {
        private:

            std::shared_ptr<SelfObservedClientManager> client_manager;

        public:

            SetDeviationCalculatorResolver(std::shared_ptr<SelfObservedClientManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto resolve(const internal_rest_controller::Request& request) -> internal_rest_controller::Response
            {
                if (request.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                SetDeviationCalculatorRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<SetDeviationCalculatorRequest>(request.content);
                SetDeviationCalculatorResponse semantic_response;

                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(semantic_request.client_id);

                if (client_box == nullptr)
                {
                    semantic_response = SetDeviationCalculatorResponse
                    {
                        .result = CLIENT_NOT_FOUND_ERROR_CODE,
                        .err_verbal_description = "client not found"
                    };
                }
                else
                {
                    try
                    {
                        client_box->set_deviation_calculator(semantic_request.kind);

                        semantic_response = SetDeviationCalculatorResponse
                        {
                            .result = SUCCESS,
                            .err_verbal_description = ""
                        };
                    }
                    catch (const std::invalid_argument& e)
                    {
                        semantic_response = SetDeviationCalculatorResponse
                        {
                            .result = INVALID_ARGUMENT_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                    catch (const std::exception& e)
                    {
                        semantic_response = SetDeviationCalculatorResponse
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

    void init()
    {
        ClientManagerSingleton::initialize();

        internal_rest_controller::hook(RestConfiguration::get_open_client_url(), std::make_unique<OpenClientResolver>(ClientManagerSingleton::get()));
        internal_rest_controller::hook(RestConfiguration::get_close_client_url(), std::make_unique<CloseClientResolver>(ClientManagerSingleton::get()));
        internal_rest_controller::hook(RestConfiguration::get_add_training_data_url(), std::make_unique<AddTrainingDataResolver>(ClientManagerSingleton::get()));
        internal_rest_controller::hook(RestConfiguration::get_set_matrix_resource_resolver_url(), std::make_unique<SetMatrixResourceResolver>(ClientManagerSingleton::get()));
        internal_rest_controller::hook(RestConfiguration::get_set_deviation_calculator_resolver_url(), std::make_unique<SetDeviationCalculatorResolver>(ClientManagerSingleton::get()));
        internal_rest_controller::hook(RestConfiguration::get_get_deviation_resolver_url(), std::make_unique<GetDeviationResolver>(ClientManagerSingleton::get()));
    }

    void deinit() noexcept
    {
        internal_rest_controller::unhook(RestConfiguration::get_get_deviation_resolver_url());
        internal_rest_controller::unhook(RestConfiguration::get_set_deviation_calculator_resolver_url());
        internal_rest_controller::unhook(RestConfiguration::get_set_matrix_resource_resolver_url());
        internal_rest_controller::unhook(RestConfiguration::get_add_training_data_url());
        internal_rest_controller::unhook(RestConfiguration::get_close_client_url());
        internal_rest_controller::unhook(RestConfiguration::get_open_client_url());

        ClientManagerSingleton::deinitialize();
    }

    //----

    class APIClient
    {
        private:

            internal_rest_controller::RequestClient client;

        public:

            APIClient(): client(){}

            void set_retry_policy(std::unique_ptr<internal_rest_controller::RetryMachineInterface>&& retry_machine)
            {
                this->client.set_retry_policy(std::move(retry_machine));
            }

            auto open_client_box(const Remote& remote,
                                 const connectivity_subsystem::SlaveConnectionConfiguration& connection_config) -> uint64_t
            {
                internal_rest_controller::Url url = RestConfiguration::get_open_client_url(remote);
                OpenClientRequest request
                {
                    .connection_config = connection_config
                };

                std::string request_payload                         = dg::network_compact_serializer::dgstd_serialize<std::string>(request);
                internal_rest_controller::ClientRequest request     = internal_rest_controller::RequestFactory{}.url(url)
                                                                                                                .payload(request_payload)
                                                                                                                .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                                                                                .get();

                internal_rest_controller::ClientResponse response   = this->client.set_request(request).get();

                if (response.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::runtime_error("unexpected serialization format");
                }

                OpenClientResponse semantic_response = dg::network_compact_serializer::dgstd_deserialize<OpenClientResponse>(response.content);

                if (!semantic_response.result.has_value())
                {
                    if (semantic_response.result.error() == INVALID_ARGUMENT_ERROR_CODE)
                    {
                        throw std::invalid_argument(semantic_response.err_verbal_description);
                    }

                    throw std::runtime_error(semantic_response.err_verbal_description);
                }
            }

            void close_client_box(const Remote& remote, uint64_t client_id)
            {
                internal_rest_controller::Url url = RestConfiguration::get_close_client_url(remote);
                CloseClientRequest request
                {
                    .client_id = client_id
                };

                std::string request_payload                         = dg::network_compact_serializer::dgstd_serialize<std::string>(request);
                internal_rest_controller::ClientRequest request     = internal_rest_controller::RequestFactory{}.url(url)
                                                                                                                .payload(request_payload)
                                                                                                                .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                                                                                .get();
            
                internal_rest_controller::ClientResponse response   = this->client.set_request(request).get();

                if (response.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::runtime_error("unexpected serialization format");
                }

                CloseClientResponse semantic_response = dg::network_compact_serializer::dgstd_deserialize<CloseClientResponse>(response.content);

                if (semantic_response.result != SUCCESS)
                {
                    if (semantic_response.result == INVALID_ARGUMENT_ERROR_CODE)
                    {
                        throw std::invalid_argument(semantic_response.err_verbal_description);
                    }

                    throw std::runtime_error(semantic_response.err_verbal_description);
                }
            }

            auto add_training_data_2(const Remote& remote, uint64_t client_id,
                                     const std::shared_ptr<tensor_model::Matrix>& inp, const std::shared_ptr<tensor_model::Matrix>& out) -> std::unique_ptr<internal_rest_controller::Promise<void>>
            {
                internal_rest_controller::Url url = RestConfiguration::get_add_training_data_url(remote);
                AddTrainingDataRequest request
                {
                    .client_id                  = client_id,
                    .input_logit_vec            = this->matrix_to_flat_logit_vec(inp),
                    .input_logit_vec_shape      = this->get_matrix_shape(inp),
                    .output_logit_vec           = this->matrix_to_flat_logit_vec(out),
                    .output_logit_vec_shape     = this->get_matrix_shape(out)
                };

                std::string request_payload                         = dg::network_compact_serializer::dgstd_serialize<std::string>(request);
                internal_rest_controller::ClientRequest request     = internal_rest_controller::RequestFactory{}.url(url)
                                                                                                                .payload(request_payload)
                                                                                                                .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                                                                                .get();

                auto resolutor = [](const internal_rest_controller::ClientResponse& response)
                {
                    if (response.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                    {
                        throw std::runtime_error("unexpected serialization format");
                    }

                    AddTrainingDataResponse semantic_response = dg::network_compact_serializer::dgstd_deserialize<AddTrainingDataResponse>(response.content);

                    if (semantic_response.result != SUCCESS)
                    {
                        if (semantic_response.result == INVALID_ARGUMENT_ERROR_CODE)
                        {
                            throw std::invalid_argument(semantic_response.err_verbal_description);
                        }

                        throw std::runtime_error(semantic_response.err_verbal_description);
                    }
                };

                return this->client.set_request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }

            void add_training_data(const Remote& remote, uint64_t client_id,
                                   const std::shared_ptr<tensor_model::Matrix>& inp, const std::shared_ptr<tensor_model::Matrix>& out)
            {
                this->add_training_data_2(remote, client_id, inp, out)->wait();
            }

            void clear_training_data(const Remote& remote, uint64_t client_id)
            {
                internal_rest_controller::Url url = RestConfiguration::get_clear_training_data_url(remote);
                ClearTrainingDataRequest request
                {
                    .client_id = client_id
                };
                std::string request_payload                         = dg::network_compact_serializer::dgstd_serialize<std::string>(request);
                internal_rest_controller::ClientRequest request     = internal_rest_controller::RequestFactory{}.url(url)
                                                                                                                .payload(request_payload)
                                                                                                                .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                                                                                .get();

                internal_rest_controller::ClientResponse response   = this->client.set_request(request).get();
                
                if (response.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::runtime_error("unexpected serialization format");
                }

                ClearTrainingDataResponse semantic_response = dg::network_compact_serializer::dgstd_deserialize<ClearTrainingDataResponse>(response.content);

                if (semantic_response.result != SUCCESS)
                {
                    if (semantic_response.result == INVALID_ARGUMENT_ERROR_CODE)
                    {
                        throw std::invalid_argument(semantic_response.err_verbal_description);
                    }

                    throw std::runtime_error(semantic_response.err_verbal_description);
                }
            }

            void set_matrix_resource_2(const Remote& remote, uint64_t client_id,
                                       const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec) -> std::unique_ptr<internal_rest_controller::Promise<void>>
            {
                internal_rest_controller::Url url = RestConfiguration::get_set_matrix_resource_url(remote);
                SetMatrixResourceRequest request
                {
                    .client_id = client_id,
                    .matrix_resource_vec = matrix_resource_vec
                };

                std::string request_payload                         = dg::network_compact_serializer::dgstd_serialize<std::string>(request);
                internal_rest_controller::ClientRequest request     = internal_rest_controller::RequestFactory{}.url(url)
                                                                                                                .payload(request_payload)
                                                                                                                .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                                                                                .get();
                
                auto resolutor = [](const internal_rest_controller::ClientResponse& response)
                {
                    if (response.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                    {
                        throw std::runtime_error("unexpected serialization format");
                    }

                    SetMatrixResourceResponse semantic_response = dg::network_compact_serializer::dgstd_deserialize<SetMatrixResourceResponse>(response.content);

                    if (semantic_response.result != SUCCESS)
                    {
                        if (semantic_response.result == INVALID_ARGUMENT_ERROR_CODE)
                        {
                            throw std::invalid_argument(semantic_response.err_verbal_description);
                        }

                        throw std::runtime_error(semantic_response.err_verbal_description);
                    }
                };

                return this->client.set_request(request)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }

            void set_matrix_resource(const Remote& remote, uint64_t client_id,
                                     const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec)
            {
                this->set_matrix_resource_2(remote, client_id, matrix_resource_vec)->wait();
            }

            void set_deviation_calculator(const Remote& remote, uint64_t client_id,
                                          generic_matrix_deviation_calculator_factory::matrix_deviation_calculator_t kind)
            {
                internal_rest_controller::Url url = RestConfiguration::get_set_deviation_calculator_url(remote);
                SetDeviationCalculatorRequest request
                {
                    .client_id  = client_id,
                    .kind       = kind
                };
                std::string request_payload                         = dg::network_compact_serializer::dgstd_serialize<std::string>(request);
                internal_rest_controller::ClientRequest request     = internal_rest_controller::RequestFactory{}.url(url)
                                                                                                                .payload(request_payload)
                                                                                                                .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                                                                                .get();

                internal_rest_controller::ClientResponse response   = this->client.set_request(request).get();

                if (response.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::runtime_error("unexpected serialization format");
                }

                SetDeviationCalculatorResponse semantic_response = dg::network_compact_serializer::dgstd_deserialize<SetDeviationCalculatorResponse>(response.content);

                if (semantic_response.result != SUCCESS)
                {
                    if (semantic_response.result == INVALID_ARGUMENT_ERROR_CODE)
                    {
                        throw std::invalid_argument(semantic_response.err_verbal_description);
                    }

                    throw std::runtime_error(semantic_response.err_verbal_description);
                }
            }

            auto get(const Remote& remote, uint64_t client_id) -> std::vector<mdc_float_t>
            {
                internal_rest_controller::Url url = RestConfiguration::get_get_deviation_url(remote);
                GetDeviationRequest request
                {
                    .client_id = client_id
                };
                std::string request_payload                         = dg::network_compact_serializer::dgstd_serialize<std::string>(request);
                internal_rest_controller::ClientRequest request     = internal_rest_controller::RequestFactory{}.url(url)
                                                                                                                .payload(request_payload)
                                                                                                                .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                                                                                .get();

                internal_rest_controller::ClientResponse response   = this->client.set_request(request).get();

                if (response.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::runtime_error("unexpected serialization format");
                }

                GetDeviationResponse semantic_response = dg::network_compact_serializer::dgstd_deserialize<GetDeviationResponse>(response.content);

                if (!semantic_response.result.has_value())
                {
                    if (semantic_repsonse.result.error() == INVALID_ARGUMENT_ERROR_CODE)
                    {
                        throw std::invalid_argument(semantic_repsonse.err_verbal_description);
                    }

                    throw std::runtime_error(semantic_response.err_verbal_description);
                }

                return std::move(semantic_response.result.value());
            }
    };

    class APIClient_2
    {
        private:

            APIClient base;
            std::unique_ptr<connectivity_subsystem::ConnectionInterface> connection;
            bool was_explicitly_closed;
            Remote remote;
            uint64_t client_id;

        public:

            APIClient_2(const Remote& remote,
                        const connectivity_subsystem::MasterConfiguration& config)
            {
                this->base                  = APIClient();

                auto tmp_config             = config;
                tmp_config.slave_count      = 1u;

                std::unique_ptr<connectivity_subsystem::MasterConnection connection = std::make_unique<connectivity_subsystem::MasterConnection>(tmp_config);

                this->client_id             = this->base.open_client_box(remote, connection->get_slave_configuration());
                this->connection            = std::move(connection);
                this->was_explicitly_closed = false;
                this->remote                = remote;
            }

            ~APIClient_2() noexcept
            {
                this->close();
            }

            auto set_retry_policy(std::unique_ptr<internal_rest_controller::RetryMachineInterface>&& retry_machine) -> APIClient_2&
            {
                if (!this->can_operate())
                {
                    throw std::runtime_error("corrupted client, client is in inoperable state");
                }

                this->base.set_retry_policy(std::move(retry_machine));

                return *this;
            }

            void add_training_data(const std::shared_ptr<tensor_model::Matrix>& inp, const std::shared_ptr<tensor_model::Matrix>& out)
            {
                if (!this->can_operate())
                {
                    throw std::runtime_error("corrupted client, client is in inoperable state");
                }

                this->base.add_training_data(this->remote, this->client_id, inp, out);
            }

            auto add_training_data_2(const std::shared_ptr<tensor_model::Matrix>& inp, const std::shared_ptr<tensor_model::Matrix>& out) -> std::unique_ptr<internal_rest_controller::Promise<void>>
            {
                if (!this->can_operate())
                {
                    throw std::runtime_error("corrupted client, client is in inoperable state");
                }

                return this->base.add_training_data_2(this->remote, this->client_id, inp, out);
            }

            void clear_training_data()
            {
                if (!this->can_operate())
                {
                    throw std::runtime_error("corrupted client, client is in inoperable state");
                }

                this->base.clear_training_data(this->remote, this->client_id);
            }

            void set_matrix_resource(const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec)
            {
                if (!this->can_operate())
                {
                    throw std::runtime_error("corrupted client, client is in inoperable state");
                }

                this->base.set_matrix_resource(this->remote, this->client_id, matrix_resource_vec);
            }

            auto set_matrix_resource_2(const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec) -> std::unique_ptr<internal_rest_controller::Promise<void>>
            {
                if (!this->can_operate())
                {
                    throw std::runtime_error("corrupted client, client is in inoperable state");
                }

                return this->base.set_matrix_resource_2(matrix_resource_vec);
            }

            void set_deviation_calculator(generic_matrix_deviation_calculator_factory::matrix_deviation_calculator_t kind)
            {
                if (!this->can_operate())
                {
                    throw std::runtime_error("corrupted client, client is in inoperable state");
                }

                this->base.set_deviation_calculator(this->remote, this->client_id, this->kind);
            }

            auto get() -> std::vector<mdc_float_t>
            {
                if (!this->can_operate())
                {
                    throw std::runtime_error("corrupted client, client is in inoperable state");
                }

                return this->base.get(this->remote, this->client_id);
            }

            void close() noexcept
            {
                if (std::exchange(this->was_explicitly_closed, true))
                {
                    return;
                }

                try
                {
                    this->base.close_client_box(this->remote, this->client_id);
                }
                catch (...){}

                this->connection->close();
            }

        private:
            
            auto can_operate() -> bool
            {
                if (!this->connection->is_alive())
                {
                    return false;
                }

                if (this->was_explicitly_closed)
                {
                    return false;
                }

                return true;
            }
    };

    class ThreadSafe_APIClient_2
    {
        private:

            APIClient_2 base;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

        public:

            ThreadSafe_APIClient_2(const Remote& remote,
                                   const connectivity_subsystem::MasterConfiguration& config): base(remote, config)
                                                                                               mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            auto set_retry_policy(std::unique_ptr<internal_rest_controller::RetryMachineInterface>&& retry_machine) -> ThreadSafe_APIClient_2&
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
                this->base.set_retry_policy(std::move(retry_machine));

                return *this;
            }

            void add_training_data(const std::shared_ptr<tensor_model::Matrix>& inp, const std::shared_ptr<tensor_model::Matrix>& out)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
                this->base.add_training_data(inp, out);
            }

            auto add_training_data_2(const std::shared_ptr<tensor_model::Matrix>& inp, const std::shared_ptr<tensor_model::Matrix>& out) -> std::unique_ptr<internal_rest_controller::Promise<void>>
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                return this->base.add_training_data_2(inp, out);
            }

            void clear_training_data()
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
                this->base.clear_training_data();
            }

            void set_matrix_resource(const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
                this->base.set_matrix_reosurce(matrix_resource_vec);
            }

            auto set_matrix_resource_2(const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec) -> std::unique_ptr<internal_rest_controller::Promise<void>>
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                return this->base.set_matrix_resource_2(matrix_resource_vec);
            }

            void set_deviation_calculator(generic_matrix_deviation_calculator_factory::matrix_deviation_calculator_t kind)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
                this->base.set_deviation_calculator(kind);
            }

            auto get() -> std::vector<mdc_float_t>
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                return this->base.get();
            }

            void close() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
                this->base.close();
            }
    };

    class TrainingDataIterableInterface
    {
        public:

            virtual ~TrainingDataIterableInterface() noexcept = default;

            virtual auto next() -> std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>> = 0;
            virtual auto has_next() -> bool = 0;
    };

    class ThreadSafeTrainingDataIterableWrapper: public virtual TrainingDataIterableInterface
    {
        private:

            std::unique_ptr<TrainingDataIterableInterface> base;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
        
        public:

            ThreadSafeTrainingDataIterableWrapper(std::unique_ptr<TrainingDataIterableInterface>&& base)
            {
                if (base == nullptr)
                {
                    throw std::invalid_argument("bad base, null");
                }

                this->base  = std::move(base);
                this->mtx   = fair_mutex::make_unique_fair_atomic_flag();
            }

            auto next() -> std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                return this->base->next();
            }

            auto has_next() -> bool
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                return this->base->has_next();
            }
    };

    class UniformDataIngestor
    {
        private:

            struct IngestionExceptionContainer
            {
                std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
                std::exception_ptr exception;
            };

            std::shared_ptr<TrainingDataIterableInterface> training_data;
            std::vector<std::shared_ptr<ThreadSafe_APIClient_2>> client_vec;
            size_t pipe_sz;
            std::shared_ptr<std::atomic<bool>> is_completed;
            std::shared_ptr<IngestionExceptionContainer> ingestion_exception_container;
            bool is_run;
            std::shared_ptr<std::atomic<bool>> is_interrupted;
            bool is_waited;

        public:

            UniformDataIngestor(): training_data(),
                                   client_vec(),
                                   pipe_sz(0u),
                                   is_completed(std::make_shared<std::atomic<bool>>(false)),
                                   ingestion_exception_container(std::make_shared<IngestionExceptionContainer>(IngestionExceptionContainer{.mtx = fair_mutex::make_unique_fair_atomic_flag(),. exception = nullptr})),
                                   is_run(false),
                                   is_interrupted(std::make_shared<std::atomic<bool>>(false)),
                                   is_waited(false){}

            auto set_data_source(std::unique_ptr<TrainingDataIterableInterface>&& training_data) -> UniformDataIngestor&
            {
                if (this->is_run)
                {
                    throw std::runtime_error("invalid operation, run process has been invoked");
                }

                if (training_data == nullptr)
                {
                    throw std::invalid_argument("bad training data iterable, null");
                }

                this->training_data = std::make_unique<ThreadSafeTrainingDataIterableWrapper>(std::move(training_data));

                return *this;
            }

            auto set_client_vector(const std::vector<std::shared_ptr<ThreadSafe_APIClient_2>>& client_vec) -> UniformDataIngestor&
            {
                if (this->is_run)
                {
                    throw std::runtime_error("invalid operation, run process has been invoked");
                }

                for (const auto& client: client_vec)
                {
                    if (client == nullptr)
                    {
                        throw std::invalid_argument("bad client, null");
                    }
                }

                this->client_vec = client_vec;

                return *this;
            }

            auto set_pipe_size(size_t pipe_sz) -> UniformDataIngestor&
            {
                if (this->is_run)
                {
                    throw std::runtime_error("invalid operation, run process has been invoked");
                }

                if (pipe_sz == 0u)
                {
                    throw std::invalid_argument("bad pipe size, 0");
                }

                this->pipe_sz = pipe_sz;

                return *this;
            }

            void interrupt() noexcept
            {
                this->is_interrupted->exchange(true, std::memory_order_relaxed);
            }

            auto is_completed() noexcept -> bool
            {
                return this->is_completed->load(std::memory_order_relaxed);
            }

            auto wait()
            {
                if (!this->is_run)
                {
                    throw std::runtime_error("invalid operation, run process was not invoked");
                }

                if (std::exchange(this->is_waited, true))
                {
                    return;
                }

                if (this->is_completed->load(std::memory_order_relaxed))
                {
                    this->throw_error();
                    return;
                }

                this->is_completed->wait(false, std::memory_order_relaxed);
                this->throw_error();
            }

            void run()
            {
                this->check_and_throw_run_requirements();

                coroutine_x::run_detached(std::make_unique<CoroutineRunner>(this->training_data,
                                                                            this->client_vec,
                                                                            this->pipe_sz,
                                                                            this->is_completed,
                                                                            this->ingestion_exception_container,
                                                                            this->is_interrupted),
                                          coroutine_x::NETWORK_COROUTINE);

                this->is_run = true;
            }

        private:

            void check_and_throw_run_requirements()
            {
                if (this->is_run)
                {
                    throw std::runtime_error("invalid operation, run process has been invoked");
                }

                if (this->training_data == nullptr)
                {
                    throw std::invalid_argument("bad argument, training data was not set");
                }

                if (this->client_vec.empty())
                {
                    throw std::invalid_argument("bad argument, empty client");
                }

                if (this->pipe_sz == 0u)
                {
                    throw std::invalid_argument("bad argument, pipe size 0");
                }
            }

            void throw_error()
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->ingestion_exception_container->mtx);

                std::rethrow_exception(this->ingestion_exception_container->exception);
            }

            class CoroutineRunner: public virtual coroutine_x::CoroutineableInterface
            {
                private:

                    std::shared_ptr<TrainingDataIterableInterface> training_data;
                    std::vector<std::shared_ptr<ThreadSafe_APIClient_2>> client_vec;
                    size_t pipe_sz;
                    std::shared_ptr<std::atomic<bool>> is_completed;
                    std::shared_ptr<IngestionExceptionContainer> ingestion_exception_container;
                    std::shared_ptr<std::atomic<bool>> is_interrupted;
                    std::deque<std::unique_ptr<internal_rest_controller::Promise<void>>> promise_vec;
                    size_t i;
                    bool is_hit_otherwise;

                public:

                    CoroutineRunner(std::shared_ptr<TrainingDataIterableInterface> training_data,
                                    std::vector<std::shared_ptr<ThreadSafe_APIClient_2>> client_vec,
                                    size_t pipe_sz,
                                    std::shared_ptr<std::atomic<bool>> is_completed,
                                    std::shared_ptr<IngestionExceptionContainer> ingestion_exception_container,
                                    std::shared_ptr<std::atomic<bool>> is_interrupted): training_data(std::move(training_data)),
                                                                                        client_vec(std::move(client_vec)),
                                                                                        pipe_sz(pipe_sz),
                                                                                        is_completed(std::move(is_completed)),
                                                                                        ingestion_exception_container(std::move(ingestion_exception_container)),
                                                                                        is_interrupted(std::move(is_interrupted)),
                                                                                        promise_vec(),
                                                                                        i(0u),
                                                                                        is_hit_otherwise(false){}

                    auto next() noexcept -> bool
                    {
                        try
                        {
                            if (!this->training_data->has_next())
                            {
                                if (this->promise_vec.empty())
                                {
                                    std::abort();
                                }

                                this->promise_vec.front()->wait();
                                this->promise_vec.pop_front();

                                return true;
                            }

                            auto [inp, out]     = this->training_data->next();

                            if (inp == nullptr)
                            {
                                std::abort();
                            }

                            if (out == nullptr)
                            {
                                std::abort();
                            }

                            size_t client_idx   = (this->i++) % this->client_vec.size();

                            if (this->promise_vec.size() == this->pipe_sz)
                            {
                                this->promise_vec.front()->wait();
                                this->promise_vec.pop_front();
                            }

                            this->promise_vec.push_back(this->client_vec[client_idx]->add_training_data_2(inp, out));
                        }
                        catch (...)
                        {
                            this->hit_otherwise();
                        }

                        return true;
                    }

                    auto has_next() noexcept -> bool
                    {
                        try
                        {
                            if (this->is_hit_otherwise)
                            {
                                return false;
                            }

                            if (this->is_interrupted->load(std::memory_order_relaxed))
                            {
                                throw std::runtime_error("data loading process interrupted");
                            }

                            if (this->training_data->has_next())
                            {
                                return true;
                            }

                            if (!this->promise_vec.empty())
                            {
                                return true;
                            }

                            this->hit_thiswise();

                            return false;
                        }
                        catch (...)
                        {
                            this->hit_otherwise();

                            return false;
                        }
                    }
                
                private:

                    void hit_thiswise() noexcept
                    {
                        {
                            fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->ingestion_exception_container->mtx);
                            this->ingestion_exception_container->exception = nullptr;
                        }
                    
                        this->is_completed->exchange(true, std::memory_order_relaxed);
                        this->is_completed->notify_all();
                    }

                    void hit_otherwise() noexcept
                    {
                        this->is_hit_otherwise = true;

                        {
                            fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->ingestion_exception_container->mtx);
                            this->ingestion_exception_container->exception = std::current_exception();
                        }

                        this->is_completed->exchange(true, std::memory_order_relaxed);
                        this->is_completed->notify_all();
                    }
            };
    };

    //---------------
}

#endif
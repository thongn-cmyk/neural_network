
    // class ThreadSafe_APIClient_2
    // {
    //     private:

    //         APIClient_2 base;
    //         std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

    //     public:

    //         ThreadSafe_APIClient_2(const Remote& remote,
    //                                const connectivity_subsystem::MasterConfiguration& config): base(remote, config)
    //                                                                                            mtx(fair_mutex::make_unique_fair_atomic_flag()){}

    //         auto set_retry_policy(std::unique_ptr<internal_rest_controller::RetryMachineInterface>&& retry_machine) -> ThreadSafe_APIClient_2&
    //         {
    //             fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
    //             this->base.set_retry_policy(std::move(retry_machine));

    //             return *this;
    //         }

    //         void add_training_data(const std::shared_ptr<tensor_model::Matrix>& inp, const std::shared_ptr<tensor_model::Matrix>& out)
    //         {
    //             fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
    //             this->base.add_training_data(inp, out);
    //         }

    //         auto add_training_data_2(const std::shared_ptr<tensor_model::Matrix>& inp, const std::shared_ptr<tensor_model::Matrix>& out) -> std::unique_ptr<internal_rest_controller::Promise<void>>
    //         {
    //             fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

    //             return this->base.add_training_data_2(inp, out);
    //         }

    //         void clear_training_data()
    //         {
    //             fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
    //             this->base.clear_training_data();
    //         }

    //         void set_matrix_resource(const std::vector<generic_deviation_projector_factory::GenericDeviationProjectorResource>& matrix_resource_vec)
    //         {
    //             fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
    //             this->base.set_matrix_reosurce(matrix_resource_vec);
    //         }

    //         auto set_matrix_resource_2(const std::vector<generic_deviation_projector_factory::GenericDeviationProjectorResource>& matrix_resource_vec) -> std::unique_ptr<internal_rest_controller::Promise<void>>
    //         {
    //             fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

    //             return this->base.set_matrix_resource_2(matrix_resource_vec);
    //         }

    //         auto get() -> std::vector<mdc_float_t>
    //         {
    //             fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

    //             return this->base.get();
    //         }

    //         void close() noexcept
    //         {
    //             fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);
    //             this->base.close();
    //         }
    // };

    // class TrainingDataIterableInterface
    // {
    //     public:

    //         virtual ~TrainingDataIterableInterface() noexcept = default;

    //         virtual auto next() -> std::unique_ptr<synchronization::Promise<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>> = 0;
    //         virtual auto has_next() -> bool = 0;
    // };

    // class ThreadSafeTrainingDataIterableWrapper: public virtual TrainingDataIterableInterface
    // {
    //     private:

    //         std::unique_ptr<TrainingDataIterableInterface> base;
    //         std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
        
    //     public:

    //         ThreadSafeTrainingDataIterableWrapper(std::unique_ptr<TrainingDataIterableInterface>&& base)
    //         {
    //             if (base == nullptr)
    //             {
    //                 throw std::invalid_argument("bad base, null");
    //             }

    //             this->base  = std::move(base);
    //             this->mtx   = fair_mutex::make_unique_fair_atomic_flag();
    //         }

    //         auto next() -> std::unique_ptr<synchronization::Promise<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>>
    //         {
    //             fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

    //             return this->base->next();
    //         }

    //         auto has_next() -> bool
    //         {
    //             fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

    //             return this->base->has_next();
    //         }
    // };

    // class UniformDataIngestor
    // {
    //     private:

    //         struct IngestionExceptionContainer
    //         {
    //             std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
    //             std::exception_ptr exception;
    //         };

    //         std::shared_ptr<TrainingDataIterableInterface> training_data;
    //         std::vector<std::shared_ptr<ThreadSafe_APIClient_2>> client_vec;
    //         size_t pipe_sz;
    //         std::shared_ptr<std::atomic<bool>> is_completed;
    //         std::shared_ptr<IngestionExceptionContainer> ingestion_exception_container;
    //         bool is_run;
    //         std::shared_ptr<std::atomic<bool>> is_interrupted;
    //         bool is_waited;

    //     public:

    //         UniformDataIngestor(): training_data(),
    //                                client_vec(),
    //                                pipe_sz(0u),
    //                                is_completed(std::make_shared<std::atomic<bool>>(false)),
    //                                ingestion_exception_container(std::make_shared<IngestionExceptionContainer>(IngestionExceptionContainer{.mtx = fair_mutex::make_unique_fair_atomic_flag(),. exception = nullptr})),
    //                                is_run(false),
    //                                is_interrupted(std::make_shared<std::atomic<bool>>(false)),
    //                                is_waited(false){}

    //         auto set_data_source(std::unique_ptr<TrainingDataIterableInterface>&& training_data) -> UniformDataIngestor&
    //         {
    //             if (this->is_run)
    //             {
    //                 throw std::runtime_error("invalid operation, run process has been invoked");
    //             }

    //             if (training_data == nullptr)
    //             {
    //                 throw std::invalid_argument("bad training data iterable, null");
    //             }

    //             this->training_data = std::make_unique<ThreadSafeTrainingDataIterableWrapper>(std::move(training_data));

    //             return *this;
    //         }

    //         auto set_client_vector(const std::vector<std::shared_ptr<ThreadSafe_APIClient_2>>& client_vec) -> UniformDataIngestor&
    //         {
    //             if (this->is_run)
    //             {
    //                 throw std::runtime_error("invalid operation, run process has been invoked");
    //             }

    //             for (const auto& client: client_vec)
    //             {
    //                 if (client == nullptr)
    //                 {
    //                     throw std::invalid_argument("bad client, null");
    //                 }
    //             }

    //             this->client_vec = client_vec;

    //             return *this;
    //         }

    //         auto set_pipe_size(size_t pipe_sz) -> UniformDataIngestor&
    //         {
    //             if (this->is_run)
    //             {
    //                 throw std::runtime_error("invalid operation, run process has been invoked");
    //             }

    //             if (pipe_sz == 0u)
    //             {
    //                 throw std::invalid_argument("bad pipe size, 0");
    //             }

    //             this->pipe_sz = pipe_sz;

    //             return *this;
    //         }

    //         void interrupt() noexcept
    //         {
    //             this->is_interrupted->exchange(true, std::memory_order_relaxed);
    //         }

    //         auto is_completed() noexcept -> bool
    //         {
    //             return this->is_completed->load(std::memory_order_relaxed);
    //         }

    //         void wait()
    //         {
    //             if (!this->is_run)
    //             {
    //                 throw std::runtime_error("invalid operation, run process was not invoked");
    //             }

    //             if (std::exchange(this->is_waited, true))
    //             {
    //                 return;
    //             }

    //             if (this->is_completed->load(std::memory_order_relaxed))
    //             {
    //                 this->throw_error();
    //                 return;
    //             }

    //             this->is_completed->wait(false, std::memory_order_acquire); //it's just better to always acquire on synchronization
    //             this->throw_error();
    //         }

    //         void run()
    //         {
    //             this->check_and_throw_run_requirements();

    //             coroutine_x::run_detached(std::make_unique<CoroutineRunner>(this->training_data,
    //                                                                         this->client_vec,
    //                                                                         this->pipe_sz,
    //                                                                         this->is_completed,
    //                                                                         this->ingestion_exception_container,
    //                                                                         this->is_interrupted),
    //                                       coroutine_x::NETWORK_COROUTINE);

    //             this->is_run = true;
    //         }

    //     private:

    //         void check_and_throw_run_requirements()
    //         {
    //             if (this->is_run)
    //             {
    //                 throw std::runtime_error("invalid operation, run process has been invoked");
    //             }

    //             if (this->training_data == nullptr)
    //             {
    //                 throw std::invalid_argument("bad argument, training data was not set");
    //             }

    //             if (this->client_vec.empty())
    //             {
    //                 throw std::invalid_argument("bad argument, empty client");
    //             }

    //             if (this->pipe_sz == 0u)
    //             {
    //                 throw std::invalid_argument("bad argument, pipe size 0");
    //             }
    //         }

    //         void throw_error()
    //         {
    //             fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->ingestion_exception_container->mtx);

    //             std::rethrow_exception(this->ingestion_exception_container->exception);
    //         }

    //         //I just think that it's better to keep the noexcept so that people can aware of the resource management, but I'll implement the coroutine_x_x

    //         class CoroutineRunner: public virtual coroutine_x::CoroutineableInterface
    //         {
    //             private:

    //                 std::unique_ptr<synchronization::Promise<std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>>>> working_promise;
    //                 std::shared_ptr<TrainingDataIterableInterface> training_data;
    //                 std::vector<std::shared_ptr<ThreadSafe_APIClient_2>> client_vec;
    //                 size_t pipe_sz;
    //                 std::shared_ptr<std::atomic<bool>> is_completed;
    //                 std::shared_ptr<IngestionExceptionContainer> ingestion_exception_container;
    //                 std::shared_ptr<std::atomic<bool>> is_interrupted;
    //                 std::deque<std::unique_ptr<internal_rest_controller::Promise<void>>> promise_vec;
    //                 size_t i;
    //                 bool is_hit_otherwise;

    //             public:

    //                 CoroutineRunner(std::shared_ptr<TrainingDataIterableInterface> training_data,
    //                                 std::vector<std::shared_ptr<ThreadSafe_APIClient_2>> client_vec,
    //                                 size_t pipe_sz,
    //                                 std::shared_ptr<std::atomic<bool>> is_completed,
    //                                 std::shared_ptr<IngestionExceptionContainer> ingestion_exception_container,
    //                                 std::shared_ptr<std::atomic<bool>> is_interrupted): working_promise(nullptr),
    //                                                                                     training_data(std::move(training_data)),
    //                                                                                     client_vec(std::move(client_vec)),
    //                                                                                     pipe_sz(pipe_sz),
    //                                                                                     is_completed(std::move(is_completed)),
    //                                                                                     ingestion_exception_container(std::move(ingestion_exception_container)),
    //                                                                                     is_interrupted(std::move(is_interrupted)),
    //                                                                                     promise_vec(),
    //                                                                                     i(0u),
    //                                                                                     is_hit_otherwise(false){}

    //                 auto next() noexcept -> bool
    //                 {
    //                     try
    //                     {
    //                         if (this->working_promise != nullptr)
    //                         {
    //                             if (!this->working_promise->is_completed())
    //                             {
    //                                 return false;
    //                             }

    //                             if (this->promise_vec.size() == this->pipe_sz)
    //                             {
    //                                 if (!this->promise_vec.front()->is_completed())
    //                                 {
    //                                     return false;
    //                                 }

    //                                 this->promise_vec.front()->wait();
    //                                 this->promise_vec.pop_front();

    //                                 return true;
    //                             }

    //                             auto [inp, out]     = this->working_promise->wait();

    //                             if (inp == nullptr)
    //                             {
    //                                 std::abort();
    //                             }

    //                             if (out == nullptr)
    //                             {
    //                                 std::abort();
    //                             }

    //                             size_t client_idx   = (this->i++) % this->client_vec.size();

    //                             this->promise_vec.push_back(this->client_vec[client_idx]->add_training_data_2(inp, out));
    //                             this->working_promise = nullptr;

    //                             return true;
    //                         }

    //                         if (!this->training_data->has_next())
    //                         {
    //                             if (this->promise_vec.empty())
    //                             {
    //                                 std::abort();
    //                             }

    //                             if (!this->promise_vec.front()->is_completed())
    //                             {
    //                                 return false;
    //                             }

    //                             this->promise_vec.front()->wait();
    //                             this->promise_vec.pop_front();

    //                             return true;
    //                         }

    //                         this->working_promise = this->training_data->next();
    //                         return true;
    //                     }
    //                     catch (...)
    //                     {
    //                         this->hit_otherwise();
    //                     }

    //                     return true;
    //                 }

    //                 auto has_next() noexcept -> bool
    //                 {
    //                     try
    //                     {
    //                         if (this->is_hit_otherwise)
    //                         {
    //                             return false;
    //                         }

    //                         if (this->is_interrupted->load(std::memory_order_relaxed))
    //                         {
    //                             throw std::runtime_error("data loading process interrupted");
    //                         }

    //                         if (this->working_promise != nullptr)
    //                         {
    //                             return true;
    //                         }

    //                         if (this->training_data->has_next())
    //                         {
    //                             return true;
    //                         }

    //                         if (!this->promise_vec.empty())
    //                         {
    //                             return true;
    //                         }

    //                         this->hit_thiswise();

    //                         return false;
    //                     }
    //                     catch (...)
    //                     {
    //                         this->hit_otherwise();

    //                         return false;
    //                     }
    //                 }

    //             private:

    //                 void hit_thiswise() noexcept
    //                 {
    //                     {
    //                         fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->ingestion_exception_container->mtx);
    //                         this->ingestion_exception_container->exception = nullptr;
    //                     }

    //                     this->is_completed->exchange(true, std::memory_order_release);
    //                     this->is_completed->notify_all();
    //                 }

    //                 void hit_otherwise() noexcept
    //                 {
    //                     this->is_hit_otherwise = true;

    //                     {
    //                         fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->ingestion_exception_container->mtx);
    //                         this->ingestion_exception_container->exception = std::current_exception();
    //                     }

    //                     this->is_completed->exchange(true, std::memory_order_release);
    //                     this->is_completed->notify_all();
    //                 }
    //         };
    // };
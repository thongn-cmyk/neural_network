#ifndef __IMMUTABLE_MEMORY_IMMUTABLE_MEMORY_H__
#define __IMMUTABLE_MEMORY_IMMUTABLE_MEMORY_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <string_view>
#include <string>

namespace immutable_memory
{
    class ImmutableMemoryInterface
    {
        public:

            virtual ~ImmutableMemoryInterface() noexcept = default;

            virtual auto get() -> std::string_view = 0;
    };

    class ImmutableMemoryHolderInterface
    {
        public:

            virtual ~ImmutableMemoryHolderInterface() noexcept = default;

            virtual auto get_holder() -> std::shared_ptr<ImmutableMemoryInterface> = 0;
    };

    class OnDestructionCallbackInterface
    {
        public:

            virtual ~OnDestructionCallbackInterface() noexcept = default;

            virtual void callback(ImmutableMemoryHolderInterface& memory) noexcept = 0;
    };
    
    class OnDestructionRegisterManagerInterface
    {
        public:

            virtual ~OnDestructionRegisterManagerInterface() noexcept = default;

            virtual void _register(const std::shared_ptr<OnDestructionCallbackInterface>& callbackable,
                                   std::optional<size_t> callbackable_id) = 0;
    };

    class ManagedImmutableMemoryInterface: public virtual ImmutableMemoryHolderInterface,
                                           public virtual OnDestructionRegisterManagerInterface,
                                           public virtual ImmutableMemoryInterface{};

    template <class ByteStreamContainer>
    class ImmutableMemory: public virtual ImmutableMemoryInterface
    {
        private:

            ByteStreamContainer bstream;

        public:

            ImmutableMemory(std::string_view bstream_view): bstream(bstream_view.data(), bstream_view.size()){}

            ImmutableMemory(const ImmutableMemory&) = delete;
            ImmutableMemory& operator =(const ImmutableMemory&) = delete;

            ImmutableMemory(ImmutableMemory&&) = delete;
            ImmutableMemory& operator =(ImmutableMemory&&) = delete;

            auto get() -> std::string_view
            {
                return std::string_view(this->bstream.data(), this->bstream.size());
            }
    };

    template <class ByteStreamContainer = std::string>
    class ManagedImmutableMemory: public virtual ManagedImmutableMemoryInterface
    {
        private:

            std::unordered_set<size_t> registered_callback_id_set;
            std::vector<std::shared_ptr<OnDestructionCallbackInterface>> callbackable_vec;
            std::shared_ptr<ImmutableMemoryInterface> mem;

        public:

            ManagedImmutableMemory(std::string_view bstream_view): registered_callback_id_set(),
                                                                   callbackable_vec(),
                                                                   mem(std::make_shared<ImmutableMemory<ByteStreamContainer>>(bstream_view)){}

            ManagedImmutableMemory(const ManagedImmutableMemory&) = delete;
            ManagedImmutableMemory(ManagedImmutableMemory&&) = delete;

            ManagedImmutableMemory& operator =(const ManagedImmutableMemory&) = delete;
            ManagedImmutableMemory& operator =(ManagedImmutableMemory&&) = delete;

            ~ManagedImmutableMemory() noexcept
            {
                for (const auto& callbackable: this->callbackable_vec)
                {
                    callbackable->callback(*this);
                }
            }

            auto get() -> std::string_view
            {
                return this->mem->get();
            }

            auto get_holder() -> std::shared_ptr<ImmutableMemoryInterface>
            {
                return this->mem;
            }

            void _register(const std::shared_ptr<OnDestructionCallbackInterface>& callbackable,
                           std::optional<size_t> callback_id)
            {
                if (callbackable == nullptr)
                {
                    throw std::invalid_argument("bad callbackable, null");
                }

                if (callback_id.has_value())
                {
                    if (this->registered_callback_id_set.contains(callback_id.value()))
                    {
                        return;
                    }

                    this->registered_callback_id_set.insert(callback_id.value());
                }

                try
                {
                    this->callbackable_vec.push_back(callbackable);
                }
                catch (...)
                {
                    std::abort();
                }
            }
    };

    template <class ByteStreamContainer = std::string>
    auto make_immutable_memory(std::string_view str_view) -> std::unique_ptr<ImmutableMemoryInterface>
    {
        return std::make_unique<ImmutableMemory<ByteStreamContainer>>(str_view);
    }

    template <class ByteStreamContainer = std::string>
    auto make_managed_immutable_memory(std::string_view str_view) -> std::unique_ptr<ManagedImmutableMemoryInterface>
    {
        return std::make_unique<ManagedImmutableMemory<ByteStreamContainer>>(str_view);
    }
}

#endif
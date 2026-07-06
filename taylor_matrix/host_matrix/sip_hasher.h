#ifndef __TAYLOR_MATRIX_HOST_MATRIX_SIP_HASHER_H__
#define __TAYLOR_MATRIX_HOST_MATRIX_SIP_HASHER_H__

#include <stdint.h>
#include <stdlib.h>
#include <array>
#include <bit>
#include <serializer/trivial_serializer.h>
#include <numeric>
#include <type_traits>

namespace taylor_matrix::host_matrix::sip_hasher
{
    struct implicit_key_tag{};

    class SipHasher
    {
        private:

            uint64_t v0;
            uint64_t v1;
            uint64_t v2;
            uint64_t v3;
            uint64_t m;
            uint64_t m_sz;

        private:

            static constexpr auto rotl(uint64_t x, int b) -> uint64_t
            {
                return (x << b) | (x >> (64 - b));
            }

        public:

            explicit constexpr SipHasher(std::array<char, 16u> key): m(0u),
                                                                     m_sz(0u)
            {
                uint64_t k0 = 0u;
                uint64_t k1 = 0u;

                for (size_t i = 0u; i < 8; ++i)
                {
                    k0 |= static_cast<uint64_t>(std::bit_cast<uint8_t>(key[i])) << (i * 8);
                    k1 |= static_cast<uint64_t>(std::bit_cast<uint8_t>(key[i + 8])) << (i * 8);
                }

                this->v0    = k0 ^ 0x736f6d6570736575ULL;
                this->v1    = k1 ^ 0x646f72616e646f6dULL;
                this->v2    = k0 ^ 0x6c7967656e657261ULL;
                this->v3    = k1 ^ 0x7465646279746573ULL;
            }

            template <class T>
            constexpr SipHasher(implicit_key_tag,
                                const T& key): SipHasher(std::bit_cast<std::array<char, 16u>>(key)){}

            template <size_t SZ>
            constexpr void update(const char * data, const std::integral_constant<size_t, SZ>)
            {
                for (size_t i = 0u; i < SZ; ++i)
                {
                    this->m     |= static_cast<uint64_t>(std::bit_cast<uint8_t>(data[i])) << (this->m_sz * 8);
                    this->m_sz  += 1;

                    if (this->m_sz == 8)
                    {
                        this->process_block(m);

                        this->m     = 0u;
                        this->m_sz  = 0u;
                    }
                }
            }

            constexpr void update(const char * data, size_t sz)
            {
                for (size_t i = 0u; i < sz; ++i)
                {
                    this->m     |= static_cast<uint64_t>(std::bit_cast<uint8_t>(data[i])) << (this->m_sz * 8);
                    this->m_sz  += 1;

                    if (this->m_sz == 8)
                    {
                        this->process_block(m);

                        this->m     = 0u;
                        this->m_sz  = 0u;
                    }
                }
            }

            template <class T>
            constexpr void update(const T& data)
            {
                constexpr size_t DATA_BYTE_SZ    = trivial_serializer::size(T{});
                std::array<char, DATA_BYTE_SZ> data_byte_arr{};

                trivial_serializer::serialize_into(data_byte_arr.data(), data);

                this->update(data_byte_arr.data(), std::integral_constant<size_t, DATA_BYTE_SZ>{});
            }

            constexpr auto get_hash() -> uint64_t
            {
                return SipHasher(*this).internal_finalize();
            }

        private:

            constexpr auto internal_finalize() -> uint64_t
            {
                constexpr size_t FINALIZATION_ROUND_SZ  = 4u;

                uint64_t final_block = this->m | (static_cast<uint64_t>(this->m_sz) << 56);
                this->process_block(final_block);

                this->v2 ^= 0xff;

                for (size_t i = 0u; i < FINALIZATION_ROUND_SZ; ++i)
                {
                    this->sipround();
                }

                return this->v0 ^ this->v1 ^ this->v2 ^ this->v3;
            }

            constexpr void sipround()
            {
                this->v0    += this->v1;
                this->v1    = rotl(v1, 13);
                this->v1    ^= this->v0;

                this->v0    = rotl(v0, 32);
                this->v2    += this->v3;
                this->v3    = rotl(this->v3, 16);
                this->v3    ^= this->v2;

                this->v0    += this->v3;
                this->v3    = rotl(v3, 21);
                this->v3    ^= this->v0;

                this->v2    += this->v1;
                this->v1    = rotl(this->v1, 17);
                this->v1    ^= this->v2;

                this->v2    = rotl(this->v2, 32);
            }

            constexpr void process_block(uint64_t block)
            {
                constexpr size_t SIPROUND_ITERATION_SZ  = 2;

                this->v3 ^= block;

                for (size_t i = 0u; i < SIPROUND_ITERATION_SZ; ++i)
                {
                    this->sipround();
                }

                this->v0 ^= block;
            }
    };
}

#endif
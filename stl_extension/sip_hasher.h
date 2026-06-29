#ifndef __STL_EXTENSION_SIP_HASHER_H__
#define __STL_EXTENSION_SIP_HASHER_H__

#include <stdint.h>
#include <stdlib.h>
#include <array>
#include <bit>

namespace sip_hasher
{
    class SipHasher
    {
        private:

            uint64_t v0, v1, v2, v3;
            uint64_t m;
            uint32_t mlen;
        
            static constexpr uint64_t rotl(uint64_t x, int b)
            {
                return (x << b) | (x >> (64 - b));
            }

            constexpr void sipround()
            {
                v0 += v1; v1 = rotl(v1, 13); v1 ^= v0;
                v0 = rotl(v0, 32);
                v2 += v3; v3 = rotl(v3, 16); v3 ^= v2;
                v0 += v3; v3 = rotl(v3, 21); v3 ^= v0;
                v2 += v1; v1 = rotl(v1, 17); v1 ^= v2;
                v2 = rotl(v2, 32);
            }

            constexpr void processBlock(uint64_t block)
            {
                constexpr uint64_t SIPROUND_ITERATIONS = 2;
                v3 ^= block;

                for (size_t i = 0u; i < SIPROUND_ITERATIONS; ++i)
                {
                    sipround();
                }

                v0 ^= block;
            }
        
            explicit constexpr SipHasher(std::array<char, 16u> key)
            {
                uint64_t k0 = 0u, k1 = 0u;

                for (size_t i = 0u; i < 8; ++i)
                {
                    k0 |= static_cast<uint64_t>(std::bit_cast<uint8_t>(key[i])) << (i * 8);
                    k1 |= static_cast<uint64_t>(std::bit_cast<uint8_t>(key[8 + i])) << (i * 8);
                }

                v0      = k0 ^ 0x736f6d6570736575ULL;
                v1      = k1 ^ 0x646f72616e646f6dULL;
                v2      = k0 ^ 0x6c7967656e657261ULL;
                v3      = k1 ^ 0x7465646279746573ULL;

                m       = 0u;
                mlen    = 0u;
            }

            template <size_t LEN>
            constexpr void update(const char * data, const std::integral_constant<size_t, LEN>)
            {
                for (size_t i = 0u; i < LEN; ++i)
                {
                    m |= static_cast<uint64_t>(std::bit_cast<uint8_t>(data[i])) << (mlen * 8);
                    mlen++;

                    if (mlen == 8)
                    {
                        processBlock(m);

                        m       = 0u;
                        mlen    = 0u;
                    }
                }
            }

            constexpr void update(const char * data, size_t len)
            {
                for (size_t i = 0u; i < len; ++i)
                {
                    m |= static_cast<uint64_t>(std::bit_cast<uint8_t>(data[i])) << (mlen * 8);
                    mlen++;

                    if (mlen == 8)
                    {
                        processBlock(m);

                        m       = 0u;
                        mlen    = 0u;
                    }
                }
            }

            constexpr uint64_t finalize()
            {
                constexpr uint64_t FINALIZATION_ROUNDS = 4;

                uint64_t finalBlock = m | (static_cast<uint64_t>(mlen) << 56);
                processBlock(finalBlock);
        
                v2 ^= 0xff;

                for (size_t i = 0u; i < FINALIZATION_ROUNDS; ++i)
                {
                    sipround();
                }

                return v0 ^ v1 ^ v2 ^ v3;
            }

        public:

            template <class KeyType>
            static constexpr uint64_t hash(KeyType key,
                                           const char * data, size_t len)
            {
                SipHasher hasher(std::bit_cast<std::array<char, 16u>>(key));
                hasher.update(data, len);

                return hasher.finalize();
            }

            template <class KeyType, size_t LEN>
            static constexpr uint64_t hash(KeyType key,
                                           const char * data, const std::integral_constant<size_t, LEN> len)
            {
                SipHasher hasher(std::bit_cast<std::array<char, 16u>>(key));
                hasher.update(data, len);

                return hasher.finalize();
            }
    };

}

#endif
#ifndef __NETWORK_SSL_SYMMETRIC_ENCODER_H__
#define __NETWORK_SSL_SYMMETRIC_ENCODER_H__

#include <random>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <stdexcept>
#include "network_compact_serializer.h"
#include "network_trivial_serializer.h"
#include "network_hash.h"
#include <bit>
#include <algorithm>
#include "network_std_container.h"
#include "stdx.h"
#include <chrono>

namespace dg_sock::ud_sym_encoder
{
    struct RandomizationStruct
    {
        uint64_t time_clue;
        uint64_t stack_clue;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const noexcept
        {
            reflector(time_clue, stack_clue);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) noexcept
        {
            reflector(time_clue, stack_clue);
        }
    };

    static auto randomization_seed() -> size_t
    {
        char stack_element{};

        RandomizationStruct struct_seed
        {
            .time_clue  = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()),
            .stack_clue = std::bit_cast<uint64_t>(static_cast<void *>(&stack_element))
        };

        return dg_sock::network_hash::hash_reflectible(struct_seed);
    }

    struct corrupted_format: std::runtime_error
    {
        corrupted_format(): std::runtime_error("bad msg, corrupted msg"){}
    };

    struct Mt19937Message
    {
        dg_sock::string random_space;
        dg_sock::string encoded;
        uint64_t secret_offset;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(random_space, encoded, secret_offset);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(random_space, encoded, secret_offset);
        }
    };

    struct MurMurMessage
    {
        uint64_t validation_key;
        dg_sock::string encoded;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(validation_key, encoded);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(validation_key, encoded);
        }
    };

    class MurMurEncoder
    {
        private:

            uint64_t secret;

        public:

            static inline constexpr uint32_t MURMUR_SERIALIZATION_SECRET = 1042927439ULL;

            MurMurEncoder(uint64_t secret): secret(secret){}

            auto encode(const dg_sock::string& arg) -> dg_sock::string
            {
                uint64_t key    = dg_sock::network_hash::murmur_hash(arg.data(), arg.size(), this->secret);
                auto msg        = MurMurMessage{.validation_key = key, 
                                                .encoded        = dg_sock::string(arg)};

                return dg_sock::network_compact_serializer::dgstd_serialize<dg_sock::string>(msg, MURMUR_SERIALIZATION_SECRET);
            }

            auto decode(const dg_sock::string& arg) -> dg_sock::string
            {
                MurMurMessage msg       = dg_sock::network_compact_serializer::dgstd_deserialize<MurMurMessage>(arg, MURMUR_SERIALIZATION_SECRET);
                uint64_t expected_key   = dg_sock::network_hash::murmur_hash(msg.encoded.data(), msg.encoded.size(), this->secret);

                if (expected_key != msg.validation_key)
                {
                    throw corrupted_format();
                }

                return dg_sock::string(std::move(msg.encoded));
            }
    };

    using mt19937 = std::mersenne_twister_engine<uint64_t, 64, 312, 156, 31,
                                                 0xb5026f5aa96619e9ULL, 29,
                                                 0x5555555555555555ULL, 17,
                                                 0x71d67fffeda60000ULL, 37,
                                                 0xfff7eee000000000ULL, 43,
                                                 6364136223846793005ULL>;

    class xrange_mt19937
    {
        private:

            dg_sock::vector<mt19937> base;
            size_t offset;

        public:

            constexpr xrange_mt19937(const dg_sock::string& seed): base()
            {
                if (seed.size() == 0u)
                {
                    this->base.push_back(mt19937{0});
                }
                else
                {
                    size_t vec_sz   = seed.size() / 8 + static_cast<size_t>(seed.size() % 8 != 0u);
                    size_t ceil2_sz = stdx::ceil2(vec_sz);

                    for (size_t j = 0u; j < ceil2_sz; ++j)
                    {
                        size_t i                = j % vec_sz;
                        std::array<char, 8> intermediate_buffer{};

                        size_t first            = i * 8;
                        size_t last             = std::min(static_cast<size_t>((i + 1) * 8), seed.size());
                        const char * secret_buf = std::next(seed.data(), first);
                        size_t secret_sz        = last - first;

                        std::memcpy(intermediate_buffer.data(), secret_buf, secret_sz);

                        uint64_t sub_seed       = std::bit_cast<uint64_t>(intermediate_buffer);

                        this->base.push_back(mt19937{sub_seed});
                    }
                }

                this->offset = 0u;
            }

            constexpr auto operator()() -> uint64_t
            {
                return this->base[this->offset++ & (this->base.size() - 1u)]();
            }
    };

    //it's best to have a high entropy string before dictionarize this

    class Mt19937Encoder
    {
        private:

            dg_sock::string secret;
            mt19937 salt_randgen;
            size_t salt_sz_per_token;
            size_t secret_offset;
            size_t dictionary_renew_sz;

            // static inline constexpr size_t DICTIONARY_RENEW_SZ  = size_t{1} << 2;

        public:

            Mt19937Encoder(dg_sock::string secret,
                           size_t salt_sz_per_token,
                           size_t dictionary_renew_sz): salt_randgen(mt19937(randomization_seed())),
                                                        salt_sz_per_token(salt_sz_per_token),
                                                        secret_offset(0u),
                                                        dictionary_renew_sz(std::max(dictionary_renew_sz, size_t{1}))
            {
                if (secret.empty())
                {
                    secret = "mersenne_twister";
                }

                if (!stdxx::is_pow2(secret.size()))
                {
                    throw std::invalid_argument("bad secret, secret size is not of base 2");
                }

                this->secret = std::move(secret);
            }

            static inline constexpr uint32_t MT19937_SERIALIZATION_SECRET = 1422722760ULL;

            auto encode(const dg_sock::string& arg) -> dg_sock::string
            {
                size_t salt_sz                  = this->salt_sz_per_token;

                dg_sock::string random_space    = this->randomize_random_buffer(salt_sz);
                size_t local_secret_offset      = this->secret_offset;
                dg_sock::string subsecret_space = this->project_subsecret_buffer(random_space, local_secret_offset);

                auto randomizer                 = xrange_mt19937(subsecret_space);
                auto encoded                    = dg_sock::string(arg.size(), ' ');
                auto word_dict                  = this->get_byte_dict(randomizer);
                size_t renew_i                  = 0u;

                for (size_t i = 0u; i < arg.size(); ++i)
                {
                    if (renew_i == this->dictionary_renew_sz)
                    {
                        word_dict   = this->get_byte_dict(randomizer);
                        renew_i     = 0u;
                    }

                    encoded[i]  = this->byte_encode(arg[i], word_dict);
                    renew_i     += 1u;
                }

                this->secret_offset             += arg.size();

                return this->serialize(Mt19937Message{.random_space     = random_space,
                                                      .encoded          = std::move(encoded),
                                                      .secret_offset    = local_secret_offset}); 
            }

            auto decode(const dg_sock::string& arg) -> dg_sock::string
            {
                Mt19937Message msg              = this->deserialize(arg);

                dg_sock::string random_space    = msg.random_space;
                size_t local_secret_offset      = msg.secret_offset;
                dg_sock::string subsecret_space = this->project_subsecret_buffer(random_space, local_secret_offset);
 
                auto randomizer                 = xrange_mt19937(subsecret_space);
                auto decoded                    = dg_sock::string(msg.encoded.size(), ' ');
                auto word_dict                  = this->get_reverse_byte_dict(randomizer);
                size_t renew_i                  = 0u;

                for (size_t i = 0u; i < msg.encoded.size(); ++i)
                {
                    if (renew_i == this->dictionary_renew_sz)
                    {
                        word_dict   = this->get_reverse_byte_dict(randomizer);
                        renew_i     = 0u;
                    }

                    decoded[i]  = this->byte_decode(msg.encoded[i], word_dict);
                    renew_i     += 1u;
                }

                return decoded;
            }

        private:

            auto randomize_random_buffer(size_t sz) -> dg_sock::string
            {
                dg_sock::string rs{};
                rs.reserve(sz);

                size_t rev_sz   = sz / sizeof(uint64_t);
                size_t rem_sz   = sz - rev_sz * sizeof(uint64_t);

                for (size_t i = 0u; i < rev_sz; ++i)
                {
                    std::array<char, sizeof(uint64_t)> byte_rep;

                    uint64_t tmp    = this->salt_randgen();
                    byte_rep        = std::bit_cast<decltype(byte_rep)>(tmp);

                    rs.insert(rs.end(), byte_rep.begin(), byte_rep.end());
                }

                std::array<char, sizeof(uint64_t)> byte_rep;

                uint64_t tmp    = this->salt_randgen();
                byte_rep        = std::bit_cast<decltype(byte_rep)>(tmp);

                rs.insert(rs.end(), byte_rep.begin(), std::next(byte_rep.begin(), rem_sz));

                return rs;
            }

            auto project_subsecret_char(char c, uint64_t local_secret_offset) -> char
            {
                std::array<char, 2u> subsecret_set{};

                subsecret_set[0]    = this->secret[local_secret_offset & (this->secret.size() - 1u)];
                subsecret_set[1]    = c;

                return std::bit_cast<char>(static_cast<uint8_t>(dg_sock::network_hash::murmur_hash(subsecret_set.data(),
                                                                                                   std::integral_constant<size_t, decltype(subsecret_set){}.size()>{})));
            }

            auto project_subsecret_buffer(const dg_sock::string& buffer, uint64_t local_secret_offset) -> dg_sock::string
            {
                dg_sock::string rs{};
                rs.reserve(buffer.size());

                for (char c: buffer)
                {
                    rs.push_back(this->project_subsecret_char(c, local_secret_offset++));
                }

                return rs;
            }

            template <class Randomizer>
            __attribute__((noinline)) auto get_byte_dict(Randomizer& randomizer) -> dg_sock::vector<uint8_t>
            {
                const size_t DICTIONARY_SZ  = size_t{1} << 8;
                const size_t ITERATION_SZ   = size_t{1} << 7;

                dg_sock::vector<uint8_t> rs(DICTIONARY_SZ);
                std::iota(rs.begin(), rs.end(), 0u);

                for (size_t i = 0u; i < ITERATION_SZ; ++i)
                {
                    size_t lhs_idx  = i;
                    size_t rhs_idx  = static_cast<size_t>(randomizer()) % DICTIONARY_SZ;

                    std::swap(rs[lhs_idx], rs[rhs_idx]);
                }

                return rs;
            }

            template <class Randomizer>
            __attribute__((noinline)) auto get_reverse_byte_dict(Randomizer& randomizer) -> dg_sock::vector<uint8_t>
            {
                dg_sock::vector<uint8_t> fwd_dict = get_byte_dict(randomizer);
                dg_sock::vector<uint8_t> rs_dict(fwd_dict.size());

                for (size_t i = 0u; i < fwd_dict.size(); ++i)
                {
                    rs_dict[fwd_dict[i]] = i;
                }

                return rs_dict;
            }

            inline auto byte_encode(char key, const dg_sock::vector<uint8_t>& dict) -> char
            {
                return std::bit_cast<char>(dict[std::bit_cast<uint8_t>(key)]);
            }

            inline auto byte_decode(char value, const dg_sock::vector<uint8_t>& reverse_dict) -> char
            {
                return std::bit_cast<char>(reverse_dict[std::bit_cast<uint8_t>(value)]);
            }

            auto serialize(const Mt19937Message& msg) -> dg_sock::string
            {
                return dg_sock::network_compact_serializer::integrity_serialize<dg_sock::string>(msg, MT19937_SERIALIZATION_SECRET);
            }

            auto deserialize(const dg_sock::string& bstream) -> Mt19937Message
            {
                return dg_sock::network_compact_serializer::integrity_deserialize<Mt19937Message>(bstream, MT19937_SERIALIZATION_SECRET);
            }
    };

    class DoubleEncoder
    {
        private:

            MurMurEncoder first_encoder;
            Mt19937Encoder second_encoder;

        public:

            DoubleEncoder(const dg_sock::string& secret,
                          size_t salt_sz_per_token,
                          size_t dictionary_renew_sz): first_encoder(dg_sock::network_hash::murmur_hash(secret.data(), secret.size())),
                                                       second_encoder(secret, salt_sz_per_token, dictionary_renew_sz){}

            auto encode(const dg_sock::string& msg) -> dg_sock::string
            {    
                return this->second_encoder.encode(this->first_encoder.encode(msg));
            }

            auto decode(const dg_sock::string& msg) -> dg_sock::string
            {
                return this->first_encoder.decode(this->second_encoder.decode(msg));
            }
    };
}

#endif
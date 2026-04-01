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

namespace dg_sock::ud_sym_encoder
{
    struct corrupted_format: std::exception{}; 
    struct invalid_argument: std::exception{}; 

    struct Mt19937Message
    {
        std::string random_space;
        std::string encoded;
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
        std::string encoded;

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

            uint64_t secret; //we are susceptible to same-salt attacks, the secret virtualized space protected by the sub-secret (a.k.a. seed) is exposed, we are now vulnerable with the exposed secret, we can easily reverse engineer the encode, to pass through the integrity decode phase, to avoid this from happening, we must specify the sub-secret space size to protect this secret space size

        public:

            static inline constexpr uint32_t MURMUR_SERIALIZATION_SECRET = 1042927439ULL;

            MurMurEncoder(uint64_t secret) noexcept: secret(secret){}

            auto encode(const std::string& arg) -> std::string
            {
                uint64_t key    = dg_sock::network_hash::murmur_hash(arg.data(), arg.size(), this->secret);
                auto msg        = MurMurMessage{.validation_key = key, 
                                                .encoded        = arg};

                return dg_sock::network_compact_serializer::dgstd_serialize<std::string>(msg, MURMUR_SERIALIZATION_SECRET);
            }

            auto decode(const std::string& arg) -> std::string
            {
                try
                {
                    MurMurMessage msg       = dg_sock::network_compact_serializer::dgstd_deserialize<MurMurMessage>(arg, MURMUR_SERIALIZATION_SECRET);
                    uint64_t expected_key   = dg_sock::network_hash::murmur_hash(msg.encoded.data(), msg.encoded.size(), this->secret);

                    if (expected_key != msg.validation_key)
                    {
                        throw corrupted_format();
                    }

                    return std::string(std::move(msg.encoded));
                }
                catch (dg_sock::network_compact_serializer::exception_space::corrupted_format& err)
                {
                    throw corrupted_format();
                }
                catch (...)
                {
                    std::rethrow_exception(std::current_exception());
                }
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

            std::vector<mt19937> base;
            size_t offset;

        public:

            constexpr xrange_mt19937(std::string_view seed)
            {
                if (seed.size() == 0u)
                {
                    this->base.push_back(mt19937{0});
                }
                else
                {
                    size_t vec_sz = seed.size() / 8 + static_cast<size_t>(seed.size() % 8 != 0);

                    for (size_t i = 0u; i < vec_sz; ++i)
                    {
                        std::array<char, 8u> intermediate_buffer{};

                        size_t first    = i * 8;
                        size_t last     = std::min(static_cast<size_t>((i + 1) * 8), seed.size());

                        std::memcpy(intermediate_buffer.data(), std::next(seed.data(), first), last - first);
                        this->base.push_back(mt19937{std::bit_cast<uint64_t>(intermediate_buffer)});
                    }
                }

                this->offset = 0u;
            }

            constexpr auto operator()() noexcept -> uint64_t
            {
                // return 0u;
                return this->base[this->offset++ % this->base.size()]();
            }
    };

    //first of, we know that we are giving 1 byte of secret for every char in the salted buffer, it's not that we peek one char out of the secret and the next, but every char lost a 1/total_char semantic space
    //the domain space is now the compromised part of the secret, which we'd expose
    //so we'd have to encode our message within the acceptable compromised domain space
    //ok this is bullet proof now

    class Mt19937Encoder
    {
        private:

            std::string secret;
            mt19937 salt_randgen;
            size_t salt_sz_per_token;
            size_t secret_offset;

        public:

            static inline std::string subsecret_test{};

            Mt19937Encoder(std::string secret,
                           size_t salt_sz_per_token): salt_randgen(mt19937(std::chrono::high_resolution_clock::now().time_since_epoch().count())),
                                                      salt_sz_per_token(salt_sz_per_token),
                                                      secret_offset(0u)
            {
                if (secret.empty())
                {
                    secret = std::string("default_key");
                }

                this->secret = std::move(secret);
            }

            static inline constexpr uint32_t MT19937_SERIALIZATION_SECRET = 1422722760ULL;

            auto encode(const std::string& arg) -> std::string
            {
                size_t salt_sz                  = this->salt_sz_per_token;

                std::string random_space        = this->randomize_random_buffer(salt_sz);
                size_t local_secret_offset      = this->secret_offset;
                std::string subsecret_space     = this->project_subsecret_buffer(random_space, local_secret_offset);

                subsecret_test                  = random_space;

                auto randomizer                 = xrange_mt19937(subsecret_space);
                auto encoded                    = std::string(arg.size(), ' ');

                for (size_t i = 0u; i < arg.size(); ++i)
                {
                    encoded[i] = this->byte_encode(arg[i], randomizer);
                }

                this->secret_offset             += arg.size();

                return this->serialize(Mt19937Message{.random_space     = random_space,
                                                      .encoded          = std::move(encoded),
                                                      .secret_offset    = local_secret_offset}); 
            }

            auto decode(const std::string& arg) -> std::string
            {
                Mt19937Message msg              = this->deserialize(arg);

                std::string random_space        = msg.random_space;
                size_t local_secret_offset      = msg.secret_offset;
                std::string subsecret_space     = this->project_subsecret_buffer(random_space, local_secret_offset);
 
                auto randomizer                 = xrange_mt19937(subsecret_space);
                auto decoded                    = std::string(msg.encoded.size(), ' ');

                for (size_t i = 0u; i < msg.encoded.size(); ++i)
                {
                    decoded[i] = this->byte_decode(msg.encoded[i], randomizer);
                }

                return decoded;
            }

        private:

            auto randomize_random_buffer(size_t sz) -> std::string
            {
                std::string rs{};
                rs.reserve(sz);

                for (size_t i = 0u; i < sz; ++i)
                {
                    rs.push_back(std::bit_cast<char>(static_cast<uint8_t>(this->salt_randgen())));
                }

                return rs;
            }

            auto project_subsecret_char(char c, uint64_t local_secret_offset) -> char
            {
                std::array<char, 2u> subsecret_set{};

                subsecret_set[0]    = this->secret[local_secret_offset++ % this->secret.size()];
                subsecret_set[1]    = c;

                return std::bit_cast<char>(static_cast<uint8_t>(dg_sock::network_hash::murmur_hash(subsecret_set.data(), subsecret_set.size())));
            }

            auto project_subsecret_buffer(const std::string& buffer, uint64_t local_secret_offset) -> std::string
            {
                std::string rs{};
                rs.reserve(buffer.size());

                for (char c: buffer)
                {
                    rs.push_back(this->project_subsecret_char(c, local_secret_offset));
                }

                return rs;
            }

            template <class Randomizer>
            auto get_byte_dict(Randomizer& randomizer) -> std::vector<uint8_t>
            {    
                std::vector<uint8_t> rs(256);
                std::iota(rs.begin(), rs.end(), 0u);

                for (size_t i = 0u; i < 256; ++i)
                {
                    size_t lhs_idx = static_cast<size_t>(randomizer()) % 256;
                    size_t rhs_idx = static_cast<size_t>(randomizer()) % 256;

                    std::swap(rs[lhs_idx], rs[rhs_idx]);
                }

                return rs;
            }

            template <class Randomizer>
            auto byte_encode(char key, Randomizer& randomizer) -> char
            {
                std::vector<uint8_t> dict = this->get_byte_dict(randomizer);

                return std::bit_cast<char>(dict[std::bit_cast<uint8_t>(key)]);
            }

            template <class Randomizer>
            auto byte_decode(char value, Randomizer& randomizer) -> char
            {    
                std::vector<uint8_t> dict = this->get_byte_dict(randomizer);
                uint8_t key = std::distance(dict.begin(), std::find(dict.begin(), dict.end(), std::bit_cast<uint8_t>(value)));

                return std::bit_cast<char>(key);
            }

            auto serialize(const Mt19937Message& msg) -> std::string
            {
                return dg_sock::network_compact_serializer::integrity_serialize<std::string>(msg, MT19937_SERIALIZATION_SECRET);
            }

            auto deserialize(const std::string& bstream) -> Mt19937Message
            {
                return dg_sock::network_compact_serializer::integrity_deserialize<Mt19937Message>(bstream, MT19937_SERIALIZATION_SECRET);
            }
    };

    class DoubleEncoder
    {
        private:

            std::unique_ptr<MurMurEncoder> first_encoder;
            std::unique_ptr<Mt19937Encoder> second_encoder;
        
        public:

            DoubleEncoder(std::string secret,
                          size_t salt_sz_per_token) noexcept: first_encoder(std::make_unique<MurMurEncoder>(dg_sock::network_hash::murmur_hash(secret.data(), secret.size()))),
                                                              second_encoder(std::make_unique<Mt19937Encoder>(secret, salt_sz_per_token)){}
            
            auto encode(const std::string& msg) -> std::string{
                
                return this->second_encoder->encode(this->first_encoder->encode(msg));
            }

            auto decode(const std::string& msg) -> std::string{

                return this->first_encoder->decode(this->second_encoder->decode(msg));
            }
    };
}

#endif
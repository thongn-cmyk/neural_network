#ifndef __MATRIX_ENCODER_DECODER_H__
#define __MATRIX_ENCODER_DECODER_H__

#include <stdint.h>
#include <stdlib.h>
#include <algorithm>
#include <utility>
#include <memory>
#include "tensor_model.h"
#include <functional>
#include "float_def.h"
#include <limits.h>
#include <optional>

namespace matrix_encoder_decoder
{
    class EncoderInterface
    {
        public:

            virtual ~EncoderInterface() noexcept = default;
            virtual auto encode(std::string_view data) -> std::shared_ptr<tensor_model::Matrix> = 0;
    };

    class DecoderInterface
    {
        public:

            virtual ~DecoderInterface() noexcept = default;
            virtual auto decode(const std::shared_ptr<tensor_model::Matrix>& matrix) -> std::string = 0;
    };

    class TwoWayEncoderInterface: public virtual EncoderInterface,
                                  public virtual DecoderInterface{};

    class BitStreamReader
    {
        private:

            std::string_view data;
            size_t offset;

        public:

            BitStreamReader(std::string_view data,
                            size_t offset): data(data),
                                            offset(offset)
            {
                size_t data_sz = this->data.size() * CHAR_BIT;

                if (this->offset > data_sz)
                {
                    throw std::invalid_argument("bad offset, out of bound");                    
                }                                
            }

            BitStreamReader(std::string_view data): data(data),
                                                    offset(0u){}

            auto read_uint64(size_t bit_width) -> std::optional<uint64_t>
            {
                if (bit_width > 64)
                {
                    throw std::invalid_argument("bad bit width, max 64 bits for uint64_t");
                }

                if (bit_width == 0u)
                {
                    return uint64_t{0u};
                }

                size_t max_last         = this->data.size() * CHAR_BIT;
                size_t tentative_last   = this->offset + bit_width;
                size_t actual_last      = std::min(tentative_last, max_last);
                size_t read_bit_sz      = actual_last - this->offset;
                uint64_t result         = 0u;

                if (read_bit_sz == 0u)
                {
                    return std::nullopt;
                }

                for (size_t i = 0u; i < read_bit_sz; ++i)
                {
                    size_t reverse_offset   = actual_last - (i + 1u);
                    result                  <<= 1;
                    result                  |= static_cast<uint64_t>(this->read_bit_at(reverse_offset));
                }

                this->offset = actual_last;

                return result;
            }

        private:

            auto read_bit_at(size_t idx) -> bool
            {
                size_t bit_sz   = this->data.size() * CHAR_BIT;

                if (idx >= bit_sz)
                {
                    std::abort();
                }

                size_t slot     = idx / CHAR_BIT;
                size_t offset   = idx % CHAR_BIT;
                uint8_t word    = std::bit_cast<uint8_t>(this->data[slot]);
                bool bit_test   = static_cast<uint8_t>(word >> offset) & uint8_t{1};

                return bit_test;
            }
    };

    class BitStream
    {
        private:

            std::string stream_container;
            size_t offset;

        public:

            BitStream(): stream_container(),
                         offset(0u){}

            void write_uint64(uint64_t word, size_t bit_sz)
            {
                size_t old_sz       = this->stream_container.size();
                size_t old_offset   = this->offset;
                uint64_t cpy_word   = word;

                try
                {
                    for (size_t i = 0u; i < bit_sz; ++i)
                    {
                        this->push_bit(cpy_word & 1ULL);
                        cpy_word >>= 1;
                    }
                }
                catch (...)
                {
                    this->stream_container.resize(old_sz);
                    this->offset = old_offset;

                    throw;
                }
            }

            auto get() const noexcept -> const std::string&
            {
                return this->stream_container;
            }

            auto bit_size() const noexcept -> size_t
            {
                return this->offset;
            }

        private:

            void push_bit(bool value)
            {
                size_t offset_slot      = this->offset / CHAR_BIT;
                size_t offset_offset    = this->offset % CHAR_BIT;

                if (offset_slot == this->stream_container.size())
                {
                    this->stream_container.push_back(char(0));
                }

                uint8_t byte_value                  = std::bit_cast<uint8_t>(this->stream_container[offset_slot]);
                uint8_t toggler                     = uint8_t{1} << offset_offset;
                uint8_t new_byte                    = byte_value | toggler;
                this->stream_container[offset_slot] = std::bit_cast<char>(new_byte);
                this->offset                        += 1;
            }
    };

    class EnumerationHolePuncher
    {
        public:

            auto binarize(size_t enumeration_idx, size_t enumeration_sz) -> std::vector<tensor_model::tensor_std_float_t>
            {
                if (enumeration_idx >= enumeration_sz)
                {
                    throw std::invalid_argument("bad enumeration, out of bound access");
                }

                std::vector<tensor_model::tensor_std_float_t> rs(enumeration_sz, 0);
                rs[enumeration_idx] = 1;

                return rs;
            }
    };

    class FixedMatrixShaper
    {
        private:

            size_t being_process_group_vec_sz;
            size_t being_vec_sz;

        public:

            FixedMatrixShaper(size_t being_process_group_vec_sz,
                              size_t being_vec_sz): being_process_group_vec_sz(being_process_group_vec_sz),
                                                    being_vec_sz(being_vec_sz){}

            auto to_matrix(const std::vector<tensor_model::tensor_std_float_t>& flat_vec) -> std::shared_ptr<tensor_model::Matrix>
            {
                size_t expected_flat_sz = this->being_vec_sz
                                            * this->being_process_group_vec_sz
                                            * tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ
                                            * tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ;
                
                if (flat_vec.size() > expected_flat_sz)
                {
                    throw std::invalid_argument("overflow matrix conversion, logit size exceeded");
                }

                std::vector<tensor_model::tensor_std_float_t> expected_flat_vec = flat_vec;
                expected_flat_vec.resize(expected_flat_sz, 0);

                return tensor_matrix_operation::make_matrix_from_flat_vec({this->being_vec_sz,
                                                                           this->being_process_group_vec_sz,
                                                                           tensor_model::PROCESS_GROUP_PROCESS_UNIT_DIMENSION_SZ,
                                                                           tensor_model::PROCESS_UNIT_LOGIT_VEC_DIMENSION_SZ},
                                                                          expected_flat_vec);
            }
    };

    class HolePunchEncoder: public virtual EncoderInterface
    {
        private:

            size_t vocab_bit_width;
            FixedMatrixShaper matrix_shaper;

        public:

            HolePunchEncoder(size_t vocab_bit_width,
                             size_t being_process_group_vec_sz,
                             size_t being_vec_sz): vocab_bit_width(stdx::safe_non_zero_access(vocab_bit_width)),
                                                   matrix_shaper(being_process_group_vec_sz, being_vec_sz){}

            auto encode(std::string_view data) -> std::shared_ptr<tensor_model::Matrix>
            {
                size_t enumeration_sz = size_t{1} << this->vocab_bit_width;
                std::vector<tensor_model::tensor_std_float_t> flat_tensor_vec{};
                BitStreamReader bit_stream(data);

                while (true)
                {
                    std::optional<uint64_t> enumeration_idx = bit_stream.read_uint64(this->vocab_bit_width);

                    if (!enumeration_idx.has_value())
                    {
                        break;
                    }

                    std::vector<tensor_model::tensor_std_float_t> word_tensor_vec = EnumerationHolePuncher{}.binarize(enumeration_idx.value(), enumeration_sz);
                    std::copy(word_tensor_vec.begin(), word_tensor_vec.end(), std::back_inserter(flat_tensor_vec));
                }

                return matrix_shaper.to_matrix(flat_tensor_vec);
            }
    };

    class HolePunchMaxDecoder: public virtual DecoderInterface
    {
        private:

            size_t vocab_bit_width;

        public:

            HolePunchMaxDecoder(size_t vocab_bit_width): vocab_bit_width(stdx::safe_non_zero_access(vocab_bit_width)){}

            auto decode(const std::shared_ptr<tensor_model::Matrix>& matrix) -> std::string
            {
                std::vector<size_t> enumeration_vec = this->decode_enumeration(matrix);
                std::string result                  = this->decode_string(enumeration_vec);

                return result;
            }
            
        private:

            auto decode_enumeration(const std::shared_ptr<tensor_model::Matrix>& matrix) -> std::vector<size_t>
            {
                if (matrix == nullptr)
                {
                    throw std::invalid_argument("bad matrix, null matrix");
                }

                std::vector<tensor_model::tensor_std_float_t> flat_tensor_vec{};
                tensor_matrix_operation::flatten(matrix, flat_tensor_vec);

                size_t enumeration_sz   = size_t{1} << this->vocab_bit_width;
                size_t iteration_sz     = flat_tensor_vec.size() / enumeration_sz + static_cast<size_t>(flat_tensor_vec.size() % enumeration_sz != 0u);
                std::vector<size_t> rs  = {};

                for (size_t i = 0u; i < iteration_sz; ++i)
                {
                    size_t first        = i * enumeration_sz;
                    size_t last         = std::min(static_cast<size_t>((i + 1) * enumeration_sz), flat_tensor_vec.size());
                    auto first_it       = std::next(flat_tensor_vec.begin(), first);
                    auto last_it        = std::next(flat_tensor_vec.begin(), last);
                    size_t max_slot_idx = std::distance(first_it, std::max_element(first_it, last_it));

                    rs.push_back(max_slot_idx);
                }

                return rs;
            }

            auto decode_string(const std::vector<size_t>& enumeration_vec) -> std::string
            {
                size_t enumeration_sz   = size_t{1} << this->vocab_bit_width;
                BitStream bit_streamer  = {};

                for (size_t enumeration: enumeration_vec)
                {
                    uint64_t numerical_representation = enumeration;

                    if (numerical_representation >= enumeration_sz)
                    {
                        throw std::invalid_argument("bad enumeration, max numerical value reached");
                    }

                    bit_streamer.write_uint64(enumeration, this->vocab_bit_width);
                }

                return bit_streamer.get();
            }
    };

    class EncoderDecoder: public virtual TwoWayEncoderInterface
    {
        private:

            std::unique_ptr<EncoderInterface> encoder;
            std::unique_ptr<DecoderInterface> decoder;
        
        public:

            EncoderDecoder(std::unique_ptr<EncoderInterface> encoder,
                           std::unique_ptr<DecoderInterface> decoder) noexcept: encoder(std::move(encoder)),
                                                                                decoder(std::move(decoder)){}

            auto encode(std::string_view data) -> std::shared_ptr<tensor_model::Matrix>
            {
                return this->encoder->encode(data);
            }

            auto decode(const std::shared_ptr<tensor_model::Matrix>& matrix) -> std::string
            {
                return this->decoder->decode(matrix);
            }
    };
}

#endif
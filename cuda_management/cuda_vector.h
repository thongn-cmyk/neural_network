#ifndef __CUDA_MANAGEMENT_CUDA_VECTOR_H__
#define __CUDA_MANAGEMENT_CUDA_VECTOR_H__

#include <stdint.h>
#include <stdlib.h>
#include "device_memory.h"
#include <type_traits>
#include "utility.h"
#include <initializer_list>
#include <iterator>
#include <cuda/std/limits>

namespace cuda_management::cuda_vector
{
    template<class T, class = void>
    struct is_iterator : std::false_type {};

    template<class T>
    struct is_iterator<T, std::void_t<typename std::iterator_traits<T>::iterator_category>> : std::true_type {};

    template <class T>
    static inline constexpr bool is_iterator_v = is_iterator<T>::value;

    template <class T, class Allocator = cuda_management::device_memory::CudaAllocator>
    struct trivial_cuda_vector
    {
        private:

            T * arr;
            size_t arr_sz;
            size_t arr_cap;

            __device__ static constexpr auto room_scale_factor() -> size_t
            {
                return 2u;
            }

            using self      = trivial_cuda_vector; 

            __device__ static constexpr auto allocate_zero_length_array() noexcept -> T *
            {
                return cuda_management::device_memory::std_new_array<T>(Allocator{}, 0u);
            } 

        public:

            using value_type        = T;
            using size_type         = std::size_t;
            using difference_type   = std::intmax_t;

            using reference         = T&;
            using const_reference   = const T&;

            using pointer           = T *;
            using const_pointer     = const T *;

            using iterator          = T *;
            using const_iterator    = const T *;

            static_assert(std::is_trivial_v<T>);

            __device__ constexpr trivial_cuda_vector(): arr(allocate_zero_length_array()),
                                                        arr_sz(0u),
                                                        arr_cap(0u){}

            template <class Iterator, std::enable_if_t<is_iterator_v<Iterator>, bool> = true>
            __device__ constexpr trivial_cuda_vector(Iterator first, Iterator last): trivial_cuda_vector()
            {
                //WARNING: unsafe stack unwind
 
                intmax_t sz = utility::distance(first, last);

                if (sz < 0)
                {
                    assert(false);
                }

                this->resize(static_cast<size_t>(sz));

                for (size_t i = 0u; i < sz; ++i)
                {
                    this->arr[i] = *(first++);
                }
            }


            __device__ constexpr trivial_cuda_vector(size_t sz, const T& value): trivial_cuda_vector()
            {
                //WARNING: unsafe stack unwind 

                this->resize(sz);

                for (size_t i = 0u; i < sz; ++i)
                {
                    this->arr[i] = value;
                }
            }

            __device__ constexpr trivial_cuda_vector(std::initializer_list<T> init_list): trivial_cuda_vector(init_list.begin(), init_list.end()){}

            __device__ constexpr trivial_cuda_vector(const self& other): trivial_cuda_vector()
            {
                //WARNING: unsafe stack unwind

                *this = other;
            }

            __device__ constexpr trivial_cuda_vector(self&& other) noexcept: arr(utility::exchange(other.arr, allocate_zero_length_array())),
                                                                             arr_sz(utility::exchange(other.arr_sz, 0u)),
                                                                             arr_cap(utility::exchange(other.arr_cap, 0u)){}

            __device__ constexpr ~trivial_cuda_vector() noexcept
            {
                this->free_resource();
            }

            __device__ constexpr auto operator =(const self& other) -> self&
            {
                if (this == &other)
                {
                    return *this;
                }

                this->resize(other.size());

                for (size_t i = 0u; i < other.size(); ++i)
                {
                    this->arr[i] = other[i];
                }

                return *this;
            }

            __device__ constexpr auto operator =(self&& other) noexcept -> self&
            {
                if (this == &other)
                {
                    return *this;
                }

                this->free_resource();

                this->arr       = utility::exchange(other.arr, allocate_zero_length_array());
                this->arr_sz    = utility::exchange(other.arr_sz, 0u);
                this->arr_cap   = utility::exchange(other.arr_cap, 0u);

                return *this;
            }

            __device__ constexpr void swap(self& other) noexcept
            {
                utility::swap(this->arr, other.arr);
                utility::swap(this->arr_sz, other.arr_sz);
                utility::swap(this->arr_cap, other.arr_cap);
            }

            __device__ constexpr void reserve(size_t cap)
            {
                if (cap <= this->arr_cap)
                {
                    return;
                }

                this->move_data_to_capacity_of(utility::ceil2(cap));
            }

            __device__ constexpr void resize(size_t sz, const T& default_value = T())
            {
                this->reserve(sz);

                size_t next_cover_sz    = utility::max(sz, this->arr_sz);
                size_t prev_cover_sz    = this->arr_sz;
                size_t uncovered_sz     = next_cover_sz - prev_cover_sz;

                for (size_t i = 0u; i < uncovered_sz; ++i)
                {
                    size_t i_1      = prev_cover_sz + i;
                    this->arr[i_1]  = default_value;
                }

                this->arr_sz = sz;
            }

            template <class ValueLike>
            __device__ constexpr void push_back(ValueLike&& value)
            {
                if (this->arr_sz == this->arr_cap)
                {
                    this->move_data_to_capacity_of(utility::max(this->arr_cap * room_scale_factor(), size_t{1}));
                }

                this->arr[this->arr_sz++]   = std::forward<ValueLike>(value);
            }

            template <class ...Args>
            __device__ constexpr void emplace_back(Args&& ...args)
            {
                if (this->arr_sz == this->arr_cap)
                {
                    this->move_data_to_capacity_of(utility::max(this->arr_cap * room_scale_factor(), size_t{1}));
                }

                this->arr[this->arr_sz++]   = T(std::forward<Args>(args)...);
            }

            __device__ constexpr void pop_back() noexcept
            {
                this->access_guard(0u);

                this->arr_sz -= 1u;
            }

            __device__ constexpr void clear() noexcept
            {
                this->arr_sz = 0u;
            }

            __device__ constexpr auto front() const noexcept -> const T&
            {
                this->access_guard(0u);

                return this->arr[0];
            }

            __device__ constexpr auto front() noexcept -> T&
            {
                this->access_guard(0u);

                return this->arr[0];
            }

            __device__ constexpr auto back() const noexcept -> const T&
            {
                this->access_guard(0u);

                return this->arr[this->arr_sz - 1u];
            }

            __device__ constexpr auto back() noexcept -> T&
            {
                this->access_guard(0u);

                return this->arr[this->arr_sz - 1u];
            }

            __device__ constexpr auto begin() noexcept -> T *
            {
                return this->arr;
            }

            __device__ constexpr auto begin() const noexcept -> const T *
            {
                return this->arr;
            }

            __device__ constexpr auto cbegin() const noexcept -> const T *
            {
                return this->arr;
            }

            __device__ constexpr auto end() noexcept -> T *
            {
                return utility::next(this->arr, this->arr_sz);
            }

            __device__ constexpr auto end() const noexcept -> const T *
            {
                return utility::next(this->arr, this->arr_sz);
            }

            __device__ constexpr auto cend() const noexcept -> const T *
            {
                return utility::next(this->arr, this->arr_sz);
            }

            __device__ constexpr auto at(size_t i) noexcept -> T&
            {
                this->access_guard(i);

                return this->arr[i];
            }

            __device__ constexpr auto at(size_t i) const noexcept -> const T&
            {
                this->access_guard(i);

                return this->arr[i];
            }

            __device__ constexpr auto operator[](size_t i) noexcept -> T&
            {
                return this->at(i);
            }

            __device__ constexpr auto operator[](size_t i) const noexcept -> const T&
            {
                return this->at(i);
            }

            __device__ constexpr auto size() const noexcept -> size_t
            {
                return this->arr_sz;
            }

            __device__ constexpr auto capacity() const noexcept -> size_t
            {
                return this->arr_cap;
            }

            __device__ static constexpr auto max_size() noexcept -> size_t
            {
                return cuda::std::numeric_limits<size_t>::max();
            }

            __device__ constexpr auto empty() const noexcept -> bool
            {
                return this->arr_sz == 0u;
            }

            __device__ constexpr auto data() noexcept -> T *
            {
                return this->arr;
            }

            __device__ constexpr auto data() const noexcept -> const T *
            {
                return this->arr;
            }

            __device__ constexpr auto operator <(const self& other) const noexcept -> bool
            {
                size_t common_sz    = utility::min(this->size(), other.size());

                for (size_t i = 0u; i < common_sz; ++i)
                {
                    if (this->at(i) < other[i])
                    {
                        return true;
                    }

                    if (other[i] < this->at(i))
                    {
                        return false;
                    }
                }

                if (this->size() < other.size())
                {
                    return true;
                }

                return false;
            }

            __device__ constexpr auto operator >=(const self& other) const noexcept -> bool
            {
                return !this->operator <(other);
            }

            __device__ constexpr auto operator >(const self& other) const noexcept -> bool
            {
                size_t common_sz    = utility::min(this->size(), other.size());

                for (size_t i = 0u; i < common_sz; ++i)
                {
                    if (this->at(i) > other[i])
                    {
                        return true;
                    }

                    if (other[i] > this->at(i))
                    {
                        return false;
                    }
                }

                if (this->size() > other.size())
                {
                    return true;
                }

                return false;
            }

            __device__ constexpr auto operator <=(const self& other) const noexcept -> bool
            {
                return !this->operator >(other);
            }

            __device__ constexpr auto operator ==(const self& other) const noexcept -> bool
            {
                if (this->size() != other.size())
                {
                    return false;
                }

                for (size_t i = 0u; i < this->size(); ++i)
                {
                    if (this->at(i) != other[i])
                    {
                        return false;
                    }
                }

                return true;
            }

            __device__ constexpr auto operator !=(const self& other) const noexcept -> bool
            {
                return !this->operator ==(other);
            }

        private:

            __device__ constexpr void free_resource() noexcept
            {
                cuda_management::device_memory::std_delete_array(Allocator{}, this->arr);
            }

            __device__ constexpr void move_data_to_capacity_of(size_t new_cap)
            {
                if (new_cap < this->arr_sz)
                {
                    assert(false);
                }

                T * new_arr = cuda_management::device_memory::std_new_array<T>(Allocator{}, new_cap);

                for (size_t i = 0u; i < arr_sz; ++i)
                {
                    new_arr[i] = this->arr[i];
                }

                cuda_management::device_memory::std_delete_array(Allocator{}, this->arr);

                this->arr       = new_arr;
                this->arr_cap   = new_cap;
            }

            __device__ constexpr void access_guard(size_t i) const
            {
                if (i >= this->arr_sz)
                {
                    assert(false);
                }
            }
    };
}

#endif
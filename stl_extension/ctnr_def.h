#ifndef __DG_CONTAINER_DEF_H__
#define __DG_CONTAINER_DEF_H__

//define HEADER_CONTROL 5

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <allocation_base/global_allocator.h>
#include "datastructure.h"
#include "hasher.h"
#include <deque>
#include <stdint.h>
#include <stdlib.h>

namespace ctnr_def
{
    template <class T>
    using container_hasher          = hasher::default_hasher<T>;

    template <class T>
    using container_equal_to        = hasher::default_equal_to<T>;

    template <class T>
    using unordered_set             = std::unordered_set<T, container_hasher<T>, container_equal_to<T>, allocation_base::global_allocator::NoExceptAllocator<T>>;

    template <class T>
    using unordered_unstable_set    = datastructure::unordered_map_variants::unordered_node_set<T, std::size_t, container_hasher<T>, container_equal_to<T>, allocation_base::global_allocator::NoExceptAllocator<T>>;

    template <class T>
    using cyclic_unordered_node_set = datastructure::unordered_map_variants::cyclic_unordered_node_set<T, std::size_t, container_hasher<T>, container_equal_to<T>, allocation_base::global_allocator::NoExceptAllocator<T>>;

    template <class Key, class Value>
    using unordered_map             = std::unordered_map<Key, Value, container_hasher<Key>, container_equal_to<Key>, allocation_base::global_allocator::NoExceptAllocator<std::pair<const Key, Value>>>;

    template <class Key, class Value>
    using unordered_unstable_map    = datastructure::unordered_map_variants::unordered_node_map<Key, Value, std::size_t, std::integral_constant<bool, true>, container_hasher<Key>, container_equal_to<Key>, allocation_base::global_allocator::NoExceptAllocator<std::pair<const Key, Value>>>;
    
    template <class Key, class Value>
    using cyclic_unordered_node_map = datastructure::unordered_map_variants::cyclic_unordered_node_map<Key, Value, container_hasher<Key>, std::size_t, std::integral_constant<bool, true>, container_equal_to<Key>, allocation_base::global_allocator::NoExceptAllocator<std::pair<const Key, Value>>>;

    template <class T>
    using vector                    = std::vector<T, allocation_base::global_allocator::NoExceptAllocator<T>>;

    template <class T>
    using deque                     = std::deque<T, allocation_base::global_allocator::NoExceptAllocator<T>>;

    template <class T>
    using pow2_cyclic_queue         = datastructure::cyclic_queue::pow2_cyclic_queue<T, allocation_base::global_allocator::NoExceptAllocator<T>>;

    using string                    = std::basic_string<char, std::char_traits<char>, allocation_base::global_allocator::NoExceptAllocator<char>>;
}

#endif
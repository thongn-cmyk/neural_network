
#include "heap.h"
#include <iostream>
#include "assert.h"
#include <random>
#include <chrono>

//I couldn't believe that 4 years later, we are here
//yes, it's a conjunction of HeapAllocation and BucketAllocation
//essentially we'd have to pass a threshold to trigger HeapAllocation
//it's not just hard, it's extremely hard to achieve 4% of total memory for management overhead

class BumpAllocator
{
    private:

        std::vector<bool> bc;
        size_t sz;

    public:

        BumpAllocator(std::vector<bool> bc, size_t sz): bc(std::move(bc)), sz(sz){}

        std::optional<std::pair<size_t, size_t>> seek_next_from(size_t offs){
            
            size_t first = offs; 

            while (first < sz && !bc[first]){
                ++first;
            }

            if (first >= sz){
                return std::nullopt;
            }

            size_t last = first + 1;

            while (last < sz && bc[last]){
                ++last;
            }

            return std::pair<size_t, size_t>{first, last - first};

        } 

        bool has_block(size_t block_sz){

            size_t offs = 0;

            while (true){

                auto nnext  = seek_next_from(offs);

                if (!nnext){
                    return false;
                }

                if (nnext.value().second >= block_sz){
                    return true;
                } 

                offs = nnext.value().first + nnext.value().second;

            }

        }

        void block(size_t offs, size_t block_sz){

            for (size_t i = offs; i < offs + block_sz; ++i){
                bc[i] = false;
            }

        }

        void unblock(size_t offs, size_t block_sz){

            for (size_t i = offs; i < offs + block_sz; ++i){
                bc[i] = true;
            }

        }

        bool is_empty(size_t offs, size_t block_sz){

            for (size_t i = offs; i < offs + block_sz; ++i){
                if (!bc[i]){
                    return false;
                }
            }

            return true;
        }

};

class SplitAllocator{

    private:

        BumpAllocator l;
        BumpAllocator r; 
        size_t sz; 

    public:

        SplitAllocator(size_t sz): l(std::vector<bool>(sz >> 1, true), sz >> 1),
                                   r(std::vector<bool>(sz >> 1, true), sz >> 1),
                                   sz(sz){}
        
        bool has_block(size_t block_sz){

            return l.has_block(block_sz) || r.has_block(block_sz);
        }

        void block(size_t offs, size_t block_sz){

            if (offs < (sz >> 1)){
                l.block(offs, block_sz);
            } else{
                r.block(offs - (sz >> 1), block_sz);
            }
        }

        void unblock(size_t offs, size_t block_sz){

            if (offs < (sz >> 1)){
                l.unblock(offs, block_sz);
            } else{
                r.unblock(offs - (sz >> 1), block_sz);
            }
        }

        bool is_empty(size_t offs, size_t block_sz){

            if (offs < (sz >> 1)){
                return l.is_empty(offs, block_sz);
            } else{
                return r.is_empty(offs - (sz >> 1), block_sz);
            }
        }
};

using interval_type             = dg_heap::heap::types::interval_type;

void clear(std::vector<interval_type>& intvs, dg_heap::heap::core::Allocatable& allocatable, SplitAllocator& ballocator){

    for (const auto& intv: intvs){
        allocatable.free(intv);
        ballocator.unblock(intv.first, intv.second + 1);
    }

    intvs.clear();
}

void random_clear(std::vector<interval_type>& intvs, dg_heap::heap::core::Allocatable& allocatable, SplitAllocator& ballocator){

    static auto random_device   = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937{});
    size_t CLEAR_SZ = 0;
    
    if (intvs.size() != 0){
        CLEAR_SZ = random_device() % intvs.size();
    }
    
    for (size_t i = 0; i < CLEAR_SZ; ++i){
        
        size_t rm_idx   = random_device() % intvs.size();
        size_t last_idx = intvs.size() - 1;

        std::swap(intvs[rm_idx], intvs[last_idx]);
        allocatable.free(intvs.back());
        ballocator.unblock(intvs.back().first, intvs.back().second + 1);

        intvs.pop_back();
    }
}

void dirtify(dg_heap::heap::core::Allocatable& allocator){

    std::vector<interval_type> intervals{};

    while (true){
        auto intv = allocator.alloc(5);
        if (!intv){
            break;
        }
        intervals.push_back(*intv);
    }

    auto rand_dev   = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937{});
    auto rm_sz      = rand_dev() % intervals.size();

    while (rm_sz > 0){
        auto rm_idx = rand_dev() % intervals.size();
        std::swap(intervals[rm_idx], intervals.back());
        allocator.free(intervals.back());
        intervals.pop_back();
        --rm_sz;
    } 

}

//it's time we finna use our bad guys
//because the other allocator takes too much space, potentially x20 -> x30 of what we'd want to use, or what we'd have
//so I guess that dg::vector<>, dg::string etc. are kept, but for loops + non-expensive, non-persistent, scope-friendly operation
//the only thing that has been stopping us from doing so is the range-compatibility of this allocator
//it just seems to me that we dont have a reasonable way to not over-allocate things, and keep it to an exponential boundary

int main(){

    using namespace std::chrono;

    //I feel lazy today
    //I'll be back to implement this allocator correctly

    constexpr size_t HEIGHT         = 12;
    constexpr size_t BASE_LENGTH    = size_t{1} << (HEIGHT - 1);
    constexpr size_t COUT_SZ        = size_t{1} << 14;

    // std::vector<bool> bc{};
    // bc.resize(BASE_LENGTH, true);
    // BumpAllocator b_allocator(std::move(bc), BASE_LENGTH);

    SplitAllocator b_allocator(BASE_LENGTH);

    std::shared_ptr<char[]> buf = dg_heap::heap::user_interface::make(HEIGHT);

    // std::cout << l;
    std::shared_ptr<dg_heap::heap::core::Allocatable> allocator  = dg_heap::heap::user_interface::get_allocator_x(buf.get());
    auto random_device  = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937{});
    std::vector<interval_type> intvs{};

    size_t test_sz = 0u;

    while (true){

        size_t block_sz = (random_device() % BASE_LENGTH) + 1;
        std::optional<interval_type> intv = allocator->alloc(block_sz);
        
        if (!bool{intv} && b_allocator.has_block(block_sz)){
            
            allocator  = dg_heap::heap::user_interface::get_allocator_x(buf.get());
            intv = allocator->alloc(block_sz);

            if (!bool{intv} && b_allocator.has_block(block_sz)){
                std::cout << "mayday1" << std::endl;
                assert(false);
            }
        } 

        if (!bool{intv}){
            random_clear(intvs, *allocator, b_allocator);
            continue;
        }

        if (bool{intv} && !b_allocator.is_empty(intv->first, intv->second + 1)){
            std::cout << "mayday2" << std::endl;
            assert(false);
        }

        b_allocator.block(intv->first, intv->second + 1);
        intvs.push_back(intv.value());

        if (test_sz % COUT_SZ == 0u)
        {
            std::cout << test_sz << "\n";
        }

        test_sz++;
    }
}
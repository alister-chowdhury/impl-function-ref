#include <cstdlib>
#include <utility>


struct daisy_chain
{
    daisy_chain* next = nullptr;
    char*  data;
};



struct allocator
{
    daisy_chain*    nodes = nullptr;
    daisy_chain*    freenodes = nullptr;
};


void add_nodes(allocator* a, const unsigned int count)
{
    char* data = (char*)malloc(64 * count);
    daisy_chain* chains = (daisy_chain*)malloc(sizeof(daisy_chain)*count);
    for(unsigned int i=0; i<(count-1); ++i)
    {
        chains[i] = { &chains[i+1], &data[64*i] };
    }
    chains[count-1] = {a->nodes, &data[(count-1)*64]};
    a->nodes = chains;
}

void freeA(allocator* a, char* d)
{
    daisy_chain* nd = a->freenodes;
    a->freenodes = nd->next;
    nd->data = d;
    nd->next = a->nodes;
    a->nodes = nd;
}

char* allocA(allocator* a)
{
    if(a->nodes == nullptr) { add_nodes(a, 64); }
    daisy_chain* nd = a->nodes;
    a->nodes = nd->next;
    nd->next = a->freenodes;
    a->freenodes = nd;
    return nd->data;
}





#include <cstdint>
#include <cstdio>

#if defined(_MSC_VER) && defined(_M_X64)
    #include <intrin.h>

#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #include <x86intrin.h>

#else
    #error "Only really intended for x64 gcc/clang/msc"
#endif




// uint64_t cycles = measure_cycles([](){ some_function(); });
// NB: You'll need to adjust for any overheads involved (typically) calling
//     an empty function a few times, then subtracting the average etc.
template <typename CallbackT>
inline uint64_t measure_cycles(CallbackT callback) {

#ifdef _MSC_VER
    int64_t first = __rdtsc();
    callback();
    return __rdtsc() - first;
#else
    int64_t first = _rdtsc();
    callback();
    return _rdtsc() - first;
#endif

}



int main(void)
{

    allocator al;
    uint64_t ta, tb, tc, td;
    char *a, *b, *c, *d;

    for(int i=0;i<10;++i)
    {
        ta = measure_cycles([&]{ a = allocA(&al); });
        tb = measure_cycles([&]{ b = allocA(&al); });
        tc = measure_cycles([&]{ c = allocA(&al); });
        td = measure_cycles([&]{ d = allocA(&al); });
        std::printf("ALLOC = %zu %zu %zu %zu\n", ta, tb, tc, td);

        ta = measure_cycles([&]{ freeA(&al, a); });
        tb = measure_cycles([&]{ freeA(&al, b); });
        tc = measure_cycles([&]{ freeA(&al, c); });
        td = measure_cycles([&]{ freeA(&al, d); });
        std::printf("FREE = %zu %zu %zu %zu\n", ta, tb, tc, td);
        std::puts("");
    }


}




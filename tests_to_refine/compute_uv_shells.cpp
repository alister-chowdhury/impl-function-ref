// THIS DOESNT WORK CURRENTLY!


#include <cstdint>
#include <set>
#include <unordered_set>
#include <vector>
#include <array>
#include <algorithm>



std::vector<std::set<uint32_t>>   quadForwardEdgeConnections(
    const uint32_t    polyCount,
    const uint32_t    vertexCount,
    const uint32_t*   vertsPerFace
) {

    std::vector<std::set<uint32_t>> result( vertexCount );
    
    const uint32_t* iter = vertsPerFace;
    const uint32_t* end = vertsPerFace + 4 * polyCount;

    for(; iter < end ; iter += 4 ) {

        std::array<uint32_t, 4> verts { iter[0], iter[1], iter[2], iter[3] };
        std::sort(verts.begin(), verts.end());

        // Only ever write to the smallest vertex id
        // this makes lower vertex ids point to more relatives
        // which in turn means when actually computing the set of connected graphs
        // there are less depths to recurse.
        if(verts[0] != verts[1]) {
            result[verts[0]].insert(&verts[1], &verts[4]);
        }
        else if(verts[0] != verts[2]) {
            result[verts[0]].insert(&verts[2], &verts[4]);
        }
        else if(verts[0] != verts[3]) {
            result[verts[0]].insert(verts[3]);
        }

    }

    return result;

}


inline void computeShellsIter(
    const uint32_t  idx,
    std::vector<std::set<uint32_t>>& forwardVerts,
    std::vector<uint32_t>& writeback
) {

    // Steal the contents of the target forward vertices this prevents the empty check from passing.
    // if another vertex ends up pointing to it.
    auto stolen = std::move(forwardVerts[idx]);
    forwardVerts[idx].clear();

    for(const uint32_t nextIdx : stolen ) {
        writeback.push_back(nextIdx);
        if(forwardVerts[nextIdx].empty()) {
            continue;
        }
        computeShellsIter(idx, forwardVerts, writeback);
    }
}


std::vector<std::vector<uint32_t>>   computeShells(
    const uint32_t    polyCount,
    const uint32_t    vertexCount,
    const uint32_t*   vertsPerFace
) {

    std::vector<std::set<uint32_t>> forwardVerts = quadForwardEdgeConnections(
        polyCount,
        vertexCount,
        vertsPerFace  
    );


    std::vector<std::vector<uint32_t>> result;

    uint32_t i = 0;
    const uint32_t size = forwardVerts.size();

    for(; i<size; ++i) {
        if(forwardVerts[i].empty()) {
            continue;
        }
        result.emplace_back();
        result.back().push_back(i);
        computeShellsIter(i, forwardVerts, result.back());

    }

    return result;
}



template<typename IterT>
auto get_max(IterT s, IterT e) {
    auto c = *s++;
    while(s < e) {
        auto d = *s;
        if(d > c) { c = d; }
        ++s;
    }
    return c;
}


#include <cstdio>

int main(void) {


    const uint32_t vertsPerFace[] = {
        0, 1, 3, 4,
        0, 1, 5, 6,
        7, 8, 9, 10,
    };

    auto result = computeShells(
        sizeof(vertsPerFace)/(4*sizeof(uint32_t)),
        get_max(vertsPerFace, vertsPerFace+(sizeof(vertsPerFace)/sizeof(uint32_t)))+1,
        vertsPerFace
    );

    int i = 0;
    int sz = result.size();


    for(; i<sz; ++i) {
        std::printf("%i:\n", i);
        std::puts("   ");
        for(const uint32_t k : result[i]) {
            std::printf("%u ", k);
        }
        std::puts("\n");
    }


}

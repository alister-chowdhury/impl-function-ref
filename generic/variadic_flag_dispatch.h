// This contains a helper function `forward_flags` to forward booleans to a series of std::true_type and std::false_type
// allowing them be used as configuration for compile-time specialisations.
//
// This is done by generating an indexable table, and using the boolean sequence as an ID.
// the result is (atleast on GCC and Clang) an efficient switch-like lookup.
//
// Example usage:
//
//
//
// template<
//     // Mesh info
//     bool has_normals,
//     // Texture info
//     bool write_normals,
//     bool write_curvature
// >
// struct Rasterizer { ... };
//
//
//
// void raster_mesh(const MeshData* mesh, TextureData* texture_data) {
//
//     const bool has_normals = mesh->normals == nullptr;
//     const bool write_normals = texture_data->normals == nullptr;
//     const bool write_curvature = texture_data->curvature == nullptr;
//
//     forward_flags(
//
//         [&](auto... flags) {
//             Rasterizer<decltype(flags)::value...> rasterizer;
//             rasterizer.mesh = mesh;
//             rasterizer.texture_data = texture_data;
//             rasterizer.run();
//         },
//
//         // Mesh info
//         has_normals,
//
//         // Texture info
//         write_normals,
//         write_curvature
//     );
// }
//


#include <type_traits>
#include <utility>


namespace detail {


// Helper functions to generate indexable ids based upon boolean sequences
inline unsigned long bool_to_id() { return 0; }

template<typename... Bools>
inline unsigned long bool_to_id(const bool a, Bools... others) {
    return (bool_to_id(others...) << 1) | ((unsigned long)a);
}

template<unsigned long size>
inline constexpr unsigned long max_bool_id() { return (1u << size) - 1; }


// Given a boolean index, this unpacks the bits into a series of true_types and false_types
// before calling the provided function.
template<unsigned long size, unsigned long bool_id, typename F, typename... Flags>
struct forward_flags_call_helper {
    using Func = typename std::conditional_t<
        bool(bool_id & 1),
        forward_flags_call_helper<size-1, (bool_id >> 1), F, Flags..., std::true_type>,
        forward_flags_call_helper<size-1, (bool_id >> 1), F, Flags..., std::false_type>
    >::Func;

};

template<unsigned long bool_id, typename F, typename... Flags>
struct forward_flags_call_helper<0, bool_id, F, Flags...> {
    using Func = forward_flags_call_helper<0, bool_id, F, Flags...>;
    inline static void call(F&& f) { f(Flags{}...); }
};

template<unsigned long size, unsigned long bool_id, typename F>
inline void forward_flags_call(F&& f) {
    forward_flags_call_helper<size, bool_id, F>::Func::call(std::forward<F>(f));
}

template<
    unsigned long size,
    unsigned long... bool_ids,
    typename F
>
inline void forward_flags_call_dispatch(
    unsigned long local_id,
    std::integer_sequence<unsigned long, bool_ids...>,
    F&& f
) {
    using fcall = void(*)(F&& f);
    const static fcall table[sizeof...(bool_ids)]{
        forward_flags_call<size, bool_ids, F>...
    };
    table[local_id](std::forward<F>(f));
}


} // namespace detail


// Actual routine
template<typename... Bools, typename F>
inline void forward_flags(F&& f, Bools... bools)
{
    const unsigned long local_id = detail::bool_to_id(bools...);
    detail::forward_flags_call_dispatch<sizeof...(Bools)>(
        local_id,
        std::make_integer_sequence<
            unsigned long,
            detail::max_bool_id<sizeof...(Bools)>() + 1
        >{},
        std::forward<F>(f)
    );
}

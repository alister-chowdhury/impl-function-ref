/**

// Example usage:

#include <cstdio>

// Integral constant like class, with a static constexpr less than function.
template<int x>
struct entry {
    const static int value = x;
    template<typename T> static constexpr bool lt() { return value < T::value; }

    const static void view_number() {
        std::printf("%i\n", x);
    }
};

using Unsorted = Container<
    entry<5>, entry<1>, entry<2>, entry<64>, entry<5>,
    entry<0>, entry<10>, entry<2>, entry<4>, entry<15>,
    entry<0>, entry<1>, entry<22>, entry<4>, entry<5>
>;
using Sorted = templated_sort<Unsorted>; // <--- the actual thing you call


// Basically just call view_number() on each entry.
template<typename... Entries>
void display_container(Container<Entries...>) {
    using T = int[];
    (void)T{
        ((void)Entries::view_number(), 0)...
    };
}

int main(void) {
    std::printf("Unsorted:\n");
    display_container(Unsorted{});

    std::printf("\nSorted:\n");
    display_container(Sorted{});
}

**/

#include <cstddef>
#include <type_traits>

template<typename... EntriesT>
struct Container {};

template<
    typename LtContainer,
    typename EqContainer,
    typename GtContainer,
    typename...
>
struct tmplt_qsort_impl_build_containers {
    using lt_container = LtContainer;
    using eq_container = EqContainer;
    using gt_container = GtContainer;
};

template<
    typename LtContainer,
    typename EqContainer,
    typename GtContainer,
    typename Pivot,
    typename Current,
    typename... Nexts
>
struct tmplt_qsort_impl_build_containers<
    LtContainer, EqContainer, GtContainer, Pivot, Current, Nexts...
> {
    
    const static bool lt = Current::template lt<Pivot>();
    const static bool gt = Pivot::template lt<Current>();
    const static bool eq = !(lt | gt);

    template<bool flag, typename Target, typename... Existings>
    static constexpr std::conditional_t<
        flag,
        Container<Target, Existings...>,
        Container<Existings...>
    > join_container_if(Container<Existings...>);

    using next_iter_t = tmplt_qsort_impl_build_containers<
        decltype(join_container_if<lt, Current>(LtContainer{})),
        decltype(join_container_if<eq, Current>(EqContainer{})),
        decltype(join_container_if<gt, Current>(GtContainer{})),
        Pivot,
        Nexts...
    >;

    using lt_container = typename next_iter_t::lt_container;
    using eq_container = typename next_iter_t::eq_container;
    using gt_container = typename next_iter_t::gt_container;

};


template<typename ContainerT>
struct tmplt_qsort_impl {

    template<typename T, typename... Others>
    static constexpr T get_container_first(Container<T, Others...>);

    template<typename Pivot, typename... Entries>
    static constexpr tmplt_qsort_impl_build_containers<
        Container<>, Container<>, Container<>,
        Pivot, Entries...
    > split_container(Container<Entries...>);

    template<typename... Firsts, typename... Seconds, typename... Thirds>
    static constexpr Container<Firsts..., Seconds..., Thirds...> combine(
        Container<Firsts...>,
        Container<Seconds...>,
        Container<Thirds...>
    );

    // Use the first value as the pivot (maybe make this the middle value in the future)
    using pivot = decltype(get_container_first(ContainerT{}));
    // Split containers into lt, eq, gt
    using split_containers = decltype(split_container<pivot>(ContainerT{}));
    // Recurse and join the results
    using type = decltype(combine(
        typename tmplt_qsort_impl<typename split_containers::lt_container>::type{},
        typename split_containers::eq_container{},
        typename tmplt_qsort_impl<typename split_containers::gt_container>::type{}
    ));

};

// Empty + Single entry containers.
template<>
struct tmplt_qsort_impl<Container<>> {
    using type = Container<>;
};
template<typename T>
struct tmplt_qsort_impl<Container<T>> {
    using type = Container<T>;
};


// Public interface
template<typename ContainerT>
using templated_sort = typename tmplt_qsort_impl<ContainerT>::type;

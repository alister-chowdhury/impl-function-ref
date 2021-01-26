#include <utility>


using function_type = decltype(std::declval<void (&)(const char*)noexcept>());


function_type get_best_func();
function_type func = get_best_func();


void doit(const char* s) noexcept {
    func(s);
}



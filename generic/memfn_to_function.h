#pragma once


#include <functional>


template<typename Class, typename Ret, typename... Args>
std::function<Ret(Args...)> memfn_to_function(
    Class* self,
    Ret (Class::*meth)(Args...)
) {
    return [self, meth](Args&&... args) -> Ret
    {
        return (self->* meth)(std::forward<Args>(args)...);
    };
}

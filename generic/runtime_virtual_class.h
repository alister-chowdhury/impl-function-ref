#pragma once

// This containers the helper createVirtualFunction for creating runtime  virtual-tables
// using a base class as the actual implementation detail.
//
// (May or may not be useful in practice)
//
// example usage:
//
//   #include <cstdio>
//
//   struct PointerTable
//   {
//       void (* destroy) (void*);
//       bool (* dispatchMeshShader)(void*, int);
//   };
//
//   struct VirtualClass
//   {
//       ~VirtualClass() { table.destroy(ctx); }
//       bool dispatchMeshShader(int count) { return table.dispatchMeshShader(ctx, count); }
//
//       void*           ctx;
//       PointerTable    table; // in real-life this would probably be a pointer in of itself
//   };
//
//   struct RealClass
//   {
//       template<bool hasMeshShaderSupport>
//       bool dispatchMeshShader(int count)
//       {
//           if constexpr(!hasMeshShaderSupport) { return false; }
//           // do real work
//           return true;
//       }
//   };
//
//   VirtualClass createVirtualClass(bool hasMeshShaderSupport)
//   {
//       VirtualClass cls;
//       cls.ctx = (void*) new RealClass;
//       cls.table.destroy = +[](void* ctx) { delete ((RealClass*)ctx); };
//       if(hasMeshShaderSupport)
//       {
//           cls.table.dispatchMeshShader = createVirtualFunction<&RealClass::template dispatchMeshShader<true>>();
//       }
//       else
//       {
//           cls.table.dispatchMeshShader = createVirtualFunction<&RealClass::template dispatchMeshShader<false>>();
//       }
//       return cls;
//   }
//
//   int main(void)
//   {
//       VirtualClass a = createVirtualClass(true);
//       VirtualClass b = createVirtualClass(false);
//       std::printf("a.dispatchMeshShader = %i\n", int(a.dispatchMeshShader(10)));
//       std::printf("b.dispatchMeshShader = %i\n", int(b.dispatchMeshShader(10)));
//   }
//


#include <utility>

#if defined(_MSC_VER)
    #define FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
    #define FORCEINLINE __attribute__((always_inline)) inline
#else
    #define FORCEINLINE inline
#endif


namespace detail
{

template<auto method, typename Class, typename Ret, typename... Args>
FORCEINLINE auto createVirtualFunctionBody(Ret (Class::*dummy)(Args...))
{
    using Signature = Ret(*)(void*, Args...);
    auto func = +[](void* ctx, Args... args)
    {
        Class* cls = (Class*)ctx;
        return ((Class*)ctx->*method)(std::forward<Args>(args)...);
    };
    return (Signature)func;
}

} // namespace detail

template<auto method>
FORCEINLINE auto createVirtualFunction()
{
    return detail::createVirtualFunctionBody<method>(method);
}


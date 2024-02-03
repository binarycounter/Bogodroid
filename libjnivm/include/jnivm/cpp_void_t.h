#pragma once
#include <type_traits>

namespace jnivm {
#ifdef __cpp_lib_void_t
    template<typename... Ts>
    using void_t = std::void_t<Ts...>;
#else
    template<typename... Ts> struct make_void { typedef void type;};
    template<typename... Ts> using void_t = typename make_void<Ts...>::type;
#endif
}
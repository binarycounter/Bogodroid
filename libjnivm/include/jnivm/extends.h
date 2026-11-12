#pragma once
#include "object.h"
#include "env.h"
#include "arrayBase.h"
#include <typeindex>
#include <stdexcept>

namespace jnivm {

    namespace impl {
        template<class, class... BaseClasses> class Extends : public virtual BaseClasses... {
        public:
            using BaseClasseTuple = std::tuple<BaseClasses...>;
            template<class T>
            using ArrayBaseType = ArrayBase<T, BaseClasses...>;
            static std::vector<std::shared_ptr<Class>> GetBaseClasses(ENV* env) {
                std::vector<std::shared_ptr<Class>> ret = { env->GetVM()->typecheck[typeid(BaseClasses)]... };
#ifndef NDEBUG
                for(size_t i = 0, size = ret.size(); i < size; ++i) {
                    if(!ret[i]) {
                        static const std::type_info* const types[] = {&typeid(BaseClasses)...};
                        throw std::runtime_error("Fatal BaseClass not registred!" + std::string(types[i]->name()));
                    }
                }
#endif
                return ret;
            }
        };

        // template<class Base, class Result, class...Interfaces> struct ExtendsResolver;
        // template<class Base, class Result, class Interface, class...Interfaces> struct ExtendsResolver<Base, Result, Interface, Interfaces...> {
        //     using Resolver = std::conditional_t<std::is_base_of<Interface, Base>::value, typename ExtendsResolver<Base, Result, Interfaces...>::Resolver, typename ExtendsResolver<Base, decltype(std::tuple_cat(Result{}, std::tuple<Interface>{})), Interfaces...>::Resolver>;
        // };
        // template<class Base, class...Interfaces> struct ExtendsResolver<Base, std::tuple<Interfaces...>> {
        //     using Resolver = impl::Extends<void, Base, Interfaces...>;
        // };
    }
    template<class Base = Object, class... Interfaces>
    using Extends = /* typename impl::ExtendsResolver<Base, std::tuple<>, Interfaces...>::Resolver; */impl::Extends<void, Base, Interfaces...>;
}
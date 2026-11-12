#pragma once
#include <memory>

namespace jnivm {
    template<class T> struct remove_shared {
        using Type = T;
    };
    template<class T> struct remove_shared<std::shared_ptr<T>> {
        using Type = T;
    };
    template<class T> struct remove_shared<std::weak_ptr<T>> {
        using Type = T;
    };
    template<class T> struct remove_shared<T*> {
        using Type = T;
    };
    template<class T> struct remove_shared<T&> {
        using Type = T;
    };
}
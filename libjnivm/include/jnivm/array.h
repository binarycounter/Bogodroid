#pragma once
#include "object.h"
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include <jni.h>
#include "arrayBase.h"
#include <type_traits>
#include "removeshared.h"

namespace jnivm {

    namespace impl {
        template<class T, class OrgType = void, bool primitive=!std::is_class<T>::value>
        class Array;

        template<> class Array<void> : public Object {
        private:
            jsize length = 0;
            void* data = nullptr;
        protected:
            Array() {}
            Array(void* data, jsize length) : data(data), length(length) {}
            void setArray(void* data) {
                this->data = data;
            }
        public:
            using Type = void;
            inline void* getArray() {
                return data;
            }
            inline const void* getArray() const {
                return data;
            }
            inline const jsize getSize() const {
                return length;
            }
            virtual ~Array() {

            }
        };

        template<class T, class OrgType, bool primitive>
        class Array : public Array<void> {
        private:
            bool owner = true;
        public:
            using Type = T;
            Array(jsize length) : Array<void>(new T[length], length) {}
            Array() : Array<void>(nullptr, 0) {
                owner = false;
            }
            Array(const std::vector<T> & vec) : Array<void>(new T[vec.size()], vec.size()) {
                if(vec.size() > 0) {
                    memcpy(getArray(), vec.data(), sizeof(T) * vec.size());
                }
            }

            Array(T* data, jsize length, bool copy) : Array<void>(copy ? new T[length] : data, length) {
                if(copy) {
                    memcpy(getArray(), data, sizeof(T) * length);
                } else {
                    owner = false;
                }
            }

            Array(T* data, jsize length) : Array(data, length, true) {}

            virtual ~Array() {
                if(owner) {
                    delete[] getArray();
                }
            }

            inline T* getArray() {
                return (T*)Array<void>::getArray();
            }
            inline const T* getArray() const {
                return (const T*)Array<void>::getArray();
            }
            inline T& operator[](jint i) {
                return getArray()[i];
            }
            inline const T& operator[](jint i) const {
                return getArray()[i];
            }
        };
        template<class> struct array_type_t {};

        template<class T, bool isconst>
        struct Arrayguard {
            Array<T,void,false> & ref;
            jint i;
            template<class Z>
            operator std::shared_ptr<Z>() const {
                return ref.Get(i, array_type_t<T>{});
            }
            operator T*() const {
                return ref.Get(i, array_type_t<T>{}).get();
            }
            operator bool() const {
                return ref.Get(i, array_type_t<T>{}).get() != nullptr;
            }
            T& operator*() const {
                return *ref.Get(i, array_type_t<T>{}).get();
            }
            T* operator->() const {
                return ref.Get(i, array_type_t<T>{}).get();
            }
            template<class Z>
            Arrayguard &operator=(std::shared_ptr<Z> other) {
                static_assert(std::is_base_of<T, Z>::value, "Invalid Assignment");
                ref.Set(i, other);
                return *this;
            }

            template<class Z>
            Arrayguard &operator=(Z* p) {
                static_assert(std::is_base_of<T, Z>::value, "Invalid Assignment");
                if(p) {
#if defined(__cpp_lib_enable_shared_from_this) && __cpp_lib_enable_shared_from_this >= 201603
                    auto val = p->weak_from_this().lock();
                    ref.Set(i, val ? std::shared_ptr<Z>(val, p) : std::make_shared<Z>(*p));
#else
                    ref.Set(i, std::make_shared<Z>(*p));
#endif
                } else {
                    ref.Set(i, nullptr);
                }
                return *this;
            }
            template<class Z>
            Arrayguard &operator=(Z& p) {
                static_assert(std::is_base_of<T, Z>::value, "Invalid Assignment");
#if defined(__cpp_lib_enable_shared_from_this) && __cpp_lib_enable_shared_from_this >= 201603
                auto val = p.weak_from_this().lock();
                ref.Set(i, val ? std::shared_ptr<Z>(val, &p) : std::make_shared<Z>(p));
#else
                ref.Set(i, std::make_shared<Z>(p));
#endif
                return *this;
            }
        };

        template<class T>
        struct Arrayguard<T, true> {
            const Array<T,void,false> & ref;
            jint i;
            operator std::shared_ptr<T>() const {
                return ref.Get(i, array_type_t<T>{});
            }
            operator T*() const {
                return ref.Get(i, array_type_t<T>{}).get();
            }
            operator bool() const {
                return ref.Get(i, array_type_t<T>{}).get() != nullptr;
            }
            T& operator*() const {
                return *ref.Get(i, array_type_t<T>{}).get();
            }
            T* operator->() const {
                return ref.Get(i, array_type_t<T>{}).get();
            }
        };

        template<>
        class Array<Object> : public virtual Array<void> {
        public:
            virtual std::shared_ptr<Object> Get(jint i, array_type_t<Object>) const {
                return ((std::shared_ptr<Object>*) getArray())[i];
            }
            virtual void Set(jint i, const std::shared_ptr<Object>& val) {
                ((std::shared_ptr<Object>*) getArray())[i] = val;
            }
        public:
            Array() : Array<void>(nullptr, 0) {}
            Array(jsize size) : Array<void>(new std::shared_ptr<Object>[size], size) {}
            inline Arrayguard<Object, false> operator[](jint i) {
                return { *this, i };
            }
            inline const Arrayguard<Object, true> operator[](jint i) const {
                return { *this, i };
            }
            virtual ~Array() {
                if(Array<void>::getArray()) {
                    delete[] (std::shared_ptr<Object>*)Array<void>::getArray();
                    Array<void>::setArray(nullptr);
                }
            }
        };

        template<class Y>
        class Array<Y, void, false> : public virtual Y::template ArrayBaseType<Y> {
        public:
            virtual std::shared_ptr<Y> Get(jint i, array_type_t<Y>) const {
                return ((std::shared_ptr<Y>*) Array<void>::getArray())[i];
            }
            virtual void Set(jint i, const std::shared_ptr<Y>& val) {
                ((std::shared_ptr<Y>*) Array<void>::getArray())[i] = val;
            }
        public:
            //using T = std::shared_ptr<Y>;
            //using Type = T;
            Array(jsize length) : Array<void>(new std::shared_ptr<Y>[length], length) {}
            Array() : Array<void>(nullptr, 0) {}

            inline Arrayguard<Y, false> operator[](jint i) {
                return { *this, i };
            }
            inline const Arrayguard<Y, true> operator[](jint i) const {
                return { *this, i };
            }
            virtual ~Array() {
                if(Array<void>::getArray()) {
                    delete[] (std::shared_ptr<Y>*)Array<void>::getArray();
                    Array<void>::setArray(nullptr);
                }
            }
        };

        template<class Y, class Z>
        class Array<Y, Z, false> : public virtual Array<Y, void, false>/* , public virtual Y::template ArrayBaseType<Z> */ {
        public:
            virtual std::shared_ptr<Y> Get(jint i, array_type_t<Y>) const override {
                return ((std::shared_ptr<Z>*) Array<void>::getArray())[i];
            }
            virtual void Set(jint i, const std::shared_ptr<Y>& val) override {
                std::shared_ptr<Z> nval(val, dynamic_cast<Z*>(val.get()));
                if(val && !nval) {
                    throw std::runtime_error("Class Type Exception");
                }
                ((std::shared_ptr<Z>*) Array<void>::getArray())[i] = nval;
            }
        };

        template<class T, class Base, class...others> class ArrayBaseImpl;

        template<class T, class...Base>
        class ArrayBaseImpl<T, std::tuple<Base...>> : public virtual Array<Base, void>... {
            
        };

        template<class Z, class Base, class Y, class...others> class ArrayBaseImpl<Z, Base, Y, others...> : public ArrayBaseImpl<Z, Base, others...> {
            virtual std::shared_ptr<Y> Get(jint i, array_type_t<Y>) const override {
                return ((std::shared_ptr<Z>*) Array<void>::getArray())[i];
            }
            virtual void Set(jint i, const std::shared_ptr<Y>& val) override {
                std::shared_ptr<Z> nval(val, dynamic_cast<Z*>(val.get()));
                if(val && !nval) {
                    throw std::runtime_error("Class Type Exception");
                }
                ((std::shared_ptr<Z>*) Array<void>::getArray())[i] = nval;
            }
        };

        template<class T, class...Y> class ArrayBase : public ArrayBaseImpl<T, std::tuple<Y...>, Y...> {
            
        };
    }

    template<class T> using Array = impl::Array<typename remove_shared<T>::Type>;
}
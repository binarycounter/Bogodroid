#ifndef __JAVAC_H__
#define __JAVAC_H__

#include "baron/baron.h"
#include <fstream>
#include <thread>

void InitJNIJavaClasses(FakeJni::Jvm* vm);

void HookStringExtensions(FakeJni::Jvm* vm);
void HookClassExtensions(FakeJni::Jvm* vm);
void HookIntExtensions(FakeJni::Jvm* vm);
void HookObjectExtensions(FakeJni::Jvm* vm);

namespace jnivm {
namespace java {
    namespace lang {

        class Long : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("java/lang/Long")
            long long value;
            Long(jlong val)
                : value(val)
            {
            }
            long longValue();
        };

        class Boolean : public virtual Object {
        private:
            jboolean value;

        public:
            DEFINE_CLASS_NAME("java/lang/Boolean")

            // The constructor the application is calling
            Boolean(jboolean val);

            // The unboxing method to get the primitive value back
            jboolean booleanValue();

            // Standard static factory method
            static std::shared_ptr<Boolean> valueOf(jboolean val);

            // Standard public static fields
            static std::shared_ptr<Boolean> TRUE_;
            static std::shared_ptr<Boolean> FALSE_;
        };

        class ClassLoader : public FakeJni::JObject {
        private:
            std::weak_ptr<jnivm::Class> clazz;

        public:
            DEFINE_CLASS_NAME("java/lang/ClassLoader")
            ClassLoader(jnivm::Object* clazz);
            static std::shared_ptr<FakeJni::JString> findLibrary(std::shared_ptr<FakeJni::JString> name);
        };
        class StringBuilder : public FakeJni::JObject {
        private:
            FakeJni::JString str;

        public:
            DEFINE_CLASS_NAME("java/lang/StringBuilder")
            StringBuilder();
            std::shared_ptr<StringBuilder> append(std::shared_ptr<FakeJni::JString> str);
            std::shared_ptr<FakeJni::JString> toString();
        };

        // Interface, no implementation
        class Runnable : public virtual FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("java/lang/Runnable")
            virtual void run() = 0;
        };

        // A concrete implementation of the Runnable interface for C++ lambdas.
        class LambdaRunnable : public Runnable {

        private:
            std::function<void()> mFunc;

            // The constructor is made private to force users to go through the factory method.
            LambdaRunnable(std::function<void()> f)
                : mFunc(std::move(f))
            {
            }

        public:
            DEFINE_CLASS_NAME("java/lang/LambdaRunnable", Runnable)

            static std::shared_ptr<LambdaRunnable> Create(std::function<void()> f)
            {
                // Use `new` and wrap in shared_ptr because the constructor is private.
                return std::shared_ptr<LambdaRunnable>(new LambdaRunnable(std::move(f)));
            }

            void run() override
            {
                if (mFunc) {
                    mFunc();
                }
            }
        };

        class Thread : public jnivm::java::lang::Runnable {
        public:
            DEFINE_CLASS_NAME("java/lang/Thread", jnivm::java::lang::Runnable)
            std::thread native_thread;
            std::string name;

            Thread(std::shared_ptr<FakeJni::JString> name);
            Thread(std::shared_ptr<Runnable> runnable);

            // This is the method the app is calling
            virtual void start();

            // The main body of the thread, from the Runnable interface
            void run() override;

            // A method to join the thread
            void join();

        private:
            void _thread_entry_point();
        };

        class System : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("java/lang/System")
            static long nanoTime();
        };

        // Fake method reflect, just so Unity cann call "toString" on it
        namespace reflect {
            class FakeMethod : public FakeJni::JObject {
            public:
                DEFINE_CLASS_NAME("java/lang/reflect/Method")
                FakeMethod(std::shared_ptr<FakeJni::JString> method_name);
                std::shared_ptr<FakeJni::JString> toString();

            private:
                std::shared_ptr<FakeJni::JString> method_name;
            };
        }
    }

    namespace io {
        class InputStream : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("java/io/InputStream")
            std::ifstream* file;
            InputStream(std::shared_ptr<FakeJni::JString> filename);
        };

        class File : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("java/io/File")
            std::shared_ptr<FakeJni::JString> path;
            File(std::shared_ptr<FakeJni::JString> path);
            std::shared_ptr<FakeJni::JString> getPath();
        };
    }

    namespace util {
        class Iterator : public virtual FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("java/util/Iterator")
            virtual bool hasNext();
            virtual std::shared_ptr<FakeJni::JObject> next();
        };

        class Set : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("java/util/Set");
            std::shared_ptr<jnivm::java::util::Iterator> iterator();
        };

        class Map : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("java/util/Map");
            std::shared_ptr<jnivm::java::util::Set> entrySet();
        };

        class List : public virtual FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("java/util/List")
            virtual std::shared_ptr<Iterator> iterator();
            virtual int size();
            virtual void add(std::shared_ptr<FakeJni::JObject> obj){}
            // Internal helper to get element at index, for the iterator
            virtual std::shared_ptr<FakeJni::JObject> get(int index){return 0;}
        };

        class ArrayList : public jnivm::java::util::List {
        private:
            std::vector<std::shared_ptr<FakeJni::JObject>> elements;

        public:
            DEFINE_CLASS_NAME("java/util/ArrayList", jnivm::java::util::List)
            std::shared_ptr<Iterator> iterator() override;
            int size() override;
            void add(std::shared_ptr<FakeJni::JObject> obj) override;
            std::shared_ptr<FakeJni::JObject> get(int index) override;
        };

        class ArrayListIterator : public jnivm::java::util::Iterator {
        private:
            std::shared_ptr<ArrayList> list;
            size_t index;

        public:
            DEFINE_CLASS_NAME("java/util/ArrayList$Iterator", jnivm::java::util::Iterator)
            ArrayListIterator(std::shared_ptr<ArrayList> l);
            bool hasNext() override;
            std::shared_ptr<FakeJni::JObject> next() override;
        };

        class Scanner : public FakeJni::JObject {
        private:
            std::shared_ptr<jnivm::java::io::InputStream> istream;
            FakeJni::JString delimiter = (FakeJni::JString) " ";

        public:
            DEFINE_CLASS_NAME("java/util/Scanner")
            Scanner(std::shared_ptr<jnivm::java::io::InputStream> stream, std::shared_ptr<FakeJni::JString> str);
            std::shared_ptr<Scanner> useDelimiter(std::shared_ptr<FakeJni::JString> str);
            std::shared_ptr<FakeJni::JString> next();
            std::shared_ptr<FakeJni::JString> nextLine();
        };

        class Locale : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("java/util/Locale")
            static std::shared_ptr<jnivm::java::util::Locale> getDefault();
            std::shared_ptr<FakeJni::JString> toLanguageTag();
        };

    }
}
}

// --- Template Metaprogramming helper to detect std::shared_ptr ---
template <typename T>
struct is_shared_ptr : std::false_type { };

template <typename T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type { };
// -----------------------------------------------------------------

template <typename T>
std::shared_ptr<jnivm::java::lang::Object> autobox(T value)
{
    // Path 1: Handle primitive long types
    if constexpr (std::is_same_v<T, jlong> || std::is_same_v<T, long long> || std::is_same_v<T, long>) {
        return std::make_shared<jnivm::java::lang::Long>(value);
    }
    // Path 2: Handle std::shared_ptr types
    else if constexpr (is_shared_ptr<T>::value) {
        // Get the type inside the shared_ptr (e.g., Message)
        using ContainedType = typename T::element_type;

        // Now check if the contained type inherits from Object
        if constexpr (std::is_base_of_v<jnivm::java::lang::Object, ContainedType>) {
            // It's a valid object pointer, so upcast it to the base type.
            return std::static_pointer_cast<jnivm::java::lang::Object>(value);
        } else {
            // This would trigger if you passed something like std::shared_ptr<int>
            static_assert(!std::is_same_v<T, T>, "Cannot autobox a shared_ptr to a non-Object type");
            return nullptr;
        }
    }
    // Path 3: Fallback for unsupported types
    else {
        static_assert(!std::is_same_v<T, T>, "Unsupported type for autoboxing in JNIBridge");
        return nullptr;
    }
}
#endif
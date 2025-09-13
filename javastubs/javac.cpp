#include "javac.h"
#include "../globals.h"
#include "baron/baron.h"
#include "logging.h"

///// Long

long jnivm::java::lang::Long::longValue()
{
    return this->value;
}

///// Boolean

// Initialize the static singleton instances
std::shared_ptr<jnivm::java::lang::Boolean> jnivm::java::lang::Boolean::TRUE_ = std::make_shared<Boolean>(JNI_TRUE);
std::shared_ptr<jnivm::java::lang::Boolean> jnivm::java::lang::Boolean::FALSE_ = std::make_shared<Boolean>(JNI_FALSE);

jnivm::java::lang::Boolean::Boolean(jboolean val)
    : value(val)
{
}

jboolean jnivm::java::lang::Boolean::booleanValue()
{
    return value;
}

// The valueOf factory is the standard way to get a Boolean.
// It's efficient because it reuses the static singleton objects.
std::shared_ptr<jnivm::java::lang::Boolean> jnivm::java::lang::Boolean::valueOf(jboolean val)
{
    return val ? TRUE_ : FALSE_;
}

///// Classloader

jnivm::java::lang::ClassLoader::ClassLoader(jnivm::Object* obj)
{
    verbose("JBRIDGE", "New Classloader for class %s", obj->getClass().getName().c_str());
    this->clazz = obj->clazz;
};

std::shared_ptr<FakeJni::JString> jnivm::java::lang::ClassLoader::findLibrary(std::shared_ptr<FakeJni::JString> name)
{
    verbose("JBRIDGE", "findLibrary stub %s", name.get()->c_str());
    return name;
}

///// StringBuilder

jnivm::java::lang::StringBuilder::StringBuilder()
{
    str = (FakeJni::JString) "";
}

std::shared_ptr<jnivm::java::lang::StringBuilder> jnivm::java::lang::StringBuilder::append(std::shared_ptr<FakeJni::JString> str_to_append)
{
    str = str.append(str_to_append.get()->c_str());
    return std::shared_ptr<jnivm::java::lang::StringBuilder>(this);
}

std::shared_ptr<FakeJni::JString> jnivm::java::lang::StringBuilder::toString()
{
    verbose("JBRIDGE", "toString: %s", str.c_str());
    return std::shared_ptr<FakeJni::JString>(&str);
}

///// InputStream

jnivm::java::io::InputStream::InputStream(std::shared_ptr<FakeJni::JString> filename)
{
    verbose("JBRIDGE", "IOStream opening file %s", filename.get()->c_str());
    std::ifstream* file = new std::ifstream(filename.get()->c_str(), std::ios::binary);
    if (!file->is_open()) {
        verbose("JBRIDGE", "IOStream ERROR on file %s", filename.get()->c_str());
    }
    this->file = file;
}

///// File

jnivm::java::io::File::File(std::shared_ptr<FakeJni::JString> path)
{
    this->path = path;
}

std::shared_ptr<FakeJni::JString> jnivm::java::io::File::getPath()
{
    return path;
}

///// Thread
#include <thread>
jnivm::java::lang::Thread::Thread(std::shared_ptr<FakeJni::JString> name) { }
jnivm::java::lang::Thread::Thread(std::shared_ptr<Runnable> runnable)
{
    // This constructor isn't strictly needed for HandlerThread but is good to have
}

void jnivm::java::lang::Thread::start()
{
    // The start method now creates a std::thread that executes our
    // private, JNI-safe entry point.
    native_thread = std::thread(&Thread::_thread_entry_point, this);
}

void jnivm::java::lang::Thread::_thread_entry_point()
{
    // This is the "wrapper" that runs on the new thread.
    JNIEnv* env = nullptr;
    int result = vm.AttachCurrentThread(&env, nullptr);
    if (result != JNI_OK || !env) {
        verbose("Thread", "Failed to attach thread '%s' to JNI VM.", name.c_str());
        return;
    }

    // Use a try/catch block to guarantee we always detach the thread,
    // even if the user's run() method throws an exception.
    try {
        // Now that the thread is safely attached, call the virtual run() method.
        // This will execute HandlerThread::run() or any other subclass's logic.
        this->run();
    } catch (const std::exception& e) {
        verbose("Thread", "Caught C++ exception in thread '%s': %s", name.c_str(), e.what());
        // You might want to forward this exception to a global handler here.
    } catch (...) {
        verbose("Thread", "Caught unknown exception in thread '%s'", name.c_str());
    }

    // The run() method has finished. Detach the thread before it exits.
    vm.DetachCurrentThread();
}

void jnivm::java::lang::Thread::run()
{
    // Default implementation does nothing. Subclasses (like HandlerThread)
    // will override this to do the actual work.
}

void jnivm::java::lang::Thread::join()
{
    if (native_thread.joinable()) {
        native_thread.join();
    }
}

///// System

long jnivm::java::lang::System::nanoTime()
{
    return time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now()).time_since_epoch().count();
}

///// FakeMethod

jnivm::java::lang::reflect::FakeMethod::FakeMethod(std::shared_ptr<FakeJni::JString> method)
{
    this->method_name = method;
}

std::shared_ptr<FakeJni::JString> jnivm::java::lang::reflect::FakeMethod::toString()
{
    verbose("JBRIDGE", "Reflect: Returning Method name %s", this->method_name.get()->c_str());
    return this->method_name;
}

///// Map

std::shared_ptr<jnivm::java::util::Set> jnivm::java::util::Map::entrySet()
{
    // Return an empty set (so Unity just sees no prefs).
    return std::make_shared<Set>();
}

///// Set

std::shared_ptr<jnivm::java::util::Iterator> jnivm::java::util::Set::iterator()
{
    return std::make_shared<Iterator>();
}

///// Iterator

bool jnivm::java::util::Iterator::hasNext()
{
    return false; // Always empty
}

std::shared_ptr<FakeJni::JObject> jnivm::java::util::Iterator::next()
{
    return nullptr; // Never called if hasNext = false
}
///// Scanner

jnivm::java::util::Scanner::Scanner(std::shared_ptr<jnivm::java::io::InputStream> stream, std::shared_ptr<FakeJni::JString> str)
{
    verbose("JBRIDGE", "initialized Scanner with stream %p and str %s", stream.get(), str.get()->c_str());
    istream = stream;
}

std::shared_ptr<jnivm::java::util::Scanner> jnivm::java::util::Scanner::useDelimiter(std::shared_ptr<FakeJni::JString> str)
{
    verbose("JBRIDGE", "Scanner using delimiter %s", str.get()->c_str());
    delimiter = *str;
    return std::shared_ptr<Scanner>(this);
}

std::shared_ptr<FakeJni::JString> jnivm::java::util::Scanner::next()
{
    std::string str;
    (istream->file)->seekg(0, std::ios::end);
    str.reserve((istream->file)->tellg());
    (istream->file)->seekg(0, std::ios::beg);

    str.assign((std::istreambuf_iterator<char>(*(istream->file))),
        std::istreambuf_iterator<char>());
    verbose("JBRIDGE", "Scanner returning: %s", str.c_str());
    return std::make_shared<FakeJni::JString>(str);
}

std::shared_ptr<FakeJni::JString> jnivm::java::util::Scanner::nextLine()
{
    std::string str;
    (istream->file)->seekg(0, std::ios::end);
    str.reserve((istream->file)->tellg());
    (istream->file)->seekg(0, std::ios::beg);

    str.assign((std::istreambuf_iterator<char>(*(istream->file))),
        std::istreambuf_iterator<char>());
    verbose("JBRIDGE", "Scanner returning: %s", str.c_str());
    return std::make_shared<FakeJni::JString>(str);
}

// Descriptors

BEGIN_NATIVE_DESCRIPTOR(jnivm::java::lang::Long) { FakeJni::Constructor<Long, jlong> {} },
    { FakeJni::Function<&Long::longValue> {}, "longValue", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::java::lang::Boolean) { FakeJni::Constructor<Boolean, jboolean> {} },
    { FakeJni::Function<&Boolean::booleanValue> {}, "booleanValue", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Boolean::valueOf> {}, "valueOf", FakeJni::JMethodID::STATIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::java::lang::ClassLoader) { FakeJni::Constructor<ClassLoader, Object*> {} },
    { FakeJni::Function<&ClassLoader::findLibrary> {}, "findLibrary", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR
    BEGIN_NATIVE_DESCRIPTOR(jnivm::java::lang::StringBuilder) { FakeJni::Constructor<StringBuilder> {} },
    { FakeJni::Function<&StringBuilder::append> {}, "append", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&StringBuilder::toString> {}, "toString", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR
    BEGIN_NATIVE_DESCRIPTOR(jnivm::java::io::InputStream) { FakeJni::Constructor<InputStream, std::shared_ptr<FakeJni::JString>> {} },
    END_NATIVE_DESCRIPTOR
    BEGIN_NATIVE_DESCRIPTOR(jnivm::java::io::File) { FakeJni::Constructor<File, std::shared_ptr<FakeJni::JString>> {} },
    { FakeJni::Function<&File::getPath> {}, "getPath", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::java::util::Map) { FakeJni::Constructor<Map> {} },
    { FakeJni::Function<&Map::entrySet> {}, "entrySet", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR
    BEGIN_NATIVE_DESCRIPTOR(jnivm::java::util::Set) { FakeJni::Constructor<Set> {} },
    { FakeJni::Function<&Set::iterator> {}, "iterator", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::java::util::Iterator) { FakeJni::Constructor<Iterator> {} },
    { FakeJni::Function<&Iterator::hasNext> {}, "hasNext", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Iterator::next> {}, "next", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::java::lang::Runnable) { FakeJni::Function<&Runnable::run> {}, "run", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::java::lang::LambdaRunnable)
        END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::java::lang::System) { FakeJni::Constructor<System> {} },
    { FakeJni::Function<&System::nanoTime> {}, "nanoTime", FakeJni::JMethodID::STATIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::java::lang::Thread) { FakeJni::Constructor<Thread, std::shared_ptr<FakeJni::JString>> {} },
    { FakeJni::Constructor<Thread, std::shared_ptr<jnivm::java::lang::Runnable>> {} },
    { FakeJni::Function<&Thread::start> {}, "start", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Thread::join> {}, "join", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::java::lang::reflect::FakeMethod) { FakeJni::Constructor<FakeMethod, std::shared_ptr<FakeJni::JString>> {} },
    { FakeJni::Function<&FakeMethod::toString> {}, "toString", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::java::util::Scanner) { FakeJni::Constructor<Scanner, std::shared_ptr<jnivm::java::io::InputStream>, std::shared_ptr<FakeJni::JString>> {} },
    { FakeJni::Function<&Scanner::useDelimiter> {}, "useDelimiter", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Scanner::next> {}, "next", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Scanner::next> {}, "nextLine", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    void InitJNIJavaClasses(FakeJni::Jvm* vm)
{
    verbose("JBRIDGE", "Initializing Java JNI Classes");
    vm->registerClass<jnivm::java::lang::Long>();
    vm->registerClass<jnivm::java::lang::Boolean>();
    vm->registerClass<jnivm::java::lang::ClassLoader>();
    vm->registerClass<jnivm::java::lang::StringBuilder>();
    vm->registerClass<jnivm::java::lang::Thread>();
    vm->registerClass<jnivm::java::lang::Runnable>();
    vm->registerClass<jnivm::java::lang::LambdaRunnable>(); // Non-standard, for internal use
    vm->registerClass<jnivm::java::lang::System>();
    vm->registerClass<jnivm::java::lang::reflect::FakeMethod>();
    vm->registerClass<jnivm::java::io::InputStream>();
    vm->registerClass<jnivm::java::io::File>();
    vm->registerClass<jnivm::java::util::Map>();
    vm->registerClass<jnivm::java::util::Set>();
    vm->registerClass<jnivm::java::util::Iterator>();
    vm->registerClass<jnivm::java::util::Scanner>();
}

///// Extensions to built-in Java classes

void HookStringExtensions(FakeJni::Jvm* vm)
{
    verbose("JBRIDGE", "Hooking String Extensions");
    FakeJni::LocalFrame frame(*vm);
    auto stringClass = vm->findClass("java/lang/String");

    // String.equals
    stringClass->HookInstanceFunction(&frame.getJniEnv(), "equals", [](jnivm::ENV* env, jnivm::Object* self, jnivm::Object* obj) {
        verbose("JBRIDGE", "String %s == %s = %d", (*dynamic_cast<FakeJni::JString*>(self)).c_str(), (*dynamic_cast<FakeJni::JString*>(obj)).c_str(), (*dynamic_cast<FakeJni::JString*>(self)) == (*dynamic_cast<FakeJni::JString*>(obj)));
        return (*dynamic_cast<FakeJni::JString*>(self)) == (*dynamic_cast<FakeJni::JString*>(obj));
    });

    // Hook constructor with lambda
    stringClass->Hook(&frame.getJniEnv(), "<init>",
        [](jnivm::ENV* env, jnivm::Class* c, std::shared_ptr<jnivm::Array<jbyte>> bytes,
            std::shared_ptr<jnivm::String> charset) -> std::shared_ptr<jnivm::String> {
            std::string charsetName = charset.get()->asStdString();
            auto byteData = bytes.get()->getArray();
            auto byteSize = bytes.get()->getSize();
            std::string result = "";

            if (charsetName == "UTF-8") {
                result = std::string(reinterpret_cast<const char*>(byteData),
                    byteSize);
            }
            return std::make_shared<jnivm::String>(result);
        });

    // String.getBytes with specified charset
    stringClass->HookInstanceFunction(&frame.getJniEnv(), "getBytes",
        [](jnivm::ENV* env, jnivm::Object* self, std::shared_ptr<jnivm::String> charset)
            -> std::shared_ptr<jnivm::Array<jbyte>> {
            std::string charsetName = charset.get()->asStdString();
            std::string stringValue = (*dynamic_cast<FakeJni::JString*>(self)).asStdString();

            std::vector<jbyte> bytes;

            if (charsetName == "UTF-8") {
                bytes.assign(stringValue.begin(), stringValue.end());
            }

            auto result = std::make_shared<FakeJni::JByteArray>(bytes);
            return result;
        });
}

// void HookReflectExtensions(FakeJni::Jvm* vm)
// {
//     verbose("JBRIDGE", "Hooking Reflect Extensions");
//     FakeJni::LocalFrame frame(*vm);
//     auto methodClass = vm->findClass("java/lang/reflect/Method");

//     methodClass->HookInstanceFunction(&frame.getJniEnv(), "toString", [](jnivm::ENV* env, jnivm::Object* self, jnivm::Object* obj) {
//         verbose("JBRIDGE", "ToString on Reflect Method called");
//         return NULL;
//     });

// }

void HookIntExtensions(FakeJni::Jvm* vm)
{
    verbose("JBRIDGE", "Hooking Int Extensions");
    FakeJni::LocalFrame frame(*vm);
    auto intClass = vm->findClass("java/lang/Integer");

    intClass->Hook(&frame.getJniEnv(), "parseInt", [vm](std::shared_ptr<FakeJni::JString> string) {
        verbose("JBRIDGE", "String Convert: %s", string.get()->c_str());
        return std::stoi(string.get()->asStdString());
    });
}

void HookClassExtensions(FakeJni::Jvm* vm)
{
    verbose("JBRIDGE", "Hooking Class Extensions");
    FakeJni::LocalFrame frame(*vm);
    auto classClass = vm->findClass("java/lang/Class");

    // Class.getClassLoader
    classClass->HookInstanceFunction(&frame.getJniEnv(), "getClassLoader", [](jnivm::ENV* env, jnivm::Object* self) {
        verbose("JBRIDGE", "getClassLoader for Class %s", self->getClass().getName().c_str());
        return std::make_shared<jnivm::java::lang::ClassLoader>(self);
    });

    // Class.forName (static)
    classClass->Hook(&frame.getJniEnv(), "forName", [vm](std::shared_ptr<FakeJni::JString> name, bool b, std::shared_ptr<jnivm::java::lang::ClassLoader> loader) {
        verbose("JBRIDGE", "Class forName %s", name.get()->c_str());
        return vm->findClass(name.get()->c_str());
    });
}

void HookObjectExtensions(FakeJni::Jvm* vm)
{
    verbose("JBRIDGE", "Hooking Object Extensions");
    FakeJni::LocalFrame frame(*vm);
    auto objClass = vm->findClass("java/lang/Object");

    // Object.getClass
    objClass->HookInstanceFunction(&frame.getJniEnv(), "getClass", [](jnivm::ENV* env, jnivm::Object* self) {
        verbose("JBRIDGE", "getClass for Object %s", self->getClass().getName().c_str());
        return env->GetClass(self->getClass().getName().c_str());
    });

    objClass->HookInstanceFunction(&frame.getJniEnv(), "toString", [](jnivm::ENV* env, jnivm::Object* self) {
        verbose("JBRIDGE", "toString for Object %s", self->getClass().getName().c_str());
        return std::make_shared<FakeJni::JString>("I dunno");
    });
}

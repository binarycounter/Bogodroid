#include "toml++/toml.hpp"
extern toml::table config;

#include "android.h"
#include "baron/baron.h"
#include "javac.h"
#include "logging.h"
#include <fstream>
#include <pthread.h>
#include <inttypes.h>

///// Bundle

bool jnivm::android::os::Bundle::containsKey(std::shared_ptr<FakeJni::JString> key)
{

    if (!config["package"].as_table()->contains("mainIntentBundle"))
        return false;

    verbose("JBRIDGE", "Bundle containsKey %s %d", key.get()->c_str(), config["package"].as_table()->contains("mainIntentBundle"));

    return config["package"]["mainIntentBundle"].as_table()->contains(key.get()->c_str());
}

std::shared_ptr<FakeJni::JString> jnivm::android::os::Bundle::getString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> def)
{
    if (!containsKey(key))
        return def;

    verbose("JBRIDGE", "Bundle key %s = %s", key.get()->c_str(), config["package"]["mainIntentBundle"][key.get()->c_str()].value_or<std::string>("").c_str());
    return std::make_shared<FakeJni::JString>(config["package"]["mainIntentBundle"][key.get()->c_str()].value_or<std::string>(""));
}

///// Process

void jnivm::android::os::Process::setThreadPriority(int i, int j) { }

///// Looper

// Key for storing the thread-local Looper object
static pthread_key_t gLooperKey;
static pthread_once_t gLooperKeyOnce = PTHREAD_ONCE_INIT;

// Singleton for the main looper
static std::shared_ptr<jnivm::android::os::Looper> gMainLooper;
static pthread_once_t gMainLooperOnce = PTHREAD_ONCE_INIT;

// Destructor for the thread-local Looper when a thread exits
static void looper_destructor(void* ptr)
{
    if (ptr) {
        // The shared_ptr in the thread-local storage goes out of scope,
        // so its reference count is decremented automatically.
        // We just need to release the stored raw pointer.
        delete static_cast<std::shared_ptr<jnivm::android::os::Looper>*>(ptr);
    }
}

static void create_looper_key_once()
{
    pthread_key_create(&gLooperKey, looper_destructor);
}

// --- Looper Implementation ---

jnivm::android::os::Looper::Looper()
    : nativeLooper(nullptr)
    , mQuitting(false)
{
    pthread_mutex_init(&mQueueMutex, nullptr);
    // Prepare the underlying native ALooper for this thread
    nativeLooper = ALooper_prepare(0);
}

jnivm::android::os::Looper::~Looper()
{
    pthread_mutex_destroy(&mQueueMutex);
    // ALooper_release is handled by the ALooper's own TLS destructor
}

void jnivm::android::os::Looper::prepare()
{
    pthread_once(&gLooperKeyOnce, create_looper_key_once);
    if (pthread_getspecific(gLooperKey) != nullptr) {
        throw std::runtime_error("Only one Looper may be created per thread");
    }
    // Store a shared_ptr to the Looper in thread-local storage
    auto looper_ptr = new std::shared_ptr<jnivm::android::os::Looper>(new jnivm::android::os::Looper());
    pthread_setspecific(gLooperKey, looper_ptr);
}

std::shared_ptr<jnivm::android::os::Looper> jnivm::android::os::Looper::myLooper()
{
    pthread_once(&gLooperKeyOnce, create_looper_key_once);
    auto ptr = static_cast<std::shared_ptr<jnivm::android::os::Looper>*>(pthread_getspecific(gLooperKey));
    return ptr ? *ptr : nullptr;
}

// Initialization function for the main looper singleton
static void init_main_looper()
{
    jnivm::android::os::Looper::prepare();
    gMainLooper = jnivm::android::os::Looper::myLooper();
}

std::shared_ptr<jnivm::android::os::Looper> jnivm::android::os::Looper::getMainLooper()
{
    pthread_once(&gMainLooperOnce, init_main_looper);
    return gMainLooper;
}

void jnivm::android::os::Looper::loop()
{
    // This is the core loop that a worker thread will run.
    while (!mQuitting) {
        long long nextPollTimeout = -1; // -1 means block indefinitely

        pthread_mutex_lock(&mQueueMutex);
        if (!mQueue.empty()) {
            const auto& nextMessage = mQueue.top();
            long long now = uptimeMillis();
            nextPollTimeout = nextMessage->when - now;
            if (nextPollTimeout < 0) {
                nextPollTimeout = 0; // Message is already due, don't block
            }
        }
        pthread_mutex_unlock(&mQueueMutex);

        // This will now block for a calculated duration or until woken up
        ALooper_pollOnce(static_cast<int>(nextPollTimeout), nullptr, nullptr, nullptr);

        if (mQuitting)
            break;

        // Process all messages that are due
        while (true) {
            std::shared_ptr<Message> message;
            long long now = uptimeMillis();

            pthread_mutex_lock(&mQueueMutex);
            if (!mQueue.empty() && mQueue.top()->when <= now) {
                message = mQueue.top();
                mQueue.pop();
            }
            pthread_mutex_unlock(&mQueueMutex);

            if (!message) {
                break; // No more due messages
            }

            bool handled = false;
            // First, see if the Handler has a specific Callback object.
            // This takes precedence over everything else.
            if (message->target && message->target->mCallback) {
                handled = message->target->mCallback->handleMessage(message);
            }

            // If the message was not handled by the Handler's Callback,
            // dispatch it to the appropriate target.
            if (!handled) {
                if (message->callback) {
                    // This is a message from Handler::post(), run the Runnable.
                    message->callback->run();
                } else if (message->target) {
                    // This is a message from Handler::sendMessage(), call the Handler's
                    // own handleMessage method.
                    message->target->handleMessage(message);
                }
            }
            // The message shared_ptr goes out of scope and is automatically recycled.
        }
    }
}

void jnivm::android::os::Looper::quit()
{
    if (!mQuitting) {
        mQuitting = true;
        // Wake up the looper so it can check the mQuitting flag and exit.
        if (nativeLooper) {
            ALooper_wake(nativeLooper);
        }
    }
}

///// Message

// Initialize static pool members
jnivm::android::os::Message* jnivm::android::os::Message::sPool = nullptr;
pthread_mutex_t jnivm::android::os::Message::sPoolMutex = PTHREAD_MUTEX_INITIALIZER;

std::shared_ptr<jnivm::android::os::Message> jnivm::android::os::Message::obtain()
{
    pthread_mutex_lock(&sPoolMutex);
    if (sPool) {
        jnivm::android::os::Message* m = sPool;
        sPool = m->next;
        m->next = nullptr;
        pthread_mutex_unlock(&sPoolMutex);
        // Custom deleter to recycle the message instead of deleting it
        return std::shared_ptr<jnivm::android::os::Message>(m, [](jnivm::android::os::Message* msg) { msg->recycle(); });
    }
    pthread_mutex_unlock(&sPoolMutex);

    // Pool was empty, allocate a new one
    return std::shared_ptr<jnivm::android::os::Message>(new jnivm::android::os::Message(), [](jnivm::android::os::Message* msg) { msg->recycle(); });
}

void jnivm::android::os::Message::recycle()
{
    // Clear out all fields
    what = 0;
    arg1 = 0;
    arg2 = 0;
    obj = nullptr;
    callback = nullptr;
    when = 0;
    target = nullptr;

    pthread_mutex_lock(&sPoolMutex);
    next = sPool;
    sPool = this;
    pthread_mutex_unlock(&sPoolMutex);
}

void jnivm::android::os::Message::sendToTarget()
{
    if (target) {
        // 'this' is a raw pointer here. We need a shared_ptr to pass.
        // This is tricky. The caller (the game code) holds the shared_ptr.
        // The best approach is to have the JNI layer pass the shared_ptr.
        // Assuming your JNI bridge can do that:
        target->sendMessage(std::static_pointer_cast<jnivm::android::os::Message>(shared_from_this())); // If Message derives from std::enable_shared_from_this

        // A simpler, safer approach if you can't use shared_from_this:
        // The game code must call handler->sendMessage(message), not message->sendToTarget().
        // For your shim, you can probably just have the JNI layer do that translation.
        // For now, let's implement it with a warning:
        if (target) {
            // WARNING: This assumes the shared_ptr for this message still exists somewhere!
            target->sendMessage(std::shared_ptr<jnivm::android::os::Message>(this, [](jnivm::android::os::Message*) { /* no-op deleter */ }));
        }
    }
}

///// Handler

jnivm::android::os::Handler::Handler()
{
    mLooper = Looper::myLooper();
    if (!mLooper) {
        throw std::runtime_error("Can't create handler inside thread that has not called Looper.prepare()");
    }
}

jnivm::android::os::Handler::Handler(std::shared_ptr<jnivm::android::os::Looper> looper)
    : mLooper(looper)
{
    if (!mLooper) {
        throw std::invalid_argument("Looper cannot be null");
    }
}

jnivm::android::os::Handler::Handler(std::shared_ptr<jnivm::android::os::Looper> looper, std::shared_ptr<jnivm::android::os::Handler::Callback> callback)
    : mLooper(looper)
    , mCallback(callback)
{
    if (!mLooper) {
        throw std::invalid_argument("Looper cannot be null");
    }
}

std::shared_ptr<jnivm::android::os::Message> jnivm::android::os::Handler::obtainMessage(int what)
{
    std::shared_ptr<Message> msg = Message::obtain();
    msg->target = this;
    msg->what = what;
    return msg;
}

bool jnivm::android::os::Handler::post(std::shared_ptr<java::lang::Runnable> runnable)
{
    if (!runnable)
        return false;
    std::shared_ptr<jnivm::android::os::Message> msg = jnivm::android::os::Message::obtain();
    msg->callback = runnable;
    return sendMessageAtTime(msg, uptimeMillis());
}

bool jnivm::android::os::Handler::postDelayed(std::shared_ptr<java::lang::Runnable> runnable, long delayMillis)
{
    if (!runnable)
        return false;
    std::shared_ptr<Message> msg = Message::obtain();
    msg->callback = runnable;
    return sendMessageAtTime(msg, uptimeMillis() + delayMillis);
}

bool jnivm::android::os::Handler::sendMessageAtTime(std::shared_ptr<jnivm::android::os::Message> message, long long when)
{
    if (!message || !mLooper)
        return false;

    message->target = this;
    message->when = when;

    pthread_mutex_lock(&mLooper->mQueueMutex);
    mLooper->mQueue.push(message);
    pthread_mutex_unlock(&mLooper->mQueueMutex);

    // Always wake the looper. It needs to re-evaluate its sleep time,
    // especially if this new message is earlier than the one it was waiting for.
    ALooper_wake(mLooper->nativeLooper);

    return true;
}

bool jnivm::android::os::Handler::sendMessage(std::shared_ptr<Message> message)
{
    return sendMessageAtTime(message, uptimeMillis());
}

void jnivm::android::os::Handler::handleMessage(std::shared_ptr<Message> message)
{
    // Default implementation does nothing. Subclasses should override this.
    // The code in FrameTimeTracker that inherits from Handler will have its own version.
}

///// Handler.Callback

bool jnivm::android::os::Handler::Callback::handleMessage(std::shared_ptr<jnivm::android::os::Message> msg) { return false; }

///// HandlerThread

jnivm::android::os::HandlerThread::HandlerThread(std::shared_ptr<FakeJni::JString> name)
    : java::lang::Thread(name)
{
}

jnivm::android::os::HandlerThread::~HandlerThread()
{
    quit();
    join();
}

void jnivm::android::os::HandlerThread::run()
{
    // This is the code that executes on the new background thread.

    // 1. Prepare a Looper for this thread.
    jnivm::android::os::Looper::prepare();

    // 2. Lock, notify the waiting main thread that the looper is ready, and store it.
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mLooper = jnivm::android::os::Looper::myLooper();
        mCv.notify_one(); // Wake up anyone waiting in getLooper()
    }

    // 3. Start the message processing loop. This will block until quit() is called.
    mLooper->loop();
}

std::shared_ptr<jnivm::android::os::Looper> jnivm::android::os::HandlerThread::getLooper()
{
    std::unique_lock<std::mutex> lock(mMutex);

    // This is the critical synchronization point.
    // Wait until the run() method has prepared the looper and notified us.
    mCv.wait(lock, [this] { return mLooper != nullptr; });

    return mLooper;
}

bool jnivm::android::os::HandlerThread::quit()
{
    std::shared_ptr<jnivm::android::os::Looper> looper = getLooper();
    if (looper) {
        looper->quit();
        return true;
    }
    return false;
}

///// Environment

std::shared_ptr<FakeJni::JString>
jnivm::android::os::Environment::getExternalStorageState()
{
    return std::make_shared<FakeJni::JString>("MEDIA_REMOVED");
}

std::shared_ptr<jnivm::java::io::File> jnivm::android::os::Environment::getExternalStorageDirectory()
{
    return jnivm::android::content::Context::getExternalFilesDirInternal();
}

bool jnivm::android::os::Environment::isExternalStorageManager()
{
    return true;
}

///// PowerManager

bool jnivm::android::os::PowerManager::isSustainedPerformanceModeSupported()
{
    return false;
}

///// OS Descriptors

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Build) { FakeJni::Constructor<Build> {} },
    { FakeJni::Field<&Build::MANUFACTURER> {}, "MANUFACTURER", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&Build::MODEL> {}, "MODEL", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&Build::DEVICE> {}, "DEVICE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&Build::ID> {}, "ID", FakeJni::JFieldID::STATIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::BuildVersion) { FakeJni::Constructor<BuildVersion> {} },
    { FakeJni::Field<&BuildVersion::SDK_INT> {}, "SDK_INT", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&BuildVersion::RELEASE> {}, "RELEASE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&BuildVersion::INCREMENTAL> {}, "INCREMENTAL", FakeJni::JFieldID::STATIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Bundle) { FakeJni::Constructor<Bundle> {} },
    { FakeJni::Function<&Bundle::containsKey> {}, "containsKey", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Bundle::getString> {}, "getString", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Process) { FakeJni::Constructor<Process> {} },
    { FakeJni::Function<&Process::setThreadPriority> {}, "setThreadPriority", FakeJni::JMethodID::STATIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Message) { FakeJni::Constructor<Message> {} },
    { FakeJni::Field<&Message::what> {}, "what", FakeJni::JFieldID::PUBLIC },
    { FakeJni::Function<&Message::obtain> {}, "obtain", FakeJni::JMethodID::STATIC },
    { FakeJni::Function<&Message::recycle> {}, "recycle", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Message::sendToTarget> {}, "sendToTarget", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Looper) { FakeJni::Constructor<Looper> {} },
    { FakeJni::Function<&Looper::prepare> {}, "prepare", FakeJni::JMethodID::STATIC },
    { FakeJni::Function<&Looper::getMainLooper> {}, "getMainLooper", FakeJni::JMethodID::STATIC },
    { FakeJni::Function<&Looper::myLooper> {}, "myLooper", FakeJni::JMethodID::STATIC },
    { FakeJni::Function<&Looper::loop> {}, "loop", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Looper::quit> {}, "quit", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Handler) { FakeJni::Constructor<Handler> {} },
    { FakeJni::Constructor<Handler, std::shared_ptr<Looper>> {} },
    { FakeJni::Constructor<Handler, std::shared_ptr<Looper>, std::shared_ptr<Handler::Callback>> {} },
    { FakeJni::Function<&Handler::post> {}, "post", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Handler::postDelayed> {}, "postDelayed", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Handler::sendMessage> {}, "sendMessage", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Handler::handleMessage> {}, "handleMessage", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Handler::obtainMessage> {}, "obtainMessage", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Handler::Callback) { FakeJni::Constructor<Callback> {} },
    { FakeJni::Function<&Callback::handleMessage> {}, "handleMessage", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::HandlerThread) { FakeJni::Constructor<HandlerThread, std::shared_ptr<FakeJni::JString>> {} },
    { FakeJni::Function<&HandlerThread::getLooper> {}, "getLooper", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&HandlerThread::quit> {}, "quit", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Environment) { FakeJni::Constructor<Environment> {} },
    { FakeJni::Field<&Environment::MEDIA_MOUNTED> {}, "MEDIA_MOUNTED", FakeJni::JFieldID::STATIC },
    { FakeJni::Function<&Environment::getExternalStorageState> {}, "getExternalStorageState", FakeJni::JMethodID::STATIC },
    { FakeJni::Function<&Environment::getExternalStorageDirectory> {}, "getExternalStorageDirectory", FakeJni::JMethodID::STATIC },
    { FakeJni::Function<&Environment::isExternalStorageManager> {}, "isExternalStorageManager", FakeJni::JMethodID::STATIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::PowerManager) { FakeJni::Constructor<PowerManager> {} },
    { FakeJni::Function<&PowerManager::isSustainedPerformanceModeSupported> {}, "isSustainedPerformanceModeSupported", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR
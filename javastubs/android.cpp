#include "toml++/toml.hpp"
extern toml::table config;

#include "android.h"
#include "baron/baron.h"
#include "javac.h"
#include "logging.h"
#include <fstream>
#include <pthread.h>

///// Uri

std::shared_ptr<FakeJni::JString> jnivm::android::net::Uri::encode(std::shared_ptr<FakeJni::JString> string)
{
    return string;
}

///// Display

int jnivm::android::view::Display::getDisplayId()
{
    return 1;
}

int jnivm::android::view::Display::getRotation()
{
    return config["device"]["displayRotation"].value_or<int>(0);
}

int jnivm::android::view::Display::getWidth()
{
    return config["device"]["displayWidth"].value_or<int>(640);
}

int jnivm::android::view::Display::getHeight()
{
    return config["device"]["displayHeight"].value_or<int>(480);
}

float jnivm::android::view::Display::getRefreshRate()
{
    return config["device"]["displayRefreshRate"].value_or<float>(60);
}

long jnivm::android::view::Display::getAppVsyncOffsetNanos() { return 0; }
long jnivm::android::view::Display::getPresentationDeadlineNanos() { return 0; }

void jnivm::android::view::Display::getRealMetrics(std::shared_ptr<jnivm::android::util::DisplayMetrics> metrics)
{
    metrics->widthPixels = config["device"]["displayWidth"].value_or<int>(640);
    metrics->heightPixels = config["device"]["displayHeight"].value_or<int>(480);
    metrics->densityDpi = config["device"]["displayDpi"].value_or<int>(100);
}

std::shared_ptr<jnivm::Array<jnivm::android::view::DisplayMode>> jnivm::android::view::Display::getSupportedModes()
{
    verbose("JBRIDGE", "App requests Display Modes.... ");
    auto array = std::make_shared<FakeJni::JArray<jnivm::android::view::DisplayMode>>(1);
    (*array)[0] = std::make_shared<jnivm::android::view::DisplayMode>();
    return array;
}

///// Display$Mode

int jnivm::android::view::DisplayMode::getPhysicalWidth()
{
    return config["device"]["displayWidth"].value_or<int>(640);
}

int jnivm::android::view::DisplayMode::getPhysicalHeight()
{
    return config["device"]["displayHeight"].value_or<int>(480);
}

float jnivm::android::view::DisplayMode::getRefreshRate()
{
    return config["device"]["displayRefreshRate"].value_or<float>(60);
}

///// InputDevice

int jnivm::android::view::InputDevice::getSources()
{
    return 1025; // hardcoded to SOURCE_GAMEPAD for now
}

std::shared_ptr<jnivm::android::view::InputDevice> jnivm::android::view::InputDevice::getDevice(int device)
{
    return std::make_shared<jnivm::android::view::InputDevice>();
}

std::shared_ptr<FakeJni::JArray<int>> jnivm::android::view::InputDevice::getDeviceIds()
{
    verbose("JBRIDGE", "App requests InputDevice IDs....");
    auto array = std::make_shared<FakeJni::JArray<int>>(2);
    (*array)[0] = 1; // Touch
    (*array)[1] = 2; // Controller

    return array;
}

///// Window

void jnivm::android::view::Window::setFlags(int flag1, int flag2)
{
    verbose("JBRIDGE", "Window.setFlags %d - %d", flag1, flag2);
}

std::shared_ptr<jnivm::android::view::View>
jnivm::android::view::Window::getDecorView()
{
    return std::make_shared<jnivm::android::view::View>();
}

///// View

std::shared_ptr<jnivm::android::view::Display>
jnivm::android::view::View::getDisplay()
{
    return std::make_shared<jnivm::android::view::Display>();
}

int jnivm::android::view::View::getSystemUiVisibility()
{
    return jnivm::android::view::View::SYSTEM_UI_FLAG_FULLSCREEN && SYSTEM_UI_FLAG_HIDE_NAVIGATION;
}

void jnivm::android::view::View::setSystemUiVisibility(int visibility)
{
}

///// Choreographer

static std::shared_ptr<jnivm::android::view::Choreographer> gChoreographerInstance;
static pthread_once_t gChoreographerOnce = PTHREAD_ONCE_INIT;

static void create_global_choreographer_once()
{
    gChoreographerInstance = std::shared_ptr<jnivm::android::view::Choreographer>(new jnivm::android::view::Choreographer());
}

std::shared_ptr<jnivm::android::view::Choreographer> jnivm::android::view::Choreographer::getInstance()
{
    pthread_once(&gChoreographerOnce, create_global_choreographer_once);
    return gChoreographerInstance;
}

jnivm::android::view::Choreographer::Choreographer()
{

    mHandlerThread = std::make_shared<os::HandlerThread>(std::make_shared<FakeJni::JString>("Choreographer"));
    mHandlerThread->start();

    mLooper = mHandlerThread->getLooper();
    mHandler = std::make_shared<os::Handler>(mLooper);

    pthread_mutex_init(&mCallbacksMutex, nullptr);
    mLastVSyncTimeNanos.store(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
            .count());

    verbose("Choreographer", "Global Instance created with its own HandlerThread.");

    mHandler->post(java::lang::LambdaRunnable::Create([this]() {
        this->watchdogLoop();
    }));
}

jnivm::android::view::Choreographer::~Choreographer()
{
    if (mHandlerThread) {
        mHandlerThread->quit(); // This will stop the looper and thus the watchdog.
        mHandlerThread->join();
    }
    pthread_mutex_destroy(&mCallbacksMutex);
}

// The watchdog loop, now implemented as a recurring Handler task.
void jnivm::android::view::Choreographer::watchdogLoop()
{
    const long long vsyncThresholdNanos = 16'000'000LL; // 1 second

    long long now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
                        .count();

    long long lastSync = mLastVSyncTimeNanos.load();

    if (now - lastSync > vsyncThresholdNanos) {
        verbose("Choreographer", "VSync stall detected. Injecting fallback frame.");
        // We are already on the handler thread, so we can call dispatch directly.
        dispatchFrameCallbacks(false);
    }

    // Schedule the next check in 100ms.
    mHandler->postDelayed(java::lang::LambdaRunnable::Create([this]() {
        this->watchdogLoop();
    }),
        2);
}

void jnivm::android::view::Choreographer::postFrameCallback(std::shared_ptr<FrameCallback> callback)
{
    if (!callback)
        return;
    pthread_mutex_lock(&mCallbacksMutex);
    verbose("Choreographer", "[Thread: %p] [Instance: %p] postFrameCallback ENTER. Queue size before: %zu",
        pthread_self(), this, mCallbacks.size());
    mCallbacks.push_back(callback);
    verbose("Choreographer", "[Thread: %p] [Instance: %p] postFrameCallback EXIT. Queue size after: %zu",
        pthread_self(), this, mCallbacks.size());
    pthread_mutex_unlock(&mCallbacksMutex);
}

// signalVSync is the entry point for eglSwapBuffers.
void jnivm::android::view::Choreographer::signalVSync()
{
    // Post the work to our handler thread to ensure all dispatches are serialized.
    mHandler->post(java::lang::LambdaRunnable::Create([this]() {
        this->dispatchFrameCallbacks(true);
    }));
}

// The core dispatch logic, with the critical isRealVSync flag.
void jnivm::android::view::Choreographer::dispatchFrameCallbacks(bool isRealVSync)
{
    long long now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
                        .count();

    // Only a real vsync signal updates the master timestamp.
    if (isRealVSync) {
        mLastVSyncTimeNanos.store(now);
    }

    std::vector<std::shared_ptr<FrameCallback>> callbacksToRun;
    verbose("Choreographer", "[Thread: %p] [Instance: %p] dispatch ENTER (isRealVSync: %s). Queue size before swap: %zu",
        pthread_self(), this, isRealVSync ? "true" : "false", mCallbacks.size());
    pthread_mutex_lock(&mCallbacksMutex);
    mCallbacks.swap(callbacksToRun);
    pthread_mutex_unlock(&mCallbacksMutex);

    verbose("Choreographer", "[Thread: %p] [Instance: %p] dispatch EXIT. Callbacks to run: %zu. Queue size after swap: %zu",
        pthread_self(), this, callbacksToRun.size(), mCallbacks.size());

    if (callbacksToRun.empty()) {
        verbose("Choreographer", "No callbacks to dispatch (isRealVSync: %s)", isRealVSync ? "true" : "false");
        return;
    }

    verbose("Choreographer", "Dispatching %zu callbacks (isRealVSync: %s)",
        callbacksToRun.size(), isRealVSync ? "true" : "false");

    // We are on the handler thread, so we can execute the callbacks directly and synchronously.
    for (const auto& callback : callbacksToRun) {
        callback->doFrame(now);
    }
}

///// ContextThemeWrapper

std::shared_ptr<jnivm::android::content::res::Resources> jnivm::android::view::ContextThemeWrapper::getResources()
{
    return std::make_shared<jnivm::android::content::res::Resources>();
}

///// DisplayManager

std::shared_ptr<jnivm::android::view::Display>
jnivm::android::hardware::display::DisplayManager::getDisplay(int disp)
{
    return std::make_shared<jnivm::android::view::Display>();
}

///// AudioDeviceInfo

int jnivm::android::media::AudioDeviceInfo::getType()
{
    return AudioDeviceInfo::TYPE_WIRED_HEADPHONES;
}

///// AudioManager

bool jnivm::android::media::AudioManager::isBluetoothA2dpOn()
{
    return false;
}

std::shared_ptr<FakeJni::JString> jnivm::android::media::AudioManager::getProperty(std::shared_ptr<FakeJni::JString> property)
{
    if (*property == PROPERTY_OUTPUT_FRAMES_PER_BUFFER)
        return std::make_shared<FakeJni::JString>("64"); // ... uh i dunno i haven't written the audio implementation yet
    if (*property == PROPERTY_OUTPUT_SAMPLE_RATE)
        return std::make_shared<FakeJni::JString>("24000"); // ... uhhh sure
    return nullptr;
}

std::shared_ptr<jnivm::Array<jnivm::android::media::AudioDeviceInfo>> jnivm::android::media::AudioManager::getDevices(int type)
{
    auto array = std::make_shared<FakeJni::JArray<jnivm::android::media::AudioDeviceInfo>>(1);
    (*array)[0] = std::make_shared<jnivm::android::media::AudioDeviceInfo>();
    return array;
}

int jnivm::android::media::AudioManager::getStreamVolume(int stream)
{
    return 100; // WERE BLASTING FULL VOLUME YEEE HAAA
}

///// MediaRoute$RouteInfo

std::shared_ptr<jnivm::android::view::Display> jnivm::android::media::MediaRouterRouteInfo::getPresentationDisplay()
{
    return std::make_shared<jnivm::android::view::Display>();
}

///// MediaRouter

std::shared_ptr<jnivm::android::media::MediaRouterRouteInfo> jnivm::android::media::MediaRouter::getSelectedRoute(int type)
{
    return std::make_shared<jnivm::android::media::MediaRouterRouteInfo>();
}

///// PackageManager

std::shared_ptr<jnivm::android::content::pm::PackageInfo>
jnivm::android::content::pm::PackageManager::getPackageInfo(std::shared_ptr<FakeJni::JString> packageName, int number)
{
    return std::make_shared<jnivm::android::content::pm::PackageInfo>();
}

bool jnivm::android::content::pm::PackageManager::hasSystemFeature(std::shared_ptr<FakeJni::JString> feature)
{
    verbose("JBRIDGE", "App asks about availability of feature: %s", feature.get()->c_str());
    return false; // We don't claim support of anything right now
}

///// AssetManager

std::shared_ptr<jnivm::java::io::InputStream>
jnivm::android::content::res::AssetManager::open(std::shared_ptr<FakeJni::JString> filename)
{
    verbose("JBRIDGE", "AssetManager opening file %s", filename.get()->c_str());
    return std::make_shared<jnivm::java::io::InputStream>(std::make_shared<FakeJni::JString>(std::string("assets/").append(filename.get()->c_str())));
}

int jnivm::android::content::res::Resources::getIdentifier(std::shared_ptr<FakeJni::JString> name, std::shared_ptr<FakeJni::JString> defType, std::shared_ptr<FakeJni::JString> defPackage)
{
    verbose("JBRIDGE", "Resources requesting identifier for %s - %s - %s", name.get()->c_str(), defType.get()->c_str(), defPackage.get()->c_str());
    return 1337420;
}

///// SharedPreferences

bool jnivm::android::content::SharedPreferences::contains(std::shared_ptr<FakeJni::JString> key)
{
    return false;
}

int jnivm::android::content::SharedPreferences::getInt(std::shared_ptr<FakeJni::JString> key, int def)
{
    return def;
}

std::shared_ptr<FakeJni::JString> jnivm::android::content::SharedPreferences::getString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> def)
{
    return def;
}

std::shared_ptr<jnivm::java::util::Map> jnivm::android::content::SharedPreferences::getAll()
{
    return std::make_shared<jnivm::java::util::Map>();
}

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferences::edit()
{
    return std::make_shared<SharedPreferencesEditor>();
}

///// SharedPreferencesEditor

void jnivm::android::content::SharedPreferencesEditor::apply()
{
}

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferencesEditor::putInt(std::shared_ptr<FakeJni::JString> key, int val)
{
    return std::shared_ptr<SharedPreferencesEditor>(this);
}

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferencesEditor::putString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> val)
{
    return std::shared_ptr<SharedPreferencesEditor>(this);
}

///// Context

std::shared_ptr<jnivm::android::content::res::AssetManager>
jnivm::android::content::Context::getAssets()
{
    return std::make_shared<jnivm::android::content::res::AssetManager>();
}

std::shared_ptr<jnivm::android::content::pm::ApplicationInfo>
jnivm::android::content::Context::getApplicationInfo()
{
    return std::make_shared<jnivm::android::content::pm::ApplicationInfo>();
}

std::shared_ptr<FakeJni::JObject>
jnivm::android::content::Context::getSystemService(std::shared_ptr<FakeJni::JString> service)
{
    if (*service == LOCATION_SERVICE)
        return nullptr;

    if (*service == AUDIO_SERVICE)
        return std::make_shared<jnivm::android::media::AudioManager>();

    if (*service == DISPLAY_SERVICE)
        return std::make_shared<jnivm::android::hardware::display::DisplayManager>();

    if (*service == POWER_SERVICE)
        return std::make_shared<jnivm::android::os::PowerManager>();

    if (*service == MEDIA_ROUTER_SERVICE)
        return std::make_shared<jnivm::android::media::MediaRouter>();

    return nullptr;
}

std::shared_ptr<FakeJni::JString>
jnivm::android::content::Context::getPackageName()
{
    return std::make_shared<FakeJni::JString>(config["package"]["packageName"].value_or<std::string>("package.name.not.defined"));
}

std::shared_ptr<jnivm::android::content::pm::PackageManager>
jnivm::android::content::Context::getPackageManager()
{
    return std::make_shared<jnivm::android::content::pm::PackageManager>();
}

std::shared_ptr<jnivm::android::content::SharedPreferences>
jnivm::android::content::Context::getSharedPreferences(std::shared_ptr<FakeJni::JString> str, int num)
{
    return std::make_shared<jnivm::android::content::SharedPreferences>();
}

std::shared_ptr<FakeJni::JString>
jnivm::android::content::Context::getPackageCodePath()
{
    return std::make_shared<FakeJni::JString>(config["paths"]["android_package_code"].value_or<std::string>("./path_not_defined_code"));
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getExternalFilesDir(std::shared_ptr<FakeJni::JString> path)
{
    return std::make_shared<jnivm::java::io::File>(std::make_shared<FakeJni::JString>(config["paths"]["android_external_files"].value_or<std::string>("./path_not_defined_external")));
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getFilesDir()
{
    return std::make_shared<jnivm::java::io::File>(std::make_shared<FakeJni::JString>(config["paths"]["android_files"].value_or<std::string>("./path_not_defined_files")));
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getCacheDir()
{
    return std::make_shared<jnivm::java::io::File>(std::make_shared<FakeJni::JString>(config["paths"]["android_cache"].value_or<std::string>("./path_not_defined_cache")));
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getExternalCacheDir()
{
    return std::make_shared<jnivm::java::io::File>(std::make_shared<FakeJni::JString>(config["paths"]["android_cache"].value_or<std::string>("./path_not_defined_cache")));
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getObbDir()
{
    // return std::make_shared<jnivm::java::io::File>(config["paths"]["obb_dir"][0].value_or<std::string>("/path_not_defined_obb"));
    return NULL;
}

std::shared_ptr<jnivm::Array<jnivm::java::io::File>>
jnivm::android::content::Context::getObbDirs()
{
    return NULL;
}

int jnivm::android::content::Context::checkCallingOrSelfPermission(std::shared_ptr<FakeJni::JString> permission)
{
    verbose("JBRIDGE", "Granting permission: %s", permission.get()->c_str());
    return jnivm::android::content::pm::PackageManager::PERMISSION_GRANTED; // Sure why not, what could go wrong....
}

std::shared_ptr<jnivm::android::content::res::Resources> jnivm::android::content::Context::getResources()
{
    return std::make_shared<jnivm::android::content::res::Resources>();
}

std::shared_ptr<jnivm::android::view::Window> jnivm::android::content::Context::getWindow()
{
    return std::make_shared<jnivm::android::view::Window>();
}

///// Activity

void jnivm::android::app::Activity::runOnUiThread(std::shared_ptr<jnivm::java::lang::Runnable> runnable)
{
    verbose("JBRIDGE", "RunOnUiThread Running runnable!");
    runnable->run();
}

std::shared_ptr<jnivm::android::content::Intent>
jnivm::android::app::Activity::getIntent()
{
    return std::make_shared<jnivm::android::content::Intent>();
}

int jnivm::android::app::Activity::getRequestedOrientation()
{
    return config["device"]["displayOrientation"].value_or<int>(0);
}

void jnivm::android::app::Activity::setRequestedOrientation(int orientation)
{
    // Stub
}

std::shared_ptr<jnivm::android::content::res::Resources> jnivm::android::app::Activity::getResources()
{
    return std::make_shared<jnivm::android::content::res::Resources>();
}

std::shared_ptr<jnivm::android::view::Window> jnivm::android::app::Activity::getWindow()
{
    return std::make_shared<jnivm::android::view::Window>();
}

std::shared_ptr<jnivm::android::view::View> jnivm::android::app::Activity::findViewById(int id)
{
    return std::make_shared<jnivm::android::view::SurfaceView>(); // Sure, lol
}

///// Intent

std::shared_ptr<jnivm::android::os::Bundle>
jnivm::android::content::Intent::getExtras()
{
    return std::make_shared<jnivm::android::os::Bundle>();
}

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

bool jnivm::android::os::PowerManager::isSustainedPerformanceModeSupported()
{
    return false;
}

///// Descriptors

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::net::Uri) { FakeJni::Constructor<Uri> {} },
    { FakeJni::Function<&Uri::encode> {}, "encode", FakeJni::JMethodID::STATIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::util::DisplayMetrics) { FakeJni::Constructor<DisplayMetrics> {} },
    { FakeJni::Field<&DisplayMetrics::widthPixels> {}, "widthPixels", FakeJni::JFieldID::PUBLIC },
    { FakeJni::Field<&DisplayMetrics::heightPixels> {}, "heightPixels", FakeJni::JFieldID::PUBLIC },
    { FakeJni::Field<&DisplayMetrics::densityDpi> {}, "densityDpi", FakeJni::JFieldID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::view::Display) { FakeJni::Constructor<Display> {} },
    { FakeJni::Function<&Display::getDisplayId> {}, "getDisplayId", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Display::getRotation> {}, "getRotation", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Display::getAppVsyncOffsetNanos> {}, "getAppVsyncOffsetNanos", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Display::getPresentationDeadlineNanos> {}, "getPresentationDeadlineNanos", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Display::getWidth> {}, "getWidth", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Display::getHeight> {}, "getHeight", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Display::getRefreshRate> {}, "getRefreshRate", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Display::getRealMetrics> {}, "getRealMetrics", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Display::getSupportedModes> {}, "getSupportedModes", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::view::DisplayMode) { FakeJni::Constructor<DisplayMode> {} },
    { FakeJni::Function<&DisplayMode::getPhysicalWidth> {}, "getPhysicalWidth", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&DisplayMode::getPhysicalHeight> {}, "getPhysicalHeight", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&DisplayMode::getRefreshRate> {}, "getRefreshRate", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::view::Surface) { FakeJni::Constructor<Surface> {} },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::view::InputDevice) { FakeJni::Constructor<InputDevice> {} },
    { FakeJni::Function<&InputDevice::getSources> {}, "getSources", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&InputDevice::getDevice> {}, "getDevice", FakeJni::JMethodID::STATIC },
    { FakeJni::Function<&InputDevice::getDeviceIds> {}, "getDeviceIds", FakeJni::JMethodID::STATIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::view::Window) { FakeJni::Constructor<Window> {} },
    { FakeJni::Function<&Window::setFlags> {}, "setFlags", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Window::getDecorView> {}, "getDecorView", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::view::View) { FakeJni::Constructor<View> {} },
    { FakeJni::Field<&View::SYSTEM_UI_FLAG_IMMERSIVE_STICKY> {}, "SYSTEM_UI_FLAG_IMMERSIVE_STICKY", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&View::SYSTEM_UI_FLAG_LAYOUT_STABLE> {}, "SYSTEM_UI_FLAG_LAYOUT_STABLE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&View::SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN> {}, "SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&View::SYSTEM_UI_FLAG_HIDE_NAVIGATION> {}, "SYSTEM_UI_FLAG_HIDE_NAVIGATION", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&View::SYSTEM_UI_FLAG_FULLSCREEN> {}, "SYSTEM_UI_FLAG_FULLSCREEN", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&View::SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION> {}, "SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION", FakeJni::JFieldID::STATIC },
    { FakeJni::Function<&View::getDisplay> {}, "getDisplay", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&View::getSystemUiVisibility> {}, "getSystemUiVisibility", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&View::setSystemUiVisibility> {}, "setSystemUiVisibility", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::view::SurfaceView) { FakeJni::Constructor<SurfaceView> {} },
    { FakeJni::Function<&InputDevice::getDevice> {}, "getDevice", FakeJni::JMethodID::STATIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::view::Choreographer) { FakeJni::Function<&Choreographer::getInstance> {}, "getInstance", FakeJni::JMethodID::STATIC },
    { FakeJni::Function<&Choreographer::postFrameCallback> {}, "postFrameCallback", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Choreographer::dispatchFrameCallbacks> {}, "dispatchFrameCallbacks", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::view::Choreographer::FrameCallback) { FakeJni::Function<&FrameCallback::doFrame> {}, "doFrame", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::view::ContextThemeWrapper) { FakeJni::Constructor<ContextThemeWrapper> {} },
    { FakeJni::Function<&ContextThemeWrapper::getResources> {}, "getResources", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::hardware::display::DisplayManager) { FakeJni::Constructor<DisplayManager> {} },
    { FakeJni::Function<&DisplayManager::getDisplay> {}, "getDisplay", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::media::AudioDeviceInfo) { FakeJni::Constructor<AudioDeviceInfo> {} },
    { FakeJni::Field<&AudioDeviceInfo::TYPE_BLUETOOTH_A2DP> {}, "TYPE_BLUETOOTH_A2DP", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&AudioDeviceInfo::TYPE_WIRED_HEADPHONES> {}, "TYPE_WIRED_HEADPHONES", FakeJni::JFieldID::STATIC },
    { FakeJni::Function<&AudioDeviceInfo::getType> {}, "getType", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::media::AudioManager) { FakeJni::Constructor<AudioManager> {} },
    { FakeJni::Field<&AudioManager::PROPERTY_OUTPUT_FRAMES_PER_BUFFER> {}, "PROPERTY_OUTPUT_FRAMES_PER_BUFFER", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&AudioManager::PROPERTY_OUTPUT_SAMPLE_RATE> {}, "PROPERTY_OUTPUT_SAMPLE_RATE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&AudioManager::GET_DEVICES_OUTPUTS> {}, "GET_DEVICES_OUTPUTS", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&AudioManager::STREAM_MUSIC> {}, "STREAM_MUSIC", FakeJni::JFieldID::STATIC },
    { FakeJni::Function<&AudioManager::isBluetoothA2dpOn> {}, "isBluetoothA2dpOn", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&AudioManager::getProperty> {}, "getProperty", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&AudioManager::getDevices> {}, "getDevices", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&AudioManager::getStreamVolume> {}, "getStreamVolume", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::media::MediaRouterRouteInfo) { FakeJni::Constructor<MediaRouterRouteInfo> {} },
    { FakeJni::Function<&MediaRouterRouteInfo::getPresentationDisplay> {}, "getPresentationDisplay", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::media::MediaRouter) { FakeJni::Constructor<MediaRouter> {} },
    { FakeJni::Field<&MediaRouter::ROUTE_TYPE_LIVE_VIDEO> {}, "ROUTE_TYPE_LIVE_VIDEO", FakeJni::JFieldID::STATIC },
    { FakeJni::Function<&MediaRouter::getSelectedRoute> {}, "getSelectedRoute", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::pm::ActivityInfo) { FakeJni::Constructor<ActivityInfo> {} },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_PORTRAIT> {}, "SCREEN_ORIENTATION_PORTRAIT", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_REVERSE_PORTRAIT> {}, "SCREEN_ORIENTATION_REVERSE_PORTRAIT", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_REVERSE_LANDSCAPE> {}, "SCREEN_ORIENTATION_REVERSE_LANDSCAPE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_LANDSCAPE> {}, "SCREEN_ORIENTATION_LANDSCAPE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_FULL_USER> {}, "SCREEN_ORIENTATION_FULL_USER", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_USER_PORTRAIT> {}, "SCREEN_ORIENTATION_USER_PORTRAIT", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_USER_LANDSCAPE> {}, "SCREEN_ORIENTATION_USER_LANDSCAPE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_SENSOR> {}, "SCREEN_ORIENTATION_SENSOR", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_UNSPECIFIED> {}, "SCREEN_ORIENTATION_UNSPECIFIED", FakeJni::JFieldID::STATIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::pm::PackageInfo) { FakeJni::Constructor<PackageInfo> {} },
    { FakeJni::Field<&PackageInfo::versionName> {}, "versionName", FakeJni::JFieldID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::pm::ApplicationInfo) { FakeJni::Constructor<ApplicationInfo> {} },
    { FakeJni::Field<&ApplicationInfo::splitPublicSourceDirs> {}, "splitPublicSourceDirs", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::pm::PackageManager) { FakeJni::Constructor<PackageManager> {} },
    { FakeJni::Field<&PackageManager::FEATURE_AUDIO_LOW_LATENCY> {}, "FEATURE_AUDIO_LOW_LATENCY", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&PackageManager::PERMISSION_GRANTED> {}, "PERMISSION_GRANTED", FakeJni::JFieldID::STATIC },
    { FakeJni::Function<&PackageManager::getPackageInfo> {}, "getPackageInfo", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&PackageManager::hasSystemFeature> {}, "hasSystemFeature", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::res::AssetManager) { FakeJni::Constructor<AssetManager> {} },
    { FakeJni::Function<&AssetManager::open> {}, "open", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::res::Resources) { FakeJni::Constructor<Resources> {} },
    { FakeJni::Function<&Resources::getIdentifier> {}, "getIdentifier", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::app::Activity) { FakeJni::Constructor<Activity> {} },
    { FakeJni::Function<&Activity::runOnUiThread> {}, "runOnUiThread", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Activity::getIntent> {}, "getIntent", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Activity::getRequestedOrientation> {}, "getRequestedOrientation", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Activity::setRequestedOrientation> {}, "setRequestedOrientation", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Activity::getResources> {}, "getResources", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Activity::getWindow> {}, "getWindow", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Activity::findViewById> {}, "findViewById", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::app::NativeActivity) { FakeJni::Constructor<NativeActivity> {} },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::SharedPreferencesEditor) { FakeJni::Constructor<SharedPreferencesEditor> {} },
    { FakeJni::Function<&SharedPreferencesEditor::apply> {}, "apply", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferencesEditor::putInt> {}, "putInt", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferencesEditor::putString> {}, "putString", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::SharedPreferences) { FakeJni::Constructor<SharedPreferences> {} },
    { FakeJni::Function<&SharedPreferences::contains> {}, "contains", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferences::getInt> {}, "getInt", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferences::getString> {}, "getString", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferences::getAll> {}, "getAll", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferences::edit> {}, "edit", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::Context) { FakeJni::Constructor<Context> {} },
    { FakeJni::Field<&Context::LOCATION_SERVICE> {}, "LOCATION_SERVICE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&Context::DISPLAY_SERVICE> {}, "DISPLAY_SERVICE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&Context::AUDIO_SERVICE> {}, "AUDIO_SERVICE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&Context::MEDIA_ROUTER_SERVICE> {}, "MEDIA_ROUTER_SERVICE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&Context::POWER_SERVICE> {}, "POWER_SERVICE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&Context::MODE_PRIVATE> {}, "MODE_PRIVATE", FakeJni::JFieldID::STATIC },
    { FakeJni::Function<&Context::getSystemService> {}, "getSystemService", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getAssets> {}, "getAssets", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getApplicationInfo> {}, "getApplicationInfo", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getPackageName> {}, "getPackageName", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getPackageManager> {}, "getPackageManager", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getPackageCodePath> {}, "getPackageCodePath", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getSharedPreferences> {}, "getSharedPreferences", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getExternalFilesDir> {}, "getExternalFilesDir", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getFilesDir> {}, "getFilesDir", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getCacheDir> {}, "getCacheDir", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getExternalCacheDir> {}, "getExternalCacheDir", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getObbDir> {}, "getObbDir", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getObbDirs> {}, "getObbDirs", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::checkCallingOrSelfPermission> {}, "checkCallingOrSelfPermission", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getResources> {}, "getResources", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getWindow> {}, "getWindow", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::Intent) { FakeJni::Constructor<Intent> {} },
    { FakeJni::Function<&Intent::getExtras> {}, "getExtras", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

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
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::PowerManager) { FakeJni::Constructor<PowerManager> {} },
    { FakeJni::Function<&PowerManager::isSustainedPerformanceModeSupported> {}, "isSustainedPerformanceModeSupported", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    void InitJNIAndroidClasses(FakeJni::Jvm* vm)
{
    verbose("JBRIDGE", "Initializing Android JNI Classes");
    vm->registerClass<jnivm::android::net::Uri>();
    vm->registerClass<jnivm::android::util::DisplayMetrics>();
    vm->registerClass<jnivm::android::view::DisplayMode>();
    vm->registerClass<jnivm::android::view::Display>();
    vm->registerClass<jnivm::android::view::Surface>();
    vm->registerClass<jnivm::android::view::InputDevice>();
    vm->registerClass<jnivm::android::view::Window>();
    vm->registerClass<jnivm::android::view::View>();
    vm->registerClass<jnivm::android::view::SurfaceView>();
    vm->registerClass<jnivm::android::view::Choreographer>();
    vm->registerClass<jnivm::android::view::Choreographer::FrameCallback>();
    vm->registerClass<jnivm::android::view::ContextThemeWrapper>();
    vm->registerClass<jnivm::android::hardware::display::DisplayManager>();
    vm->registerClass<jnivm::android::media::MediaRouterRouteInfo>();
    vm->registerClass<jnivm::android::media::MediaRouter>();
    vm->registerClass<jnivm::android::media::AudioDeviceInfo>();
    vm->registerClass<jnivm::android::media::AudioManager>();
    vm->registerClass<jnivm::android::os::Build>();
    vm->registerClass<jnivm::android::os::BuildVersion>();
    vm->registerClass<jnivm::android::os::Process>();
    vm->registerClass<jnivm::android::os::Bundle>();
    vm->registerClass<jnivm::android::os::Message>();
    vm->registerClass<jnivm::android::os::Looper>();
    vm->registerClass<jnivm::android::os::Handler>();
    vm->registerClass<jnivm::android::os::Handler::Callback>();
    vm->registerClass<jnivm::android::os::HandlerThread>();
    vm->registerClass<jnivm::android::os::Environment>();
    vm->registerClass<jnivm::android::os::PowerManager>();
    vm->registerClass<jnivm::android::content::pm::ActivityInfo>();
    vm->registerClass<jnivm::android::content::pm::PackageInfo>();
    vm->registerClass<jnivm::android::content::pm::ApplicationInfo>();
    vm->registerClass<jnivm::android::content::pm::PackageManager>();
    vm->registerClass<jnivm::android::content::res::AssetManager>();
    vm->registerClass<jnivm::android::content::res::Resources>();
    vm->registerClass<jnivm::android::content::SharedPreferences>();
    vm->registerClass<jnivm::android::content::SharedPreferencesEditor>();
    vm->registerClass<jnivm::android::content::Context>();
    vm->registerClass<jnivm::android::content::Intent>();
    vm->registerClass<jnivm::android::app::Activity>();
    vm->registerClass<jnivm::android::app::NativeActivity>();
}
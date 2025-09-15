#include "toml++/toml.hpp"
extern toml::table config;

#include "android.h"
#include "baron/baron.h"
#include "javac.h"
#include "logging.h"
#include <fstream>
#include <pthread.h>
#include <inttypes.h>

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
    verbose("Choreographer", "[Thread: %" PRIxPTR "] [Instance: %p] postFrameCallback ENTER. Queue size before: %zu",
        (uintptr_t)pthread_self(), this, mCallbacks.size());
    mCallbacks.push_back(callback);
    verbose("Choreographer", "[Thread: %" PRIxPTR "] [Instance: %p] postFrameCallback EXIT. Queue size after: %zu",
        (uintptr_t)pthread_self(), this, mCallbacks.size());
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
    verbose("Choreographer", "[Thread: %" PRIxPTR "] [Instance: %p] dispatch ENTER (isRealVSync: %s). Queue size before swap: %zu",
        (uintptr_t)pthread_self(), this, isRealVSync ? "true" : "false", mCallbacks.size());
    pthread_mutex_lock(&mCallbacksMutex);
    mCallbacks.swap(callbacksToRun);
    pthread_mutex_unlock(&mCallbacksMutex);

    verbose("Choreographer", "[Thread: %" PRIxPTR "] [Instance: %p] dispatch EXIT. Callbacks to run: %zu. Queue size after swap: %zu",
        (uintptr_t)pthread_self(), this, callbacksToRun.size(), mCallbacks.size());

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

///// View Descriptors

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
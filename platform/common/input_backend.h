#pragma once

#include "android.h" // Your main android shim header
#include <SDL2/SDL.h>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#define INPUT_ID_KEYBOARD 1
#define INPUT_ID_XBOX 2
#define INPUT_ID_MOUSE 3

class InputBackend {
private:
    std::unordered_map<int, std::shared_ptr<jnivm::android::view::InputDevice>> devices;
    std::mutex deviceMutex;
    bool running = false;

    // Callbacks to dispatch events to the Android shim layer
    std::function<void(std::shared_ptr<jnivm::android::view::KeyEvent>)> onKey;
    std::function<void(std::shared_ptr<jnivm::android::view::MotionEvent>)> onMotion;

    InputBackend(); // Private constructor for singleton

public:
    // Singleton access
    static InputBackend& instance();

    ~InputBackend();

    // Prevent copying
    InputBackend(const InputBackend&) = delete;
    void operator=(const InputBackend&) = delete;

    // ---- Device Management ----
    std::shared_ptr<jnivm::android::view::InputDevice> addDevice(int id, const std::string& name, int vendor, int product, int sources);
    std::shared_ptr<jnivm::android::view::InputDevice> getDevice(int id);
    std::shared_ptr<FakeJni::JArray<FakeJni::JInt>> getDeviceIds();

    // ---- Event Loop ----
    void runEventLoop();
    void stop();

    // ---- Callbacks ----
    void setKeyCallback(std::function<void(std::shared_ptr<jnivm::android::view::KeyEvent>)> cb);
    void setMotionCallback(std::function<void(std::shared_ptr<jnivm::android::view::MotionEvent>)> cb);

    // Utilities
    static constexpr int toAndroidKeycode(SDL_Scancode sdl_scancode);
    static constexpr int toAndroidKeycode(SDL_ControllerButtonEvent sdl_button);
};
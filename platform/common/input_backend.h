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
#define INPUT_ID_TOUCH 4

class InputBackend {
private:
    std::unordered_map<int, std::shared_ptr<jnivm::android::view::InputDevice>> devices;
    std::mutex deviceMutex;
    bool running = false;

    // Callbacks to dispatch events to the Android shim layer
    std::function<void(std::shared_ptr<jnivm::android::view::KeyEvent>)> onKey;
    std::function<void(std::shared_ptr<jnivm::android::view::MotionEvent>)> onMotion;
    std::function<void(bool)> onFocus; // window focus gained (true) / lost (false)

    std::unordered_map<int, float> mControllerAxisState;

    // ---- Touchscreen ----
    // A finger that is currently down. The vector is kept in the order the fingers went down, which is also
    // Android's pointer index order; "id" is the stable Android pointer id (smallest unused number).
    struct ActiveFinger {
        SDL_FingerID sdlId;
        int id;
        float x, y, pressure;
    };
    std::vector<ActiveFinger> fingers;
    long touchDownTime = 0;
    void handleFingerEvent(const SDL_Event& e);
    void sendTouchEvent(int action, long eventTime);
    void cancelTouches(); // ends any gesture in progress (focus lost, ...)

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
    void setFocusCallback(std::function<void(bool)> cb);

    // Utilities
    static constexpr int toAndroidKeycode(SDL_Scancode sdl_scancode);
    static int toAndroidKeycode(SDL_ControllerButtonEvent sdl_button); // not constexpr: depends on [input] settings
};
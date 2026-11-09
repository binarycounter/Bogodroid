#include "input_backend.h"
#include "android.h"
#include "logging.h"
#include "display_info.h"
#include <algorithm>
#include <chrono>
#include <iostream>

#include "toml++/toml.hpp"
extern toml::table config;

static bool input_enable_controller = false;
static bool input_mouse_touch_mode = false;
static bool input_mouse_accurate_mode = false;
// Controller layout. SDL reports the physical Xbox-style layout (A = bottom button). With swap_ab/swap_xy (the
// default, matching handhelds with Nintendo-style labels) A<->B and X<->Y are exchanged before reaching Android.
static bool input_swap_ab = true;
static bool input_swap_xy = true;
// What the controller's Back/Select button sends: 0 = KEYCODE_BUTTON_SELECT, 1 = KEYCODE_BACK, 2 = both.
static int input_back_mode = 0;
// Also send KEYCODE_BACK when the keyboard's Escape is pressed (Android's Back key doubles as Escape in Unity).
static bool input_escape_as_back = false;
// Real touchscreen support: SDL finger events become Android multi-touch MotionEvents.
// [input] touch = true/false/"auto" (default auto: on when SDL finds a touch device).
static bool input_touch_enabled = false;
static int buttonState;

InputBackend& InputBackend::instance()
{
    static InputBackend backend;
    return backend;
}

InputBackend::InputBackend()
{
    input_enable_controller = config["input"]["controller"].value_or<bool>(false);
    input_mouse_touch_mode = config["input"]["touch_mode"].value_or<bool>(false);
    input_mouse_accurate_mode = config["input"]["accurate_mode"].value_or<bool>(false);
    input_swap_ab = config["input"]["swap_ab"].value_or<bool>(true);
    input_swap_xy = config["input"]["swap_xy"].value_or<bool>(true);
    std::string backBtn = config["input"]["back_button"].value_or<std::string>("select");
    input_back_mode = (backBtn == "back") ? 1 : (backBtn == "both") ? 2 : 0;
    input_escape_as_back = config["input"]["escape_as_back"].value_or<bool>(false);
    if (auto forced = config["input"]["touch"].value<bool>()) {
        input_touch_enabled = *forced;
    } else {
        // "auto" (or unset): enabled only when the hardware has a touchscreen. SDL lists touch devices once the
        // video subsystem is up.
        input_touch_enabled = SDL_InitSubSystem(SDL_INIT_VIDEO) == 0 && SDL_GetNumTouchDevices() > 0;
    }

    if (SDL_Init(SDL_INIT_EVENTS) < 0) {
        std::cerr << "SDL Init failed: " << SDL_GetError() << std::endl;
        return;
    }

    // Add default devices
    addDevice(INPUT_ID_KEYBOARD, "Bogodroid Keyboard", 0x046d, 0xc316, jnivm::android::view::InputDevice::SOURCE_KEYBOARD);
    auto mouse = addDevice(INPUT_ID_MOUSE, "Bogodroid Mouse", 0x046D, 0xC077, input_mouse_touch_mode ? jnivm::android::view::InputDevice::SOURCE_TOUCHSCREEN : jnivm::android::view::InputDevice::SOURCE_MOUSE);
    mouse->addMotionRange(jnivm::android::view::MotionEvent::AXIS_X, mouse->source, 0.0f, (float)display_get_width(), 0.0f, 1.0f);
    mouse->addMotionRange(jnivm::android::view::MotionEvent::AXIS_Y, mouse->source, 0.0f, (float)display_get_height(), 0.0f, 1.0f);
    mouse->addMotionRange(jnivm::android::view::MotionEvent::AXIS_VSCROLL, mouse->source, -1.0f, 1.0f, 0.0f, 0.0f);
    mouse->addMotionRange(jnivm::android::view::MotionEvent::AXIS_HSCROLL, mouse->source, -1.0f, 1.0f, 0.0f, 0.0f);

    if (input_touch_enabled) {
        auto touch = addDevice(INPUT_ID_TOUCH, "Bogodroid Touchscreen", 0x0001, 0x0001, jnivm::android::view::InputDevice::SOURCE_TOUCHSCREEN);
        touch->addMotionRange(jnivm::android::view::MotionEvent::AXIS_X, touch->source, 0.0f, (float)display_get_width(), 0.0f, 0.0f);
        touch->addMotionRange(jnivm::android::view::MotionEvent::AXIS_Y, touch->source, 0.0f, (float)display_get_height(), 0.0f, 0.0f);
        touch->addMotionRange(jnivm::android::view::MotionEvent::AXIS_PRESSURE, touch->source, 0.0f, 1.0f, 0.0f, 0.0f);
        touch->addMotionRange(jnivm::android::view::MotionEvent::AXIS_SIZE, touch->source, 0.0f, 1.0f, 0.0f, 0.0f);
        verbose("InputBackend", "Touchscreen enabled");
    }

    if (input_enable_controller) {
        if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
            std::cerr << "SDL Joystick Init failed: " << SDL_GetError() << std::endl;
            return;
        }

        // mouse->addMotionRange(jnivm::android::view::MotionEvent::AXIS_VSCROLL, mouse->source, -1.0f, 1.0f, 0.0f, 0.0f);
        auto xbox = addDevice(INPUT_ID_XBOX, "Xbox 360 Controller", 0x045E, 0x028E, jnivm::android::view::InputDevice::SOURCE_GAMEPAD | jnivm::android::view::InputDevice::SOURCE_JOYSTICK);
        xbox->addMotionRange(jnivm::android::view::MotionEvent::AXIS_X, xbox->source, -1.0f, 1.0f, 0.12f, 0.0f);
        xbox->addMotionRange(jnivm::android::view::MotionEvent::AXIS_Y, xbox->source, -1.0f, 1.0f, 0.12f, 0.0f);
        xbox->addMotionRange(jnivm::android::view::MotionEvent::AXIS_RZ, xbox->source, -1.0f, 1.0f, 0.12f, 0.0f);
        xbox->addMotionRange(jnivm::android::view::MotionEvent::AXIS_Z, xbox->source, -1.0f, 1.0f, 0.12f, 0.0f);
        xbox->addMotionRange(jnivm::android::view::MotionEvent::AXIS_GAS, xbox->source, 0.0f, 1.0f, 0.0f, 0.0f);
        xbox->addMotionRange(jnivm::android::view::MotionEvent::AXIS_BRAKE, xbox->source, 0.0f, 1.0f, 0.0f, 0.0f);

        // Open game controllers
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (SDL_IsGameController(i)) {
                SDL_GameControllerOpen(i);
                verbose("InputBackend", "Opened Game Controller: %s", SDL_GameControllerNameForIndex(i));
            }
        }

        mControllerAxisState[jnivm::android::view::MotionEvent::AXIS_X] = 0.0f;
        mControllerAxisState[jnivm::android::view::MotionEvent::AXIS_Y] = 0.0f;
        mControllerAxisState[jnivm::android::view::MotionEvent::AXIS_RZ] = 0.0f;
        mControllerAxisState[jnivm::android::view::MotionEvent::AXIS_Z] = 0.0f;
        mControllerAxisState[jnivm::android::view::MotionEvent::AXIS_BRAKE] = 0.0f;
        mControllerAxisState[jnivm::android::view::MotionEvent::AXIS_GAS] = 0.0f;
        // mControllerAxisState[jnivm::android::view::MotionEvent::AXIS_LTRIGGER] = 0.0f;
        // mControllerAxisState[jnivm::android::view::MotionEvent::AXIS_RTRIGGER] = 0.0f;
    }
}

InputBackend::~InputBackend()
{
    if (input_enable_controller) {
        // Clean up controllers
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (SDL_IsGameController(i)) {
                SDL_GameController* controller = SDL_GameControllerFromInstanceID(SDL_JoystickGetDeviceInstanceID(i));
                if (controller) {
                    SDL_GameControllerClose(controller);
                }
            }
        }
    }
    SDL_Quit();
}

// ---- Device Management ----
std::shared_ptr<jnivm::android::view::InputDevice> InputBackend::addDevice(int id, const std::string& name, int vendor, int product, int sources)
{
    std::lock_guard<std::mutex> lock(deviceMutex);
    auto device = std::make_shared<jnivm::android::view::InputDevice>();
    device->id = id;
    device->name = std::make_shared<FakeJni::JString>(name);
    device->vendor = vendor;
    device->product = product;
    device->source = sources;
    devices[id] = device;
    return device;
}

std::shared_ptr<jnivm::android::view::InputDevice> InputBackend::getDevice(int id)
{
    std::lock_guard<std::mutex> lock(deviceMutex);
    auto it = devices.find(id);
    return (it != devices.end()) ? it->second : nullptr;
}

std::shared_ptr<FakeJni::JArray<FakeJni::JInt>> InputBackend::getDeviceIds()
{
    std::lock_guard<std::mutex> lock(deviceMutex);
    auto ids = std::make_shared<FakeJni::JArray<FakeJni::JInt>>(devices.size());
    int i = 0;
    for (const auto& kv : devices) {
        (*ids)[i++] = kv.first;
    }
    return ids;
}

static long nowMillis()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// Builds one MotionEvent carrying every finger that is currently down and hands it to the game.
void InputBackend::sendTouchEvent(int action, long eventTime)
{
    if (!onMotion || fingers.empty())
        return;
    auto event = std::make_shared<jnivm::android::view::MotionEvent>(devices[INPUT_ID_TOUCH], action, fingers[0].x, fingers[0].y);
    event->timestamp = eventTime;
    event->downTime = touchDownTime;
    for (auto& f : fingers) {
        jnivm::android::view::MotionEvent::Pointer p;
        p.id = f.id;
        p.x = f.x;
        p.y = f.y;
        p.pressure = f.pressure;
        event->pointers.push_back(p);
    }
    onMotion(event);
}

void InputBackend::handleFingerEvent(const SDL_Event& e)
{
    using ME = jnivm::android::view::MotionEvent;
    const SDL_TouchFingerEvent& t = e.tfinger;
    // SDL gives positions as 0..1 of the touch surface; Android wants pixels of the screen.
    float px = t.x * (float)display_get_width();
    float py = t.y * (float)display_get_height();
    float pressure = t.pressure > 0.0f ? t.pressure : 1.0f;
    long now = nowMillis();

    auto it = std::find_if(fingers.begin(), fingers.end(), [&](const ActiveFinger& f) { return f.sdlId == t.fingerId; });

    if (e.type == SDL_FINGERDOWN) {
        if (it != fingers.end())
            return; // already down (duplicate event)
        // The new finger gets the smallest Android pointer id that no other finger is using.
        int id = 0;
        while (std::any_of(fingers.begin(), fingers.end(), [&](const ActiveFinger& f) { return f.id == id; }))
            id++;
        bool first = fingers.empty();
        if (first)
            touchDownTime = now;
        fingers.push_back({ t.fingerId, id, px, py, pressure });
        int index = (int)fingers.size() - 1;
        sendTouchEvent(first ? ME::ACTION_DOWN : (ME::ACTION_POINTER_DOWN | (index << ME::ACTION_POINTER_INDEX_SHIFT)), now);
    } else if (e.type == SDL_FINGERMOTION) {
        if (it == fingers.end())
            return;
        if (it->x == px && it->y == py && it->pressure == pressure)
            return; // nothing changed
        it->x = px;
        it->y = py;
        it->pressure = pressure;
        sendTouchEvent(ME::ACTION_MOVE, now); // MOVE always lists all pointers, as on Android
    } else if (e.type == SDL_FINGERUP) {
        if (it == fingers.end())
            return;
        it->x = px;
        it->y = py;
        int index = (int)(it - fingers.begin());
        // The lifted finger is still part of the UP / POINTER_UP event, then it is removed.
        sendTouchEvent(fingers.size() == 1 ? ME::ACTION_UP : (ME::ACTION_POINTER_UP | (index << ME::ACTION_POINTER_INDEX_SHIFT)), now);
        fingers.erase(fingers.begin() + index);
    }
}

void InputBackend::cancelTouches()
{
    if (fingers.empty())
        return;
    sendTouchEvent(jnivm::android::view::MotionEvent::ACTION_CANCEL, nowMillis());
    fingers.clear();
}

void InputBackend::setFocusCallback(std::function<void(bool)> cb)
{
    onFocus = cb;
}

// ---- Event Loop ----
void InputBackend::runEventLoop()
{
    running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                // Window closed / quit requested by the OS: shut the game down instead of just killing input.
                request_quit("SDL_QUIT");
                running = false;
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                if (!onKey)
                    break;
                int action = (e.type == SDL_KEYDOWN) ? jnivm::android::view::KeyEvent::ACTION_DOWN : jnivm::android::view::KeyEvent::ACTION_UP;
                int keyCode = InputBackend::toAndroidKeycode(e.key.keysym.scancode);

                auto keyEvent = std::make_shared<jnivm::android::view::KeyEvent>(devices[INPUT_ID_KEYBOARD], action, keyCode, 0);
                onKey(keyEvent);
                if (input_escape_as_back && e.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                    onKey(std::make_shared<jnivm::android::view::KeyEvent>(devices[INPUT_ID_KEYBOARD], action, jnivm::android::view::KeyEvent::KEYCODE_BACK, 0));
                break;
            }

            case SDL_MOUSEMOTION: {
                if (!onMotion)
                    break;
                // SDL also turns touches into fake mouse events; with real touch support those would be duplicates.
                if (input_touch_enabled && e.motion.which == SDL_TOUCH_MOUSEID)
                    break;

                // In touch screen emulation move events are only generated when a button is pressed.
                if (input_mouse_touch_mode && !buttonState)
                    break;

                // Mouse motion is a generic motion event.
                auto motionEvent = std::make_shared<jnivm::android::view::MotionEvent>(devices[INPUT_ID_MOUSE],
                    buttonState == 0 ? jnivm::android::view::MotionEvent::ACTION_HOVER_MOVE : jnivm::android::view::MotionEvent::ACTION_MOVE,
                    (float)e.motion.x, (float)e.motion.y);
                motionEvent->buttonState = buttonState;
                onMotion(motionEvent);
                break;
            }

            case SDL_MOUSEBUTTONDOWN: {
                if (!onMotion)
                    break;
                if (input_touch_enabled && e.button.which == SDL_TOUCH_MOUSEID)
                    break;

                int previousButtonState = buttonState; // Store previous state for comparison

                // Update button state based on which button was pressed
                if (e.button.button == SDL_BUTTON_LEFT)
                    buttonState |= jnivm::android::view::MotionEvent::BUTTON_PRIMARY;
                if (e.button.button == SDL_BUTTON_RIGHT)
                    buttonState |= jnivm::android::view::MotionEvent::BUTTON_SECONDARY;
                if (e.button.button == SDL_BUTTON_MIDDLE)
                    buttonState |= jnivm::android::view::MotionEvent::BUTTON_TERTIARY;

                // Create the main motion event
                auto motionEvent = std::make_shared<jnivm::android::view::MotionEvent>(
                    devices[INPUT_ID_MOUSE],
                    jnivm::android::view::MotionEvent::ACTION_DOWN,
                    (float)e.button.x, (float)e.button.y);

                if (input_mouse_touch_mode) {
                    if(previousButtonState != 0)
                        break; // Don't send another event if another button is already pressed down
                    motionEvent->buttonState = jnivm::android::view::MotionEvent::BUTTON_PRIMARY;
                } else
                    motionEvent->buttonState = buttonState;

                // In non-touch mode, send an additional button press event
                if (!input_mouse_touch_mode && input_mouse_accurate_mode) {
                    auto motionEvent2 = std::make_shared<jnivm::android::view::MotionEvent>(
                        devices[INPUT_ID_MOUSE],
                        jnivm::android::view::MotionEvent::ACTION_BUTTON_PRESS,
                        (float)e.button.x, (float)e.button.y);
                    motionEvent2->buttonState = buttonState;
                    onMotion(motionEvent2);

                    if(previousButtonState == 0) // Additional HOVER_EXIT event is sent when the first button is pressed
                    {
                        auto motionEvent3 = std::make_shared<jnivm::android::view::MotionEvent>(
                            devices[INPUT_ID_MOUSE],
                            jnivm::android::view::MotionEvent::ACTION_HOVER_EXIT,
                            (float)e.button.x, (float)e.button.y);
                            motionEvent3->buttonState = buttonState;
                            onMotion(motionEvent3);
                    }
                }

                onMotion(motionEvent);
                break;
            }

            case SDL_MOUSEBUTTONUP: {
                if (!onMotion)
                    break;
                if (input_touch_enabled && e.button.which == SDL_TOUCH_MOUSEID)
                    break;

                // Update button state based on which button was released
                if (e.button.button == SDL_BUTTON_LEFT)
                    buttonState &= ~jnivm::android::view::MotionEvent::BUTTON_PRIMARY;
                if (e.button.button == SDL_BUTTON_RIGHT)
                    buttonState &= ~jnivm::android::view::MotionEvent::BUTTON_SECONDARY;
                if (e.button.button == SDL_BUTTON_MIDDLE)
                    buttonState &= ~jnivm::android::view::MotionEvent::BUTTON_TERTIARY;

                // Create the main motion event
                auto motionEvent = std::make_shared<jnivm::android::view::MotionEvent>(
                    devices[INPUT_ID_MOUSE],
                    jnivm::android::view::MotionEvent::ACTION_UP,
                    (float)e.button.x, (float)e.button.y);

                if (input_mouse_touch_mode)
                {
                    if(buttonState != 0)
                        break; // Don't send another event if another button is still pressed down
                    motionEvent->buttonState = 0;
                }
                else
                    motionEvent->buttonState = buttonState;

                // In non-touch mode, send an additional button release event
                if (!input_mouse_touch_mode && input_mouse_accurate_mode) {
                    auto motionEvent2 = std::make_shared<jnivm::android::view::MotionEvent>(
                        devices[INPUT_ID_MOUSE],
                        jnivm::android::view::MotionEvent::ACTION_BUTTON_RELEASE,
                        (float)e.button.x, (float)e.button.y);
                    motionEvent2->buttonState = buttonState;
                    onMotion(motionEvent2);

                    if(buttonState == 0) // Additional HOVER_ENTER event is sent when the last button is released
                    {
                        auto motionEvent3 = std::make_shared<jnivm::android::view::MotionEvent>(
                            devices[INPUT_ID_MOUSE],
                            jnivm::android::view::MotionEvent::ACTION_HOVER_ENTER,
                            (float)e.button.x, (float)e.button.y);
                            motionEvent3->buttonState = buttonState;
                            onMotion(motionEvent3);
                    }
                }

                onMotion(motionEvent);
                break;
            }

            case SDL_FINGERDOWN:
            case SDL_FINGERMOTION:
            case SDL_FINGERUP:
                if (input_touch_enabled)
                    handleFingerEvent(e);
                break;

            case SDL_MOUSEWHEEL: {
                if (!onMotion)
                    break;
                // Android reports wheel movement as a SCROLL motion event carrying VSCROLL/HSCROLL axis values.
                auto motionEvent = std::make_shared<jnivm::android::view::MotionEvent>(devices[INPUT_ID_MOUSE],
                    jnivm::android::view::MotionEvent::ACTION_SCROLL, 0.0f, 0.0f);
                motionEvent->axisValues[jnivm::android::view::MotionEvent::AXIS_VSCROLL] = (float)e.wheel.y;
                motionEvent->axisValues[jnivm::android::view::MotionEvent::AXIS_HSCROLL] = (float)e.wheel.x;
                onMotion(motionEvent);
                break;
            }

            case SDL_CONTROLLERDEVICEADDED: {
                // A gamepad was plugged in after startup. The virtual "Xbox controller" Android device always
                // exists, so only the SDL side needs to start listening to it.
                if (input_enable_controller && SDL_IsGameController(e.cdevice.which)) {
                    SDL_GameControllerOpen(e.cdevice.which);
                    verbose("InputBackend", "Controller added: %s", SDL_GameControllerNameForIndex(e.cdevice.which));
                }
                break;
            }

            case SDL_CONTROLLERDEVICEREMOVED: {
                if (input_enable_controller) {
                    SDL_GameController* controller = SDL_GameControllerFromInstanceID(e.cdevice.which);
                    if (controller)
                        SDL_GameControllerClose(controller);
                    verbose("InputBackend", "Controller removed");
                }
                break;
            }

            case SDL_WINDOWEVENT: {
                if (input_touch_enabled && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                    cancelTouches();
                if (!onFocus)
                    break;
                if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                    onFocus(false);
                else if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
                    onFocus(true);
                break;
            }

            case SDL_CONTROLLERAXISMOTION: {
                if (!input_enable_controller)
                    break;
                if (!onMotion)
                    break;

                int axis = -1;
                float value = 0.0f;
                bool isTrigger = false;

                switch (e.caxis.axis) {
                case SDL_CONTROLLER_AXIS_LEFTX:
                    axis = jnivm::android::view::MotionEvent::AXIS_X;
                    break;
                case SDL_CONTROLLER_AXIS_LEFTY:
                    axis = jnivm::android::view::MotionEvent::AXIS_Y;
                    break;
                case SDL_CONTROLLER_AXIS_RIGHTX:
                    axis = jnivm::android::view::MotionEvent::AXIS_Z;
                    break;
                case SDL_CONTROLLER_AXIS_RIGHTY:
                    axis = jnivm::android::view::MotionEvent::AXIS_RZ;
                    break;
                case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
                    axis = jnivm::android::view::MotionEvent::AXIS_BRAKE;
                    isTrigger = true;
                    break;
                case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                    axis = jnivm::android::view::MotionEvent::AXIS_GAS;
                    isTrigger = true;
                    break;
                }

                if (axis == -1)
                    break; // Not an axis we are mapping.

                // Normalize the value
                if (isTrigger) {
                    value = e.caxis.value / 32767.0f;
                } else {
                    value = e.caxis.value < 0 ? e.caxis.value / 32768.0f : e.caxis.value / 32767.0f;
                }

                // Update the single value in our state map
                mControllerAxisState[axis] = value;

                auto dev = devices[INPUT_ID_XBOX];
                auto motionEvent = std::make_shared<jnivm::android::view::MotionEvent>(
                    dev, jnivm::android::view::MotionEvent::ACTION_MOVE, 0.0f, 0.0f);

                motionEvent->axisValues = mControllerAxisState;

                onMotion(motionEvent);
                break;
            }

            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP: {
                if (!input_enable_controller)
                    break;
                if (!onKey)
                    break;
                // Controller buttons are sent as KeyEvents.
                int action = (e.type == SDL_CONTROLLERBUTTONDOWN) ? jnivm::android::view::KeyEvent::ACTION_DOWN : jnivm::android::view::KeyEvent::ACTION_UP;
                int keyCode = toAndroidKeycode(e.cbutton);

                auto keyEvent = std::make_shared<jnivm::android::view::KeyEvent>(devices[INPUT_ID_XBOX], action, keyCode, 0);
                onKey(keyEvent);
                if (input_back_mode == 2 && e.cbutton.button == SDL_CONTROLLER_BUTTON_BACK)
                    onKey(std::make_shared<jnivm::android::view::KeyEvent>(devices[INPUT_ID_XBOX], action, jnivm::android::view::KeyEvent::KEYCODE_BACK, 0));
                break;
            }

            default:
                break;
            }
        }
        SDL_Delay(4); // Be a good citizen
    }
}

int InputBackend::toAndroidKeycode(SDL_ControllerButtonEvent sdl_button)
{
    using KE = jnivm::android::view::KeyEvent;
    uint8_t button = sdl_button.button;
    switch (button) {
    case SDL_CONTROLLER_BUTTON_A:
        return input_swap_ab ? KE::KEYCODE_BUTTON_B : KE::KEYCODE_BUTTON_A;
    case SDL_CONTROLLER_BUTTON_B:
        return input_swap_ab ? KE::KEYCODE_BUTTON_A : KE::KEYCODE_BUTTON_B;
    case SDL_CONTROLLER_BUTTON_X:
        return input_swap_xy ? KE::KEYCODE_BUTTON_Y : KE::KEYCODE_BUTTON_X;
    case SDL_CONTROLLER_BUTTON_Y:
        return input_swap_xy ? KE::KEYCODE_BUTTON_X : KE::KEYCODE_BUTTON_Y;
    case SDL_CONTROLLER_BUTTON_BACK:
        return input_back_mode == 1 ? KE::KEYCODE_BACK : KE::KEYCODE_BUTTON_SELECT;
    case SDL_CONTROLLER_BUTTON_GUIDE:
        return jnivm::android::view::KeyEvent::KEYCODE_BUTTON_MODE;
    case SDL_CONTROLLER_BUTTON_START:
        return jnivm::android::view::KeyEvent::KEYCODE_BUTTON_START;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
        return jnivm::android::view::KeyEvent::KEYCODE_BUTTON_THUMBL;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
        return jnivm::android::view::KeyEvent::KEYCODE_BUTTON_THUMBR;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        return jnivm::android::view::KeyEvent::KEYCODE_BUTTON_L1;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
        return jnivm::android::view::KeyEvent::KEYCODE_BUTTON_R1;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        return jnivm::android::view::KeyEvent::KEYCODE_DPAD_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        return jnivm::android::view::KeyEvent::KEYCODE_DPAD_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        return jnivm::android::view::KeyEvent::KEYCODE_DPAD_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        return jnivm::android::view::KeyEvent::KEYCODE_DPAD_RIGHT;
    default:
        return jnivm::android::view::KeyEvent::KEYCODE_UNKNOWN;
    }
}

constexpr int InputBackend::toAndroidKeycode(SDL_Scancode sdl_scancode)
{
    switch (sdl_scancode) {
    case SDL_SCANCODE_0:
        return jnivm::android::view::KeyEvent::KEYCODE_0;
    case SDL_SCANCODE_1:
        return jnivm::android::view::KeyEvent::KEYCODE_1;
    case SDL_SCANCODE_2:
        return jnivm::android::view::KeyEvent::KEYCODE_2;
    case SDL_SCANCODE_3:
        return jnivm::android::view::KeyEvent::KEYCODE_3;
    case SDL_SCANCODE_4:
        return jnivm::android::view::KeyEvent::KEYCODE_4;
    case SDL_SCANCODE_5:
        return jnivm::android::view::KeyEvent::KEYCODE_5;
    case SDL_SCANCODE_6:
        return jnivm::android::view::KeyEvent::KEYCODE_6;
    case SDL_SCANCODE_7:
        return jnivm::android::view::KeyEvent::KEYCODE_7;
    case SDL_SCANCODE_8:
        return jnivm::android::view::KeyEvent::KEYCODE_8;
    case SDL_SCANCODE_9:
        return jnivm::android::view::KeyEvent::KEYCODE_9;
    case SDL_SCANCODE_A:
        return jnivm::android::view::KeyEvent::KEYCODE_A;
    case SDL_SCANCODE_APOSTROPHE:
        return jnivm::android::view::KeyEvent::KEYCODE_APOSTROPHE;
    case SDL_SCANCODE_B:
        return jnivm::android::view::KeyEvent::KEYCODE_B;
    case SDL_SCANCODE_BACKSLASH:
        return jnivm::android::view::KeyEvent::KEYCODE_BACKSLASH;
    case SDL_SCANCODE_BACKSPACE:
        return jnivm::android::view::KeyEvent::KEYCODE_DEL;
    case SDL_SCANCODE_C:
        return jnivm::android::view::KeyEvent::KEYCODE_C;
    case SDL_SCANCODE_CAPSLOCK:
        return jnivm::android::view::KeyEvent::KEYCODE_CAPS_LOCK;
    case SDL_SCANCODE_CLEAR:
        return jnivm::android::view::KeyEvent::KEYCODE_CLEAR;
    case SDL_SCANCODE_COMMA:
        return jnivm::android::view::KeyEvent::KEYCODE_COMMA;
    case SDL_SCANCODE_D:
        return jnivm::android::view::KeyEvent::KEYCODE_D;
    case SDL_SCANCODE_DELETE:
        return jnivm::android::view::KeyEvent::KEYCODE_FORWARD_DEL;
    case SDL_SCANCODE_DOWN:
        return jnivm::android::view::KeyEvent::KEYCODE_DPAD_DOWN;
    case SDL_SCANCODE_E:
        return jnivm::android::view::KeyEvent::KEYCODE_E;
    case SDL_SCANCODE_END:
        return jnivm::android::view::KeyEvent::KEYCODE_MOVE_END;
    case SDL_SCANCODE_EQUALS:
        return jnivm::android::view::KeyEvent::KEYCODE_EQUALS;
    case SDL_SCANCODE_ESCAPE:
        return jnivm::android::view::KeyEvent::KEYCODE_ESCAPE;
    case SDL_SCANCODE_F:
        return jnivm::android::view::KeyEvent::KEYCODE_F;
    case SDL_SCANCODE_F1:
        return jnivm::android::view::KeyEvent::KEYCODE_F1;
    case SDL_SCANCODE_F2:
        return jnivm::android::view::KeyEvent::KEYCODE_F2;
    case SDL_SCANCODE_F3:
        return jnivm::android::view::KeyEvent::KEYCODE_F3;
    case SDL_SCANCODE_F4:
        return jnivm::android::view::KeyEvent::KEYCODE_F4;
    case SDL_SCANCODE_F5:
        return jnivm::android::view::KeyEvent::KEYCODE_F5;
    case SDL_SCANCODE_F6:
        return jnivm::android::view::KeyEvent::KEYCODE_F6;
    case SDL_SCANCODE_F7:
        return jnivm::android::view::KeyEvent::KEYCODE_F7;
    case SDL_SCANCODE_F8:
        return jnivm::android::view::KeyEvent::KEYCODE_F8;
    case SDL_SCANCODE_F9:
        return jnivm::android::view::KeyEvent::KEYCODE_F9;
    case SDL_SCANCODE_F10:
        return jnivm::android::view::KeyEvent::KEYCODE_F10;
    case SDL_SCANCODE_F11:
        return jnivm::android::view::KeyEvent::KEYCODE_F11;
    case SDL_SCANCODE_F12:
        return jnivm::android::view::KeyEvent::KEYCODE_F12;
    case SDL_SCANCODE_G:
        return jnivm::android::view::KeyEvent::KEYCODE_G;
    case SDL_SCANCODE_GRAVE:
        return jnivm::android::view::KeyEvent::KEYCODE_GRAVE;
    case SDL_SCANCODE_H:
        return jnivm::android::view::KeyEvent::KEYCODE_H;
    case SDL_SCANCODE_HOME:
        return jnivm::android::view::KeyEvent::KEYCODE_MOVE_HOME;
    case SDL_SCANCODE_I:
        return jnivm::android::view::KeyEvent::KEYCODE_I;
    case SDL_SCANCODE_INSERT:
        return jnivm::android::view::KeyEvent::KEYCODE_INSERT;
    case SDL_SCANCODE_J:
        return jnivm::android::view::KeyEvent::KEYCODE_J;
    case SDL_SCANCODE_K:
        return jnivm::android::view::KeyEvent::KEYCODE_K;
    case SDL_SCANCODE_KP_0:
        return jnivm::android::view::KeyEvent::KEYCODE_NUMPAD_0;
    case SDL_SCANCODE_KP_1:
        return jnivm::android::view::KeyEvent::KEYCODE_NUMPAD_1;
    case SDL_SCANCODE_KP_2:
        return jnivm::android::view::KeyEvent::KEYCODE_NUMPAD_2;
    case SDL_SCANCODE_KP_3:
        return jnivm::android::view::KeyEvent::KEYCODE_NUMPAD_3;
    case SDL_SCANCODE_KP_4:
        return jnivm::android::view::KeyEvent::KEYCODE_NUMPAD_4;
    case SDL_SCANCODE_KP_5:
        return jnivm::android::view::KeyEvent::KEYCODE_NUMPAD_5;
    case SDL_SCANCODE_KP_6:
        return jnivm::android::view::KeyEvent::KEYCODE_NUMPAD_6;
    case SDL_SCANCODE_KP_7:
        return jnivm::android::view::KeyEvent::KEYCODE_NUMPAD_7;
    case SDL_SCANCODE_KP_8:
        return jnivm::android::view::KeyEvent::KEYCODE_NUMPAD_8;
    case SDL_SCANCODE_KP_9:
        return jnivm::android::view::KeyEvent::KEYCODE_NUMPAD_9;
    case SDL_SCANCODE_KP_DIVIDE:
        return jnivm::android::view::KeyEvent::KEYCODE_NUMPAD_DIVIDE;
    case SDL_SCANCODE_KP_ENTER:
        return jnivm::android::view::KeyEvent::KEYCODE_NUMPAD_ENTER;
    case SDL_SCANCODE_KP_MINUS:
        return jnivm::android::view::KeyEvent::KEYCODE_NUMPAD_SUBTRACT;
    case SDL_SCANCODE_KP_MULTIPLY:
        return jnivm::android::view::KeyEvent::KEYCODE_NUMPAD_MULTIPLY;
    case SDL_SCANCODE_KP_PERIOD:
        return jnivm::android::view::KeyEvent::KEYCODE_NUMPAD_DOT;
    case SDL_SCANCODE_KP_PLUS:
        return jnivm::android::view::KeyEvent::KEYCODE_NUMPAD_ADD;
    case SDL_SCANCODE_L:
        return jnivm::android::view::KeyEvent::KEYCODE_L;
    case SDL_SCANCODE_LALT:
        return jnivm::android::view::KeyEvent::KEYCODE_ALT_LEFT;
    case SDL_SCANCODE_LCTRL:
        return jnivm::android::view::KeyEvent::KEYCODE_CTRL_LEFT;
    case SDL_SCANCODE_LEFT:
        return jnivm::android::view::KeyEvent::KEYCODE_DPAD_LEFT;
    case SDL_SCANCODE_LEFTBRACKET:
        return jnivm::android::view::KeyEvent::KEYCODE_LEFT_BRACKET;
    case SDL_SCANCODE_LSHIFT:
        return jnivm::android::view::KeyEvent::KEYCODE_SHIFT_LEFT;
    case SDL_SCANCODE_M:
        return jnivm::android::view::KeyEvent::KEYCODE_M;
    case SDL_SCANCODE_MENU:
        return jnivm::android::view::KeyEvent::KEYCODE_MENU;
    case SDL_SCANCODE_MINUS:
        return jnivm::android::view::KeyEvent::KEYCODE_MINUS;
    case SDL_SCANCODE_MUTE:
        return jnivm::android::view::KeyEvent::KEYCODE_MUTE;
    case SDL_SCANCODE_N:
        return jnivm::android::view::KeyEvent::KEYCODE_N;
    case SDL_SCANCODE_NUMLOCKCLEAR:
        return jnivm::android::view::KeyEvent::KEYCODE_NUM_LOCK;
    case SDL_SCANCODE_O:
        return jnivm::android::view::KeyEvent::KEYCODE_O;
    case SDL_SCANCODE_P:
        return jnivm::android::view::KeyEvent::KEYCODE_P;
    case SDL_SCANCODE_PAGEDOWN:
        return jnivm::android::view::KeyEvent::KEYCODE_PAGE_DOWN;
    case SDL_SCANCODE_PAGEUP:
        return jnivm::android::view::KeyEvent::KEYCODE_PAGE_UP;
    case SDL_SCANCODE_PAUSE:
        return jnivm::android::view::KeyEvent::KEYCODE_BREAK;
    case SDL_SCANCODE_PERIOD:
        return jnivm::android::view::KeyEvent::KEYCODE_PERIOD;
    case SDL_SCANCODE_POWER:
        return jnivm::android::view::KeyEvent::KEYCODE_POWER;
    case SDL_SCANCODE_PRINTSCREEN:
        return jnivm::android::view::KeyEvent::KEYCODE_SYSRQ;
    case SDL_SCANCODE_Q:
        return jnivm::android::view::KeyEvent::KEYCODE_Q;
    case SDL_SCANCODE_R:
        return jnivm::android::view::KeyEvent::KEYCODE_R;
    case SDL_SCANCODE_RALT:
        return jnivm::android::view::KeyEvent::KEYCODE_ALT_RIGHT;
    case SDL_SCANCODE_RCTRL:
        return jnivm::android::view::KeyEvent::KEYCODE_CTRL_RIGHT;
    case SDL_SCANCODE_RETURN:
        return jnivm::android::view::KeyEvent::KEYCODE_ENTER;
    case SDL_SCANCODE_RIGHT:
        return jnivm::android::view::KeyEvent::KEYCODE_DPAD_RIGHT;
    case SDL_SCANCODE_RIGHTBRACKET:
        return jnivm::android::view::KeyEvent::KEYCODE_RIGHT_BRACKET;
    case SDL_SCANCODE_RSHIFT:
        return jnivm::android::view::KeyEvent::KEYCODE_SHIFT_RIGHT;
    case SDL_SCANCODE_S:
        return jnivm::android::view::KeyEvent::KEYCODE_S;
    case SDL_SCANCODE_SCROLLLOCK:
        return jnivm::android::view::KeyEvent::KEYCODE_SCROLL_LOCK;
    case SDL_SCANCODE_SELECT:
        return jnivm::android::view::KeyEvent::KEYCODE_DPAD_CENTER;
    case SDL_SCANCODE_SEMICOLON:
        return jnivm::android::view::KeyEvent::KEYCODE_SEMICOLON;
    case SDL_SCANCODE_SLASH:
        return jnivm::android::view::KeyEvent::KEYCODE_SLASH;
    case SDL_SCANCODE_SPACE:
        return jnivm::android::view::KeyEvent::KEYCODE_SPACE;
    case SDL_SCANCODE_T:
        return jnivm::android::view::KeyEvent::KEYCODE_T;
    case SDL_SCANCODE_TAB:
        return jnivm::android::view::KeyEvent::KEYCODE_TAB;
    case SDL_SCANCODE_U:
        return jnivm::android::view::KeyEvent::KEYCODE_U;
    case SDL_SCANCODE_UP:
        return jnivm::android::view::KeyEvent::KEYCODE_DPAD_UP;
    case SDL_SCANCODE_V:
        return jnivm::android::view::KeyEvent::KEYCODE_V;
    case SDL_SCANCODE_VOLUMEDOWN:
        return jnivm::android::view::KeyEvent::KEYCODE_VOLUME_DOWN;
    case SDL_SCANCODE_VOLUMEUP:
        return jnivm::android::view::KeyEvent::KEYCODE_VOLUME_UP;
    case SDL_SCANCODE_W:
        return jnivm::android::view::KeyEvent::KEYCODE_W;
    case SDL_SCANCODE_X:
        return jnivm::android::view::KeyEvent::KEYCODE_X;
    case SDL_SCANCODE_Y:
        return jnivm::android::view::KeyEvent::KEYCODE_Y;
    case SDL_SCANCODE_Z:
        return jnivm::android::view::KeyEvent::KEYCODE_Z;
    default:
        return jnivm::android::view::KeyEvent::KEYCODE_UNKNOWN;
    }
}

// constexpr int InputBackend::toAndroidMetaState(SDL_KeyboardEvent event)
// {
//     SDL_Keymod mods = event.keysym.mod;
// }

void InputBackend::stop()
{
    running = false;
}

// ---- Callbacks ----
void InputBackend::setKeyCallback(std::function<void(std::shared_ptr<jnivm::android::view::KeyEvent>)> cb) { onKey = std::move(cb); }
void InputBackend::setMotionCallback(std::function<void(std::shared_ptr<jnivm::android::view::MotionEvent>)> cb) { onMotion = std::move(cb); }
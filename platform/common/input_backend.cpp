#include "input_backend.h"
#include "android.h"
#include "logging.h"
#include <iostream>

InputBackend& InputBackend::instance()
{
    static InputBackend backend;
    return backend;
}

InputBackend::InputBackend()
{
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        std::cerr << "SDL Init failed: " << SDL_GetError() << std::endl;
        return;
    }

    // Add default devices
    addDevice(INPUT_ID_KEYBOARD, "Bogodroid Keyboard", 0x046d, 0xc316, jnivm::android::view::InputDevice::SOURCE_KEYBOARD);
    auto mouse = addDevice(INPUT_ID_MOUSE, "Bogodroid Mouse", 0x046D, 0xC077, jnivm::android::view::InputDevice::SOURCE_MOUSE);
    mouse->addMotionRange(jnivm::android::view::MotionEvent::AXIS_X, mouse->source, 0.0f, 640.0f, 0.0f, 1.0f); // Example screen width
    mouse->addMotionRange(jnivm::android::view::MotionEvent::AXIS_Y, mouse->source, 0.0f, 480.0f, 0.0f, 1.0f); // Example screen height
    // mouse->addMotionRange(jnivm::android::view::MotionEvent::AXIS_VSCROLL, mouse->source, -1.0f, 1.0f, 0.0f, 0.0f);
    auto xbox = addDevice(INPUT_ID_XBOX, "Microsoft X-Box 360 pad", 0x045E, 0x028E, jnivm::android::view::InputDevice::SOURCE_GAMEPAD | jnivm::android::view::InputDevice::SOURCE_JOYSTICK);
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

InputBackend::~InputBackend()
{
    // Clean up controllers
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            SDL_GameController* controller = SDL_GameControllerFromInstanceID(SDL_JoystickGetDeviceInstanceID(i));
            if (controller) {
                SDL_GameControllerClose(controller);
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

// ---- Event Loop ----
void InputBackend::runEventLoop()
{
    running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
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
                break;
            }

            case SDL_MOUSEMOTION: {
                if (!onMotion)
                    break;
                // Mouse motion is a generic motion event.
                auto motionEvent = std::make_shared<jnivm::android::view::MotionEvent>(devices[INPUT_ID_MOUSE],
                    jnivm::android::view::MotionEvent::ACTION_MOVE,
                    (float)e.motion.x, (float)e.motion.y);
                onMotion(motionEvent);
                break;
            }

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                if (!onMotion)
                    break;
                int action = (e.type == SDL_MOUSEBUTTONDOWN) ? jnivm::android::view::MotionEvent::ACTION_DOWN : jnivm::android::view::MotionEvent::ACTION_UP;
                auto motionEvent = std::make_shared<jnivm::android::view::MotionEvent>(devices[INPUT_ID_MOUSE],
                    action, (float)e.button.x, (float)e.button.y);
                onMotion(motionEvent);
                break;
            }

            case SDL_CONTROLLERAXISMOTION: {
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
                    dev, jnivm::android::view::MotionEvent::ACTION_MOVE, 0.0f , 0.0f );

                motionEvent->axisValues = mControllerAxisState;

                onMotion(motionEvent);
                break;
            }

            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP: {
                if (!onKey)
                    break;
                // Controller buttons are sent as KeyEvents.
                int action = (e.type == SDL_CONTROLLERBUTTONDOWN) ? jnivm::android::view::KeyEvent::ACTION_DOWN : jnivm::android::view::KeyEvent::ACTION_UP;
                int keyCode = toAndroidKeycode(e.cbutton);

                auto keyEvent = std::make_shared<jnivm::android::view::KeyEvent>(devices[INPUT_ID_XBOX], action, keyCode, 0);
                onKey(keyEvent);
                break;
            }

            default:
                break;
            }
        }
        SDL_Delay(1); // Be a good citizen
    }
}

constexpr int InputBackend::toAndroidKeycode(SDL_ControllerButtonEvent sdl_button)
{
    uint8_t button = sdl_button.button;
    switch (button) {
    case SDL_CONTROLLER_BUTTON_A:
        return jnivm::android::view::KeyEvent::KEYCODE_BUTTON_B;
    case SDL_CONTROLLER_BUTTON_B:
        return jnivm::android::view::KeyEvent::KEYCODE_BUTTON_A;
    case SDL_CONTROLLER_BUTTON_X:
        return jnivm::android::view::KeyEvent::KEYCODE_BUTTON_Y;
    case SDL_CONTROLLER_BUTTON_Y:
        return jnivm::android::view::KeyEvent::KEYCODE_BUTTON_X;
    case SDL_CONTROLLER_BUTTON_BACK:
        return jnivm::android::view::KeyEvent::KEYCODE_BUTTON_SELECT;
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
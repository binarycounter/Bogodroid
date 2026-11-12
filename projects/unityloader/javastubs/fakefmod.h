#ifndef __FAKEFMOD_H__
#define __FAKEFMOD_H__

#include "baron/baron.h"
#include "android.h" // <<< Include for Runnable
#include <atomic>
#include <vector>
#include <SDL2/SDL.h>

namespace jnivm {
namespace org {
namespace fmod {

    class FMODAudioDevice : public virtual FakeJni::JObject {
    private:
        std::shared_ptr<jnivm::android::os::HandlerThread> mAudioThread;
        std::atomic<bool> mRunning;

        SDL_AudioDeviceID mAudioDevice;
        std::vector<uint8_t> mAudioBuffer;
        
        int local_fmodGetInfo(int info_id);
        int local_fmodProcess();

    public:
        DEFINE_CLASS_NAME("org/fmod/FMODAudioDevice")
        FMODAudioDevice();
        ~FMODAudioDevice();

        void start();
        void stop();
        void close();
        int startAudioRecord(int v1, int v2, int v3);
        void stopAudioRecord();

        void runAudio();
    };

}
}
}
#endif
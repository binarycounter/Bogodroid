#ifndef __FAKEFMOD_H__
#define __FAKEFMOD_H__

#include "baron/baron.h"

namespace jnivm
{
    namespace org
    {
        namespace fmod
        {

                class FMODAudioDevice : public FakeJni::JObject
                {
                public:
                    DEFINE_CLASS_NAME("org/fmod/FMODAudioDevice")
                    FMODAudioDevice();
                    void start();
                    void stop();
                    void close();
                    int startAudioRecord(int v1, int v2, int v3); //No idea what these are yet
                    void stopAudioRecord();
                };

        }
    }
}
#endif
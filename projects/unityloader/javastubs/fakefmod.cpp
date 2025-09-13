#include "fakefmod.h"
#include "baron/baron.h"



jnivm::org::fmod::FMODAudioDevice::FMODAudioDevice() { }

void jnivm::org::fmod::FMODAudioDevice::start(){}
void jnivm::org::fmod::FMODAudioDevice::stop(){}
void jnivm::org::fmod::FMODAudioDevice::close(){}
int jnivm::org::fmod::FMODAudioDevice::startAudioRecord(int v1, int v2, int v3){return 0;}
void jnivm::org::fmod::FMODAudioDevice::stopAudioRecord(){}

BEGIN_NATIVE_DESCRIPTOR(jnivm::org::fmod::FMODAudioDevice){FakeJni::Constructor<FMODAudioDevice>{}},
{FakeJni::Function<&FMODAudioDevice::start>{}, "start", FakeJni::JMethodID::PUBLIC},
{FakeJni::Function<&FMODAudioDevice::stop>{}, "stop", FakeJni::JMethodID::PUBLIC},
{FakeJni::Function<&FMODAudioDevice::close>{}, "close", FakeJni::JMethodID::PUBLIC},
{FakeJni::Function<&FMODAudioDevice::startAudioRecord>{}, "startAudioRecord", FakeJni::JMethodID::PUBLIC},
{FakeJni::Function<&FMODAudioDevice::stopAudioRecord>{}, "stopAudioRecord", FakeJni::JMethodID::PUBLIC},
END_NATIVE_DESCRIPTOR

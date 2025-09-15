#include "fakefmod.h"
#include "../globals.h" 
#include "android.h"
#include "logging.h"
#include <SDL2/SDL.h>
#include <cmath> 
#include <algorithm> 
#include <jnivm/bytebuffer.h>

using namespace jnivm::org::fmod;

FMODAudioDevice::FMODAudioDevice() : mRunning(false), mAudioDevice(0) { }

FMODAudioDevice::~FMODAudioDevice() {
    stop();
}

static FakeJni::JClass* fmodClass = nullptr;

void FMODAudioDevice::start() {
    if (mAudioThread) {
        stop();
    }

    if (!fmodClass) {
        fmodClass = vm.findClass("org/fmod/FMODAudioDevice").get();
    }

    mRunning.store(true);

    mAudioThread = std::make_shared<jnivm::android::os::HandlerThread>(std::make_shared<FakeJni::JString>("Audio"));
    
    mAudioThread->start();

    auto mLooper = mAudioThread->getLooper();
    auto mHandler = std::make_shared<jnivm::android::os::Handler>(mLooper);

    mHandler->post(jnivm::java::lang::LambdaRunnable::Create([this]() {
            this->runAudio();
        }));
    
}

void FMODAudioDevice::stop() {
    if (mAudioThread) {
        mRunning.store(false); // Break out of the audio loop
        mAudioThread->quit(); // Quit the Looper
        mAudioThread->join(); // Join the thread
        mAudioThread = nullptr;
    }
}

void FMODAudioDevice::close() {
    stop();
}

void FMODAudioDevice::runAudio() {
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        verbose("FMODAudioDevice", "SDL_Init(SDL_INIT_AUDIO) failed: %s", SDL_GetError());
        return;
    }

    // Step 2: Gather info from FMOD and set up the audio device
    int sampleRate = local_fmodGetInfo(0); 
    int numChannels = local_fmodGetInfo(4); 
    int bufferLengthSamples = local_fmodGetInfo(1);
    int numBuffers = local_fmodGetInfo(2); 

    if (sampleRate == 0) {
        verbose("FMODAudioDevice", "fmodGetInfo returned 0 for sample rate. Aborting.");
        return;
    }

    int sampleSize = 2; // FMOD typically uses 16-bit audio (2 bytes)
    int frameSize = numChannels * sampleSize;
    int bufferSize = bufferLengthSamples * frameSize;
    
    mAudioBuffer.resize(bufferSize);

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = sampleRate;
    want.format = AUDIO_S16LSB; 
    want.channels = numChannels;
    want.samples = bufferLengthSamples; 
    want.callback = nullptr; 

    mAudioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (mAudioDevice == 0) {
        verbose("FMODAudioDevice", "SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return;
    }
    verbose("FMODAudioDevice", "SDL Audio device opened. Rate: %d, Channels: %d", have.freq, have.channels);
    
    SDL_PauseAudioDevice(mAudioDevice, 0);
    
    while (mRunning.load()) {
        while(SDL_GetQueuedAudioSize(mAudioDevice) < 4096){
            local_fmodProcess();
            SDL_QueueAudio(mAudioDevice, mAudioBuffer.data(), mAudioBuffer.size());    
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    verbose("FMODAudioDevice", "Exiting audio loop.");
    
    SDL_CloseAudioDevice(mAudioDevice);
    mAudioDevice = 0;
}

int FMODAudioDevice::local_fmodGetInfo(int info_id) {
    FakeJni::LocalFrame frame(vm);
    auto fmodGetInfo = fmodClass->getMethod("(I)I", "fmodGetInfo");
    return fmodGetInfo.invoke(frame.getJniEnv(),fmodClass,info_id).i;
}

int FMODAudioDevice::local_fmodProcess()
{
    FakeJni::LocalFrame frame(vm);
    auto buffer = frame.getJniEnv().NewDirectByteBuffer(mAudioBuffer.data(),mAudioBuffer.size());
    auto fmodProcess = fmodClass->getMethod("(Ljava/nio/ByteBuffer;)I", "fmodProcess");
    return fmodProcess.invoke(frame.getJniEnv(),fmodClass,buffer).i;
}

int FMODAudioDevice::startAudioRecord(int v1, int v2, int v3) {
    verbose("FMODAudioDevice", "startAudioRecord called but not implemented.");
    return 0;
}

void FMODAudioDevice::stopAudioRecord() {
    verbose("FMODAudioDevice", "stopAudioRecord called but not implemented.");
}

BEGIN_NATIVE_DESCRIPTOR(jnivm::org::fmod::FMODAudioDevice)
    {FakeJni::Constructor<FMODAudioDevice>{}},
    {FakeJni::Function<&FMODAudioDevice::start>{}, "start", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&FMODAudioDevice::stop>{}, "stop", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&FMODAudioDevice::close>{}, "close", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&FMODAudioDevice::startAudioRecord>{}, "startAudioRecord", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&FMODAudioDevice::stopAudioRecord>{}, "stopAudioRecord", FakeJni::JMethodID::PUBLIC},
END_NATIVE_DESCRIPTOR
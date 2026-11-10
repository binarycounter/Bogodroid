// Safe "unsupported" stand-ins for Android's media and audio NDK libraries (libmediandk, AImageReader, libaaudio).
// There's no video decoder or AAudio here; without these the imports would be unresolved and any call would abort
// the game. With them, Unity's VideoPlayer (and plugins using AAudio/Oboe) see a normal failure and carry on.
#include "platform.h"
#include "thunk_gen.h"
#include "so_util.h"
#include "logging.h"
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

namespace {
    const int AMEDIA_OK = 0;
    const int AMEDIA_ERROR_UNSUPPORTED = -10002;
    const long AMEDIACODEC_INFO_TRY_AGAIN_LATER = -1;
    const int AAUDIO_ERROR_UNAVAILABLE = -889;
    const int AAUDIO_STREAM_STATE_DISCONNECTED = 13;

    // Opaque objects handed out so callers that don't check for NULL still have something to pass back to us.
    struct Dummy { int unused; } g_extractor, g_dataSource;

    void once(const char* what)
    {
        static bool logged[4] = {};
        int i = what[1] == 'M' ? 0 : what[1] == 'I' ? 1 : 2;
        if (!logged[i]) { logged[i] = true; printf("[MEDIA] %s: not supported here (no decoder / AAudio)\n", what); }
    }
}

extern "C" {
///// AMediaFormat keys are data symbols (const char*)
#define KEY(name, value) ABI_ATTR const char* AMEDIAFORMAT_KEY_##name = value;
KEY(MIME, "mime") KEY(WIDTH, "width") KEY(HEIGHT, "height") KEY(DURATION, "durationUs") KEY(FRAME_RATE, "frame-rate")
KEY(SAMPLE_RATE, "sample-rate") KEY(CHANNEL_COUNT, "channel-count") KEY(COLOR_FORMAT, "color-format") KEY(STRIDE, "stride")
KEY(SLICE_HEIGHT, "slice-height") KEY(LANGUAGE, "language") KEY(ROTATION, "rotation-degrees") KEY(COLOR_STANDARD, "color-standard")
KEY(COLOR_RANGE, "color-range") KEY(COLOR_TRANSFER, "color-transfer") KEY(ENCODER_DELAY, "encoder-delay")
KEY(AAC_MAX_OUTPUT_CHANNEL_COUNT, "aac-max-output-channel_count") KEY(BIT_RATE, "bitrate") KEY(MAX_INPUT_SIZE, "max-input-size")
#undef KEY

///// AMediaExtractor / AMediaDataSource / AMediaFormat
ABI_ATTR void* AMediaExtractor_new() { once("AMediaExtractor"); return &g_extractor; }
ABI_ATTR int AMediaExtractor_delete(void*) { return AMEDIA_OK; }
ABI_ATTR int AMediaExtractor_setDataSource(void*, const char*) { return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR int AMediaExtractor_setDataSourceFd(void*, int, off64_t, off64_t) { return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR int AMediaExtractor_setDataSourceCustom(void*, void*) { return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR size_t AMediaExtractor_getTrackCount(void*) { return 0; }
ABI_ATTR void* AMediaExtractor_getTrackFormat(void*, size_t) { return nullptr; }
ABI_ATTR int AMediaExtractor_selectTrack(void*, size_t) { return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR int AMediaExtractor_seekTo(void*, int64_t, int) { return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR bool AMediaExtractor_advance(void*) { return false; }
ABI_ATTR ssize_t AMediaExtractor_readSampleData(void*, uint8_t*, size_t) { return -1; }
ABI_ATTR int64_t AMediaExtractor_getSampleTime(void*) { return -1; }
ABI_ATTR int AMediaExtractor_getSampleTrackIndex(void*) { return -1; }

ABI_ATTR void* AMediaDataSource_new() { return &g_dataSource; }
ABI_ATTR void AMediaDataSource_delete(void*) {}
ABI_ATTR void AMediaDataSource_setUserdata(void*, void*) {}
ABI_ATTR void AMediaDataSource_setReadAt(void*, void*) {}
ABI_ATTR void AMediaDataSource_setGetSize(void*, void*) {}
ABI_ATTR void AMediaDataSource_setClose(void*, void*) {}

ABI_ATTR void* AMediaFormat_new() { return nullptr; }
ABI_ATTR int AMediaFormat_delete(void*) { return AMEDIA_OK; }
ABI_ATTR bool AMediaFormat_getInt32(void*, const char*, int32_t*) { return false; }
ABI_ATTR bool AMediaFormat_getInt64(void*, const char*, int64_t*) { return false; }
ABI_ATTR bool AMediaFormat_getFloat(void*, const char*, float*) { return false; }
ABI_ATTR bool AMediaFormat_getString(void*, const char*, const char**) { return false; }
ABI_ATTR bool AMediaFormat_getBuffer(void*, const char*, void**, size_t*) { return false; }
ABI_ATTR void AMediaFormat_setInt32(void*, const char*, int32_t) {}
ABI_ATTR void AMediaFormat_setInt64(void*, const char*, int64_t) {}
ABI_ATTR void AMediaFormat_setString(void*, const char*, const char*) {}

///// AMediaCodec: no decoder can be created
ABI_ATTR void* AMediaCodec_createDecoderByType(const char*) { once("AMediaCodec"); return nullptr; }
ABI_ATTR void* AMediaCodec_createCodecByName(const char*) { once("AMediaCodec"); return nullptr; }
ABI_ATTR int AMediaCodec_delete(void*) { return AMEDIA_OK; }
ABI_ATTR int AMediaCodec_configure(void*, void*, void*, void*, uint32_t) { return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR int AMediaCodec_start(void*) { return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR int AMediaCodec_stop(void*) { return AMEDIA_OK; }
ABI_ATTR int AMediaCodec_flush(void*) { return AMEDIA_OK; }
ABI_ATTR ssize_t AMediaCodec_dequeueInputBuffer(void*, int64_t) { return AMEDIACODEC_INFO_TRY_AGAIN_LATER; }
ABI_ATTR ssize_t AMediaCodec_dequeueOutputBuffer(void*, void*, int64_t) { return AMEDIACODEC_INFO_TRY_AGAIN_LATER; }
ABI_ATTR uint8_t* AMediaCodec_getInputBuffer(void*, size_t, size_t*) { return nullptr; }
ABI_ATTR uint8_t* AMediaCodec_getOutputBuffer(void*, size_t, size_t*) { return nullptr; }
ABI_ATTR void* AMediaCodec_getOutputFormat(void*) { return nullptr; }
ABI_ATTR int AMediaCodec_queueInputBuffer(void*, size_t, off_t, size_t, uint64_t, uint32_t) { return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR int AMediaCodec_releaseOutputBuffer(void*, size_t, bool) { return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR int AMediaCodec_setOutputSurface(void*, void*) { return AMEDIA_ERROR_UNSUPPORTED; }

///// AImageReader / AImage / AHardwareBuffer
ABI_ATTR int AImageReader_new(int32_t, int32_t, int32_t, int32_t, void** reader) { once("AImageReader"); if (reader) *reader = nullptr; return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR int AImageReader_newWithUsage(int32_t, int32_t, int32_t, uint64_t, int32_t, void** reader) { once("AImageReader"); if (reader) *reader = nullptr; return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR void AImageReader_delete(void*) {}
ABI_ATTR int AImageReader_getWindow(void*, void** window) { if (window) *window = nullptr; return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR int AImageReader_acquireLatestImage(void*, void** image) { if (image) *image = nullptr; return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR int AImageReader_acquireNextImage(void*, void** image) { if (image) *image = nullptr; return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR int AImageReader_setImageListener(void*, void*) { return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR int AImageReader_setBufferRemovedListener(void*, void*) { return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR void AImage_delete(void*) {}
ABI_ATTR void AImage_deleteAsync(void*, int) {}
ABI_ATTR int AImage_getWidth(void*, int32_t* w) { if (w) *w = 0; return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR int AImage_getHeight(void*, int32_t* h) { if (h) *h = 0; return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR int AImage_getTimestamp(void*, int64_t* t) { if (t) *t = 0; return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR int AImage_getHardwareBuffer(void*, void** b) { if (b) *b = nullptr; return AMEDIA_ERROR_UNSUPPORTED; }
ABI_ATTR void AHardwareBuffer_acquire(void*) {}
ABI_ATTR void AHardwareBuffer_release(void*) {}
ABI_ATTR void AHardwareBuffer_describe(const void*, void* desc) {}
ABI_ATTR void* ANativeWindow_toSurface(void*, void*) { return nullptr; }

///// AAudio: no streams can be opened (callers fall back, as on devices without AAudio)
ABI_ATTR int AAudio_createStreamBuilder(void** builder) { once("AAudio"); if (builder) *builder = nullptr; return AAUDIO_ERROR_UNAVAILABLE; }
ABI_ATTR const char* AAudio_convertResultToText(int r) { return r == 0 ? "AAUDIO_OK" : "AAUDIO_ERROR_UNAVAILABLE"; }
ABI_ATTR const char* AAudio_convertStreamStateToText(int) { return "AAUDIO_STREAM_STATE_DISCONNECTED"; }
ABI_ATTR int AAudioStreamBuilder_openStream(void*, void** stream) { if (stream) *stream = nullptr; return AAUDIO_ERROR_UNAVAILABLE; }
ABI_ATTR int AAudioStreamBuilder_delete(void*) { return 0; }
ABI_ATTR void AAudioStreamBuilder_setNoop(void*, int) {}
ABI_ATTR int AAudioStream_close(void*) { return 0; }
ABI_ATTR int AAudioStream_requestStart(void*) { return AAUDIO_ERROR_UNAVAILABLE; }
ABI_ATTR int AAudioStream_requestPause(void*) { return AAUDIO_ERROR_UNAVAILABLE; }
ABI_ATTR int AAudioStream_requestFlush(void*) { return AAUDIO_ERROR_UNAVAILABLE; }
ABI_ATTR int AAudioStream_requestStop(void*) { return AAUDIO_ERROR_UNAVAILABLE; }
ABI_ATTR int AAudioStream_getState(void*) { return AAUDIO_STREAM_STATE_DISCONNECTED; }
ABI_ATTR int AAudioStream_waitForStateChange(void*, int, int* next, int64_t) { if (next) *next = AAUDIO_STREAM_STATE_DISCONNECTED; return AAUDIO_ERROR_UNAVAILABLE; }
ABI_ATTR int AAudioStream_read(void*, void*, int32_t, int64_t) { return AAUDIO_ERROR_UNAVAILABLE; }
ABI_ATTR int AAudioStream_write(void*, const void*, int32_t, int64_t) { return AAUDIO_ERROR_UNAVAILABLE; }
ABI_ATTR int AAudioStream_getInt0(void*) { return 0; }
}

#define F(name) NO_THUNK(#name, (uintptr_t)&name)
#define D(name) NO_THUNK(#name, (uintptr_t)&name)
// The many AAudioStreamBuilder_set*/AAudioStream_get* functions all have one int-ish argument or none and their
// results are only read after a successful open, which never happens: share two no-op implementations.
#define SETTER(name) NO_THUNK(#name, (uintptr_t)&AAudioStreamBuilder_setNoop)
#define GETTER(name) NO_THUNK(#name, (uintptr_t)&AAudioStream_getInt0)

DynLibFunction symtable_media[] = {
    D(AMEDIAFORMAT_KEY_MIME), D(AMEDIAFORMAT_KEY_WIDTH), D(AMEDIAFORMAT_KEY_HEIGHT), D(AMEDIAFORMAT_KEY_DURATION),
    D(AMEDIAFORMAT_KEY_FRAME_RATE), D(AMEDIAFORMAT_KEY_SAMPLE_RATE), D(AMEDIAFORMAT_KEY_CHANNEL_COUNT),
    D(AMEDIAFORMAT_KEY_COLOR_FORMAT), D(AMEDIAFORMAT_KEY_STRIDE), D(AMEDIAFORMAT_KEY_SLICE_HEIGHT),
    D(AMEDIAFORMAT_KEY_LANGUAGE), D(AMEDIAFORMAT_KEY_ROTATION), D(AMEDIAFORMAT_KEY_COLOR_STANDARD),
    D(AMEDIAFORMAT_KEY_COLOR_RANGE), D(AMEDIAFORMAT_KEY_COLOR_TRANSFER), D(AMEDIAFORMAT_KEY_ENCODER_DELAY),
    D(AMEDIAFORMAT_KEY_AAC_MAX_OUTPUT_CHANNEL_COUNT), D(AMEDIAFORMAT_KEY_BIT_RATE), D(AMEDIAFORMAT_KEY_MAX_INPUT_SIZE),
    F(AMediaExtractor_new), F(AMediaExtractor_delete), F(AMediaExtractor_setDataSource), F(AMediaExtractor_setDataSourceFd),
    F(AMediaExtractor_setDataSourceCustom), F(AMediaExtractor_getTrackCount), F(AMediaExtractor_getTrackFormat),
    F(AMediaExtractor_selectTrack), F(AMediaExtractor_seekTo), F(AMediaExtractor_advance), F(AMediaExtractor_readSampleData),
    F(AMediaExtractor_getSampleTime), F(AMediaExtractor_getSampleTrackIndex),
    F(AMediaDataSource_new), F(AMediaDataSource_delete), F(AMediaDataSource_setUserdata), F(AMediaDataSource_setReadAt),
    F(AMediaDataSource_setGetSize), F(AMediaDataSource_setClose),
    F(AMediaFormat_new), F(AMediaFormat_delete), F(AMediaFormat_getInt32), F(AMediaFormat_getInt64), F(AMediaFormat_getFloat),
    F(AMediaFormat_getString), F(AMediaFormat_getBuffer), F(AMediaFormat_setInt32), F(AMediaFormat_setInt64), F(AMediaFormat_setString),
    F(AMediaCodec_createDecoderByType), F(AMediaCodec_createCodecByName), F(AMediaCodec_delete), F(AMediaCodec_configure),
    F(AMediaCodec_start), F(AMediaCodec_stop), F(AMediaCodec_flush), F(AMediaCodec_dequeueInputBuffer),
    F(AMediaCodec_dequeueOutputBuffer), F(AMediaCodec_getInputBuffer), F(AMediaCodec_getOutputBuffer),
    F(AMediaCodec_getOutputFormat), F(AMediaCodec_queueInputBuffer), F(AMediaCodec_releaseOutputBuffer),
    F(AMediaCodec_setOutputSurface),
    F(AImageReader_new), F(AImageReader_newWithUsage), F(AImageReader_delete), F(AImageReader_getWindow),
    F(AImageReader_acquireLatestImage), F(AImageReader_acquireNextImage), F(AImageReader_setImageListener),
    F(AImageReader_setBufferRemovedListener), F(AImage_delete), F(AImage_deleteAsync), F(AImage_getWidth), F(AImage_getHeight),
    F(AImage_getTimestamp), F(AImage_getHardwareBuffer), F(AHardwareBuffer_acquire), F(AHardwareBuffer_release),
    F(AHardwareBuffer_describe), F(ANativeWindow_toSurface),
    F(AAudio_createStreamBuilder), F(AAudio_convertResultToText), F(AAudio_convertStreamStateToText),
    F(AAudioStreamBuilder_openStream), F(AAudioStreamBuilder_delete),
    SETTER(AAudioStreamBuilder_setDeviceId), SETTER(AAudioStreamBuilder_setSampleRate), SETTER(AAudioStreamBuilder_setChannelCount),
    SETTER(AAudioStreamBuilder_setSamplesPerFrame), SETTER(AAudioStreamBuilder_setFormat), SETTER(AAudioStreamBuilder_setSharingMode),
    SETTER(AAudioStreamBuilder_setDirection), SETTER(AAudioStreamBuilder_setBufferCapacityInFrames),
    SETTER(AAudioStreamBuilder_setPerformanceMode), SETTER(AAudioStreamBuilder_setUsage), SETTER(AAudioStreamBuilder_setContentType),
    SETTER(AAudioStreamBuilder_setInputPreset), SETTER(AAudioStreamBuilder_setSessionId), SETTER(AAudioStreamBuilder_setFramesPerDataCallback),
    SETTER(AAudioStreamBuilder_setAllowedCapturePolicy), SETTER(AAudioStreamBuilder_setPrivacySensitive),
    NO_THUNK("AAudioStreamBuilder_setDataCallback", (uintptr_t)&AAudioStreamBuilder_setNoop),
    NO_THUNK("AAudioStreamBuilder_setErrorCallback", (uintptr_t)&AAudioStreamBuilder_setNoop),
    F(AAudioStream_close), F(AAudioStream_requestStart), F(AAudioStream_requestPause), F(AAudioStream_requestFlush),
    F(AAudioStream_requestStop), F(AAudioStream_getState), F(AAudioStream_waitForStateChange), F(AAudioStream_read), F(AAudioStream_write),
    GETTER(AAudioStream_getSampleRate), GETTER(AAudioStream_getChannelCount), GETTER(AAudioStream_getSamplesPerFrame),
    GETTER(AAudioStream_getFormat), GETTER(AAudioStream_getBufferSizeInFrames), GETTER(AAudioStream_getBufferCapacityInFrames),
    GETTER(AAudioStream_getFramesPerBurst), GETTER(AAudioStream_getFramesPerDataCallback), GETTER(AAudioStream_getXRunCount),
    GETTER(AAudioStream_getDeviceId), GETTER(AAudioStream_getSharingMode), GETTER(AAudioStream_getPerformanceMode),
    GETTER(AAudioStream_getDirection), GETTER(AAudioStream_setBufferSizeInFrames),
    { NULL, 0 }
};

#ifndef __LAYTON_H__
#define __LAYTON_H__

#include "baron/baron.h"
#include "android.h"

// libll1.so (Professor Layton and the Curious Village HD, Level-5's custom
// engine) does not use the standard Android API surface. It resolves a
// fixed set of ~20 custom native methods by name on its own MainActivity
// class (movie playback, PNG decode, save directory, license checks, the
// software keyboard) instead of AAssetManager/ANativeActivity. This mirrors
// the method list found in the Switch homebrew port's jni.c dispatch table
// (reference/layton_nx-main/source/jni.c) for the same binary.
namespace jnivm
{
    namespace com
    {
        namespace Level5
        {
            namespace LT1R
            {
                class MainActivity : public jnivm::android::app::Activity
                {
                public:
                    DEFINE_CLASS_NAME("com/Level5/LT1R/MainActivity")

                    // Movie (CRI ADX2 cutscene) playback
                    bool MO_PlayMovie(std::shared_ptr<FakeJni::JString> file);
                    bool MO_PlayMovieRegion(std::shared_ptr<FakeJni::JString> file, int offset, int size);
                    bool MO_GetState();
                    int MO_GetPosition();
                    void MO_PauseMovie(bool pause);
                    void MO_ReleaseMovie();
                    void MO_SetVolume(float volume);
                    std::shared_ptr<FakeJni::JFloatArray> MO_UpdateTexture();

                    // Software keyboard
                    bool UI_GetEditState();
                    std::shared_ptr<FakeJni::JString> UI_GetEditText();
                    void UI_StartEditText(std::shared_ptr<FakeJni::JString> initial, int editType);
                    void UI_SetIdleTimerDisabled(bool disabled);

                    // Save data / storage
                    std::shared_ptr<FakeJni::JString> CARD_GetFilesDirName();
                    std::shared_ptr<FakeJni::JString> CARD_GetExternalFilesDirName();
                    void CARD_CreateDirectory(std::shared_ptr<FakeJni::JString> dir);
                    long CARD_GetAvailableBytes();

                    // Level-5 ID sign-in flow: report done/declined so nothing blocks
                    bool L5iD_IsEndRequest();

                    // Licensing / DLC purchase state (2 == licensed, matches Vita/Switch ports)
                    int LVL_GetState();
                    int LSH_GetState();
                    int SBS_GetState();

                    // Misc
                    std::shared_ptr<FakeJni::JString> DL_GetFileName();
                    std::shared_ptr<FakeJni::JString> OS_GetAppVersion();

                    // static GL_LoadPNG(byte[]) -> int[]{ w, h, rgba... }
                    static std::shared_ptr<FakeJni::JIntArray> GL_LoadPNG(std::shared_ptr<FakeJni::JByteArray> data);
                };
            }
        }
    }
}
#endif

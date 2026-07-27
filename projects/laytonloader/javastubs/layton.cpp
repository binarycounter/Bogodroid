#include "baron/baron.h"
#include "layton.h"
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

using namespace jnivm::com::Level5::LT1R;

// ---------------------------------------------------------------------------
// Movie (CRI ADX2 cutscene) playback -- not implemented yet (Milestone 1).
// Reporting "no movie" lets the game skip cutscenes instead of blocking.
// ---------------------------------------------------------------------------

bool MainActivity::MO_PlayMovie(std::shared_ptr<FakeJni::JString> file)
{
    printf("MO_PlayMovie(%s) -- movie playback not implemented yet\n", file ? file->c_str() : "");
    return false;
}

bool MainActivity::MO_PlayMovieRegion(std::shared_ptr<FakeJni::JString> file, int offset, int size)
{
    printf("MO_PlayMovie(%s, %d, %d) -- movie playback not implemented yet\n", file ? file->c_str() : "", offset, size);
    return false;
}

bool MainActivity::MO_GetState()
{
    return false;
}

int MainActivity::MO_GetPosition()
{
    return 0;
}

void MainActivity::MO_PauseMovie(bool pause)
{
    (void)pause;
}

void MainActivity::MO_ReleaseMovie()
{
}

void MainActivity::MO_SetVolume(float volume)
{
    (void)volume;
}

std::shared_ptr<FakeJni::JFloatArray> MainActivity::MO_UpdateTexture()
{
    // SurfaceTexture transform matrix; standard Android flip matrix so any
    // consumer of this (before real movie playback exists) gets a sane UV
    // quad instead of an identity/garbage one.
    static const float surfaceMat[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
    };
    auto arr = std::make_shared<FakeJni::JFloatArray>(16);
    for (int i = 0; i < 16; i++)
        (*arr)[i] = surfaceMat[i];
    return arr;
}

// ---------------------------------------------------------------------------
// Software keyboard -- not implemented yet
// ---------------------------------------------------------------------------

bool MainActivity::UI_GetEditState()
{
    return false;
}

std::shared_ptr<FakeJni::JString> MainActivity::UI_GetEditText()
{
    return std::make_shared<FakeJni::JString>("");
}

void MainActivity::UI_StartEditText(std::shared_ptr<FakeJni::JString> initial, int editType)
{
    printf("UI_StartEditText(%s, %d) -- software keyboard not implemented yet\n", initial ? initial->c_str() : "", editType);
}

void MainActivity::UI_SetIdleTimerDisabled(bool disabled)
{
    (void)disabled;
}

// ---------------------------------------------------------------------------
// Save data / storage
// ---------------------------------------------------------------------------

std::shared_ptr<FakeJni::JString> MainActivity::CARD_GetFilesDirName()
{
    return std::make_shared<FakeJni::JString>(".");
}

std::shared_ptr<FakeJni::JString> MainActivity::CARD_GetExternalFilesDirName()
{
    return std::make_shared<FakeJni::JString>(".");
}

void MainActivity::CARD_CreateDirectory(std::shared_ptr<FakeJni::JString> dir)
{
    const std::string path = dir ? dir->asStdString() : "";
    printf("CARD_CreateDirectory(%s)\n", path.c_str());
    if (!path.empty())
        std::filesystem::create_directories(path);
}

long MainActivity::CARD_GetAvailableBytes()
{
    return 1ll << 30; // report 1 GB free, same as the Switch port
}

// ---------------------------------------------------------------------------
// Level-5 ID sign-in flow -- report everything done/declined so nothing blocks
// ---------------------------------------------------------------------------

bool MainActivity::L5iD_IsEndRequest()
{
    return true;
}

// ---------------------------------------------------------------------------
// Licensing / DLC purchase state (2 == licensed, matches Vita/Switch ports)
// ---------------------------------------------------------------------------

int MainActivity::LVL_GetState() { return 2; }
int MainActivity::LSH_GetState() { return 2; }
int MainActivity::SBS_GetState() { return 2; }

// ---------------------------------------------------------------------------
// Misc
// ---------------------------------------------------------------------------

std::shared_ptr<FakeJni::JString> MainActivity::DL_GetFileName()
{
    return std::make_shared<FakeJni::JString>("");
}

std::shared_ptr<FakeJni::JString> MainActivity::OS_GetAppVersion()
{
    return std::make_shared<FakeJni::JString>("1.0.8");
}

// static GL_LoadPNG(byte[]) -> int[]{ w, h, rgba... }
std::shared_ptr<FakeJni::JIntArray> MainActivity::GL_LoadPNG(std::shared_ptr<FakeJni::JByteArray> data)
{
    if (!data)
        return nullptr;

    int w = 0, h = 0;
    uint8_t *px = stbi_load_from_memory((const uint8_t *)data->getArray(), data->getSize(), &w, &h, NULL, 4);
    if (!px)
        return nullptr;

    // the engine uploads these as BGRA, so swap R/B (matches the Vita/Switch ports)
    for (int i = 0; i < w * h; i++) {
        uint8_t t = px[i * 4 + 0];
        px[i * 4 + 0] = px[i * 4 + 2];
        px[i * 4 + 2] = t;
    }

    auto result = std::make_shared<FakeJni::JIntArray>(w * h + 2);
    (*result)[0] = w;
    (*result)[1] = h;
    memcpy(&result->getArray()[2], px, (size_t)w * h * 4);
    stbi_image_free(px);
    return result;
}

BEGIN_NATIVE_DESCRIPTOR(MainActivity){FakeJni::Constructor<MainActivity>{}},
    {FakeJni::Function<&MainActivity::MO_PlayMovie>{}, "MO_PlayMovie", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::MO_PlayMovieRegion>{}, "MO_PlayMovie", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::MO_GetState>{}, "MO_GetState", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::MO_GetPosition>{}, "MO_GetPosition", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::MO_PauseMovie>{}, "MO_PauseMovie", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::MO_ReleaseMovie>{}, "MO_ReleaseMovie", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::MO_SetVolume>{}, "MO_SetVolume", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::MO_UpdateTexture>{}, "MO_UpdateTexture", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::UI_GetEditState>{}, "UI_GetEditState", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::UI_GetEditText>{}, "UI_GetEditText", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::UI_StartEditText>{}, "UI_StartEditText", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::UI_SetIdleTimerDisabled>{}, "UI_SetIdleTimerDisabled", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::CARD_GetFilesDirName>{}, "CARD_GetFilesDirName", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::CARD_GetExternalFilesDirName>{}, "CARD_GetExternalFilesDirName", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::CARD_CreateDirectory>{}, "CARD_CreateDirectory", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::CARD_GetAvailableBytes>{}, "CARD_GetAvailableBytes", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::L5iD_IsEndRequest>{}, "L5iD_IsEndRequest", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::LVL_GetState>{}, "LVL_GetState", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::LSH_GetState>{}, "LSH_GetState", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::SBS_GetState>{}, "SBS_GetState", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::DL_GetFileName>{}, "DL_GetFileName", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::OS_GetAppVersion>{}, "OS_GetAppVersion", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&MainActivity::GL_LoadPNG>{}, "GL_LoadPNG", FakeJni::JMethodID::STATIC},
END_NATIVE_DESCRIPTOR

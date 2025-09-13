#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H
#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Use standard 64-bit types if off64_t isn't available
#if !defined(_OFF64_T) && !defined(_OFF64_T_DECLARED)
typedef int64_t off64_t;
#endif

typedef enum {
    AASSET_MODE_UNKNOWN = 0,
    AASSET_MODE_RANDOM = 1,
    AASSET_MODE_STREAMING = 2,
    AASSET_MODE_BUFFER = 3
} AAssetMode;

typedef struct AAssetManager AAssetManager;
typedef struct AAsset AAsset;
typedef struct AAssetDir AAssetDir;

// AAssetManager functions
AAssetManager* AAssetManager_create(const char* path);
void AAssetManager_destroy(AAssetManager* mgr);

// AAsset functions
AAsset* AAssetManager_open(AAssetManager* mgr, const char* filename, int mode);
void AAsset_close(AAsset* asset);
const void* AAsset_getBuffer(AAsset* asset);
off_t AAsset_getLength(AAsset* asset);
off64_t AAsset_getLength64(AAsset* asset);
int AAsset_read(AAsset* asset, void* buf, size_t count);
off_t AAsset_seek(AAsset* asset, off_t offset, int whence);
off64_t AAsset_seek64(AAsset* asset, off64_t offset, int whence);

// AAssetDir functions
AAssetDir* AAssetManager_openDir(AAssetManager* mgr, const char* dirName);
void AAssetDir_close(AAssetDir* assetDir);
const char* AAssetDir_getNextFileName(AAssetDir* assetDir);
void AAssetDir_rewind(AAssetDir* assetDir);

#ifdef __cplusplus
}
#endif

#endif // ASSET_MANAGER_H
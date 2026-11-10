#include "asset_manager.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "logging.h"
#include "apk.h"

// Ensure large file support
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

struct AAssetManager {
    char* assets_path;
};

struct AAsset {
    int fd;
    FILE* file;
    off64_t length;
    off64_t pos;
    void* buffer;
    AAssetMode mode;
};

struct AAssetDir {
    DIR* dir;
    char* path;
    struct dirent** files;
    char** apkNames; // listing from the APK when the folder isn't extracted
    int count;
    int current;
};

// Helper function to get full path
static char* get_full_path(AAssetManager* mgr, const char* path) {
    if(mgr==NULL)
    {
        //Not sure why this happens but lets hack around it.
        char* full = malloc(strlen("assets") + strlen(path) + 2);
    sprintf(full, "%s/%s", "assets", path);
    return full;
    }
    char* full = malloc(strlen(mgr->assets_path) + strlen(path) + 2);
    sprintf(full, "%s/%s", mgr->assets_path, path);
    return full;
}

AAssetManager* AAssetManager_create(const char* path) {
    AAssetManager* mgr = malloc(sizeof(AAssetManager));
    mgr->assets_path = strdup(path);
    return mgr;
}

void AAssetManager_destroy(AAssetManager* mgr) {
    free(mgr->assets_path);
    free(mgr);
}

AAssetManager* AAssetManager_fromJava(void* env, void* obj) {
    return AAssetManager_create("assets");
}


AAsset* AAssetManager_open(AAssetManager* mgr, const char* filename, int mode) {
    // Asset names are relative to the APK's assets/ folder; tolerate callers that include it or a leading slash.
    const char* name = filename;
    while (*name == '/') name++;
    if (strncmp(name, "assets/", 7) == 0) name += 7;
    char* full_path = get_full_path(mgr, name);
    struct stat st;

    if(stat(full_path, &st) < 0) {
        // Not extracted: read it from the APK when one is configured (full_path is "assets/<name>").
        void* data = NULL;
        size_t size = 0;
        if (bogo_apk_read(full_path, &data, &size) == 0) {
            bogo_log("[ASSET] AAssetManager_open(%s) -> APK entry %s\n", filename, full_path);
            AAsset* asset = malloc(sizeof(AAsset));
            memset(asset, 0, sizeof(AAsset));
            asset->fd = -1;
            asset->mode = mode;
            asset->length = size;
            asset->buffer = data;
            free(full_path);
            return asset;
        }
        printf("[ASSET] AAssetManager_open(%s) -> not found (%s)\n", filename, full_path);
        free(full_path);
        return NULL;
    }
    bogo_log("[ASSET] AAssetManager_open(%s) -> %s\n", filename, full_path);

    AAsset* asset = malloc(sizeof(AAsset));
    memset(asset, 0, sizeof(AAsset));
    asset->fd = -1; // buffer mode never opens one; 0 would make AAsset_close close stdin

    asset->mode = mode;
    asset->length = st.st_size;
    asset->pos = 0;

    if(mode == AASSET_MODE_BUFFER) {
        // Read entire file into buffer
        FILE* f = fopen(full_path, "rb");
        if(f) {
            asset->buffer = malloc(asset->length);
            (void)! fread(asset->buffer, 1, asset->length, f);
            fclose(f);
        }
    }
    else {
        // Open file descriptor
        asset->fd = open(full_path, O_RDONLY);
        if(mode == AASSET_MODE_STREAMING) {
            asset->file = fdopen(asset->fd, "rb");
        }
    }

    free(full_path);
    return asset;
}

void AAsset_close(AAsset* asset) {
    if(asset->buffer) free(asset->buffer);
    if(asset->file) fclose(asset->file);
    else if(asset->fd >= 0) close(asset->fd);
    free(asset);
}

const void* AAsset_getBuffer(AAsset* asset) {
    return asset->buffer;
}

off_t AAsset_getLength(AAsset* asset) {
    return (off_t)asset->length;
}

off64_t AAsset_getLength64(AAsset* asset) {
    return asset->length;
}

int AAsset_read(AAsset* asset, void* buf, size_t count) {
    if(asset->buffer) {
        size_t remain = asset->length - asset->pos;
        if(remain <= 0) return 0;
        if(count > remain) count = remain;
        memcpy(buf, (char*)asset->buffer + asset->pos, count);
        asset->pos += count;
        return count;
    }
    else if(asset->file) {
        size_t read = fread(buf, 1, count, asset->file);
        asset->pos += read;
        return read;
    }
    else if(asset->fd >= 0) {
        ssize_t read = pread(asset->fd, buf, count, asset->pos);
        if(read > 0) asset->pos += read;
        return read;
    }
    return -1;
}

off_t AAsset_seek(AAsset* asset, off_t offset, int whence) {
    return (off_t)AAsset_seek64(asset, (off64_t)offset, whence);
}

off64_t AAsset_seek64(AAsset* asset, off64_t offset, int whence) {
    switch(whence) {
        case SEEK_SET:
            asset->pos = offset;
            break;
        case SEEK_CUR:
            asset->pos += offset;
            break;
        case SEEK_END:
            asset->pos = asset->length + offset;
            break;
    }
    
    if(asset->pos < 0) asset->pos = 0;
    if(asset->pos > asset->length) asset->pos = asset->length;
    
    if(asset->file) fseek(asset->file, (long)asset->pos, SEEK_SET);
    return asset->pos;
}

AAssetDir* AAssetManager_openDir(AAssetManager* mgr, const char* dirName) {
    char* full_path = get_full_path(mgr, dirName);
    
    AAssetDir* dir = malloc(sizeof(AAssetDir));
    dir->dir = opendir(full_path);
    dir->path = full_path;
    dir->files = NULL;
    dir->apkNames = NULL;
    dir->count = 0;
    dir->current = 0;

    if(!dir->dir) {
        dir->apkNames = bogo_apk_list(full_path, &dir->count);
        return dir;
    }

    if(dir->dir) {
        struct dirent* entry;
        while((entry = readdir(dir->dir)) != NULL) {
            if(entry->d_type == DT_REG) dir->count++;
        }
        rewinddir(dir->dir);
        
        dir->files = malloc(sizeof(struct dirent*) * dir->count);
        int i = 0;
        while((entry = readdir(dir->dir)) != NULL) {
            if(entry->d_type == DT_REG) {
                dir->files[i++] = entry;
            }
        }
    }
    
    return dir;
}

void AAssetDir_close(AAssetDir* assetDir) {
    if(assetDir->apkNames) {
        for(int i = 0; i < assetDir->count; i++) free(assetDir->apkNames[i]);
        free(assetDir->apkNames);
    }
    if(assetDir->dir) closedir(assetDir->dir);
    free(assetDir->path);
    free(assetDir->files);
    free(assetDir);
}

const char* AAssetDir_getNextFileName(AAssetDir* assetDir) {
    if(assetDir->current >= assetDir->count) return NULL;
    if(assetDir->apkNames) return assetDir->apkNames[assetDir->current++];
    return assetDir->files[assetDir->current++]->d_name;
}

void AAssetDir_rewind(AAssetDir* assetDir) {
    assetDir->current = 0;
}
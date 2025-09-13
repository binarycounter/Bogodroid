#include "asset_manager.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

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

AAsset* AAssetManager_open(AAssetManager* mgr, const char* filename, int mode) {
    char* full_path = get_full_path(mgr, filename);
    struct stat st;
    
    if(stat(full_path, &st) < 0) {
        free(full_path);
        return NULL;
    }

    AAsset* asset = malloc(sizeof(AAsset));
    memset(asset, 0, sizeof(AAsset));
    
    asset->mode = mode;
    asset->length = st.st_size;
    asset->pos = 0;

    if(mode == AASSET_MODE_BUFFER) {
        // Read entire file into buffer
        FILE* f = fopen(full_path, "rb");
        if(f) {
            asset->buffer = malloc(asset->length);
            fread(asset->buffer, 1, asset->length, f);
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
    dir->count = 0;
    dir->current = 0;

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
    if(assetDir->dir) closedir(assetDir->dir);
    free(assetDir->path);
    free(assetDir->files);
    free(assetDir);
}

const char* AAssetDir_getNextFileName(AAssetDir* assetDir) {
    if(assetDir->current >= assetDir->count) return NULL;
    return assetDir->files[assetDir->current++]->d_name;
}

void AAssetDir_rewind(AAssetDir* assetDir) {
    assetDir->current = 0;
}
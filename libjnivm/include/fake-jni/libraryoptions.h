#pragma once

namespace FakeJni {
    struct LibraryOptions {
        void*(*dlopen)(const char*, int);
        void*(*dlsym)(void *handle, const char*);
        int(*dlclose)(void*);
        LibraryOptions(void*(*dlopen)(const char*, int), void*(*dlsym)(void *handle, const char*), int(*dlclose)(void*));
        LibraryOptions();
    };
}
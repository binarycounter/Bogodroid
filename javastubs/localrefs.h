#pragma once
// Scoped release of JNI local references.
//
// Every JNI call on a thread keeps the objects it creates alive in that thread's local frame, and PushLocalFrame /
// PopLocalFrame are no-ops here, so nothing ever frees them: a thread that keeps calling into the game grows
// without bound. The Choreographer thread, which calls Unity hundreds of times a second, ran a handheld out of
// memory after about twenty minutes (std::bad_alloc inside the frame's vector).
//
// A LocalRefScope drops what was added while it was alive. Use it only around a call whose results are not used
// afterwards (a void callback into the game, an input event): anything the game wants to keep it must have copied
// into a global reference, which is held elsewhere and is not affected.
#include <jnivm/env.h>
#include <cstddef>

struct LocalRefScope {
#ifdef EnableJNIVMGC
    explicit LocalRefScope(JNIEnv& env)
        : frame(&jnivm::ENV::FromJNIEnv(&env)->localframe.front())
        , mark(frame->size())
    {
    }
    ~LocalRefScope()
    {
        if (frame->size() > mark)
            frame->resize(mark);
    }
    std::vector<std::shared_ptr<jnivm::Object>>* frame;
    size_t mark;
#else
    explicit LocalRefScope(JNIEnv&) { }
#endif
    LocalRefScope(const LocalRefScope&) = delete;
    LocalRefScope& operator=(const LocalRefScope&) = delete;
};

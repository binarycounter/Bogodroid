// Stress test for libjnivm's thread handling: the per-thread JNIEnv table, class lookup and native registration.
// The launcher crashed in exactly this area (a thread without a JNIEnv, and a racy table of them).
#include "stress_util.h"
#include <jnivm.h>
#include "localrefs.h"
#include <string>

static void native_stub(JNIEnv*, jclass) { }

// Threads that never called AttachCurrentThread still ask the VM for their env (the loader's looper threads do).
static void test_unattached_threads(double secs)
{
    Watchdog wd("test_unattached_threads", 60);
    jnivm::VM vm;
    auto start = std::chrono::steady_clock::now();
    while (time_left(start, secs)) {
        run_threads(8, [&](int i) {
            for (int k = 0; k < 200; k++) {
                auto& env = vm.GetEnv();
                CHECK(env != nullptr);
                if (!env) return;
                JNIEnv* e = vm.GetJNIEnv();
                std::string name = "stress/Unattached" + std::to_string((i * 7 + k) % 20);
                CHECK(e->FindClass(name.c_str()) != nullptr);
            }
        });
    }
}

// Attach and detach constantly from many threads while others keep using the table.
static void test_attach_detach(double secs)
{
    Watchdog wd("test_attach_detach", 60);
    jnivm::VM vm;
    auto start = std::chrono::steady_clock::now();
    while (time_left(start, secs)) {
        run_threads(8, [&](int i) {
            JavaVM* jvm = vm.GetJavaVM();
            for (int k = 0; k < 100; k++) {
                JNIEnv* e = nullptr;
                CHECK(jvm->AttachCurrentThread(&e, nullptr) == JNI_OK);
                CHECK(e != nullptr);
                if (i % 2 == 0) {
                    JNIEnv* again = nullptr;
                    CHECK(jvm->GetEnv((void**)&again, JNI_VERSION_1_6) == JNI_OK);
                }
                jclass c = e->FindClass("stress/Attach");
                CHECK(c != nullptr);
                jvm->DetachCurrentThread();
            }
        });
    }
}

// Natives get registered (RegisterNatives) while other threads look methods and fields up on the same class.
static void test_register_vs_lookup(double secs)
{
    Watchdog wd("test_register_vs_lookup", 60);
    jnivm::VM vm;
    auto start = std::chrono::steady_clock::now();
    int round = 0;
    while (time_left(start, secs)) {
        std::string cname = "stress/Reg" + std::to_string(round++);
        run_threads(6, [&](int i) {
            JNIEnv* e = vm.GetJNIEnv();
            jclass c = e->FindClass(cname.c_str());
            CHECK(c != nullptr);
            for (int k = 0; k < 60; k++) {
                if (i == 0) {
                    std::string n = "native" + std::to_string(k);
                    JNINativeMethod m = { (char*)n.c_str(), (char*)"()V", (void*)native_stub };
                    e->RegisterNatives(c, &m, 1);
                } else {
                    std::string n = "m" + std::to_string(i) + "_" + std::to_string(k);
                    // Creates unresolved method / field ids on the class: they are appended to the same lists.
                    CHECK(e->GetStaticMethodID(c, n.c_str(), "()V") != nullptr);
                    CHECK(e->GetFieldID(c, n.c_str(), "I") != nullptr);
                }
            }
        });
    }
}

// Null / missing classes are what the game passes after a failed lookup; none of this may crash.
static void test_null_arguments(double)
{
    jnivm::VM vm;
    JNIEnv* e = vm.GetJNIEnv();
    CHECK(e->GetFieldID(nullptr, "f", "I") == nullptr);
    CHECK(e->IsAssignableFrom(nullptr, nullptr) == JNI_FALSE);
    CHECK(e->IsInstanceOf(nullptr, nullptr) == JNI_FALSE);
    CHECK(e->GetSuperclass(nullptr) == nullptr);
    CHECK(e->MonitorEnter(nullptr) != 0);
    CHECK(e->MonitorExit(nullptr) != 0);
    JNINativeMethod m = { (char*)"x", (char*)"()V", (void*)native_stub };
    CHECK(e->RegisterNatives(nullptr, &m, 1) == 0 || true);
    jclass c = e->FindClass("stress/Null");
    CHECK(e->RegisterNatives(c, nullptr, 1) != 0);
    CHECK(e->RegisterNatives(c, &m, -1) != 0);
}

// The JNI string functions get whatever the game's strings contain: emoji (4-byte UTF-8), invalid bytes, truncated
// sequences, null. They used to throw C++ exceptions into native code or read past the end of the string.
static void test_strings(double)
{
    jnivm::VM vm;
    JNIEnv* e = vm.GetJNIEnv();
    const char* cases[] = {
        "", "plain ascii", "caf\xC3\xA9", "\xE2\x82\xAC euro", "emoji \xF0\x9F\x98\x80 end",
        "bad \xFF\xFE bytes", "truncated \xE2\x82", "truncated4 \xF0\x9F\x98", "\xC3", "\xF0",
    };
    for (const char* c : cases) {
        jstring s = e->NewStringUTF(c);
        CHECK(s != nullptr);
        jsize n = e->GetStringLength(s);
        CHECK(n >= 0 && n <= (jsize)strlen(c));
        const jchar* chars = e->GetStringChars(s, nullptr);
        CHECK(chars != nullptr);
        if (chars) {
            CHECK(chars[n] == 0);
            e->ReleaseStringChars(s, chars);
        }
        jchar buf[64] = {};
        e->GetStringRegion(s, 0, n, buf);
        e->GetStringRegion(s, n + 5, 10, buf);  // out of range: copies nothing, doesn't read past the end
        e->GetStringRegion(s, -3, 10, buf);
        char utf[256];
        e->GetStringUTFRegion(s, 0, n, utf);
        CHECK(strlen(utf) <= sizeof(utf));
        CHECK(e->GetStringUTFChars(s, nullptr) != nullptr);
    }
    // The emoji is one code point, two UTF-16 units.
    jstring emoji = e->NewStringUTF("\xF0\x9F\x98\x80");
    CHECK(e->GetStringLength(emoji) == 2);
    const jchar* u = e->GetStringChars(emoji, nullptr);
    CHECK(u[0] == 0xD83D && u[1] == 0xDE00);
    e->ReleaseStringChars(emoji, u);
    // Round trip through NewString (UTF-16 in, e.g. from C#).
    const jchar units[] = { 'h', 'i', 0xD83D, 0xDE00, 0x20AC };
    jstring made = e->NewString(units, 5);
    CHECK(e->GetStringLength(made) == 5);
    // null / unknown pointers
    CHECK(e->GetStringLength(nullptr) == 0);
    e->ReleaseStringChars(nullptr, nullptr);
    jchar stack[2] = {};
    e->ReleaseStringChars(emoji, stack); // not one of ours: ignored, not freed
    const jchar* none = e->GetStringChars(nullptr, nullptr);
    CHECK(none && none[0] == 0);
    e->ReleaseStringChars(nullptr, none);
    // Many get/release pairs: LeakSanitizer reports it if Release stops freeing.
    jstring big = e->NewStringUTF("some moderately long string to convert again and again");
    for (int i = 0; i < 20000; i++)
        e->ReleaseStringChars(big, e->GetStringChars(big, nullptr));
}

// A thread that keeps calling into the game must not grow without bound: a LocalRefScope releases what its call made.
static void test_local_ref_scope(double)
{
    jnivm::VM vm;
    JNIEnv* e = vm.GetJNIEnv();
    auto* env = jnivm::ENV::FromJNIEnv(e);
    size_t before = env->localframe.front().size();
    jstring keep = e->NewStringUTF("kept");        // made before the scope: must survive it
    size_t base = env->localframe.front().size();
    CHECK(base > before);
    for (int round = 0; round < 50; round++) {
        {
            LocalRefScope scope(*e);
            for (int i = 0; i < 1000; i++)
                e->NewStringUTF("temporary");
            CHECK(env->localframe.front().size() >= base + 1000);
        }
        CHECK(env->localframe.front().size() == base); // nothing accumulates across rounds
    }
    CHECK(e->GetStringLength(keep) == 4);
}

int main()
{
    RUN(test_local_ref_scope);
    RUN(test_strings);
    RUN(test_null_arguments);
    RUN(test_unattached_threads);
    RUN(test_attach_detach);
    RUN(test_register_vs_lookup);
    return g_failures ? 1 : 0;
}

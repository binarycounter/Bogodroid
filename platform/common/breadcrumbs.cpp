#include "breadcrumbs.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>

namespace {
constexpr unsigned kEntries = 128; // power of two
// Every field is a relaxed atomic: writers on different threads can land on the same slot after the ring wraps, and
// the crash handler reads while others still write. A torn entry is then merely skipped (see seq), never undefined.
constexpr unsigned kDetailWords = 5; // 40 bytes
struct Entry {
    std::atomic<uint32_t> seq; // 0 = never written / being written; stored last
    std::atomic<int64_t> ms;
    std::atomic<int32_t> tid;
    std::atomic<int64_t> value;
    std::atomic<const char*> what;
    std::atomic<uint64_t> detail[kDetailWords];
};
Entry g_ring[kEntries];
std::atomic<uint32_t> g_next{1};

int64_t now_ms()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Async-signal-safe number formatting.
size_t put_num(char* out, int64_t v)
{
    char tmp[24];
    size_t n = 0;
    bool neg = v < 0;
    uint64_t u = neg ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;
    do {
        tmp[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    size_t o = 0;
    if (neg) out[o++] = '-';
    while (n) out[o++] = tmp[--n];
    return o;
}
}

void bogo_breadcrumb(const char* what, const char* detail, long value)
{
    uint32_t seq = g_next.fetch_add(1, std::memory_order_relaxed);
    Entry& e = g_ring[seq & (kEntries - 1)];
    char buf[kDetailWords * 8] = {};
    if (detail)
        strncpy(buf, detail, sizeof(buf) - 1); // keeps a terminator
    e.seq.store(0, std::memory_order_relaxed);
    e.ms.store(now_ms(), std::memory_order_relaxed);
    e.tid.store((int32_t)syscall(SYS_gettid), std::memory_order_relaxed);
    e.value.store(value, std::memory_order_relaxed);
    e.what.store(what, std::memory_order_relaxed);
    for (unsigned i = 0; i < kDetailWords; i++) {
        uint64_t w;
        memcpy(&w, buf + i * 8, 8);
        e.detail[i].store(w, std::memory_order_relaxed);
    }
    e.seq.store(seq, std::memory_order_release);
}

void bogo_breadcrumbs_dump(int fd)
{
    uint32_t last = g_next.load(std::memory_order_relaxed);
    uint32_t first = last > kEntries ? last - kEntries : 1;
    static const char header[] = "recent loader activity (oldest first; time ms, thread id, event, detail, value):\n";
    (void)!write(fd, header, sizeof(header) - 1);
    for (uint32_t s = first; s < last; s++) {
        const Entry& e = g_ring[s & (kEntries - 1)];
        if (e.seq.load(std::memory_order_acquire) != s)
            continue; // overwritten meanwhile, or still being written
        int64_t ms = e.ms.load(std::memory_order_relaxed);
        int32_t tid = e.tid.load(std::memory_order_relaxed);
        int64_t value = e.value.load(std::memory_order_relaxed);
        const char* what = e.what.load(std::memory_order_relaxed);
        char detail[kDetailWords * 8 + 1];
        for (unsigned i = 0; i < kDetailWords; i++) {
            uint64_t w = e.detail[i].load(std::memory_order_relaxed);
            memcpy(detail + i * 8, &w, 8);
        }
        detail[sizeof(detail) - 1] = 0;
        if (e.seq.load(std::memory_order_acquire) != s)
            continue; // changed while we read it
        char line[160];
        size_t o = 0;
        line[o++] = ' ';
        line[o++] = ' ';
        o += put_num(line + o, ms);
        line[o++] = ' ';
        o += put_num(line + o, tid);
        line[o++] = ' ';
        const char* w = what ? what : "?";
        size_t wl = strnlen(w, 48);
        memcpy(line + o, w, wl);
        o += wl;
        if (detail[0]) {
            line[o++] = ' ';
            size_t dl = strnlen(detail, sizeof(detail));
            memcpy(line + o, detail, dl);
            o += dl;
        }
        if (value) {
            line[o++] = ' ';
            o += put_num(line + o, value);
        }
        line[o++] = '\n';
        (void)!write(fd, line, o);
    }
}

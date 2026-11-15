#pragma once
// A tiny in-memory trail of "what was the loader doing" events, printed by the crash handler.
// A crash report with only native frames says where it died; the breadcrumbs say what led up to it (which JNI
// callback, which library load, which thread had just started or exited).
//
// Cheap enough for per-frame use: a few stores into a fixed ring, no locks, no allocation. `what` should be a string
// literal (it is stored by pointer); `detail` is copied (truncated).

void bogo_breadcrumb(const char* what, const char* detail = nullptr, long value = 0);

// Writes the trail, oldest first, to a file descriptor. Only uses async-signal-safe calls: it runs inside the
// crash handler.
void bogo_breadcrumbs_dump(int fd);

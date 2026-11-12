#!/usr/bin/env python3
"""Turn a (debug) unityloader log into a to-do list of what the loader still has to implement.

Usage:  python3 tools/unity_probe_report.py run.txt [more logs...] > report.md

Works best with a Debug build of unityloader (it traces unresolved JNI calls and stubbed libc functions) run with
BOGODROID_VERBOSE=1, on a build of tools/unity-probe/ProbeRunner.cs. Any other game's log works too.
Binary garbage in the log is ignored.
"""
import re
import sys
from collections import Counter, OrderedDict

# --- aarch64 syscall numbers that show up as "Unimplemented syscall: N" (only the common ones) ---
SYSCALLS = {
    5: "setxattr", 17: "getcwd", 29: "ioctl", 35: "unlinkat", 56: "openat", 62: "lseek", 63: "read", 64: "write",
    73: "ppoll", 78: "readlinkat", 79: "newfstatat", 94: "exit_group", 98: "futex", 99: "set_robust_list",
    101: "nanosleep", 113: "clock_gettime", 118: "sched_setparam", 119: "sched_setscheduler", 120: "sched_getscheduler",
    122: "sched_setaffinity", 123: "sched_getaffinity", 124: "sched_yield", 129: "kill", 131: "tgkill",
    134: "rt_sigaction", 135: "rt_sigprocmask", 167: "prctl", 172: "getpid", 178: "gettid", 220: "clone",
    222: "mmap", 226: "mprotect", 233: "madvise", 261: "prlimit64", 278: "getrandom",
}

RES = {
    "unresolved": re.compile(r"Unresolved symbol, Class=`([^`]*)`, (Static|Native)?(Method|Field)=`([^`]*)`, Signature=`([^`]*)`"),
    "called_unknown": re.compile(r"Call Unknown (?:Member|Static) Function Class=`([^`]*)` Method=`([^`]*)` Signature=`([^`]*)`"),
    "field_unknown": re.compile(r"Invoked Unknown Field (Getter|Setter) Class=`([^`]*)` Field=`([^`]*)`"),
    "missing_sym": re.compile(r"^Missing: (\S+) \("),
    "stubbed": re.compile(r'Warning, stubbed function "([^"]+)"'),
    "syscall": re.compile(r"Unimplemented syscall: (-?\d+)"),
    "java_fail": re.compile(r"Failed to create java object for (\S+)"),
    "no_native": re.compile(r"Could not find native method (\S+)"),
    "feature": re.compile(r"App asks about availability of feature: (\S+)"),
    "service": re.compile(r"App requesting unknown system service (\S+)"),
    "secure_key": re.compile(r"Secure\.getString\(\) called with unknown key: (\S+)"),
    "sysprop": re.compile(r"\[SYSPROP\] __system_property_(?:find|get)\(([^)]*)\)", re.I),
    "dlopen": re.compile(r"Guest called dlopen for (\S+)"),
    "dlsym_nil": re.compile(r"dlsym\((?:0x[0-9a-f]+|\(nil\)), ([^)]+)\) = 0x\(nil\)"),
    "probe": re.compile(r"\[PROBE\] (OK|FAIL) (.+)"),
    "unity_problem": re.compile(r"LOG\[Unity\]: (.*(?:Failed|failed|Error|error|Exception|not supported|Couldn't|Could not|Unable).*)"),
    "crash": re.compile(r"LOG\[CRASH\]: (signal \d+.*|Cause: .*)|^Error: signal \d+"),
}

# Things the loader provides on purpose / that are loader-internal probing, not game requests.
IGNORE_MISSING = re.compile(r"^_ZTH")
IGNORE_UNRESOLVED_CLASS = re.compile(r"^com/unity3d/player/UnityPlayer(ForActivityOrService)?$")


def read(path):
    with open(path, "rb") as f:
        return f.read().decode("utf-8", "replace").splitlines()


def main(paths):
    if not paths:
        print(__doc__)
        return 1
    lines = []
    for p in paths:
        lines += read(p)

    unresolved = OrderedDict()   # (class, kind, name, sig) -> count of 'Call Unknown'
    missing = Counter()
    stubbed = Counter()
    syscalls = Counter()
    java_fail, no_native = Counter(), Counter()
    features, services, secure = Counter(), Counter(), Counter()
    sysprops, dlopens, dlsym_nil = Counter(), Counter(), Counter()
    probes, unity_problems, crashes = [], Counter(), []
    called = Counter()

    for line in lines:
        m = RES["unresolved"].search(line)
        if m and not IGNORE_UNRESOLVED_CLASS.match(m.group(1)) and m.group(2) != "Native":
            unresolved.setdefault((m.group(1), (m.group(2) or "") + m.group(3), m.group(4), m.group(5)), 0)
        m = RES["called_unknown"].search(line)
        if m:
            called[(m.group(1), m.group(2), m.group(3))] += 1
        m = RES["field_unknown"].search(line)
        if m:
            unresolved.setdefault((m.group(2), "Field " + m.group(1).lower(), m.group(3), ""), 0)
        m = RES["missing_sym"].search(line)
        if m and not IGNORE_MISSING.match(m.group(1)):
            missing[m.group(1)] += 1
        for key, counter in (("stubbed", stubbed), ("java_fail", java_fail), ("no_native", no_native), ("feature", features),
                             ("service", services), ("secure_key", secure), ("sysprop", sysprops), ("dlopen", dlopens),
                             ("dlsym_nil", dlsym_nil)):
            m = RES[key].search(line)
            if m:
                counter[m.group(1)] += 1
        m = RES["syscall"].search(line)
        if m:
            syscalls[int(m.group(1))] += 1
        m = RES["probe"].search(line)
        if m and "BEGIN" not in line:
            probes.append((m.group(1), m.group(2).strip()))
        m = RES["unity_problem"].search(line)
        if m and "[PROBE]" not in line:
            unity_problems[m.group(1).strip()[:200]] += 1
        m = RES["crash"].search(line)
        if m and len(crashes) < 12:
            crashes.append(line.strip()[:200])

    out = []
    w = out.append
    w("# Bogodroid loader report\n")
    w("Generated from %d log line(s). Counts are how often the game asked; higher = more likely to matter.\n" % len(lines))

    if crashes:
        w("## Crashes\n")
        for c in crashes:
            w("- `%s`" % c)
        w("")

    if probes:
        fails = [p for p in probes if p[0] == "FAIL"]
        w("## Probe results (%d ok, %d failed)\n" % (len(probes) - len(fails), len(fails)))
        for result, text in probes:
            w("- **%s** %s" % (result, text[:260]))
        w("")

    # Java API the game asked for that has no implementation. Merge the 'called' counts in.
    rows = []
    for (cls, kind, name, sig), _ in unresolved.items():
        n = called.get((cls, name, sig), 0)
        rows.append((n, cls, kind, name, sig))
    rows.sort(key=lambda r: (-r[0], r[1], r[3]))
    w("## Java methods/fields with no implementation (%d)\n" % len(rows))
    w("Add these to javastubs/ (or the loader's Java classes). `calls` = times the game actually called it.\n")
    w("| calls | class | member | signature |")
    w("|---:|---|---|---|")
    for n, cls, kind, name, sig in rows:
        w("| %d | `%s` | %s `%s` | `%s` |" % (n, cls, kind, name, sig))
    w("")

    def section(title, counter, fmt=lambda k: "`%s`" % k, hint=""):
        if not counter:
            return
        w("## %s (%d)\n" % (title, len(counter)))
        if hint:
            w(hint + "\n")
        for k, n in counter.most_common():
            w("- %s  x%d" % (fmt(k), n))
        w("")

    section("Java classes Unity could not create", java_fail, hint="Needs a stub class (see UnityPlayerUtilities in projects/unityloader/javastubs/unity.*).")
    section("Native methods Unity expected but didn't register", no_native)
    section("Missing native symbols (libc/libm/NDK/EGL/GLES)", missing, hint="Add to thunks/ (see impl_tab.h / ndk.cpp). Mostly only matters if the code path is actually hit.")
    section("libc functions that are only stubbed", stubbed, hint="Seen as 'Warning, stubbed function' in Debug builds.")
    section("Unimplemented syscalls", syscalls, fmt=lambda k: "%d (%s)" % (k, SYSCALLS.get(k, "?")))
    section("Android system features the game asked about", features, hint="Answer via [device] features in the config, or in PackageManager.hasSystemFeature.")
    section("Android system services requested but not provided", services)
    section("Settings.Secure keys requested", secure)
    section("System properties read", sysprops, hint="All currently answered with 'not found'.")
    section("Libraries the game dlopen'ed", dlopens)
    section("dlsym lookups that failed", dlsym_nil)
    section("Problems Unity reported itself", unity_problems)

    print("\n".join(out))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

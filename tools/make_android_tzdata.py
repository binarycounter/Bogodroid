#!/usr/bin/env python3
"""Build an Android-format 'tzdata' file from the host's /usr/share/zoneinfo.

Unity (Mono/IL2CPP) reads time zones on Android from a single packed file instead of /usr/share/zoneinfo:
    header   : "tzdata" + 5 char version + NUL (12 bytes), then big-endian int32 index_offset, data_offset, final_offset
    index    : per zone, 52 bytes: name[40], int32 offset (from data_offset), int32 length, int32 unused
    data     : the zones' TZif files back to back
The loader looks for it at <game files>/usr/share/zoneinfo/tzdata (it tries /usr/share/zoneinfo/tzdata, which
resolves to ./usr/share/zoneinfo/tzdata next to the game's libs and assets).

usage: make_android_tzdata.py OUTPUT_FILE [ZONEINFO_DIR]
"""
import os
import struct
import sys

SKIP_DIRS = {"posix", "right"}
SKIP_FILES = {"posixrules", "localtime", "Factory", "leapseconds"}


def collect(root):
    zones = {}
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if not (dirpath == root and d in SKIP_DIRS)]
        for fn in filenames:
            if fn in SKIP_FILES or "." in fn:
                continue
            path = os.path.join(dirpath, fn)
            name = os.path.relpath(path, root)
            try:
                with open(path, "rb") as f:
                    data = f.read()
            except OSError:
                continue
            if data[:4] == b"TZif" and len(name.encode()) < 40:
                zones[name] = data
    return zones


def version(root):
    try:
        with open(os.path.join(root, "tzdata.zi")) as f:
            first = f.readline().split()
            if len(first) >= 3 and first[1] == "version":
                return first[2][:5].ljust(5, "x")
    except OSError:
        pass
    return "2024a"


def main():
    out = sys.argv[1]
    root = sys.argv[2] if len(sys.argv) > 2 else "/usr/share/zoneinfo"
    zones = collect(root)
    if "UTC" not in zones:
        sys.exit("no usable zoneinfo found in " + root)
    names = sorted(zones)
    index_offset = 24
    data_offset = index_offset + 52 * len(names)
    index, data = b"", b""
    for n in names:
        index += n.encode().ljust(40, b"\0") + struct.pack(">iii", len(data), len(zones[n]), 0)
        data += zones[n]
    header = b"tzdata" + version(root).encode() + b"\0" + struct.pack(">iii", index_offset, data_offset, data_offset + len(data))
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "wb") as f:
        f.write(header + index + data)
    print("wrote %s: %d zones, %d bytes" % (out, len(names), len(header + index + data)))


if __name__ == "__main__":
    main()

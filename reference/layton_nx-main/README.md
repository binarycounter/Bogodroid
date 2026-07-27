<div align=center>

<img src="extras/banner.png" alt="Banner" width="40%">

</div>
<h1 align=center>Professor Layton: Curious Village HD — Nintendo Switch port</h1>

A wrapper/port of the Android release of **Professor Layton: Curious Village HD**
(v1.0.8). It loads the original game binary (`libll1.so`, arm64),
resolves its imports against native Switch implementations and patches it so it
runs as if inside a minimal Android environment.

No game assets or original program code are included. You must own the game
and supply the files yourself.

## How to install

Create a folder for the game on your SD card, `/switch/layton/`, and place:

1. `layton_nx.nro`
2. `libll1.so` — extracted from the APK's `lib/arm64-v8a/` folder.
3. `assets/` — the **contents** of the APK's `assets/` folder (the `data`,
   `data-en`, `data-de`, … and `data-EU` subfolders).

```
/switch/layton/
  layton_nx.nro
  libll1.so
  assets/
    data/
    data-en/
    data-EU/
    ...
```

Launch with a **game override** (hold R while starting a title) or a
forwarder — applet/album mode does not work

Saves are written to `data/` next to the `.nro`.

## Configuration

`config.txt` is created next to the `.nro` on first run:

* `screen_width` / `screen_height` — render resolution; `-1` picks
  1280×720 in handheld and 1920×1080 docked.
* `portrait` — `1` (default) renders the game in portrait, to be played with
  the console held rotated; `2` is the same portrait view rotated the other way
  (180°), for holding the console the opposite way up; `0` is landscape.


## Known limitations
* In landscape mode cutscenes always play fullscreen, so the movie player's
  rotate button only affects the on-screen controls there. Portrait mode
  keeps the original rotate behaviour.


## Build

devkitA64 plus these portlibs:

```
pacman -S switch-mesa switch-libdrm_nouveau switch-ffmpeg switch-dav1d \
          switch-bzip2 switch-zlib
```

## Credits

* **TheOfficialFloW** — for the original Android so-loader (gtasa_vita).
* **Rinnegatamante** — the Vita Layton port this is based on.
* **fgsfds** — the Switch so-loader groundwork reused here.

### Support

If you enjoy my work and want to support me :

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/D1D1P2MOG)

## Legal

No affiliation with Level-5. "Professor Layton" is a trademark of its owner.
This repository contains no assets or program code from the original game,
and none may be distributed with builds. Users must extract the required
files from their own legally obtained copy.

Source code is provided under the MIT License (see LICENSE).

# The build image plus a virtual display and software OpenGL ES, so the real loader can run a Unity game inside the
# container (headless, under qemu on an x86 machine, natively on an ARM64 one).
FROM bogodroid-build
RUN apt-get update && apt-get install -y --no-install-recommends \
        xvfb xauth libgl1-mesa-dri libegl1-mesa libgles2-mesa libglx-mesa0 libasound2 \
    && rm -rf /var/lib/apt/lists/*

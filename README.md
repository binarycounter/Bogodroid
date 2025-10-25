# Bogodroid NEO

This is an experimental project attempting to create a wrapper environment around native libraries from ARM Android Apps, with the intent of running the libraries (or entire apps and games) on ARM Linux (armhf and arm64) handhelds. The current focus is on supporting the Unity game engine.

The project is heavily based on [JohnnyOnFlame/gmloader-net](https://github.com/JohnnyonFlame/gmloader-next) and [ChristopherHX/libjnivm](https://github.com/ChristopherHX/libjnivm).

The idea is to:
- load the libraries, patch them at runtime
- wrap, bridge or stub any required symbols from Android system libraries
- Provide a JNI interface for communication with placeholder "java" classes
- Provide implementations and stubs for common java and android classes
- Ultimately wrap Audio, GLES and Input to host interfaces and libraries.

This project does not aim to be able to run your App out of the box. It is meant as a framework or starting point for your own porting project. For existing porting projects, check the "Ports in progress" table and the projects folder.

# How to Build

### 1. Install Prerequisites

First, ensure you have the necessary tools and libraries installed. On a Debian-based system (like Ubuntu), you can install them with the following command:

```bash
sudo apt update
sudo apt install cmake git libzip-dev libsdl2-dev
```

### 2. Clone the Repository

Clone the project repository to your local machine using git:

```bash
git clone --recursive --branch neo https://github.com/binarycounter/Bogodroid/
cd Bogodroid
```

### 3. Configure the Project with CMake

Create a dedicated build directory. This keeps the main project folder clean.

```bash
mkdir build && cd build
```

Next, run CMake to configure the project and generate the build files. You must specify a build type.

#### To create a **Release** build:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

#### To create a **Debug** build (**SLOWER!** Debug symbols and extensive trace logging):

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

#### To create a **Release with Debug Info** build (optimized, but includes debug symbols):

```bash
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

### 4. Compile the Code

Once CMake has finished configuring, you can compile the project using `make`. The command below uses all available CPU cores to speed up the process.

```bash
make -j$(nproc)
```

The final executable will be located in the `build` directory.

---

### Building a Specific Project

By default, the build system compiles the `unityloader` project. If you wish to build a different project, you can specify it during the CMake configuration step (step 3) by adding the `-DPROJ` flag.

For example, to build a project named `hexagonloader` as a `Release` build, you would run:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DPROJ=hexagonloader
```

# Running

*Please note that most loaders require /proc to be mounted, which is not the case in the chroot environment in the VM image. Run `mount -t proc none /proc` as root inside the chroot environment to fix this.*

`unityloader` and `limboloader` use a TOML config file to configure paths and properties. Example config files are provided in `configs/`. If you run the program directly from the build directory and have extracted your Unity game APK in `gamefiles/unity/` you can use this config without changes:

`./unityloader ../configs/unity.toml`

# Debugging with QEMU and GDB-multiarch

(This expects the chroot environment as provided by [this VM](https://forum.odroid.com/viewtopic.php?p=306185#p306185))

1. Exit your chroot, we will call the binary directly
2. Enter the build directory 
3. Start the binary with: 
`QEMU_LD_PREFIX=/mnt/data/aarch64/ qemu-arm-static -g 5 ./unityloader ../configs/unity.toml`
4. Open another terminal and type in `gdb-multiarch`
5. Enter the location of your chroot: `set sysroot /mnt/data/armhf`
6. Enter the location of your binary: `file /mnt/data/armhf/root/bogodroid/build/unityloader`
7. Connect to the open GDB session in QEMU: `target remote :5`
6. "c" to run the program, "si" to step a single instruction, "bt" to read stack trace, "break" to set breakpoint. ***Refer to the GDB manpage and internet resources on how to debug***

# JNI Tracing your App

(This requires a rooted Android device and a computer with ADB and Python installed)

1. [Install a Frida server on your Android device](https://frida.re/docs/android/)
2. Install jnitrace on your computer: `pip install jnitrace`
3. Run the trace: `jnitrace -o jnitrace.json com.example.myapplication`
4. Press CTRL-C to finish aquiring data
5. `jnitrace.json` will contain all JNI calls made by the application
6. `python tools/jnitrace_parse.py <input_json_file> <output_json_file>` will give you a consolidated tree of all classes, methods and fields (and their signatures) accessed during the tracing period.

See `tools/unity_traces` for a trace of a minimal Unity game starting up





# Ports in Progress

| Name                              | Project Name  | Status        | Notes                                                                                                              |
|-----------------------------------|---------------|---------------|--------------------------------------------------------------------------------------------------------------------|
| Unity 2021.3f (aarch64 il2cpp)                      | unityloader   | Working  | Runs simple games. Graphics, audio and input work. |
| Unity 2021.3f (aarch64 mono)                      | unityloader   | Working POC  | Simple demos and benchmarks are working, with low but reasonable performance. Implementation is still incomplete. |
| Super Hexagon                     | hexagonloader | Not Booting   | Unchanged from Pre-NEO Bogodroid, needs complete rework  |
| Limbo                             | limboloader   | Loading Screen| Unchanged from Pre-NEO Bogodroid, needs complete rework  |
| Crazy Taxi Classic                | taxiloader    | Investigated  | Requires better file handling for loading the big OBB files, before it can be attempted.                           |
| Terraria                          | -             | Not attempted | (Engine: FNA, Monogame port might be easier)                                                                       |
| Baba is you                       | -             | Not attempted |                                                                                                                    |
| Super Meat Boy Forever            | -             | Not attempted |                                                                                                                    |
| Streets of rage 4                 | -             | Not attempted | (Engine: FNA, Monogame port might be easier)                                                                       |
| Chrono Trigger                    | -             | Not attempted | (Engine: cocos2d-x)                                                                                                |
| Dead Cells                        | -             | Not attempted |                                                                                                                    |
| Brotato                           | -             | Not attempted |                                                                                                                    |
| Castlevania Symphony of the Night | -             | Not attempted |                                                                                                                    |
| Five Nights At Freddy             | -             | Not attempted |                                                                                                                    |
| Gris                              | -             | Not attempted |                                                                                                                    |
| The Way Home                      | -             | Not attempted |                                                                                                                    |
| Vampire Survivors                 | -             | Not attempted |                                                                                                                    |
| Scourgebringer                    | -             | Not attempted |                                                                                                                    |
| Haak                              | -             | Not attempted |                                                                                                                    |



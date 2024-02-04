# Bogodroid

### **WARNING: VERY EARLY IN DEVELOPMENT, VERY NON-FUNCTIONAL, CODE QUALITY QUESTIONABLE**

This is an experimental project attempting to create a wrapper environment around native libraries (armeabi) from Android Apps, with the intent of running the libraries (or entire apps and games) on ARM Linux (armhf and arm64) handhelds.

The project is heavily based on [JohnnyOnFlame/droidports](https://github.com/JohnnyonFlame/droidports) and [ChristopherHX/libjnivm](https://github.com/ChristopherHX/libjnivm).

The idea is to:
- load the libraries, patch them at runtime
- wrap, bridge or stub any required symbols from Android system libraries
- Provide a JNI interface for communication with placeholder "java" classes
- Provide implementations and stubs for common java and android classes
- Ultimately wrap Audio, GLES and Input to host interfaces and libraries.

This project does not aim to be able to run your App out of the box. It is meant as a framework or starting point for your own porting project. For existing porting projects, check the "Ports in progress" table and the projects folder.

# How to Build (Debug)

(This process will be simplified later on)

1. Create a armhf chroot environment for building and testing. I recommend [this VM](https://forum.odroid.com/viewtopic.php?p=306185#p306185) image as a starting point.
2. Clone the repository into your chroot
3. Create the folder "./libjnivm/build" inside the repository and enter it
4. `cmake .. -DJNIVM_ENABLE_TRACE=ON -DJNIVM_ENABLE_GC=ON -DJNIVM_ENABLE_DEBUG=ON -DJNIVM_USE_FAKE_JNI_CODEGEN=ON `
5. `make -j8`
6. Create the folder "./build" inside the repository and enter it
7. `cmake ..` 
8. `make -j8`

By default this will build the `unityloader` project. If you want to build a different project, you can specify the project in step 7 with `cmake .. --build -DPROJ=<project name>`

# Running

Currently, the `unityloader` and `hexagonloader` expect a commandline argument to a folder where you extracted the apk of your target. one folder above that should contain the folder `support_files` from the `gamefiles` directory. The easiest way is to extract your apk in the `gamefiles` folder, and run directly from the build directory, like this: 

`./unityloader ../gamefiles/testgame/`

# Debugging with QEMU and GDB-multiarch

(This expects the chroot environment as provided by [this VM](https://forum.odroid.com/viewtopic.php?p=306185#p306185))

1. Exit your chroot, we will call the binary directly
2. Enter the build directory 
3. Start the binary with: 
`QEMU_LD_PREFIX=/mnt/data/armhf/ qemu-arm-static -g 5 ./unityloader ../gamefiles/testgame/`
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

| Name                              | Arch  | Status        | Notes                                                                                                              |
|-----------------------------------|-------|---------------|--------------------------------------------------------------------------------------------------------------------|
| Unity (Mono)                      | ARMv7 | Not Booting   | Makes it 70% through initialization. Crashes when Mono creates first thread.  Pthread bridge needs to be reworked. |
| Super Hexagon                     | ARMv7 | Not Booting   | Crashes very early during init while writing first log message. Issue with libc++_shared.so                      |
| Crazy Taxi Classic                | ARMv7 | Investigated  | Requires better file handling for loading the big OBB files, before it can be attempted.                                                       |
| Limbo                             | ARMv7 | Not attempted |                                                                                                                    |
| Terraria                          | ARMv7 | Not attempted | (Engine: FNA, Monogame port might be easier)                                                                       |
| Baba is you                       | ARMv7 | Not attempted |                                                                                                                    |
| Super Meat Boy Forever            | ARMv7 | Not attempted |                                                                                                                    |
| Streets of rage 4                 | ARMv7 | Not attempted | (Engine: FNA, Monogame port might be easier)                                                                       |
| Chrono Trigger                    | ARMv7 | Not attempted | (Engine: cocos2d-x)                                                                                                |
| Dead Cells                        | ARMv7 | Not attempted |                                                                                                                    |
| Brotato                           | ARMv7 | Not attempted |                                                                                                                    |
| Castlevania Symphony of the Night | ARMv7 | Not attempted |                                                                                                                    |
| Five Nights At Freddy             | ARMv7 | Not attempted |                                                                                                                    |
| Gris                              | ARMv7 | Not attempted |                                                                                                                    |
| The way home                      | ARMv7 | Not attempted |                                                                                                                    |
| Vampire Survivors                 | ARMv7 | Not attempted |                                                                                                                    |
| Scourgebringer                    | ARMv7 | Not attempted |                                                                                                                    |
| Haak                              | ARMv7 | Not attempted |                                                                                                                    |




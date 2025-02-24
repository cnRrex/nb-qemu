### 当前修改的说明 Description of current modifications
Not usable, cannot bu built
未完成，不可编译
#### 原nb-qemu的补充说明
基本逻辑：
（引用原作者）
1. 添加了一个断点逻辑（当前是利用修改过的arm的yield），在翻译时如果遇到下一个指令的地址是要求停止的地址(pc_stop)，则生成yield，这个yield触发中断，并修改qemu的逻辑，让qemu的cpu_loop返回退出。
2. nb-qemu和qemu一起编译，初始化时执行qemu的main，在某处时停下
3. 通过qemu-user运行一个nb-qemu-guest elf程序（必须设定interpreter为安卓的linker（guest的linker）），设置此elf的入口点为pc_stop,以便初始化时先运行完linker的加载，然后返回到nb-qemu
4. nb-qemu会搜索此elf的所有符号，将需要的函数地址记录，以便执行
5. JNI环境的接口是在qemu定义了两个系统调用(JavaVM,JNIEnv)，系统调用在nb-qemu中处理，这样就能进行简单的guest环境访问java环境，这部分命名为Java Bridge
6. 安卓ndk系统库（opengles等）的包装使用了arm的svc指令，在处理svc号码时,交由svc_handler处理，这部分命名为OS Bridge
7. 函数蹦床由libffi的闭包生成，通过修改CPUArchState的寄存器和栈环境为函数的地址和参数，设置函数返回地址为nb-qemu-guest elf的入口（也就是pc_stop），然后cpu_loop运行，再处理返回结果，实现单个函数的执行。
8. 原方法修改了libc,增加线程自分配和自删除。每个新线程都要创建qemu cpu，并且要对app的新线程的qemu cpu创建guest的线程环境。linker的搜索系统库位置改为arm的库文件夹


（我的补充和已/待修改）
1. 分离qemu android和nb-qemu，QemuAndroid原本是引用qemu的一些函数和包装，但是因为整个构建系统的修改很麻烦，因此将这些接口转移至qemu，导出为QemuAndroidItf，把qemu-aarch64编译为libqemu-aarch64.so,这样qemu就可以用原构建系统通过termux构建环境构建，nb-qemu则可用Android或cmake等构建系统构建（尚未写完），这样还支持多个guest(比如arm64或riscv64)，只需要打开对应的qemu库即可。
2. 修改了原cpu的创建和修改逻辑，这一部分的cpu创建部分主要参考qemu的源码逻辑，而对于guest的线程环境创建，我的新方法是hook了qemu的线程系统调用，阻止新线程的创建或当前线程的退出，并直接应用至当前环境。这样就不需要修改guest的libc,只需要在nb-qemu-guest里写最简化的pthread_create和pthread_exit即可。在新建和删除cpu时还需要一个最简化的模拟guest调用，因此这两个函数都不涉及参数传递，只有pthread_create会返回失败与否。
3. 修改了qemu_call的逻辑，原本的逻辑是仅有整数寄存器，并且不易于扩展。为了适应多guest，我定义了call struct和ret struct，这些结构携带参数数据和调用指南，告诉qemu android如何进行call,比如使用多少整数寄存器，多少浮点寄存器，返回值怎么取。由于抽象成寄存器类型，因此调用约定abi可以在nb-qemu侧处理。
4. 因为修改了qemu_call的逻辑，guest的函数接口已不能正常使用，计划直接使用蹦床创建，在初始化时对这些接口创建蹦床，以便直接使用
5. qemu那边进行了一些初步补丁，包括android linker指定，默认binfmt_misc的P flag, ashmem/binder系统调用等等（存在问题）
6. （TODO）由于原本的pc_stop逻辑仅适用于arm，且存在一定的开销。当前的pc_stop仅仅出现在nb-qemu-guest的入口。因此可以考虑在入口处放一个软件中断/调试中断指令，修改qemu处理中断的逻辑，如果中断处pc是pc_stop,那么就cpu_loop_exit
7. （TODO）arm32在安卓中是aapcs,纯整数寄存器的ABI调用约定，但是在libvulkan中用的aapcs-vfp,也就是有硬件浮点寄存器的ABI，尚未修改
8. （TODO）当前系统库包装需要重构，尽管svc是很不错的方法，但我们仍需要一种新蹦床，也就是guest调用host函数，系统库函数包装原理要参考berberis
9. （TODO）qemu中如果设置了guest_base，那么很多guest地址和host地址将对不上（当前有相当一部分地址是直接使用而没有g2h或h2g的，尽管默认时能用）
10. （TODO）考虑64位host，32位guest，最大的挑战是64位jvm的指针和数据结构如何映射至32位的guest

qemu修改：
https://github.com/cnRrex/qemu/tree/nb-qemu/9.1


#### 以下是ATL的说明，许多已不适用
---------------
### QEMU-based native bridge for ATL
original project: https://github.com/goffioul/ax86-nb-qemu

#### how to use

```
make
sudo make install
```
extract the aarch64 native libs to `~/.local/share/android_translation_layer/.../lib/`
(x86_64 libs would get extracted automatically, but presumably there were none if you decided
to use native bridge)
```
NB_QEMU_SYSROOT=/usr/local/share/libnb-qemu-guest/ android-translation-layer ... -X '-Xforce-nb-testing' -X '-XX:NativeBridge=/usr/local/lib64/libnb-qemu.so'
```

#### what's a native bridge?

https://android.googlesource.com/platform/art/+/refs/heads/main/libnativebridge/README.md

The only user used to be Intel's libhoudini (proprietary) for arm on x86 and aarch64 on x86_64.
Now https://android.googlesource.com/platform/frameworks/libs/binary_translation/+/refs/heads/main/
also exists, though this currently only supports riscv64 on x86_64. It is however open source
and it seems possible to extend it to support other targets.

This is an alternative to these. It uses QEMU for the translation, and currently works for an arm
or aarch64 target (x86/x86_64 hosts tested). It is open source (like berberis), but has arm/aarch64
support (like houdini). Someone adding arm/aarch64 target support to berberis would probably make
this obsolete, but for now it seems to work.

#### overview

`qemu/` - patched up qemu
 - compile a static library instead of a standalone executable (gets linked into the native bridge library)
 - a mechanism to yield on demand so that synchronous calls can be made to non-native code
 - a mechanism to call back to host code (a "syscall" per thunked function)
 - misc

`libnb-qemu/` - host side code
 - the native bridge library itself (FIXME - mixed in with everything else)
 - `*.def` - for generating thunks (both host and target side)
 - `*.itf.*` - manually written parts for host side thunks

`libnb-qemu-guest/` - the target side code (gets cross-compiled)
 - libnb.c - "entry point", housekeeping functions
 - jnienv.c - manually written target side thunks for JNI functions
 - manually written parts for target side thunks
 - `bionic/` - bionic libc with the necessary patch pre-applied (and with a hacky Makefile for cross compilation)

`genlib.py` - generates both host and target side thunks from `.def` files

`Android.mk` - original for building with AOSP build system, probably broken but could be fixed

#### changes from the original project

- support for aarch64 target on x86_64 host (previously only arm target with x86 host was supported)
	- an attempt was made to not break arm32 on x86, but no testing was done to check this
- make this work with https://gitlab.com/android_translation_layer/android_translation_layer
	- some ifdefs were added to in theory still allow compilation for AOSP, though the build system
	  is probably broken.
- add basic documentation to this readme

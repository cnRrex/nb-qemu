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

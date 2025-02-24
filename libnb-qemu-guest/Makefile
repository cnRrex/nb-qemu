CURRDIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
BUILDDIR := $(BUILDDIR)/libnb-qemu-guest

BIONIC_BUILDDDIR ?= $(CURRDIR)/bionic/builddir/

CROSSFLAGS := -target $(TARGETARCH_FULL)-pc-linux -fuse-ld=lld -shared -nostdinc -nostdlib -lgcc
CROSSFLAGS += --sysroot=$(CURRDIR)/bionic/prebuilts/ # this probably doesn't do anything other than make sure that lld can't fall back to host arch libs when this is run before bionic was compiled, causing confusing error messages
CFLAGS := -g -fPIC -D__ANDROID__
LDFLAGS_MAIN := -Wl,-e,0 # nb-qemu's qemu patches make the cpu yield when pc == entry, however this seems to break if entry is not at 0x0 (we don't need there to be any code at this address, since it never gets executed)
LDFLAGS_MAIN += -Wl,--no-rosegment # otherwise clang makes the segment at 0x0 ro rather than R+X
LDFLAGS := -Wl,--eh-frame-hdr -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now # seems that ndk passes these, so might as well do the same
LDLIBS := -L$(BIONIC_BUILDDDIR) -L$(CURRDIR)/bionic/prebuilts/ -lc -ldl
INCLUDES := -I$(shell clang -print-resource-dir)/include/ \
          -I$(TOPDIR)/libnb-qemu/ \
          -I$(CURRDIR)/bionic/libc/include/ \
          -I$(CURRDIR)/bionic/libc/kernel/android/uapi/ \
          -I$(CURRDIR)/bionic/libc/kernel/uapi/ \
          -I$(CURRDIR)/bionic/libc/kernel/uapi/asm-arm64 \
          -I$(CURRDIR)/../libnb-qemu/_includes/

target-libs: $(BUILDDIR)/libnb-qemu-guest.so $(BUILDDIR)/libandroid.so \
             $(BUILDDIR)/libEGL.so $(BUILDDIR)/libGLESv2.so \
             $(BUILDDIR)/liblog.so $(BUILDDIR)/libOpenSLES.so $(BUILDDIR)/libstdc++.so | $(BUILDDIR)

$(BUILDDIR)/libnb-qemu-guest.so: $(CURRDIR)/bionic/libc/arch-common/bionic/crtbegin_so.c $(CURRDIR)/libnb.c $(CURRDIR)/jnienv.c $(CURRDIR)/bionic/libc/arch-common/bionic/crtend_so.S
	clang $(CROSSFLAGS) $(LDFLAGS) $(LDFLAGS_MAIN) $(LDLIBS) $(CFLAGS) $(INCLUDES) -o $@ $^ \
	      -x c - <<< "const char interp_section[] __attribute__((section(\".interp\"))) = \"/linker64\";"

#gen guest thunk lib
$(BUILDDIR)/%.c : $(TOPDIR)/libnb-qemu/%.def $(TOPDIR)/genlib.py | $(BUILDDIR)
	$(TOPDIR)/genlib.py -g $< > $@

$(BUILDDIR)/libandroid.so: $(BUILDDIR)/libandroid.c
	clang $(CROSSFLAGS) $(LDFLAGS) $(CFLAGS) -o $@ $^

$(BUILDDIR)/libEGL.so: $(BUILDDIR)/libEGL.c $(CURRDIR)/libEGL.itf.c
	clang $(CROSSFLAGS) $(LDFLAGS) $(CFLAGS) $(INCLUDES) -o $@ $^

$(BUILDDIR)/libGLESv1_CM.so: $(BUILDDIR)/libGLESv1_CM.c
	clang $(CROSSFLAGS) $(LDFLAGS) $(CFLAGS) -o $@ $^

$(BUILDDIR)/libGLESv2.so: $(BUILDDIR)/libGLESv2.c
	clang $(CROSSFLAGS) $(LDFLAGS) $(CFLAGS) -o $@ $^

$(BUILDDIR)/liblog.so: $(CURRDIR)/liblog.c
	clang $(CROSSFLAGS) $(LDFLAGS) $(CFLAGS) $(LDLIBS) $(INCLUDES) -o $@ $^

$(BUILDDIR)/libOpenSLES.so: $(BUILDDIR)/libOpenSLES.c $(CURRDIR)/libOpenSLES.iid.c $(CURRDIR)/libOpenSLES.itf.c
	clang $(CROSSFLAGS) $(LDFLAGS) $(CFLAGS) $(LDLIBS) $(INCLUDES) -o $@ $^

.PHONY: $(BUILDDIR)

$(BUILDDIR):
	mkdir -p $@

include $(CURRDIR)/libstdc++_standalone/Makefile


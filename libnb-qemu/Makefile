CURRDIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
BUILDDIR := $(BUILDDIR)/libnb-qemu

LIBNB_QEMU_SRCS=JavaBridge.cpp \
                Library.cpp \
                OsBridge.cpp \
                QemuAndroid.c \
                QemuBridge.cpp \
                QemuCpu.cpp \
                Trampoline.cpp \
                libnb-qemu.cpp

LIBNB_QEMU_OBJS := $(LIBNB_QEMU_SRCS:%=$(BUILDDIR)/%.o)

CFLAGS := -g -fPIC `pkgconf --cflags glib-2.0 capstone`
DEFINES := -DHAVE_SYS_UIO_H
INCLUDES := -I$(CURRDIR)/_includes \
	    -I$(CURRDIR) \
	    -I$(CURRDIR)/../qemu/_build \
            -I$(CURRDIR)/../qemu/_build/aarch64-linux-user/ \
            -I$(CURRDIR)/../qemu/include \
            -I$(CURRDIR)/../qemu/linux-user \
            -I$(CURRDIR)/../qemu/linux-user/host/$(HOSTARCH_FULL)/ \
            -I$(CURRDIR)/../qemu/tcg/$(HOSTARCH) \
            -I$(CURRDIR)/../qemu/linux-user/$(TARGETARCH_FULL)/ \
            -I$(CURRDIR)/../qemu/target/$(TARGETARCH)/

LDLIBS := `pkgconf --libs glib-2.0 capstone` -lz -ldl -lffi -lgnutls
LDLIBS += -L$(LIBDIR_PATH)/art -lart -llog

host-libs: $(BUILDDIR)/libnb-qemu.so \
           $(BUILDDIR)/libnb-qemu-android.so $(BUILDDIR)/libnb-qemu-EGL.so \
           $(BUILDDIR)/libnb-qemu-GLESv1_CM.so $(BUILDDIR)/libnb-qemu-GLESv2.so \
           $(BUILDDIR)/libnb-qemu-OpenSLES.so

define rules=
$(filter %.$(1).o,$(LIBNB_QEMU_OBJS)): $(BUILDDIR)/%.$(1).o: $(CURRDIR)/%.$(1)
	mkdir -p $$(@D)
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) -o $$@ $$<
endef

EXTS := c cpp
$(foreach ext, $(EXTS), $(eval $(call rules,$(ext))))

$(QEMU_STATIC_LIB):
	mkdir $(QEMU_BUILDDIR)
	sh -c "cd $(QEMU_BUILDDIR) && ../configure --disable-werror --target-list=aarch64-linux-user --disable-system && make"

$(BUILDDIR)/libnb-qemu.so: $(LIBNB_QEMU_OBJS) $(QEMU_STATIC_LIB)
	$(CXX) -g -o $@ -shared $(LIBNB_QEMU_OBJS) -Wl,--whole-archive $(QEMU_STATIC_LIB) -Wl,--no-whole-archive $(LIBQEMUUTIL) $(LDLIBS)

CFLAGS_THUNKS := $(CFLAGS) -fvisibility=hidden $(LOCAL_QEMU_CFLAGS) \
                 -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES
LDLIBS_THUNKS := -L$(BUILDDIR)/ -lnb-qemu \
                 -L$(LIBDIR_PATH)/art -llog
DEFINES_LIBANDROID := -D_LARGEFILE64_SOURCE -D__ANDROID_API__=100 # the headers gate some declarations

$(BUILDDIR)/%.c : $(CURRDIR)/%.def $(TOPDIR)/genlib.py
	$(TOPDIR)/genlib.py $< > $@

$(BUILDDIR)/libnb-qemu-android.so: $(BUILDDIR)/libandroid.c $(CURRDIR)/libandroid.itf.cpp | $(BUILDDIR)/libnb-qemu.so
	$(CC) -shared $(CFLAGS_THUNKS) -Wno-deprecated-declarations $(LDLIBS_THUNKS) $(DEFINES) $(DEFINES_LIBANDROID) $(INCLUDES) -o $@ $^

$(BUILDDIR)/libnb-qemu-EGL.so: $(BUILDDIR)/libEGL.c $(CURRDIR)/libEGL.itf.c | $(BUILDDIR)/libnb-qemu.so
	$(CC) -shared $(CFLAGS_THUNKS) -lEGL $(LDLIBS_THUNKS) $(DEFINES) $(INCLUDES) -o $@ $^

$(BUILDDIR)/libnb-qemu-GLESv1_CM.so: $(BUILDDIR)/libGLESv1_CM.c | $(BUILDDIR)/libnb-qemu.so
	$(CC) -shared $(CFLAGS_THUNKS) -lGLESv1_CM $(LDLIBS_THUNKS) $(DEFINES) $(INCLUDES) -o $@ $^

$(BUILDDIR)/libnb-qemu-GLESv2.so: $(BUILDDIR)/libGLESv2.c | $(BUILDDIR)/libnb-qemu.so
	$(CC) -shared $(CFLAGS_THUNKS) -lGLESv2 $(LDLIBS_THUNKS) $(DEFINES) $(INCLUDES) -o $@ $^

$(BUILDDIR)/libnb-qemu-OpenSLES.so: $(BUILDDIR)/libOpenSLES.c $(CURRDIR)/libOpenSLES.itf.cpp | $(BUILDDIR)/libnb-qemu.so
	$(CC) -shared $(CFLAGS_THUNKS) -lOpenSLES $(LDLIBS_THUNKS) $(DEFINES) $(INCLUDES) -o $@ $^


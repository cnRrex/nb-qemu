export TOPDIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
export BUILDDIR := $(TOPDIR)/builddir

PREFIX ?= /usr/local/
LIBDIR ?= lib64
export LIBDIR_PATH := $(PREFIX)$(LIBDIR)

INSTALL_PREFIX ?= $(PREFIX)
INSTALL_DIR_HOST_THUNKS := $(INSTALL_PREFIX)/$(LIBDIR)/libnb-qemu-thunks/
INSTALL_DIR_TARGET := $(INSTALL_PREFIX)/share/libnb-qemu-guest/

# i386/arm dirs sometimes contain files needed for the 64bit variant of the ISA as well
export HOSTARCH := i386
export HOSTARCH_FULL := x86_64
export TARGETARCH := arm
export TARGETARCH_FULL := aarch64

export QEMU_BUILDDIR := $(TOPDIR)/qemu/_build/
export QEMU_STATIC_LIB := $(QEMU_BUILDDIR)/$(TARGETARCH_FULL)-linux-user/qemu-$(TARGETARCH_FULL).a
export LIBQEMUUTIL := $(QEMU_BUILDDIR)/libqemuutil.a

default:
	$(MAKE) -C $(TOPDIR)/libnb-qemu/
	$(MAKE) -C $(TOPDIR)/libnb-qemu-guest/bionic/ BUILDDIR=$(BUILDDIR)/bionic
	$(MAKE) -C $(TOPDIR)/libnb-qemu-guest/ BIONIC_BUILDDDIR=$(BUILDDIR)/bionic

install:
	install -Dt $(INSTALL_PREFIX)/$(LIBDIR)/ $(BUILDDIR)/libnb-qemu/libnb-qemu.so
	install -Dt $(INSTALL_DIR_HOST_THUNKS) $(BUILDDIR)/libnb-qemu/libnb-qemu-android.so \
	                                $(BUILDDIR)/libnb-qemu/libnb-qemu-EGL.so \
	                                $(BUILDDIR)/libnb-qemu/libnb-qemu-GLESv1_CM.so \
	                                $(BUILDDIR)/libnb-qemu/libnb-qemu-GLESv2.so \
	                                $(BUILDDIR)/libnb-qemu/libnb-qemu-OpenSLES.so
	#
	install -Dt $(INSTALL_DIR_TARGET) $(BUILDDIR)/bionic/libc.so \
	                                  $(TOPDIR)/libnb-qemu-guest/bionic/prebuilts/libdl.so \
	                                  $(TOPDIR)/libnb-qemu-guest/bionic/prebuilts/libm.so \
	                                  $(TOPDIR)/libnb-qemu-guest/bionic/prebuilts/linker64 \
	                                  $(BUILDDIR)/libnb-qemu-guest/libandroid.so \
	                                  $(BUILDDIR)/libnb-qemu-guest/libEGL.so \
	                                  $(BUILDDIR)/libnb-qemu-guest/libGLESv2.so \
	                                  $(BUILDDIR)/libnb-qemu-guest/liblog.so \
	                                  $(BUILDDIR)/libnb-qemu-guest/libnb-qemu-guest.so \
	                                  $(BUILDDIR)/libnb-qemu-guest/libOpenSLES.so \
	                                  $(BUILDDIR)/libnb-qemu-guest/libstdc++.so
	#
	ln -s libGLESv2.so $(INSTALL_DIR_TARGET)/libGLESv3.so

clean:
	rm -rf $(TOPDIR)/builddir

CURRDIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
BUILDDIR := $(CURRDIR)/builddir

PREFIX ?= /usr/local/
LIBDIR ?= lib64
LIBDIR_PATH = $(PREFIX)$(LIBDIR)

# i386/arm dirs sometimes contain files needed for the 64bit variant of the ISA as well
HOSTARCH := i386
HOSTARCH_FULL := x86_64
TARGETARCH := arm
TARGETARCH_FULL := aarch64

QEMU_BUILDDIR := $(CURRDIR)/qemu/_build/
QEMU_STATIC_LIB := $(QEMU_BUILDDIR)/$(TARGETARCH_FULL)-linux-user/qemu-$(TARGETARCH_FULL).a
LIBQEMUUTIL := $(QEMU_BUILDDIR)/libqemuutil.a

default: all

include $(CURRDIR)/libnb-qemu/Makefile

$(QEMU_STATIC_LIB):
	mkdir $(QEMU_BUILDDIR)
	sh -c "cd $(QEMU_BUILDDIR) && ../configure --disable-werror --target-list=aarch64-linux-user --disable-system && make"

#ifndef QEMU_INCLUDE_H_
#define QEMU_INCLUDE_H_

#define NEED_CPU_H
#undef NDEBUG

#define __USE_GNU
#include <sys/ucontext.h>

#include <qemu/osdep.h>
#include <qemu/guest-random.h>
#include <disas/disas.h>
#include <tcg/tcg.h>
#include <elf.h>
#include <qemu.h>
#include <cpu_loop-common.h>

#endif

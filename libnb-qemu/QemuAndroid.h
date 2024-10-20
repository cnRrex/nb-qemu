/*
 * nb-qemu
 * 
 * Copyright (c) 2019 Michael Goffioul
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef QEMU_ANDROID_H_
#define QEMU_ANDROID_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int qemu_android_initialize(const char *procname, const char *tmpdir);
intptr_t qemu_android_lookup_symbol(const char *name);
intptr_t qemu_android_malloc(size_t size);
void qemu_android_free(intptr_t addr);
void qemu_android_memcpy(intptr_t dest, const void *src, size_t length);
const char *qemu_android_get_string(intptr_t addr);
void qemu_android_release_string(const char *s, intptr_t addr);
void *qemu_android_get_memory(intptr_t addr, size_t length);
void qemu_android_release_memory(void *ptr, intptr_t addr, size_t length);
#define qemu_android_call0(c, x) qemu_android_call9(c, x, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
#define qemu_android_call1(c, x, a1) qemu_android_call9(c, x, a1, 0, 0, 0, 0, 0, 0, 0, 0, 0)
#define qemu_android_call2(c, x, a1, a2) qemu_android_call9(c, x, a1, a2, 0, 0, 0, 0, 0, 0, 0, 0)
#define qemu_android_call3(c, x, a1, a2, a3) qemu_android_call9(c, x, a1, a2, a3, 0, 0, 0, 0, 0, 0, 0)
#define qemu_android_call4(c, x, a1, a2, a3, a4) qemu_android_call9(c, x, a1, a2, a3, a4, 0, 0, 0, 0, 0, 0)
intptr_t qemu_android_call9(void *cpu, intptr_t addr, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, char *stack, int stack_size);
uint64_t qemu_android_call9_ll(void *cpu, intptr_t addr, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, char *stack, int stack_size);
typedef intptr_t(*qemu_android_syscall_handler_t)(
    void *cpu_env, int num,
    intptr_t arg1, intptr_t arg2, intptr_t arg3,
    intptr_t arg4, intptr_t arg5, intptr_t arg6);
void qemu_android_register_syscall_handler(qemu_android_syscall_handler_t func);
void *qemu_android_get_cpu();
void *qemu_android_new_cpu();
void qemu_android_delete_cpu(void *cpu);
typedef void(*qemu_android_svc_handler_t)(void *cpu_env, uint16_t num);
void qemu_android_register_svc_handler(qemu_android_svc_handler_t func);
intptr_t qemu_android_h2g(void *addr);
void *qemu_android_g2h(intptr_t addr);

#ifdef __cplusplus
};
#endif

#endif

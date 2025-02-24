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

#ifndef QEMU_ANDROID_INTERFACE_H_
#define QEMU_ANDROID_INTERFACE_H_

#include <stddef.h>
#include <stdint.h>

//TODO: target related abi_ulong
//when 64bit-host 32-bit-guest all these interface need to redesign
//these two is not use in qemu, just a copy for nb-qemu to use
typedef intptr_t (*qemu_android_syscall_handler_t)(void *cpu_env, int num,
                                                   intptr_t arg1, intptr_t arg2,
                                                   intptr_t arg3, intptr_t arg4,
                                                   intptr_t arg5,
                                                   intptr_t arg6);

typedef void (*qemu_android_svc_handler_t)(void *cpu_env, uint16_t num);

// 128 bit type, when use little-endian, it can use memcpy to this structure
typedef struct {
    uint64_t low;
    uint64_t high;
} QA_UInt128;

// abi_ptr type, use for h2g when 64-bit host 32-bit guest is ready
typedef struct {
    union{
        uint32_t addr32;
        uint64_t addr64;
    };
} QA_abi_ptr;

//tell qemuAndroid how to copy return
enum retProcessType {
  RET_TYPE_UNKNOWN = 0,
  RET_TYPE_VOID, //process a void return
  RET_TYPE_INT_REG_32, //process an 32bit return from reg
  RET_TYPE_FLOAT_REG_32, //process an 32bit return from fpreg
  RET_TYPE_INT_REG_64, //process an 64bit return from reg
  RET_TYPE_FLOAT_REG_64, //process an 64bit return from fpreg
  RET_TYPE_INT_REG_128, //process an 128bit return from reg
  RET_TYPE_FLOAT_REG_128, //process an 128bit return from fpreg
  //not implement
  RET_TYPE_FLOAT_STACK_32, //process an 32bit return from x86 x87 float stack
  RET_TYPE_FLOAT_STACK_64, //process an 64bit return from x86 x87 float stack
  RET_TYPE_FLOAT_STACK_128, //process an 128bit return from x86 x87 float stack
  //not implement
  RET_TYPE_STACK_32, //process an 32bit return from stack
  RET_TYPE_STACK_64, //process an 64bit return from stack
  RET_TYPE_STACK_128, //process an 128bit return from stack
};

//Struct for qemu_android call
//user pthread_key_t to store them
struct QemuAndroidCallData {
    uint32_t *int32_regs; //array for 32bit target regs
    uint64_t *int64_regs; //array for 64bit target regs
    uint32_t *float32_regs; //32bit fpu regs
    uint64_t *float64_regs; //64bit fpu regs
    QA_UInt128 *float128_regs; //128bit fpu regs
    char *stack; //store stack
    uint32_t stack_size; //stack size;
    int int_regs_used; //on non-mips platform, usually we dont need too much regs
    int float_regs_used; //set these to reduce copy times
};

//user pthread_key_t to store them
struct QemuAndroidCallRet {
    enum retProcessType ret_type; //return type
    union{
        uint32_t int32_ret; //32bit type return
        uint64_t int64_ret; //64bit type return
        QA_UInt128 int128_ret; //128bit type return
    };
};

/* Functions will be  exported by qemuAndroid.
 * This may changed much unlike NativeBridgeCallbacks */
struct QemuAndroidCallbacks {
    int (*initialize)(const char *procname, const char *tmpdir,
                            const char **qemu_envp, const char *guest_entry);
    intptr_t (*lookup_symbol)(const char *name);
    intptr_t (*malloc)(size_t size);
    void (*free)(intptr_t addr);
    void (*memcpy)(intptr_t dest, const void *src, size_t length);
    const char* (*get_string)(intptr_t addr);
    void (*release_string)(const char *s, intptr_t addr);
    void* (*get_memory)(intptr_t addr, size_t length);
    void (*release_memory)(void *ptr, intptr_t addr, size_t length);
    intptr_t (*h2g)(void *addr);
    void* (*g2h)(intptr_t addr);
    void (*register_syscall_handler)(qemu_android_syscall_handler_t func);
    void (*register_svc_handler)(qemu_android_svc_handler_t func);
    //void* (*get_regs)(void *env, unsigned int index);
    //void* (*get_sp_reg)(void *env);
    //void* (*get_lr_reg)(void *env);
    //void* (*get_pc_reg)(void *env);
    void* (*get_cpu)(void);
    void* (*new_cpu)(void);
    int (*delete_cpu)(void *cpu);
    void (*call)(void *cpu,
                 intptr_t addr,
                 struct QemuAndroidCallData *data,
                 struct QemuAndroidCallRet *ret);
};

#endif

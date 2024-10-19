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

#define LOG_TAG "libnb-qemu"
//#define LOG_NDEBUG 0

#include <log/log.h>
#include "QemuAndroid.h"
#include "QemuInclude.h"
#include <glib.h>

#define QEMU_LOG_MASK "page,unimp" // exec,nochain

static intptr_t thread_allocate_;
static intptr_t thread_deallocate_;

int qemu_android_initialize(const char *procname, const char *tmpdir)
{
    const char *sysroot_path = getenv("NB_QEMU_SYSROOT");
    if(!sysroot_path)
         LOG_ALWAYS_FATAL("%s: NB_QEMU_SYSROOT not set", __func__);
    gchar *sysroot_path_arg = g_strdup_printf("LD_LIBRARY_PATH=%s", sysroot_path);
    gchar *entry_point_path = g_strdup_printf("%s/%s", sysroot_path, "libnb-qemu-guest.so");
    char *argv[] = { LOG_TAG, "-d", QEMU_LOG_MASK, "-E", sysroot_path_arg, "-L", (char *)sysroot_path, "-T", (char *)tmpdir, "-0", (char *)procname, entry_point_path };
    int result = qemu_main(sizeof(argv)/sizeof(char*), argv, NULL);
    free(sysroot_path_arg);
    free(entry_point_path);
    if (result == 0) {
        thread_allocate_ = qemu_android_lookup_symbol("nb_qemu_allocateThread");
        ALOGV("QemuAndroid::thread_allocate_: %p", (void *)thread_allocate_);
        thread_deallocate_ = qemu_android_lookup_symbol("nb_qemu_deallocateThread");
        ALOGV("QemuAndroid::thread_deallocate_: %p", (void *)thread_deallocate_);
    }
    return result;
}

intptr_t qemu_android_lookup_symbol(const char *name)
{
    if (syminfos) {
        struct syminfo *s = syminfos;

        while (s) {
            struct elf64_sym *syms = s->disas_symtab.elf64;

            for (int i = 0; i < s->disas_num_syms; i++) {
                if (strcmp(s->disas_strtab + syms[i].st_name, name) == 0) {
                    return syms[i].st_value;
                }
            }
            s = s->next;
        }
    }

    return 0;
}

intptr_t qemu_android_malloc(size_t size)
{
    intptr_t guest_addr;

    guest_addr = target_mmap(0, size + sizeof(size), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (guest_addr == -1)
      return 0;
    copy_to_user(guest_addr, &size, sizeof(size));
    return guest_addr + sizeof(size);
}

void qemu_android_free(intptr_t addr)
{
    size_t size;
    intptr_t guest_addr = addr - sizeof(size);

    if (copy_from_user(&size, guest_addr, sizeof(size)) == 0) {
        target_munmap(guest_addr, size);
    }
}

void qemu_android_memcpy(intptr_t dest, const void *src, size_t length)
{
    copy_to_user(dest, (void *)src, length);
}

const char *qemu_android_get_string(intptr_t addr)
{
    return (const char *)lock_user_string(addr);
}

void qemu_android_release_string(const char *s, intptr_t addr)
{
    unlock_user((void *)s, addr, 0);
}

void *qemu_android_get_memory(intptr_t addr, size_t length)
{
    return lock_user(VERIFY_READ, addr, length, 1);
}

void qemu_android_release_memory(void *ptr, intptr_t addr, size_t length)
{
    unlock_user(ptr, addr, length);
}

intptr_t qemu_android_h2g(void *addr)
{
    return h2g_nocheck(addr);
}

void *qemu_android_g2h(intptr_t addr)
{
    return g2h(addr);
}

void qemu_android_register_syscall_handler(qemu_android_syscall_handler_t func)
{
    syscall_handler = func;
}

void qemu_android_register_svc_handler(qemu_android_svc_handler_t func)
{
    svc_handler = func;
}

#ifdef __LP64__
#define REGS(x) env->xregs[x]
#define SP_REG REGS(31)
#define LR_REG REGS(30)
#define PC_REG env->pc
#else
#define REGS(r) env->regs[r]
#define SP_REG REGS(13)
#define LR_REG REGS(14)
#define PC_REG REGS(15)
#endif

static void qemu_android_call_internal(CPUState *cpu, intptr_t addr, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, char *stack, int stack_size)
{
    CPUArchState *env = cpu->env_ptr;

    intptr_t saved_lr = LR_REG;
    intptr_t saved_pc = PC_REG;
#ifndef __LP64__
    intptr_t saved_thumb = env->thumb;
#endif

#ifdef __LP64__
#define HEXFMT "0x%016w64x"
#else
#define HEXFMT "0x%08w32x"
#endif
    ALOGV("calling: "HEXFMT" ("HEXFMT", "HEXFMT", "HEXFMT", "HEXFMT", "HEXFMT", "HEXFMT", "HEXFMT", "HEXFMT", #%d)\n", addr, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, stack_size);


#ifdef LOG_NDEBUG
    if (stack) {
        ALOGV("  stack:");
        for (int i = 0; i < stack_size && i < 16; i += 4) ALOGV("    %08x\n", *(uint32_t*)&stack[i]);
    }
#endif

    PC_REG = addr & ~(target_ulong)1;
    LR_REG = info->start_code;
    REGS(0) = arg1;
    REGS(1) = arg2;
    REGS(2) = arg3;
    REGS(3) = arg4;
#ifdef __LP64__
    REGS(4) = arg5;
    REGS(5) = arg6;
    REGS(6) = arg7;
    REGS(7) = arg8;
#endif
    if (stack) {
        SP_REG -= stack_size;
        memcpy_to_target(SP_REG, stack, stack_size);
    }
#ifndef __LP64__
    env->thumb = addr & 1;
#endif
    cpu->exception_index = -1;
    cpu_loop(env);

    if (stack) {
        SP_REG += stack_size;
    }

    LR_REG = saved_lr;
    PC_REG = saved_pc;
#ifndef __LP64__
    env->thumb = saved_thumb;
#endif
    cpu->exception_index = -1;
}

intptr_t qemu_android_call9(void *_cpu, intptr_t addr, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, char *stack, int stack_size)
{
    CPUState *cpu = (CPUState *)_cpu;
    CPUArchState *env = cpu->env_ptr;
    qemu_android_call_internal(cpu, addr, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, stack, stack_size);
    return REGS(0);
}

uint64_t qemu_android_call9_ll(void *_cpu, intptr_t addr, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, char *stack, int stack_size)
{
#ifdef __LP64__
    return qemu_android_call9(_cpu, addr, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, stack, stack_size);
#else
    CPUState *cpu = (CPUState *)_cpu;
    CPUArchState *env = cpu->env_ptr;
    qemu_android_call_internal(cpu, addr, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, stack, stack_size);
    uint64_t ret = env->regs[1];
    ret <<= 32;
    ret |= env->regs[0];
    return ret;
#endif
}

void *qemu_android_get_cpu()
{
    return thread_cpu;
}

void *qemu_android_new_cpu()
{
    if (! thread_cpu && thread_allocate_) {
        CPUState *cpu;
        CPUArchState *env;
        struct target_pt_regs regs1;
        struct target_pt_regs *regs = &regs1;
        TaskState *ts;
        abi_long sp, stackp, tlsp, thrp, retp;
        struct {
            abi_long prev, next;
            pid_t tid, pid;
        } thr;
        abi_long *alloc_result;

        ALOGV("Creating new CPU");

        cpu = cpu_create(cpu_type);
        env = cpu->env_ptr;
        cpu_reset(cpu);

        ts = g_new0(TaskState, 1);
        init_task_state(ts);
        ts->info = info;
        ts->bprm = bprm;
        cpu->opaque = ts;
        task_settid(ts);

#define NEW_STACK_SIZE (16 * TARGET_PAGE_SIZE)
#define TLS_SIZE (sizeof(abi_long) * 8)

        sp = target_mmap(0, NEW_STACK_SIZE, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        LOG_ALWAYS_FATAL_IF(sp == -1, "Failed to allocate temporary");

        tlsp = sp + NEW_STACK_SIZE - TLS_SIZE;
        thrp = sp + NEW_STACK_SIZE - TARGET_PAGE_SIZE;
        retp = thrp - 2 * sizeof(abi_long);
        stackp = (retp & ~7);

        thr.tid = ts->ts_tid;
        thr.pid = getppid();
        memcpy_to_target(thrp, &thr, sizeof(thr));
        memcpy_to_target(tlsp + sizeof(abi_long), &thrp, sizeof(abi_long));

        rcu_register_thread();
        tcg_register_thread();

        cpu->random_seed = qemu_guest_random_seed_thread_part1();
        qemu_guest_random_seed_thread_part2(cpu->random_seed);

        do_init_thread(regs, info);
        target_cpu_copy_regs(env, regs);
        SP_REG = stackp;
        cpu_set_tls(env, tlsp);
        env->pc_stop = info->start_code;

        ALOGV("Allocating new CPU thread");
        //qemu_set_log(CPU_LOG_TB_IN_ASM|qemu_str_to_log_mask(QEMU_LOG_MASK));
        REGS(0) = retp;
        REGS(1) = retp + sizeof(abi_long);
/*      REGS(2) = 0;
        REGS(3) = 0;*/
        LR_REG = info->start_code;
        PC_REG = thread_allocate_ & ~(target_ulong)1;
#ifndef __LP64__
        env->thumb = thread_allocate_ & 1;
#endif
        thread_cpu = cpu;

        cpu_loop(env);
        //qemu_set_log(qemu_str_to_log_mask(QEMU_LOG_MASK));

        if (REGS(0) == 0) {
            alloc_result = (abi_long *) qemu_android_get_memory(retp, 2 * sizeof(abi_long));
            ALOGV("New CPU thread allocated: stack=%08x, tls=%08x", alloc_result[0], alloc_result[1]);
            SP_REG = alloc_result[0];
            cpu_set_tls(env, alloc_result[1]);
            qemu_android_release_memory(alloc_result, retp, 2 * sizeof(abi_long));
            target_munmap(sp, NEW_STACK_SIZE);
        }
        else {
            ALOGE("CPU thread allocation failed: %d", REGS(0));
        }

//        thread_cpu = cpu;

        ALOGV("New CPU created = %p", thread_cpu);
    }

    return thread_cpu;
}

void qemu_android_delete_cpu(void *_cpu)
{
    CPUState *cpu = (CPUState *)_cpu;
    CPUArchState *env = cpu->env_ptr;
    TaskState *ts;

    ALOGV("Deleting CPU = %p", cpu);

    if (thread_deallocate_) {
        abi_long tp, stackp, sizep;
        abi_long *dealloc_result;

        tp = target_mmap(0, sizeof(abi_long) * 2, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        LOG_ALWAYS_FATAL_IF(tp == -1, "Failed to allocate temporary stack");

        stackp = tp;
        sizep = tp + sizeof(abi_long);

        dealloc_result = (abi_long *) qemu_android_get_memory(tp, sizeof(abi_long) * 2);
        memset(dealloc_result, 0, sizeof(abi_long) * 2);
        qemu_android_release_memory(dealloc_result, tp, sizeof(abi_long) * 2);

        ALOGV("Deallocating CPU thread");
        //qemu_set_log(CPU_LOG_TB_IN_ASM|qemu_str_to_log_mask(QEMU_LOG_MASK));
        REGS(0) = stackp;
        REGS(1) = sizep;
/*      REGS(2) = 0;
        REGS(3) = 0;*/
        LR_REG = info->start_code;
        PC_REG = thread_deallocate_ & ~(target_ulong)1;
        env->thumb = thread_deallocate_ & 1;
        cpu_loop(env);
        //qemu_set_log(qemu_str_to_log_mask(QEMU_LOG_MASK));

        dealloc_result = (abi_long *) qemu_android_get_memory(tp, sizeof(abi_long) * 2);
        ALOGV("CPU thread deallocation result: stack=%08x, size=%08x", dealloc_result[0], dealloc_result[1]);
        target_munmap(dealloc_result[0], dealloc_result[1]);
        qemu_android_release_memory(dealloc_result, tp, sizeof(abi_long) * 2);
        target_munmap(tp, sizeof(abi_long) * 2);
    }

    cpu_list_lock();
    QTAILQ_REMOVE_RCU(&cpus, cpu, node);
    cpu_list_unlock();

    ts = cpu->opaque;
    thread_cpu = NULL;
    object_unref(OBJECT(cpu));
    g_free(ts);
    rcu_unregister_thread();

    ALOGV("CPU deleted = %p", cpu);
}

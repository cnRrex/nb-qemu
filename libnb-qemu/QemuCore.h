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

#ifndef QEMU_CORE_H_
#define QEMU_CORE_H_

#include "qemu_android_interface.h"
/*
 * QemuAndroid has move entirely to qemu and becoming a interface,
 * so QemuCore should handle the interface or be moved in QemuBridge
 * For future this should handle multi guest abi for qemu bridge
 */

const char* AppPackageName;

namespace QemuCore {

using syscall_handler_t = ::qemu_android_syscall_handler_t;
using svc_handler_t = ::qemu_android_svc_handler_t;

int initialize(const char *procname, const char *tmpdir);
inline intptr_t lookup_symbol(const char *name);

inline void register_syscall_handler(syscall_handler_t func);
inline void register_svc_handler(svc_handler_t func);
inline void* get_cpu(void);
inline void* new_cpu(void);
inline int delete_cpu(void *cpu);
inline void call(void *cpu, intptr_t addr, struct QemuAndroidCallData *data, struct QemuAndroidCallRet *ret);

}

#endif

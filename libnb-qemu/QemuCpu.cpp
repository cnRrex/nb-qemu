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

#include "QemuCore.h"
#include "QemuCpu.h"
#include "ArchSpecific.h"

thread_local QemuCpu QemuCpu::local_cpu_;

QemuCpu* QemuCpu::get()
{
    if (! local_cpu_.cpu_) {
        void *cpu = QemuCore::get_cpu();
        if (! cpu) {
            local_cpu_.cpu_ = QemuCore::new_cpu();
            local_cpu_.managed_ = true;
        }
        else {
            local_cpu_.cpu_ = cpu;
            local_cpu_.managed_ = false;
        }
    }
    return &local_cpu_;
}

QemuCpu::QemuCpu()
     : cpu_(nullptr), managed_(false)
{
}

QemuCpu::~QemuCpu()
{
    if (managed_) {
        QemuCore::delete_cpu(cpu_);
    }
}

void QemuCpu::call(intptr_t addr, struct QemuAndroidCallData *data, struct QemuAndroidCallRet *ret) const
{
    QemuCore::call(cpu_, addr, data, ret); //qemuAndroid call
}

//we dont support fastcall from now, the QemuBridge Itf will become fix tramp to use


intptr_t QemuCpu::call(intptr_t addr, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, char *stack, int stack_size) const
{
    return QemuCore::call9(cpu_, addr, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, stack, stack_size);
}

uint64_t QemuCpu::call64(intptr_t addr, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, char *stack, int stack_size) const
{
    return QemuCore::call9_ll(cpu_, addr, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, stack, stack_size);
}

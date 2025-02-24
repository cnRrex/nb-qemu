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

#ifndef QEMU_CORE_INCLUDE_H_
#define QEMU_CORE_INCLUDE_H_

//these function is expose for thunk lib for linking
extern "C"{
intptr_t nb_qemu_h2g(void *addr);
void* nb_qemu_g2h(intptr_t addr);
//void* nb_qemu_get_regs(void *env, unsigned int index);
//void* nb_qemu_get_sp_reg(void *env);
//void* nb_qemu_get_lr_reg(void *env);
//void* nb_qemu_get_pc_reg(void *env);

}

#endif

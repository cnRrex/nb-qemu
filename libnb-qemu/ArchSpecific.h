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

#ifndef ARCH_SPECIFIC_H_
#define ARCH_SPECIFIC_H_

#include <nativebridge/native_bridge.h>
#include <map>


/*
 * This header and files defines arch_specific variables and struct
 * When initialized then a struct should be point to the needed arch
 *
 */

//When initialize, store guest isa here.
const char* GuestIsa = NULL;

struct ArchContent{
    const char* qemu_arch; // use to construct qemu lib name, it just like x86->i386, arm64->aarch64
    bool target_64_bit;
    std::string path_supported_substring; // path substring to use for isPathSupported, use : to split
    android::NativeBridgeRuntimeValues *runtime_values; // point to the arch NativeBridgeRuntimeValues

}

// NativeBridgeRuntimeValues
// These will be used by getAppEnv
// pass in is android kRuntimeISA or API_STRING in native_bridge.cc
// like arm64 arm x86_64 x86 riscv64 mips64 mips, also the variable isa in initialize
// when initialize android pass in isa, after that SetupEnvironment will call getAppEnv

// arm64
const android::NativeBridgeRuntimeValues arm64_NativeBridgeRuntimeValues;

// arm
const android::NativeBridgeRuntimeValues arm_NativeBridgeRuntimeValues;

// x86_64
const android::NativeBridgeRuntimeValues x86_64_NativeBridgeRuntimeValues;

// x86
const android::NativeBridgeRuntimeValues x86_NativeBridgeRuntimeValues;

// riscv64
const android::NativeBridgeRuntimeValues riscv64_NativeBridgeRuntimeValues;

// mips64
const android::NativeBridgeRuntimeValues mips64_NativeBridgeRuntimeValues;

// mips
const android::NativeBridgeRuntimeValues mips_NativeBridgeRuntimeValues;


//c++11
//NOTE: first time check with find(), then it is safe to use it like array
//When we support multi-guest, this help us to get guest values in common code.
std::map<string, struct ArchContent> arch_values;


#endif



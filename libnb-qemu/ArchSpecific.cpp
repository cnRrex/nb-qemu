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


#include <nativebridge/native_bridge.h>
#include <map>
#include "ArchSpecific.h"

// NativeBridgeRuntimeValues
// These will be used by getAppEnv
// pass in is android kRuntimeISA or API_STRING in native_bridge.cc
// like arm64 arm x86_64 x86 riscv64 mips64 mips, also the variable isa in initialize
// when initialize android pass in isa, after that SetupEnvironment will call getAppEnv

// arm64
const android::NativeBridgeRuntimeValues arm64_NativeBridgeRuntimeValues = {
    .os_arch = "aarch64",
    .cpu_abi = "arm64-v8a",
    .cpu_abi2 = nullptr,
    .supported_abi = nullptr,
    .abi_count = 0,
}

// arm
const android::NativeBridgeRuntimeValues arm_NativeBridgeRuntimeValues = {
    .os_arch = "armv7l",
    .cpu_abi = "armeabi-v7a",
    .cpu_abi2 = "armeabi",
    .supported_abi = nullptr,
    .abi_count = 0,
}

// x86_64
const android::NativeBridgeRuntimeValues x86_64_NativeBridgeRuntimeValues = {
    .os_arch = "x86_64",
    .cpu_abi = "x86_64",
    .cpu_abi2 = nullptr,
    .supported_abi = nullptr,
    .abi_count = 0,
}

// x86
const android::NativeBridgeRuntimeValues x86_NativeBridgeRuntimeValues = {
    .os_arch = "i686",
    .cpu_abi = "x86",
    .cpu_abi2 = nullptr,
    .supported_abi = nullptr,
    .abi_count = 0,
}

// riscv64
const android::NativeBridgeRuntimeValues riscv64_NativeBridgeRuntimeValues = {
    .os_arch = "riscv64",
    .cpu_abi = "riscv64",
    .cpu_abi2 = nullptr,
    .supported_abi = nullptr,
    .abi_count = 0,
}

// mips64
const android::NativeBridgeRuntimeValues mips64_NativeBridgeRuntimeValues = {
    .os_arch = "mips64",
    .cpu_abi = "mips64",
    .cpu_abi2 = nullptr,
    .supported_abi = nullptr,
    .abi_count = 0,
}

// mips
const android::NativeBridgeRuntimeValues mips_NativeBridgeRuntimeValues = {
    .os_arch = "mips",
    .cpu_abi = "mips",
    .cpu_abi2 = nullptr,
    .supported_abi = nullptr,
    .abi_count = 0,
}


//c++11
//NOTE: first time check with find(), then it is safe to use it like array
//When we support multi-guest, this help us to get guest values in common code.
std::map<string, struct ArchContent> arch_values = {
//isa       qemu_arch   target_64_bit   path_supported_substring                                runtime_values
{"arm64",   {"aarch64", true,   "/lib/arm64:/lib/arm64-v8a:/system/fake-libs64",                arm64_NativeBridgeRuntimeValues}},
{"arm",     {"arm",     false,  "/lib/arm/:/lib/armeabi-v7a:/lib/armeabi:/system/fake-libs",    arm_NativeBridgeRuntimeValues}},
{"x86_64",  {"x86_64",  true,   "/lib/x86_64:/system/fake-libs64",                              x86_64_NativeBridgeRuntimeValues}},
{"x86",     {"i386",    false,  "/lib/x86:/system/fake-libs",                                   x86_NativeBridgeRuntimeValues}},
{"riscv64", {"riscv64", true,   "/lib/riscv64:/system/fake-libs64",                             riscv64_NativeBridgeRuntimeValues}},
{"mips64",  {"mips64",  true,   "/lib/mips64:/system/fake-libs64",                              mips64_NativeBridgeRuntimeValues}},
{"mips",    {"mips",    false,  "/lib/mips:/system/fake-libs",                                  mips_NativeBridgeRuntimeValues}},
};


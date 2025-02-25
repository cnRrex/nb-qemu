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

#ifndef QEMU_BRIDGE_H_
#define QEMU_BRIDGE_H_

#include <string>

namespace QemuBridge {
    bool initialize(void);
    bool is_supported(const std::string& libpath);
    bool is_path_supported(const std::string& path);
    void *load_library(const std::string& filename, void *ns = nullptr);
    void *get_trampoline(void *lib_handle, const std::string& name, const std::string& shorty);
    const char *get_error();
    bool init_anonymous_namespace(const char *public_ns_sonames, const char *anon_ns_library_path);
    void *create_namespace(const char *name,
                           const char *ld_library_path,
                           const char *default_library_path,
                           uint64_t type,
                           const char *permitted_when_isolated_path,
                           void *parent_ns);
    bool link_namespaces(void *from, void *to, const char *shared_libs_sonames);
}

#endif

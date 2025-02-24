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

#ifndef QEMU_MEMORY_H_
#define QEMU_MEMORY_H_

#include "QemuCore.h"

namespace QemuMemory {

inline intptr_t malloc(size_t size) { return QemuCore::malloc(size); }
inline void free(intptr_t addr) { QemuCore::free(addr); }
inline void memcpy(intptr_t dest, const void *src, size_t size) { QemuCore::memcpy(dest, src, size); }

class Malloc
{
public:
    Malloc(size_t size)
        : addr_(QemuCore::malloc(size)) {
    }
    ~Malloc() {
        if (addr_) {
            QemuCore::free(addr_);
        }
    }

    intptr_t get_address() const { return addr_; }
    operator bool() const { return addr_ != 0; }

    void memcpy(const void *src, size_t size) {
        QemuCore::memcpy(addr_, src, size);
    }

private:
    intptr_t addr_;
};

class String
{
public:
    String(intptr_t addr)
        : addr_(addr),
          s_(reinterpret_cast<const char *>(QemuCore::get_string(addr))) {
    }
    ~String() {
        if (s_) {
            QemuCore::release_string(s_, addr_);
        }
    }

    const char *c_str() const { return s_; }

private:
    intptr_t addr_;
    const char *s_;
};

template<class T>
class Region
{
public:
    Region(intptr_t addr, size_t num = 1)
        : addr_(addr),
          size_(sizeof(T) * num),
          p_(reinterpret_cast<T *>(QemuCore::get_memory(addr_, size_))) {
    }
    ~Region() {
        if (p_) {
            QemuCore::release_memory(p_, addr_, size_);
        }
    }

    T *get() const { return p_; }
    const T& operator[] (int index) const { return p_[index]; }

private:
    intptr_t addr_;
    size_t size_;
    T *p_;
};

}

#endif

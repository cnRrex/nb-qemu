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
#define LOG_NDEBUG 0

#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#ifdef __ANDROID__
#include <android-base/strings.h>
#endif
#include <log/log.h>
#include "Library.h"
#include "QemuBridge.h"
#include "QemuCore.h"
#include "QemuCpu.h"
#include "QemuMemory.h"
#include "Trampoline.h"
#include "ArchSpecific.h"

#ifdef __ANDROID__
using android::base::Split;
using android::base::EndsWith;
#endif

bool StartsWith(std::string str, std::string start)
{
    return !strncmp(str.c_str(), start.c_str(), start.length());
}


/* This Just like ... a class of g_ndkt_bridge*/
class QemuBridgeImpl
{
public:
    QemuBridgeImpl() {}
    ~QemuBridgeImpl() {}

public:
    bool initialize(void);
    bool is_path_supported(const std::string& path) const;
    void *load_library(const std::string& filename, void *ns);
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

private:
    intptr_t initialize_;
    intptr_t load_library_;
    intptr_t get_library_symbol_;
    intptr_t allocate_thread_;
    intptr_t get_error_;
    intptr_t init_anonymous_namespace_;
    intptr_t create_namespace_;
    intptr_t link_namespaces_;

    std::map<void *, std::shared_ptr<Library>> libraries_;
    std::map<std::string, std::shared_ptr<Library>> named_libraries_;

    std::mutex mutex_;
};
static std::shared_ptr<QemuBridgeImpl> impl_;

bool QemuBridgeImpl::initialize()
{
    //if (QemuCore::initialize(procname.c_str(), tmpdir.c_str())) {
        initialize_ = QemuCore::lookup_symbol("nb_qemu_initialize");
        ALOGV("QemuBridge::initialize_: %p", reinterpret_cast<void *>(initialize_));
        load_library_ = QemuCore::lookup_symbol("nb_qemu_loadLibrary");
        ALOGV("QemuBridge::load_library_: %p", reinterpret_cast<void *>(load_library_));
        get_library_symbol_ = QemuCore::lookup_symbol("nb_qemu_getLibrarySymbol");
        ALOGV("QemuBridge::get_library_symbol_: %p", reinterpret_cast<void *>(get_library_symbol_));
        allocate_thread_ = QemuCore::lookup_symbol("nb_qemu_allocateThread");
        ALOGV("QemuBridge::allocate_thread_: %p", reinterpret_cast<void *>(allocate_thread_));
        get_error_ = QemuCore::lookup_symbol("nb_qemu_getError");
        ALOGV("QemuBridge::get_error_: %p", reinterpret_cast<void *>(get_error_));
        create_namespace_ = QemuCore::lookup_symbol("nb_qemu_createNamespace");
        ALOGV("QemuBridge::create_namespace_: %p", reinterpret_cast<void *>(create_namespace_));
        link_namespaces_ = QemuCore::lookup_symbol("nb_qemu_linkNamespaces");
        ALOGV("QemuBridge::link_namespaces_: %p", reinterpret_cast<void *>(link_namespaces_));
        init_anonymous_namespace_ = QemuCore::lookup_symbol("nb_qemu_initAnonymousNamespace");
        ALOGV("QemuBridge::init_anonymous_namespace_: %p", reinterpret_cast<void *>(init_anonymous_namespace_));
        if (initialize_) {
            QemuCpu::get()->call(initialize_);
            return true;
        }
    //}
    return false;
}

bool QemuBridgeImpl::is_path_supported(const std::string& path) const
{
#ifdef __ANDROID__
    if (path.empty())
        return false;

    //TODO: arch related?
    //usually a path may contains ":", contains "bask.apk!"(package), or /lib/(arch)
    // /system/fake-libs64 /data/app/***/base.apk!/lib/arm64-v8a/

    for (auto& s : Split(path, ":")) {
        for (auto& support_path : Split(arch_values[GuestIsa].path_supported_substring, ":")){
            // this loop check every support_path, only when one test pass, it break, otherwise it go ahead.
            if(EndsWith(s, support_path))
                goto s_is_supported;
        }
        //when support_path check and reach here, meaning this s is not supported
        return false;
        s_is_supported:
        continue;
        // if (
        //     (s, "/arm") || EndsWith(s, "/armeabi-v7a") || EndsWith(s, "/fake-libs"))
        //     continue;
        // else
        //     return false;
    }

    return true;
#else
    /* we don't ever call this function in ATL anyway */
    return false;
#endif
}

void *QemuBridgeImpl::load_library(const std::string& filename, void *ns)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::string, std::shared_ptr<Library>>::const_iterator it = named_libraries_.find(filename);

    if (it != named_libraries_.end()) {
        return it->second->get_handle();
    }

    if (load_library_) {
        size_t length = filename.length();
        QemuMemory::Malloc p(length + 1);
        intptr_t ret = 0;

        if (p) {
            p.memcpy(filename.c_str(), length + 1);
            ret = QemuCpu::get()->call(load_library_, p.get_address(), (intptr_t) ns);
        }

        if (ret) {
            std::shared_ptr<Library> lib(new Library(filename, ret));

            libraries_[lib->get_handle()] = lib;
            named_libraries_[filename] = lib;
            ALOGI("Loaded library %s", filename.c_str());

            return lib->get_handle();
        }
    }

    return nullptr;
}

void *QemuBridgeImpl::get_trampoline(void *lib_handle, const std::string& name, const std::string& shorty)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<void *, std::shared_ptr<Library>>::const_iterator it = libraries_.find(lib_handle);

    if (it == libraries_.end()) {
        ALOGE("Invalid library handle: %p", lib_handle);
        return nullptr;
    }

    std::shared_ptr<Library> lib = it->second;
    std::shared_ptr<Trampoline> tramp = lib->get_trampoline(name);

    if (tramp) {
        return tramp->get_handle();
    }

    if (get_library_symbol_) {
        size_t length = name.length();
        QemuMemory::Malloc p(length + 1);
        intptr_t ret = 0;

        if (p) {
            p.memcpy(name.c_str(), length + 1);
            ret = QemuCpu::get()->call(get_library_symbol_, lib->get_address(), p.get_address());
        }

        if (ret) {
            if (StartsWith(name, "Java_"))
                tramp.reset(new JNITrampoline(name, ret, shorty));
            else if (name == "JNI_OnLoad" || name == "JNI_OnUnload")
                tramp.reset(new JNILoadTrampoline(name, ret));
            else
                tramp.reset(new NativeActivityTrampoline(name, ret));
            lib->add_trampoline(tramp);
            ALOGI("Loaded trampoline %s from %s", name.c_str(), lib->get_name().c_str());
            return tramp->get_handle();
        }
    }

    return nullptr;
}

const char *QemuBridgeImpl::get_error()
{
  static std::string s_error;

  if (get_error_) {
      QemuMemory::String q_error(QemuCpu::get()->call(get_error_));
      s_error = q_error.c_str();
  }

  return s_error.c_str();
}

bool QemuBridgeImpl::init_anonymous_namespace(const char *public_ns_sonames, const char *anon_ns_library_path)
{
  return QemuCpu::get()->call(init_anonymous_namespace_, (intptr_t) public_ns_sonames, (intptr_t) anon_ns_library_path);
}

void *QemuBridgeImpl::create_namespace(const char *name,
                                       const char *ld_library_path,
                                       const char *default_library_path,
                                       uint64_t type,
                                       const char *permitted_when_isolated_path,
                                       void *parent_ns)
{
  intptr_t ret;

#ifdef __LP64__
  ret = QemuCpu::get()->call(create_namespace_,
                             (intptr_t) name,
                             (intptr_t) ld_library_path,
                             (intptr_t) default_library_path,
                             (intptr_t) type,
                             (intptr_t) permitted_when_isolated_path,
                             (intptr_t) parent_ns,
                             0,
                             0,
                             nullptr,
                             0);
#else
  union {
      struct {
          uint64_t type;
          const char *permitted_when_isolated_path;
          void *parent_ns;
      } args;
      char data[16];
  } stack = { .args = { .type = type, .permitted_when_isolated_path = permitted_when_isolated_path, .parent_ns = parent_ns } };

  ret = QemuCpu::get()->call(create_namespace_,
                             (intptr_t) name,
                             (intptr_t) ld_library_path,
                             (intptr_t) default_library_path,
                             0,
                             0,
                             0,
                             0,
                             0,
                             stack.data,
                             sizeof(stack.data));
#endif
  return (void *) ret;
}

bool QemuBridgeImpl::link_namespaces(void *from, void *to, const char *shared_libs_sonames)
{
  return QemuCpu::get()->call(link_namespaces_, (intptr_t) from, (intptr_t) to, (intptr_t) shared_libs_sonames);
}



namespace QemuBridge {

//Usually only one place to initialze, so dont allocate it dynamically
// Or related to arch?

bool initialize()
{
    QemuBridgeImpl *impl = new QemuBridgeImpl();

    if (impl->initialize()) {
        impl_.reset(impl);
        return true;
    }
    else {
        delete impl;
        return false;
    }
}

bool is_supported(const std::string& /* libpath */)
{
    // Not implemented
    // usually, check if the environment have the permission to open and read/lseek the lib
    return true;
}

const struct NativeBridgeRuntimeValues *getAppEnv(const char *abi){
    if(!strcmp(abi, GuestIsa)){
        return arch_values[abi].runtime_values;
    }
    ALOGE("getAppEnv: unexpected isa %s, expect %s", abi, GuestIsa);
    return nullptr;
}

bool is_path_supported(const std::string& path)
{
    return impl_->is_path_supported(path);
}

void *load_library(const std::string& filename, void *ns)
{
    return impl_->load_library(filename, ns);
}

void *get_trampoline(void *lib_handle, const std::string& name, const std::string& shorty)
{
    return impl_->get_trampoline(lib_handle, name, shorty);
}

const char *get_error()
{
  return impl_->get_error();
}

bool init_anonymous_namespace(const char *public_ns_sonames, const char *anon_ns_library_path)
{
  return impl_->init_anonymous_namespace(public_ns_sonames, anon_ns_library_path);
}

void *create_namespace(const char *name,
                       const char *ld_library_path,
                       const char *default_library_path,
                       uint64_t type,
                       const char *permitted_when_isolated_path,
                       void *parent_ns)
{
  return impl_->create_namespace(name, ld_library_path, default_library_path, type, permitted_when_isolated_path, parent_ns);
}

bool link_namespaces(void *from, void *to, const char *shared_libs_sonames)
{
  return impl_->link_namespaces(from, to, shared_libs_sonames);
}

}//namespace QemuBridge

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

#include <stdlib.h>
#include <log/log.h>
#include <nativebridge/native_bridge.h>
#include "QemuBridge.h"
#include "JavaBridge.h"
#include "OsBridge.h"

namespace android {

const uint32_t nativeBridgeCallbackVersion = 5;

//native_bridge_namespace_t is a type for native_bridge, maintain by native_bridge, so we can define its struct, like guest or host android_namespace_t
//In v7 bridge we may need to getTrampoline for a functionPointer(registered via RegisterNatives), we need to judge if it is a guest function for us to exec

/*
 * Determine what abi we'll need to initialize
 */
static bool nb_qemu_initialize(const NativeBridgeRuntimeCallbacks* runtime_cbs, const char* private_dir, const char* instruction_set)
{
    ALOGI("initialize");
    if (!QemuCore::initialize(private_dir, instruction_set)) {
        QemuBridge::initialize();
        JavaBridge::initialize(runtime_cbs);
        OsBridge::initialize();
        return true;
    }
    return false;
}

static void *nb_qemu_loadLibrary(const char *libpath, int flag)
{
    ALOGI("loadLibrary: %s, %d", libpath, flag);
    return QemuBridge::load_library(libpath);
}

static void *nb_qemu_getTrampoline(void *handle, const char *name, const char* shorty, uint32_t len)
{
    ALOGI("getTrampoline: %s, %s, %d", name, shorty, len);
    return QemuBridge::get_trampoline(handle, name, shorty ? shorty : std::string());
}

static bool nb_qemu_isSupported(const char *libpath)
{
    ALOGI("isSupported: %s", libpath);
    return QemuBridge::is_supported(libpath);
}

static const struct NativeBridgeRuntimeValues *nb_qemu_getAppEnv(const char *abi)
{
    ALOGI("getAppEnv: %s", abi);
    return QemuBridge::getAppEnv(abi);
}

static bool nb_qemu_isCompatibleWith(uint32_t version)
{
    ALOGI("isCompatibleWith: %u", version);
    return version <= nativeBridgeCallbackVersion;
}

static NativeBridgeSignalHandlerFn nb_qemu_getSignalHandler(int signal)
{
    ALOGI("getSignalHandler: %d", signal);
    return nullptr;
}

static int nb_qemu_unloadLibrary(void *handle)
{
    ALOGI("unloadLibrary: %p", handle);
    return 0;
}

static const char *nb_qemu_getError()
{
    ALOGI("getError");
    return QemuBridge::get_error();
}

static bool nb_qemu_isPathSupported(const char *path)
{
    ALOGI("isPathSupported: %s", path);
    return QemuBridge::is_path_supported(path);
}

static bool nb_qemu_initAnonymousNamespace(const char *public_ns_sonames, const char *anon_ns_library_path)
{
    ALOGI("initAnonymousNamespace: %s, %s", public_ns_sonames, anon_ns_library_path);
    return QemuBridge::init_anonymous_namespace(public_ns_sonames, anon_ns_library_path);
}

static native_bridge_namespace_t *
nb_qemu_createNamespace(const char *name,
                        const char *ld_library_path,
                        const char *default_library_path,
                        uint64_t type,
                        const char *permitted_when_isolated_path,
                        native_bridge_namespace_t *parent_ns)
{
    ALOGI("createNamespace: %s, %s, %s, %llu, %s", name, ld_library_path, default_library_path, type, permitted_when_isolated_path);
    return (native_bridge_namespace_t *) QemuBridge::create_namespace(name, ld_library_path, default_library_path, type, permitted_when_isolated_path, parent_ns);
}

static bool nb_qemu_linkNamespaces(native_bridge_namespace_t *from, native_bridge_namespace_t *to, const char *shared_libs_soname)
{
    ALOGI("linkNamespaces: %p, %p, %s", from, to, shared_libs_soname);
    return QemuBridge::link_namespaces(from, to, shared_libs_soname);
}

static void *nb_qemu_loadLibraryExt(const char *libpath, int flag, native_bridge_namespace_t *ns)
{
    ALOGI("loadLibraryExt: %s, %d, %p", libpath, flag, ns);
    return QemuBridge::load_library(libpath, ns);
}

static native_bridge_namespace_t *nb_qemu_getVendorNamespace()
{
    ALOGI("getVendorNamespace");
    return nullptr;
}

static struct native_bridge_namespace_t* nb_qemu_getExportedNamespace(const char* name)
{
    ALOGI("getExportedNamespace: %s", name);
    return nullptr;
}

static void nb_qemu_preZygoteFork(void)
{
    //TODO: close guest memfd, log handle and other things
    ALOGI("preZygoteFork: %s");
    return nullptr;
}

extern "C" {

NativeBridgeCallbacks NativeBridgeItf = {
    // v1
    .version = nativeBridgeCallbackVersion,
    .initialize = nb_qemu_initialize,
    .loadLibrary = nb_qemu_loadLibrary,
    .getTrampoline = nb_qemu_getTrampoline,
    .isSupported = nb_qemu_isSupported,
    .getAppEnv = nb_qemu_getAppEnv,
    // v2
    .isCompatibleWith = nb_qemu_isCompatibleWith,
    .getSignalHandler = nb_qemu_getSignalHandler,
    // v3
    .unloadLibrary = nb_qemu_unloadLibrary,
    .getError = nb_qemu_getError,
    .isPathSupported = nb_qemu_isPathSupported,
//   removed in da53336c50994a20c292dd5fde0591ca53efea2a
//  .initAnonymousNamespace = nb_qemu_initAnonymousNamespace,
    .createNamespace = nb_qemu_createNamespace,
    .linkNamespaces = nb_qemu_linkNamespaces,
    .loadLibraryExt = nb_qemu_loadLibraryExt,
    // v4
    .getVendorNamespace = nb_qemu_getVendorNamespace,
    // v5
    .getExportedNamespace = nb_qemu_getExportedNamespace,
    // v6
    .preZygoteFork = nb_qemu_preZygoteFork;
    // v7
    //getTrampolineWithJNICallType: add jni_call_type
    //getTrampolineForFunctionPointer: no handle, no name, it pass in a JNI method
};

};

}

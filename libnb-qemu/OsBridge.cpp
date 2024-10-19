#define LOG_TAG "libnb-qemu"

#include <cstring>
#include <dlfcn.h>
#include <log/log.h>
#include <mutex>
#include <string>
#include <nativebridge/native_bridge.h>
#include "QemuCore.h"
#include "OsBridge.h"

extern struct NativeBridgeCallbacks NativeBridgeItf;

enum {
    HANDLER_EGL,
    HANDLER_GLESV1_CM,
    HANDLER_GLESV3,
    HANDLER_OPENSLES,
    HANDLER_ANDROID,
    HANDLER_MAX,
};

struct HandlerInfo {
    int start;
    int length;
    const char *lib;
    QemuCore::svc_handler_t handler;
};

static std::string lib_dir;
static std::mutex g_mutex_;
/* using clean offsets is cool and all, but it would be much better for performance to just have one consecutive list that we can index into */
static struct HandlerInfo g_handlers_[HANDLER_MAX] = {
    [HANDLER_EGL] = { 0x0100, 0x100, "libnb-qemu-EGL.so", nullptr },
    [HANDLER_GLESV1_CM] = { 0x0400, 0x200, "libnb-qemu-GLESv1_CM.so", nullptr },
    [HANDLER_GLESV3] = { 0x1000, 0x200, "libnb-qemu-GLESv2.so", nullptr },
    [HANDLER_OPENSLES] = { 0x0600, 0x100, "libnb-qemu-OpenSLES.so", nullptr },
    [HANDLER_ANDROID] = { 0x0700, 0x100, "libnb-qemu-android.so", nullptr }
};

/* FIXME: using a loop is unnecessarily slow, just use consecutive numbers for the "svc"s and index an array with them */
static QemuCore::svc_handler_t get_handler_for_svc(int num) {
    for (int i = 0; i < HANDLER_MAX; i++) {
        int offset = num - g_handlers_[i].start;
        if (offset >= 0 && offset < g_handlers_[i].length) {
            if (g_handlers_[i].handler == nullptr) {
                std::string thunk_lib_path = lib_dir + "/libnb-qemu-thunks/" + g_handlers_[i].lib;
                void *lib_handle = dlopen(thunk_lib_path.c_str(), RTLD_LAZY);
                if (lib_handle) {
                    g_handlers_[i].handler = reinterpret_cast<QemuCore::svc_handler_t>(dlsym(lib_handle, "nb_handle_svc"));
                } else {
                    ALOGE("error loading SVC handler thunk library: %s", dlerror());
		}
                LOG_ALWAYS_FATAL_IF(g_handlers_[i].handler == nullptr,
                                    "Unable to load SVC handler from %s", g_handlers_[i].lib);
            }
            return g_handlers_[i].handler;
        }
    }
    LOG_ALWAYS_FATAL("Unknown SVC 0x%08x", num);
}

static void handle_svc(void *cpu_env, int num) {
    get_handler_for_svc(num)(cpu_env, num);
}

namespace OsBridge {

bool initialize() {
    Dl_info libnbqemu_so_dl_info;
    // a symbol that this library exports
    dladdr(&NativeBridgeItf, &libnbqemu_so_dl_info);
    // make sure we didn't get NULL
    if (libnbqemu_so_dl_info.dli_fname) {
        // it's simpler if we can modify the string, so strdup it
        char *libnbqemu_so_full_path = strdup(libnbqemu_so_dl_info.dli_fname);
        *strrchr(libnbqemu_so_full_path, '/') = '\0'; // now we should have something like /usr/lib64/libnb-qemu
	lib_dir = libnbqemu_so_full_path;
	free(libnbqemu_so_full_path);
    } else {
        ALOGE("Couldn't get thunk dir with dladdr");
        lib_dir = "";
    }

    QemuCore::register_svc_handler(handle_svc);
    return true;
}

}

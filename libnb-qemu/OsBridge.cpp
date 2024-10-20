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

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define BASE_ADDRESS_SHIFT 11

/* !! keep these synced with the %base directive in the .def files */
enum {
    HANDLER_ANDROID = 0,
    HANDLER_EGL = 1,
    HANDLER_GLESV1_CM = 2,
    HANDLER_GLESV3 = 3,
    HANDLER_OPENSLES = 4,
};

struct HandlerInfo {
    QemuCore::svc_handler_t handler;
    const char *lib;
};

static std::mutex g_mutex_;
static struct HandlerInfo g_handlers_[] = {
    [HANDLER_ANDROID] = { nullptr, "libnb-qemu-android.so" },
    [HANDLER_EGL] = { nullptr, "libnb-qemu-EGL.so" },
    [HANDLER_GLESV1_CM] = { nullptr, "libnb-qemu-GLESv1_CM.so" },
    [HANDLER_GLESV3] = { nullptr, "libnb-qemu-GLESv2.so" },
    [HANDLER_OPENSLES] = { nullptr, "libnb-qemu-OpenSLES.so" },
};

static QemuCore::svc_handler_t get_handler_for_svc(uint16_t num) {
    int base = num >> BASE_ADDRESS_SHIFT;
    if(base > (ARRAY_SIZE(g_handlers_) - 1)) // it would be faster (but less safe) to omit this check
        LOG_ALWAYS_FATAL("Unknown SVC 0x%08x (unknown base: %x)", num, base);
    return g_handlers_[base].handler;
}

static void handle_svc(void *cpu_env, uint16_t num) {
    get_handler_for_svc(num)(cpu_env, num);
}

namespace OsBridge {

bool initialize() {
    std::string lib_dir;
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

    for (int i = 0; i < ARRAY_SIZE(g_handlers_); i++) {
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

    QemuCore::register_svc_handler(handle_svc);
    return true;
}

}

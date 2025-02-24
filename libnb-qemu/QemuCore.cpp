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

#include <map>
#include <dlfcn.h>
#include <log/log.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include "qemu_android_interface.h"
#include "ArchSpecific.h"
#include "QemuCore.h"
#include "QemuCoreInclude.h"
/*
 * QemuAndroid has move entirely to qemu and becoming a interface,
 * so QemuCore should handle the interface or be moved in QemuBridge
 * For future this should handle multi guest abi for qemu bridge
 */


namespace QemuCore {

void *libqemu_handle = nullptr;
const QemuAndroidCallbacks* qemu_callbacks = nullptr;

//inline bool initialize(const char *procname, const char *tmpdir) { return qemu_android_initialize(procname, tmpdir) == 0; }

static std::string find_app_name_from_tmpdir(const char* tmpdir){
    std::string pdir = tmpdir;
    //private_dir is /data/data/app_name/code_cache
    size_t lastSplashPos = pdir.find_last_of('/');
    if(lastSplashPos == 0)
        return "";
    size_t second_lastSplashPos = pdir.find_last_of('/', lastSplashPos-1);
    if(second_lastSplashPos == 0)
        return "";
    std::string app_name = pdir.substr(second_lastSplashPos+1, lastSplashPos-second_lastSplashPos-1);
    return app_name;
}

//QemuCore handle the QemuAndroidItf
//TODO: dlclose qemu
int initialize( const char* tmpdir, const char* isa){
    std::string qemu_lib_name;
    std::string app_name;
    //handle isa
    //usually a process only need one isa, as the 32bit is going to be deprecated, this can be unchanged.
    //First we check if we have loaded
    if (!GuestIsa){
        if(!strcmp(isa, GuestIsa)){
            // unexpected situation: we have loaded, but bridge need new isa
            ALOGE("Unexpected isa %s is needed but nb-qemu has initialized isa %s", isa, GuestIsa);
            return -1;
        }else{
            //has initialize the same isa
            ALOGI("QemuCore has already initialized");
            return 0;
        }
    }
    if(arch_values.find(isa)){
        qemu_lib_name = "libqemu-" + arch_values[isa].qemu_arch + ".so";
        void* handle = dlopen(qemu_lib_name.c_str(), RTLD_LAZY);
        if(handle != nullptr){
            //success open libqemu
            qemu_callbacks = reinterpret_cast<QemuAndroidCallbacks*>(dlsym(handle, "QemuAndroidItf"));
            if(qemu_callbacks != nullptr){
                //success
                libqemu_handle = handle;

            }else{
                //no symbol
                qemu_callbacks = nullptr;
                dlclose(handle);
            }
        }
    }else{
        //no such isa
        return -1;
    }

    if(qemu_callbacks != nullptr){
        //prepare and call QemuAndroid initialize
        //procname may not correct, get it from private_dir
        //entry_name is needed
        //use envp to pass extra arg
        app_name = find_app_name_from_tmpdir(tmpdir);
        if(app_name == ""){
            ALOGW("App name is empty from private_dir %s", tmpdir);
            AppPackageName = "qemu_unknown.app.package";
        }else{
            app_name = "qemu_" + app_name;
            AppPackageName = app_name.c_str();
        }
        //where to find nb-qemu-guest?
        //TODO: use dl_info to find it
        std::string is_64 = (arch_values[isa].target_64_bit ? "64" : "");
        std::string try_path = "/system/bin/gnemul/" + arch_values[isa].qemu_arch + "/nb-qemu-guest";
        if(access(try_path.c_str()) != 0){
            try_path = "/system/lib" + is_64 + "/" + isa + "/libnb-qemu-guest.so";
            if(access(try_path.c_str()) != 0){
                try_path = "/system/lib" + is_64 + "/gnemul/" + isa + "/libnb-qemu-guest.so";
                if(access(try_path.c_str()) != 0){
                    const char *sysroot_path = getenv("NB_QEMU_SYSROOT");
                    if(!sysroot_path)
                        LOG_ALWAYS_FATAL("%s: NB_QEMU_SYSROOT is not set", __func__);
                    try_path = sysroot_path + "/libnb-qemu-guest.so";
                    if(access(try_path.c_str()) != 0){
                        LOG_ALWAYS_FATAL("nb-qemu-guest is not found");
                    }
                }
            }
        }
        ALOGI("Found %s", try_path.c_str());
        const char* qemu_envp[] = {"QEMU_NB_DEBUG=true", "QEMU_NO_P_FLAG=true", "QEMU_LOG=unimp", NULL};
        return qemu_callbacks->initialize(AppPackageName, tmpdir, qemu_envp, try_path.c_str());
    }else{
        return -1;
    }
}
/*
struct QemuAndroidCallbacks {
    int (*initialize)(const char *procname, const char *tmpdir,
                            const char **qemu_envp, const char *guest_entry);
    intptr_t (*lookup_symbol)(const char *name);
    intptr_t (*malloc)(size_t size);
    void (*free)(intptr_t addr);
    void (*memcpy)(intptr_t dest, const void *src, size_t length);
    const char* (*get_string)(intptr_t addr);
    void (*release_string)(const char *s, intptr_t addr);
    void* (*get_memory)(intptr_t addr, size_t length);
    void (*release_memory)(void *ptr, intptr_t addr, size_t length);
    intptr_t (*h2g)(void *addr);
    void* (*g2h)(intptr_t addr);
    void (*register_syscall_handler)(qemu_android_syscall_handler_t func);
    void (*register_svc_handler)(qemu_android_svc_handler_t func);
    //void* (*get_regs)(void *env, unsigned int index);
    //void* (*get_sp_reg)(void *env);
    //void* (*get_lr_reg)(void *env);
    //void* (*get_pc_reg)(void *env);
    void* (*get_cpu)(void);
    void* (*new_cpu)(void);
    int (*delete_cpu)(void *cpu);
    void (*call)(void *cpu,
                 intptr_t addr,
                 struct QemuAndroidCallData *data,
                 struct QemuAndroidCallRet *ret);
};

 */

inline intptr_t lookup_symbol(const char *name) { return qemu_callbacks->lookup_symbol(name); }
inline intptr_t malloc(size_t size) {return qemu_callbacks->malloc(name); }
inline void free(intptr_t addr) { qemu_callbacks->free(name); }
inline void memcpy(intptr_t dest, const void *src, size_t length) { qemu_callbacks->memcpy(dest, src, length); }
inline const char* get_string(intptr_t addr) { return qemu_callbacks->get_string(addr); }
inline void release_string(const char *s, intptr_t addr) { qemu_callbacks->release_string(s, addr); }
inline void* get_memory(intptr_t addr, size_t length) { return qemu_callbacks->get_memory(addr, length); }
inline void release_memory(void *ptr, intptr_t addr, size_t length) { qemu_callbacks->release_memory(ptr, addr, length); }
inline intptr_t h2g(void *addr) { return qemu_callbacks->h2g(addr); }
inline void* g2h(intptr_t addr) { return qemu_callbacks->g2h(addr); }
inline void register_syscall_handler(syscall_handler_t func) { qemu_callbacks->register_syscall_handler(func); }
inline void register_svc_handler(svc_handler_t func) { qemu_callbacks->register_svc_handler(func); }
inline void* get_regs(void *env, unsigned int index) { return qemu_callbacks->get_regs(env, index); }
inline void* get_sp_reg(void *env) { return qemu_callbacks->get_sp_reg(env); }
inline void* get_lr_reg(void *env) { return qemu_callbacks->get_lr_reg(env); }
inline void* get_pc_reg(void *env) { return qemu_callbacks->get_pc_reg(env); }
inline void* get_cpu(void){ return qemu_callbacks->get_cpu(); }
inline void* new_cpu(void){ return qemu_callbacks->new_cpu(); }
inline int delete_cpu(void *cpu){ return qemu_callbacks->delete_cpu(cpu); }
//TODO: in different arch usable regs differ, these may need better optimization
inline void call(void *cpu, intptr_t addr, struct QemuAndroidCallData *data, struct QemuAndroidCallRet *ret){
    qemu_callbacks->call(cpu, addr, data, ret);
}

}//namespace QemuCore
/*
extern "C"{
intptr_t nb_qemu_h2g(void *addr) { return QemuCore::h2g(addr); }
void* nb_qemu_g2h(intptr_t addr) { return QemuCore::g2h(addr); }
void* nb_qemu_get_regs(void *env, unsigned int index) { return QemuCore::get_regs(env, index);  }
void* nb_qemu_get_sp_reg(void *env) { return QemuCore::get_sp_reg(env); }
void* nb_qemu_get_lr_reg(void *env) { return QemuCore::get_lr_reg(env); }
void* nb_qemu_get_pc_reg(void *env) { return QemuCore::get_pc_reg(env); }

//thunk lib need access env register, this is because we pass the handler to it
//as a result, different arch need different libqemu to link, this is awful, why not expose it with qemu_android_interface?
}
*/

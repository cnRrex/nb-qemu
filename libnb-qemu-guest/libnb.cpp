/*
 * nb-qemu-guest
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

/* Will build this to an executable, add an entry to it */
/* NB-GUEST should be build with guest_runtime for specific android version */
/* But we also think about making it easier to build without aosp */
/* Qemu log_redirector allow stdout/stderr as ALOGD in debug, or stderr as ALOGE */

#define LOG_TAG "nb-qemu-guest"
//#define LOG_NDEBUG 0

#include <dlfcn.h>
#include <android/dlext.h>
#include <string.h>
//#include <log/log.h>
//#include <utils/CallStack.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static char nb_qemu_error[4096] = { '\0' };

extern "C" {
//int __pthread_allocate_self(void **stack, void **tls);
//void __pthread_deallocate_self(void **stack, size_t *size);
bool android_init_anonymous_namespace(const char* shared_libs_sonames,
                                      const char* library_search_path);
struct android_namespace_t* android_create_namespace(const char *name,
                                                     const char *ld_library_path,
                                                     const char *default_library_path,
                                                     uint64_t type,
                                                     const char *permitted_when_isolated_path,
                                                     struct android_namespace_t *parent_ns);
bool android_link_namespaces(android_namespace_t* from,
                             android_namespace_t* to,
                             const char* shared_libs_sonames);
}

extern "C" {

static void* do_nothing(void*){
    return NULL;
}

int nb_qemu_allocateThread(void) {
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    return pthread_create(&thread, &attr, do_nothing, NULL);
}

void nb_qemu_deallocateThread(void) {
    pthread_exit();
    //fake exit, so it continue
    return;
}

void *nb_qemu_malloc(size_t size) {
    return malloc(size);
}

void nb_qemu_free(void* ptr) {
    free(ptr);
}

void nb_qemu_initialize() {
    printf("nb-qemu-guest: initialize\n");
}

//TODO: RTLD_LAZY should also an option for nb-qemu
void *nb_qemu_loadLibrary(const char *filename, void *ns) {
    printf("nb-qemu-guest: loadLibrary: %s, ns=%p\n", filename, ns);
    //TODO: support both android_dlopen_ext and dlopen
//    android_dlextinfo info = { .flags = ANDROID_DLEXT_USE_NAMESPACE, .library_namespace = (struct android_namespace_t *) ns };
//    void *ret = android_dlopen_ext(filename, RTLD_LAZY, &info);
    void *ret = dlopen(filename, RTLD_LAZY);
    if (!ret) {
        nb_qemu_error[0] = '\0';
        strncat(nb_qemu_error, dlerror(), sizeof(nb_qemu_error) - 1);
    }
    printf("nb-qemu-guest: Library loaded => %p\n", ret);

    return ret;
}

//TODO: unloadLibrary

const char *nb_qemu_getError() {
//    ALOGV("getError: %s", nb_qemu_error);
    return nb_qemu_error;
}

void *nb_qemu_getLibrarySymbol(void *handle, const char *name) {
    printf("nb-qemu-guest: getLibrarySymbol: %p, %s\n", handle, name);
    void *ret = dlsym(handle, name);
    if (!ret) {
        nb_qemu_error[0] = '\0';
        strncat(nb_qemu_error, dlerror(), sizeof(nb_qemu_error) - 1);
    }
    printf("nb-qemu-guest: getLibrarySymbol: => %p\n", ret);
    return ret;
}

//TODO: __NR_process_vm_readv is not implement in qemu
//void nb_qemu_printCallStack() {
//    android::CallStack(LOG_TAG);
//}

//v3 interface
bool nb_qemu_initAnonymousNamespace(const char *public_ns_sonames, const char *anon_ns_library_path) {
    printf("nb-qemu-guest: initAnonymousNamespace: sonames=%s, library_path=%s\n", public_ns_sonames, anon_ns_library_path);
    return android_init_anonymous_namespace(public_ns_sonames, anon_ns_library_path);
}

struct android_namespace_t *nb_qemu_createNamespace(const char *name,
                                                    const char *ld_library_path,
                                                    const char *default_library_path,
                                                    uint64_t type,
                                                    const char *permitted_when_isolated_path,
                                                    android_namespace_t *parent_ns) {
    printf("nb-qemu-guest: createNamespace:\n  name=%s\n  ld_library_path=%s\n  default_library_path=%s\n  type=%llu\n  perm_isolated=%s\n  parent=%p\n",
          name, ld_library_path, default_library_path, type, permitted_when_isolated_path, parent_ns);
    return android_create_namespace(name, ld_library_path, default_library_path, type, permitted_when_isolated_path, parent_ns);
}

bool nb_qemu_linkNamespaces(android_namespace_t *from, android_namespace_t *to, const char *shared_libs_sonames) {
    printf("nb-qemu-guest: linkNamespaces: from=%p, to=%p, libs=%s\n", from, to, shared_libs_sonames);
    return android_link_namespaces(from, to, shared_libs_sonames);
}

//exported_namespace introduced in android 8.0, and android 9.0 v4 bridge
android_namespace_t nb_qemu_getVendorNamespace(){
    printf("nb-qemu-guest: getVendorNamespace\n");
    return android_get_exported_namespace(kVendorNamespaceName);
}

//v5 bridge
android_namespace_t nb_qemu_getExportedNamespace(const char *name){

    return android_get_exported_namespace(name);
}
//v6 bridge
//preZygoteFork

};

//entry
int main(){
    fprintf(stderr, "nb-qemu-guest: Shall not run this entry!\n");
    return 0;
}

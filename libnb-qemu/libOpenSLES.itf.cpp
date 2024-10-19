#define LOG_TAG "OpenSLES-nb"

#include <dlfcn.h>
#include <log/log.h>
#include <map>
#include <memory>
#include "libOpenSLES.itf.h"
#include "Trampoline.h"

static std::map<void*, std::shared_ptr<Trampoline>> sCallbacks;

static void* get_trampoline(void *callback, const char *name, const char *sig)
{
  std::map<void*, std::shared_ptr<Trampoline>>::const_iterator it = sCallbacks.find(callback);
  std::shared_ptr<Trampoline> nativeCallback;

  if (it != sCallbacks.end())
    nativeCallback = it->second;
  else {
      nativeCallback.reset(new Trampoline(name, (uintptr_t) callback, sig));
      sCallbacks[callback] = nativeCallback;
  }

  return nativeCallback->get_handle();
}

extern "C" {

SLresult SLBufferQueueItf_RegisterCallback (SLBufferQueueItf self, void *callback, void *pContext)
{
  slBufferQueueCallback nativeCallback = (slBufferQueueCallback) get_trampoline(callback, "SLBufferQueue_Callback", "vpp");
  return (*self)->RegisterCallback(self, nativeCallback, pContext);
}

SLresult SLObjectItf_RegisterCallback (SLObjectItf self, void *callback, void *pContext)
{
  slObjectCallback nativeCallback = (slObjectCallback) get_trampoline(callback, "SLObjectItf_Callback", "vppIIIp");
  return (*self)->RegisterCallback(self, nativeCallback, pContext);
}

SLresult SLPlayItf_RegisterCallback (SLPlayItf self, void *callback, void *pContext)
{
  slPlayCallback nativeCallback = (slPlayCallback) get_trampoline(callback, "SLPlayItf_Callback", "vppI");
  return (*self)->RegisterCallback(self, nativeCallback, pContext);
}

SLresult SLAndroidSimpleBufferQueueItf_RegisterCallback (SLAndroidSimpleBufferQueueItf self, void *callback, void *pContext)
{
  slAndroidSimpleBufferQueueCallback nativeCallback = (slAndroidSimpleBufferQueueCallback) get_trampoline(callback, "SLAndroidSimpleBufferQueue_Callback", "vpp");
  return (*self)->RegisterCallback(self, nativeCallback, pContext);
}

intptr_t qemu_android_h2g(void *addr);

void *nb_iid_dlsym (char *sym) {
    static void *libOpenSLES = NULL;
    if (!libOpenSLES) {
        libOpenSLES = dlopen("libOpenSLES.so.1", RTLD_LAZY);
        if (!libOpenSLES) {
            ALOGE("unable to open libOpenSLES.so.1: %s", dlerror());
            return NULL;
        } else {
            ALOGI("initializing OpenSLES IID list");
        }
    }
    void *ret = dlsym(libOpenSLES, sym);
    ALOGE("unable to find %s: %s", sym, dlerror());
    return (void *)qemu_android_h2g(ret);
}

};

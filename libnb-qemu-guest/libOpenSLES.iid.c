#include <dlfcn.h>
#include <stdio.h>

void *__nb_nb_iid_dlsym(char *sym);

#define DEFINE_IID(iid) \
const void *iid __attribute__((visibility("default"))) = NULL;
#include "libOpenSLES.iid.h"
#undef DEFINE_IID

static void *libOpenSLES;

__attribute__((constructor))
static void initialize_IID()
{
    void *sym;
#define DEFINE_IID(iid) \
    sym = __nb_nb_iid_dlsym(#iid); \
    if (sym) \
        iid = *(void **) sym;

#include "libOpenSLES.iid.h"
#undef DEFINE_IID

}

__attribute__((destructor))
static void uninitialize_IID()
{
    if (libOpenSLES) {
        dlclose(libOpenSLES);
        libOpenSLES = NULL;
#define DEFINE_IID(iid) \
        iid = NULL;
#include "libOpenSLES.iid.h"
#undef DEFINE_IID
    }
}

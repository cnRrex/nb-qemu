#define LOG_TAG "libEGL-nb"
//#define LOG_NDEBUG 0

#include <string.h>
//#include <log/log.h>
//#include <GLES/gl.h>
//#include <GLES/glext.h>

void *eglGetProcAddress(const char *procname);
void *libEGL_eglGetProcAddress(const char *procname);
__attribute__((naked,noinline)) void libEGL_glDiscardFramebufferEXT();
__attribute__((naked,noinline)) void libEGL_glDebugMessageControlKHR();
__attribute__((naked,noinline)) void libEGL_glDebugMessageInsertKHR();
__attribute__((naked,noinline)) void libEGL_glPushDebugGroupKHR();
__attribute__((naked,noinline)) void libEGL_glPopDebugGroupKHR();
__attribute__((naked,noinline)) void libEGL_glObjectLabelKHR();
__attribute__((naked,noinline)) void libEGL_glGetObjectLabelKHR();
__attribute__((naked,noinline)) void libEGL_glInsertEventMarkerEXT();
__attribute__((naked,noinline)) void libEGL_glPushGroupMarkerEXT();
__attribute__((naked,noinline)) void libEGL_glPopGroupMarkerEXT();
__attribute__((naked,noinline)) void libEGL_glGetQueryObjectui64vEXT();
__attribute__((naked,noinline)) void libEGL_glCopyImageSubDataOES();
__attribute__((naked,noinline)) void libEGL_glDrawElementsBaseVertexOES();
__attribute__((naked,noinline)) void libEGL_glDrawRangeElementsBaseVertexOES();
__attribute__((naked,noinline)) void libEGL_glDrawElementsInstancedBaseVertexOES();
__attribute__((naked,noinline)) void libEGL_glMultiDrawElementsBaseVertexEXT();
__attribute__((naked,noinline)) void libEGL_glBlendBarrierKHR();


void *eglGetProcAddress(const char *procname) {
    void *func_HOSTPTR = libEGL_eglGetProcAddress(procname);
    void *func;
    if (!func_HOSTPTR) {
        return NULL;
    }
#define DEFINE_EXT(name) \
    else if (strcmp(procname, #name) == 0) { \
        func = (void *) libEGL_ ## name; \
    }
DEFINE_EXT(glDiscardFramebufferEXT)
DEFINE_EXT(glDebugMessageControlKHR)
DEFINE_EXT(glDebugMessageInsertKHR)
DEFINE_EXT(glPushDebugGroupKHR)
DEFINE_EXT(glPopDebugGroupKHR)
DEFINE_EXT(glObjectLabelKHR)
DEFINE_EXT(glGetObjectLabelKHR)
DEFINE_EXT(glInsertEventMarkerEXT)
DEFINE_EXT(glPushGroupMarkerEXT)
DEFINE_EXT(glPopGroupMarkerEXT)
DEFINE_EXT(glGetQueryObjectui64vEXT)
DEFINE_EXT(glCopyImageSubDataOES)
DEFINE_EXT(glDrawElementsBaseVertexOES)
DEFINE_EXT(glDrawRangeElementsBaseVertexOES)
DEFINE_EXT(glDrawElementsInstancedBaseVertexOES)
DEFINE_EXT(glMultiDrawElementsBaseVertexEXT)
DEFINE_EXT(glBlendBarrierKHR)
#undef DEFINE_EXT
    else {
        func = NULL;
    }
//    ALOGD("eglGetProcAddress(%s) => %p", procname, func);
    return func;
}

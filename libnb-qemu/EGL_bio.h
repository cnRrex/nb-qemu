#ifdef __ANDROID__
/* AOSP or target */
#define bionic_eglGetProcAddress eglGetProcAddress
#define bionic_eglGetDisplay eglGetDisplay
#define bionic_eglChooseConfig eglChooseConfig
#define bionic_eglCreatePbufferSurface eglCreatePbufferSurface
#define bionic_eglCreateWindowSurface eglCreateWindowSurface
#else
/* ATL */
void (* bionic_eglGetProcAddress(char const *procname))(void);
EGLDisplay bionic_eglGetDisplay(NativeDisplayType native_display);
EGLBoolean bionic_eglChooseConfig(EGLDisplay display, EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
EGLSurface bionic_eglCreatePbufferSurface(EGLDisplay display, EGLConfig config, EGLint const *attrib_list);
EGLSurface bionic_eglCreateWindowSurface(EGLDisplay display, EGLConfig config, void *native_window, EGLint const *attrib_list);
#endif

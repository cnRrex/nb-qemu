#!/usr/bin/env python3

import argparse
import re
import sys

parser = argparse.ArgumentParser()
parser.add_argument('file', nargs=1)
"""
    [HANDLER_EGL] = { 0x0100, 99, "libnb-qemu-EGL.so", nullptr },
    [HANDLER_GLESV1_CM] = { 0x0400, 257, "libnb-qemu-GLESv1_CM.so", nullptr },
    [HANDLER_GLESV3] = { 0x1000, 399, "libnb-qemu-GLESv3.so", nullptr },
    [HANDLER_OPENSLES] = { 0x0600, 62, "libnb-qemu-OpenSLES.so", nullptr },
    [HANDLER_ANDROID] = { 0x0700, 74, "libnb-qemu-android.so", nullptr }
"""
parser.add_argument('-b', metavar='base', type=lambda x: int(x, 0), default=0x0100)
parser.add_argument('-g', action='store_true')
parser.add_argument('-d', action='store_true')
parser.add_argument('-i', metavar='include', action='append', default=[])
parser.add_argument('-a', metavar='include_after', action='append', default=[])
parser.add_argument('-p', metavar='prefix')
parsed_args = parser.parse_args()

# FIXME have this as an argument
is64bit = True
num_args_in_regs = 8 if is64bit else 4
# first 5 MSBs differentiate among up to 32 thunked libraries,
# the other 11 bits differentiate between up to 2048 functions in each library
BASE_ADDRESS_SHIFT = 11

base = (parsed_args.b << BASE_ADDRESS_SHIFT)
guest = parsed_args.g
debug = parsed_args.d
includes = parsed_args.i
includes_after = parsed_args.a
prefix = parsed_args.p or ''
functions = []
definitions = set()

with open(parsed_args.file[0]) as f:
    for idx, line in enumerate(f):
        line = line.strip()
        if line.startswith('#'):
            continue
        if line.startswith('%'):
            cmdmode = None
            cmd, cmdarg = line[1:].split(maxsplit=1)
            if ':' in cmd:
                cmdmode, cmd = cmd.split(':', maxsplit=1)
                if cmdmode not in ['host', 'guest']:
                    raise ValueError('unknown command mode: %s' % cmdmode)
            if cmdmode is None or (cmdmode == 'host' and not guest) or (cmdmode == 'guest' and guest):
                if cmd == 'base':
                    base = (int(cmdarg, 0) << BASE_ADDRESS_SHIFT)
                elif cmd == 'include':
                    includes.append(cmdarg)
                elif cmd == 'include_after':
                    includes_after.append(cmdarg)
                elif cmd == 'prefix':
                    prefix = cmdarg
                else:
                    raise ValueError('unknown command: %s' % cmd)
        elif line:
            if line.startswith('host:'):
                if guest:
                    continue
                line = line[5:].strip()
            elif line.startswith('guest:'):
                if not guest:
                    continue
                line = line[6:].strip()
            ret, name, sig = line.split(maxsplit=2)
            sig = sig[1:-1].strip()
            args = [x.split(maxsplit=1) for x in re.split(r', *', sig)] if sig != 'void' else []
            functions.append({
                'name': name,
                'return': ret,
                'arguments': args,
                'lineno': idx + 1
            })
            if any('ptr:JNIEnv' in arg for arg in args):
                definitions.add('extern void* unwrap_jni_env(void* env);')

# FIXME: EGLNativePixmapType and EGLNativeWindowType are pointers on AOSP
# (though they are also opaque, so h2g/g2h shouldn't be necessary)
def isPointerType(type):
    return type.startswith('ptr:') or \
            type in ['GLsync', 'EGLDisplay', 'EGLConfig', 'EGLSurface', 'EGLContext', 'EGLSync', 'EGLImage',
                     'EGLNativeDisplayType', 'EGLClientBuffer', 'EGLSyncKHR', 'EGLImageKHR', 'EGLStreamKHR', 'GLeglImageOES']

def isFloatType(type):
    return type in ['float', 'GLfloat', 'double']

def getArgumentTypeAndSize(type, lineno=0):
    if type in ['ssize_t', 'off_t', 'GLintptr', 'size_t', 'GLsizeiptr',
                'EGLNativePixmapType', 'EGLNativeWindowType']:
        if is64bit:
            return type, 8
        else:
            return type, 4
    elif type in ['int', 'int32_t', 'float', 'GLint', 'GLenum',
                  'GLfloat', 'GLfixed', 'GLclampx', 'GLclampf',
                  'EGLint', 'EGLenum', 'EGLNativeFileDescriptorKHR']:
        return type, 4
    elif type in ['uint32_t', 'GLuint',
                  'GLbitfield', 'GLsizei']:
        return type, 4
    elif type in ['int16_t', 'GLshort']:
        return type, 2
    elif type in ['int8_t', 'uint8_t', 'GLboolean', 'GLubyte', 'EGLBoolean']:
        return type, 1
    elif isPointerType(type):
        if is64bit:
            return 'void*', 8
        else:
            return 'void*', 4
    elif type in ['int64_t', 'uint64_t', 'double', 'off64_t',
                  'GLint64', 'GLuint64', 'EGLTime', 'EGLTimeKHR', 'EGLnsecsANDROID', 'EGLuint64KHR', 'EGLuint64NV']:
        return type, 8
    elif type == 'void':
        return 'void', 0
    else:
        raise NotImplementedError('line %d: unsupported type: %s' % (lineno, type))

def genHostLib():
    print('''#define LOG_TAG "libnb-qemu"
%s
#include <stdint.h>
#include <log/log.h>
#include "QemuCoreInclude.h"''' % ("#define LOG_NDEBUG 0\n" if debug else ''))

    if includes:
        for inc in includes:
            print('#include %s' % inc)

    if definitions:
        print('')
        for d in definitions:
            print(d)

    print('''
#ifdef __LP64__
#define REGS(x) env->xregs[x]
#define SP_REG REGS(31)
#define LR_REG REGS(30)
#define PC_REG env->pc
#else
#define REGS(r) env->regs[r]
#define SP_REG REGS(13)
#define LR_REG REGS(14)
#define PC_REG REGS(15)
#endif
''')

    for i, func in enumerate(functions):
        print('void nb_handle_%s(CPUARMState *env) {' % func['name'])
        print('    char *sp = nb_qemu_g2h(SP_REG);')
        ret_type, ret_size = getArgumentTypeAndSize(func['return'], lineno=func['lineno'])
        if ret_size:
            print('    %s __nb_ret = (%s)%s(' % (ret_type, ret_type, func['name']))
        else:
            print('    %s(' % func['name'])
        nargs = len(func['arguments'])
        currentreg = 0
        currentfloatreg = 0
        offset = 0
        for j, arg in enumerate(func['arguments']):
            if len(arg) > 1 and arg[1].startswith('*'):
                raise NotImplementedError('line %d: unsupported pointer type' % func['lineno'])
            arg_type, arg_size = getArgumentTypeAndSize(arg[0], lineno=func['lineno'])

            if (j < num_args_in_regs):
                if is64bit and isFloatType(arg[0]):
                    regstring = '&env->vfp.zregs[%d]' % (currentfloatreg)
                    currentfloatreg += 1
                else:
                    regstring = '&REGS(%d)' % (currentreg)
                    currentreg += 1

                if arg[0] == 'ptr:JNIEnv':
                    print('        unwrap_jni_env(*(%s*)(%s))%s' % (arg_type, regstring, ',' if j < (nargs-1) else ''))
                else:
                    print('        *(%s*)(%s)%s' % (arg_type, regstring, ',' if j < (nargs-1) else ''))
            else:
                if arg_size == 8:
                    offset = (offset+7)&(~7)
                if arg[0] == 'ptr:JNIEnv':
                    print('        unwrap_jni_env(*(%s*)(&sp[%d]))%s' % (arg_type, offset, ',' if j < (nargs-1) else ''))
                else:
                    print('        *(%s*)(&sp[%d])%s' % (arg_type, offset, ',' if j < (nargs-1) else ''))
                offset += ((arg_size+3)&(~3))
        print('    );')
        if ret_size:
            if is64bit or ret_size <= 4:
                if isPointerType(func['return']):
                    print('    REGS(0) = nb_qemu_h2g(__nb_ret);')
                elif is64bit and isFloatType(func['return']):
                    print('    *(%s*)&env->vfp.zregs[0] = __nb_ret;' % (func['return']))
                elif ret_size == 4:
                    print('    REGS(0) = *(abi_ulong*)(&__nb_ret);')
                elif ret_size == 2:
                    print('    REGS(0) = (*(abi_ulong*)(&__nb_ret)) & 0xffff;')
                else:
                    print('    REGS(0) = (*(abi_ulong*)(&__nb_ret)) & 0xff;')
            elif ret_size == 8:
                print('    REGS(0) = __nb_ret & 0xffffffff;')
                print('    REGS(1) = (__nb_ret >> 32) & 0xffffffff;')
            else:
                raise NotImplementedError('line %d: unsupported return type: %s' % (func['lineno'], ret_type))
        print('}')

    print('__attribute__((visibility("default")))')
    print('void nb_handle_svc(CPUARMState *env, int svc) {')
    print('    switch (svc) {')
    for i, func in enumerate(functions):
        debug_line = ''
        if debug:
            debug_line = 'ALOGV("%s"); ' % func['name']
        print('        case 0x%04x: %snb_handle_%s(env); break;' % ((base + i), debug_line, func['name']))
    print('        default: LOG_ALWAYS_FATAL("Unknown SVC %08x", svc); break;')
    print('    }')
    print('}')

    if includes_after:
        print('')
        for inc in includes_after:
            print('#include %s' % inc)

def genGuestLib():
    if includes:
        for inc in includes:
            print('#include %s' % inc)
        print('')
    for i, func in enumerate(functions):
        if is64bit:
            print('''__attribute__((naked,noinline)) void %s%s() {
    __asm__ volatile(
        "svc #0x%04x\\n"
        "ret"
    );
}''' % (prefix, func['name'], (base + i)))
        else:
            print('''__attribute__((naked,noinline)) void %s%s() {
    __asm__ volatile(
        "svc #0x%04x\\n"
        "bx lr"
    );
}''' % (prefix, func['name'], (base + i)))
    if includes_after:
        print('')
        for inc in includes_after:
            print('#include %s' % inc)

if guest:
    genGuestLib()
else:
    genHostLib()

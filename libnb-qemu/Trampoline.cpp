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
//#define LOG_NDEBUG 0

#include <errno.h>
#include <string.h>
#include <log/log.h>
#include "JavaBridge.h"
#include "QemuCore.h"
#include "QemuCpu.h"
#include "QemuMemory.h"
#include "Trampoline.h"

/*
see dyn (dyncall): https://metacpan.org/pod/Dyn

Type

Signature character     C/C++ data type
----------------------------------------------------
v                       void
B                       _Bool, bool ###
c                       char
C                       unsigned char
s                       short
S                       unsigned short
i                       int
I                       unsigned int
j                       long ###
J                       unsigned long ###
l                       long long, int64_t
L                       unsigned long long, uint64_t
f                       float
d                       double
p                       void *
Z                       const char * (pointer to a C string) ###
A                       aggregate (struct/union described out-of-band via DCaggr)

*/
/*
 * We haven't support *struct* and some complex type, and VA function, it need more research
 */


static ffi_type *type_to_ffi(const char t)
{
  //TODO: long double and complex type
  switch (t) {
    case 'c': return &ffi_type_sint8;
    case 'C': return &ffi_type_uint8;
    case 's': return &ffi_type_sint16;
    case 'S': return &ffi_type_uint16;
    case 'i': return &ffi_type_sint32;
    case 'I': return &ffi_type_uint32;
    case 'l': return &ffi_type_sint64;
    case 'L': return &ffi_type_uint64;
    case 'f': return &ffi_type_float;
    case 'd': return &ffi_type_double;
    case 'p': return &ffi_type_pointer;
    case 'v': return &ffi_type_void;
    default:
        ALOGE("Unsupported type: %c", t);
        return nullptr;
  }
}

static char jni_to_type(const char t)
{
  switch (t) {
    case 'Z': return 'C';
    case 'B': return 'c';
    case 'C': return 'S';
    case 'S': return 's';
    case 'I': return 'i';
    case 'J': return 'l';
    case 'F': return 'f';
    case 'D': return 'd';
    case 'L': return 'p';
    case 'V': return 'v';
    default:
        ALOGE("Unsupported JNI type: %c", t);
        return '?';
  }
}

static std::string shorty_to_signature(const std::string& shorty)
{
  std::string signature(shorty.length() + 2, ' ');

  signature[0] = jni_to_type(shorty[0]);
  signature[1] = signature[2] = 'p';
  for (int i = 1; i < shorty.length(); i++)
    signature[i+2] = jni_to_type(shorty[i]);

  return signature;
}

/******  Trampoline Arg data  ******/
//NOTE: every thread call allocate for once, alloc pointer store in pthread key
QemuAndroidCallData* Trampoline::alloc_call_data(int stack_size, int int_regs_used, int float_regs_used)
{
    QemuAndroidCallData *data;
    data = new QemuAndroidCallData();
    //allocate regs, if used is -1 mean default, we dont need allocate only when 0
    switch(GuestIsa){
        case "arm":{
            if(int_regs_used != 0)
                data->int32_regs = new uint32_t[4](); //all pass in 4 regs
            break;
        }
        case "arm64":{
            if(int_regs_used != 0)
                data->int64_regs = new uint64_t[8](); // 8 regs
            if(float_regs_used != 0)
                data->float128_regs = new QA_UInt128[8](); // 8 vfp 128-bit regs
            break;
        }
        case "x86":{
            //x86 pass args by stack;
            break;
        }
        case "x86_64":{
            if(int_regs_used != 0)
                data->int64_regs = new uint64_t[6](); // 6 regs
            if(float_regs_used != 0)
                data->float128_regs = new QA_UInt128[8](); // 8 xmm 128-bit regs
            break;
        }
        case "riscv64"{
            if(int_regs_used != 0)
                data->int64_regs = new uint64_t[8](); // 8 gpr
            if(float_regs_used != 0)
                data->float64_regs = new uint64_t[8](); // 8 64-bit fpr
            break;
        }
        default:
            delete data;
            return nullptr;
    }
    if(stack_size){
        data->stack = reinterpret_cast<char *>(calloc(stack_size, 1));
    }
    data->int_regs_used = int_regs_used;
    data->float_regs_used = float_regs_used;
    return data;
}

void Trampoline::free_call_data(QemuAndroidCallData* data)
{
    if(data){
        free(data->stack);
        delete data->int32_regs;
        delete data->int64_regs;
        delete data->float32_regs;
        delete data->float64_regs;
        delete data->float128_regs;
        delete data;
    }
}

QemuAndroidCallRet* Trampoline::alloc_call_ret(enum retProcessType ret_type)
{
    QemuAndroidCallRet *ret;
    ret = new QemuAndroidCallRet();
    if(ret_type) ret->ret_type = ret_type;
    return ret;
}

void Trampoline::free_call_ret(QemuAndroidCallRet* ret)
{
    delete ret;
}

//no check TODO: check failed
QemuAndroidCallData* Trampoline::get_call_data()
{
    QemuAndroidCallData* data = static_cast<QemuAndroidCallData*>(pthread_getspecific(call_data_key_));
    if (!data) {
        data = Trampoline::alloc_call_data(stacksize_, int_regs_used_, float_regs_used_);
        if(data)
          pthread_setspecific(call_data_key_, data);
        else
          abort();
    }
    return data;
}

QemuAndroidCallRet* Trampoline::get_call_ret()
{
    QemuAndroidCallRet* ret = static_cast<QemuAndroidCallRet*>(pthread_getspecific(call_ret_key_));
    if (!ret) {
        ret = Trampoline::alloc_call_ret(ret_type_);
        if(ret)
          pthread_setspecific(call_ret_key_, ret);
        else
          abort();
    }
    return ret;
}

/**
 * QemuAndroid have calling_conventions about what regs the return type use.
 * when calling, QemuAndroid return the guest addr maybe
 * In build tramp we should handle...
 * 1. what regs will each args use
 * 2. how many, and size?
 * 3. if contains pointer then? usually if data is from host, then the pointer need h2g,
 * 4. the stack alignment
 * 5. where to get ret
 *
 * In call we should handle...
 * 1. use the handled database to copy args to CallData or CallRet to ret
 * 2. if contains pointer then?
 */

/******  Class Trampoline  ******/

//allocate a trampoline: use the given guest addr and fcn signature to build a callable ffi_tramp
Trampoline::Trampoline(const std::string& name, intptr_t address, const std::string& signature)
     : name_(name),
       address_(address),
       signature_(signature),
       host_address_(0),
       closure_(nullptr),
       rtype_(nullptr),
       argtypes_(nullptr),
       nargs_(0),
       nstackargs_(0),
       stacksize_(0),
       //stackoffsets_(nullptr),
       int_regs_used_(0),
       float_regs_used_(0),
       arg_store_type_(nullptr),
       arg_store_offsets_(nullptr)
{
  void *code_address = nullptr;

#define ALIGN_DOWN(x, align) ((x)&(~(align-1))
#define ALIGN_UP(x, align) ALIGN_DOWN(x+align-1, align)
#define ALIGN_DWORD(x) ((x+7)&(~7))
#define ALIGN_QWORD(x) ((x+15)&(~15))

  closure_ = reinterpret_cast<ffi_closure *>(ffi_closure_alloc(sizeof(ffi_closure), &code_address));
  if (closure_) {
      if (! signature.empty()) {
          nargs_ = signature.length() - 1;
          rtype_ = type_to_ffi(signature[0]);
          if (! rtype_) //must have this to ensure build things up
            return;
          rtype_char_ = signature[0];
          argtypes_ = reinterpret_cast<ffi_type **>(calloc(sizeof(ffi_type *), nargs_));
          arg_store_type_ = reinterpret_cast<enum argProcessType *>(calloc(sizeof(enum argProcessType), nargs_));
          arg_store_offsets_ = reinterpret_cast<int *>(calloc(sizeof(int), nargs_));
          //Calling convention
          //handle args and ret_type determined by GuestIsa
          switch(GuestIsa){
            case "arm":{
              //prepare arg
              for (int i = 0; i < nargs_; i++) {
                  argtypes_[i] = type_to_ffi(signature[i+1]);
                  if (! argtypes_[i])
                    return;
                  switch (signature[i+1]) { //check target type, if in 64host-32guest, check argtypes_[i]->size to handle thunk
                    case 'v': //void TODO: should not reach here
                      break;
                    case 'c': //char
                    case 'C': //uchar
                    case 's'; //short
                    case 'S': //ushort
                    case 'i': //int
                    case 'I': //uint
                    case 'f': //float
                      if (int_regs_used_ < 4) {
                        arg_store_type_[i] = ARG_TYPE_INT32_REG;
                        arg_store_offsets_[i] = int_regs_used_;
                        int_regs_used_++;
                      }else{
                        arg_store_type_[i] = ARG_TYPE_STACK32;
                        arg_store_offsets_[i] = stacksize_;
                        stacksize_ += 4;
                      }
                      break;
                    case 'l': //int64
                    case 'L': //uint64
                    case 'd': //double
                      if (int_regs_used_ < 4) {
                        //first check alignment
                        if (int_regs_used_ % 2) //when 1 or 3, remain 1 reg
                            int_regs_used_++;//reg alignment
                      }
                      if (int_regs_used_ < 4) //after alignment, int_regs_used_ is 0 or 2 or 4
                          arg_store_type_[i] = ARG_TYPE_INT32_REG_DOUBLE;
                          arg_store_offsets_[i] = int_regs_used_;
                          int_regs_used_+=2;
                      }else{
                        stacksize_ = ALIGN_UP(stacksize_, 8);//when 8 byte type, align the stack size
                        arg_store_type_[i] = ARG_TYPE_STACK64;
                        arg_store_offsets_[i] = stacksize_;
                        stacksize_ += 8;
                      }
                      break;
                    case 'p': //pointer
                      if (int_regs_used_ < 4) {
                        arg_store_type_[i] = ARG_TYPE_INT32_REG_PTR;
                        arg_store_offsets_[i] = int_regs_used_;
                        int_regs_used_++;
                      }else{
                        arg_store_type_[i] = ARG_TYPE_STACK32_PTR;
                        arg_store_offsets_[i] = stacksize_;
                        stacksize_ += 4;
                      }
                      break;
                    default:
                      //TODO: handle other type and struct
                      //when size is over 8, alignment in stack is still keep 8
                  }//switch sig
              }//for arg
              // The whole stack of arguments must be double-word aligned
              if (stacksize_) {
                stacksize_ = ALIGN_UP(stacksize_, 8);
              }
              //prepare ret_type_ in armv7-androideabi
              switch(rtype_char_){
                case 'v': //void
                  ret_type_ = RET_TYPE_VOID;
                  break;
                case 'c': //char
                case 'C': //uchar
                case 's'; //short
                case 'S': //ushort
                case 'i': //int
                case 'I': //uint
                case 'f': //float
                case 'p': //pointer
                  ret_type_ = RET_TYPE_INT_REG_32;
                  break;
                case 'l': //int64
                case 'L': //uint64
                case 'd': //double
                  ret_type_ = RET_TYPE_INT_REG_64;
                  break;
                default:
                  ALOGE("Unsupported type: %c", rtype_char_);
                  ret_type_ = RET_TYPE_UNKNOW;
              }
            }//case arm
            case "arm64":{
              for (int i = 0; i < nargs_; i++) {
                  argtypes_[i] = type_to_ffi(signature[i+1]);
                  if (! argtypes_[i])
                    return;
                  switch (signature[i+1]) { //check target type, if in 64host-32guest, check argtypes_[i]->size to handle thunk
                    case 'v': //void TODO: should not reach here
                      break;
                    case 'c': //char
                    case 'C': //uchar
                    case 's'; //short
                    case 'S': //ushort
                    case 'i': //int
                    case 'I': //uint
                    case 'l': //int64
                    case 'L': //uint64
                      if (int_regs_used_ < 8) {
                        arg_store_type_[i] = ARG_TYPE_INT64_REG;
                        arg_store_offsets_[i] = int_regs_used_;
                        int_regs_used_++;
                      }else{
                        arg_store_type_[i] = ARG_TYPE_STACK64;
                        arg_store_offsets_[i] = stacksize_;
                        stacksize_ += 8;
                      }
                      break;
                    case 'f': //float
                    case 'd': //double
                      if (float_regs_used_ < 8) {
                        arg_store_type_[i] = ARG_TYPE_FLOAT128_REG_LOW; //although we use this struct, we will check its size to asign value
                        arg_store_offsets_[i] = float_regs_used_;
                        float_regs_used_++;
                      }else{
                        arg_store_type_[i] = ARG_TYPE_STACK64;
                        arg_store_offsets_[i] = stacksize_;
                        stacksize_ += 8;
                      }
                      break;
                    case 'p': //pointer
                      if (int_regs_used_ < 8) {
                        arg_store_type_[i] = ARG_TYPE_INT64_REG_PTR;
                        arg_store_offsets_[i] = int_regs_used_;
                        int_regs_used_++;
                      }else{
                        arg_store_type_[i] = ARG_TYPE_STACK64_PTR;
                        arg_store_offsets_[i] = stacksize_;
                        stacksize_ += 8;
                      }
                      break;
                    default:
                      //TODO: handle other type and struct
                      //when size is over 8, regs need alignment, this may skip regs
                  }//switch sig
              }//for arg
              //alignment in stack is 8, if bigger then align bigger
              //prepare ret_type_ in arm64
              switch(rtype_char_){
                case 'v': //void
                  ret_type_ = RET_TYPE_VOID;
                  break;
                case 'c': //char
                case 'C': //uchar
                case 's'; //short
                case 'S': //ushort
                case 'i': //int
                case 'I': //uint
                  ret_type_ = RET_TYPE_INT_REG_32;
                  break;
                case 'l': //int64
                case 'L': //uint64
                case 'p': //pointer
                  ret_type_ = RET_TYPE_INT_REG_64;
                  break;
                case 'f': //float
                  ret_type_ = RET_TYPE_FLOAT_REG_32;
                  break;
                case 'd': //double
                  ret_type_ = RET_TYPE_FLOAT_REG_64;
                  break;
                default:
                  ALOGE("Unsupported type: %c", rtype_char_);
                  ret_type_ = RET_TYPE_UNKNOW;
              }
            }//case arm64
            case "x86":{
              //pure stack，alignment in stack is 4, if bigger then align bigger
              return;
            }
            case "x86_64":{
              //when size is over 8, alignment in stack is still keep 8
              return;
            }
            case "riscv64":{
              //
              return;
            }
            default:
                //unknow arch
              ALOGE("Cannot create trampoline for unknown GuestIsa: %s", GuestIsa);
              return;
          }//switch isa

/*
          for (int i = 0, nregs = __LP64__ ? 8 : 4; i < nargs_; i++) {
              argtypes_[i] = type_to_ffi(signature[i+1]);
              if (! argtypes_[i])
                return;
              // Check valid size and attempt to use register(s)
              // TODO: calling conventions about args
              switch (argtypes_[i]->size) {
                  case 8:
#ifndef __LP64__
                    if (nregs >= 2) {
                        nregs = nregs == 3 ? 2 : nregs;
                        nregs -= 2;
                        continue;
                    }
                    nregs = 0;
                    break;
#endif
                  case 4:
                  case 2:
                  case 1:
                    if (nregs) {
                        nregs--;

                        continue;
                    }
                    break;
                  default:
                    ALOGE("Unsupported argument size: %d", argtypes_[i]->size);
                    return;
              }
              // Argument must be passed in the stack
              if (! stackoffsets_)
                stackoffsets_ = reinterpret_cast<int *>(calloc(sizeof(int), nargs_ - i));
#ifndef __LP64__
              // Double-word sized arguments must be double-word aligned in the stack
              if (argtypes_[i]->size == 8)
                stacksize_ = ALIGN_DWORD(stacksize_);
#endif
              // Add argument to the stack
              stackoffsets_[nstackargs_] = stacksize_;
              nstackargs_++;
              stacksize_ += argtypes_[i]->size == 8 ? 8 : 4;
          }
          // The whole stack of arguments must be double-word aligned
          if (stacksize_) {
              stacksize_ = ALIGN_DWORD(stacksize_);
          }
      }
      else {
          ALOGE("Cannot create trampoline for unknown function: %s", name.c_str());
      }
*/

      //create key for call data and ret
      //TODO: check success
      pthread_key_create(&call_data_key_, &destructor_call_data);
      pthread_key_create(&call_ret_key_, &destructor_call_ret);

      //build cif
      if (ffi_prep_cif(&cif_, FFI_DEFAULT_ABI, nargs_, rtype_, argtypes_) == FFI_OK) {
        //build closure
          if (ffi_prep_closure_loc(closure_, &cif_, Trampoline::call_trampoline, this, code_address) == FFI_OK) {
              host_address_ = code_address;
          }
      }
  }
}

Trampoline::~Trampoline()
{
  ffi_closure_free(closure_);
  free(argtypes_);
  free(arg_store_type_);
  free(arg_store_offsets_);
  //free(stackoffsets_);
  pthread_key_delete(call_data_key_);
  pthread_key_delete(call_ret_key_);
}

//return the tramp address build from ffi
void *Trampoline::get_handle() const
{
  return reinterpret_cast<void *>(host_address_);
}

//static function: redirect to the parent class function "call", this is prepare for ffi to build a tramp func
void Trampoline::call_trampoline(ffi_cif *cif, void *ret, void **args, void *self)
{
  reinterpret_cast<Trampoline *>(self)->call(ret, args);
}

//virtual function: perform a call through QemuAndroid, require return type signature
void Trampoline::call(void *ret, void **args)
{
  ALOGV("Trampoline[%s] -- START", name_.c_str());
  //### ### ### ### ### ###
  //get call and ret data
  QemuAndroidCallData *call_data = get_call_data();
  QemuAndroidCallRet *call_ret = get_call_ret();

  //copy args
  for (int i = 0; i < nargs_; i++ ) {
      //get handled arg
      void *arg = get_call_argument(i, args[i]);
      switch(arg_store_type_[i]){
        case ARG_TYPE_INT32_REG:
          call_data->int32_regs[arg_store_offsets_[i]] = *(uint32_t *)arg;
          break;
        case ARG_TYPE_INT32_REG_DOUBLE:
          *(uint64_t *)(&(call_data->int32_regs[arg_store_offsets_[i]])) = *(uint64_t *)arg;
          break;
        case ARG_TYPE_INT32_REG_PTR:
          call_data->int32_regs[arg_store_offsets_[i]] = QemuCore::h2g(*(uintptr_t *)arg);
          break;
        case ARG_TYPE_INT64_REG:
          call_data->int64_regs[arg_store_offsets_[i]] = *(uint64_t *)arg;
          break;
        case ARG_TYPE_INT64_REG_DOUBLE:
          memcpy(&(call_data->int64_regs[arg_store_offsets_[i]]), arg, 16);//little-endian
          break;
        case ARG_TYPE_INT64_REG_PTR:
          call_data->int64_regs[arg_store_offsets_[i]] = QemuCore::h2g(*(uintptr_t *)arg);
          break;
        case ARG_TYPE_FLOAT32_REG:
          call_data->float32_regs[arg_store_offsets_[i]] = *(uint32_t *)arg;
          break;
        case ARG_TYPE_FLOAT32_REG_DOUBLE:
          *(uint64_t *)(&(call_data->float32_regs[arg_store_offsets_[i]])) = *(uint64_t *)arg;
          break;
        case ARG_TYPE_FLOAT64_REG:
          call_data->float64_regs[arg_store_offsets_[i]] = *(uint64_t *)arg;
          break;
        case ARG_TYPE_FLOAT64_REG_DOUBLE:
          memcpy(&(call_data->float64_regs[arg_store_offsets_[i]]), arg, 16);//little-endian
          break;
        case ARG_TYPE_FLOAT128_REG:
          memcpy(&(call_data->float128_regs[arg_store_offsets_[i]]), arg, 16);//little-endian
          break;
        case ARG_TYPE_FLOAT128_REG_LOW:
          call_data->float128_regs[arg_store_offsets_[i]].low = *(uint64_t *)arg;
          break;
        case ARG_TYPE_STACK32:
          *(uint32_t*)(&(call_data->stack[arg_store_offsets_[i]])) = *(uint32_t *)arg;
          break;
        case ARG_TYPE_STACK64:
          *(uint64_t*)(&(call_data->stack[arg_store_offsets_[i]])) = *(uint64_t *)arg;
          break;
        case ARG_TYPE_STACK128:
          memcpy(&(call_data->stack[arg_store_offsets_[i]]), arg, 16);
          break;
        case ARG_TYPE_STACK32_PTR:
          *(uint32_t*)(&(call_data->stack[arg_store_offsets_[i]])) = QemuCore::h2g(*(uintptr_t *)arg);
          break;
        case ARG_TYPE_STACK64_PTR:
          *(uint64_t*)(&(call_data->stack[arg_store_offsets_[i]])) = QemuCore::h2g(*(uintptr_t *)arg);
          break;
        case ARG_TYPE_STACK_COMPLEX:
          //
        case ARG_TYPE_INT32_AND_STACK:
          //
        case ARG_TYPE_INT64_SPLIT_STACK:
          //
        case ARG_TYPE_UNKNOW:
        default:
          ALOGE("Cannot call trampoline for unknown ARG_TYPE: %d", arg_store_type_[i]);
      }
  }

  //perform call
  QemuCpu::get()->call(address_, call_data, call_ret);

  //process return
  switch (rtype_char_) {
    case 'v': //void
        break;
    case 'c': //char
    case 'C': //uchar
        *((uint8_t *)ret) = call_ret->int32_ret & 0xff;
        break;
    case 's'; //short
    case 'S': //ushort
        *((uint16_t *)ret) = call_ret->int32_ret & 0xffff;
        break;
    case 'i': //int
    case 'I': //uint
    case 'f': //float
        *((uint32_t *)ret) = call_ret->int32_ret;
        break;
    case 'l': //int64
    case 'L': //uint64
    case 'd': //double
        *((uint64_t *)ret) = call_ret->int64_ret;
        break;
    case 'p': //pointer
        if (arch_values[GuestIsa].target_64_bit){
            *((uintptr_t *)ret) = (uintptr_t)QemuCore::g2h(call_ret->int64_ret);
        }else{
            *((uintptr_t *)ret) = (uintptr_t)QemuCore::g2h(call_ret->int32_ret);
        }
        break;
    default:
        ALOGE("Cannot call trampoline for unknown rtype: %c", rtype_char_);
  }

  ALOGV("Trampoline[%s] -- END", name_.c_str());//TODO: print call ret directly

  //### ### ### ### ### ###
}

//virtual function: nothing to do here, just return the arg back
void *Trampoline::get_call_argument(int index, void *arg)
{
  return arg;
}

/******  JNITrampoline  ******/

//when reset to JNITrampoline, define tramp and shorty for this object
JNITrampoline::JNITrampoline(const std::string& name, intptr_t address, const std::string& shorty)
     : Trampoline(name, address, shorty_to_signature(shorty)),
       shorty_(shorty)
{
}

//jni_env
void *JNITrampoline::get_call_argument(int index, void *arg)
{
  switch (index) {
    case 0:
      return &JavaBridge::wrap_jni_env(*(JNIEnv **)arg);
    default:
      return Trampoline::get_call_argument(index, arg);
  }
}

/******  JNILoadTrampoline  ******/
//when reset to JNILoadTrampoline, define the load type
JNILoadTrampoline::JNILoadTrampoline(const std::string& name, intptr_t address)
     : Trampoline(name, address, name == "JNI_OnLoad" ? "ipp" : "vpp")
{
}

//java_vm
void *JNILoadTrampoline::get_call_argument(int index, void *arg)
{
  switch (index) {
    case 0:
      return &JavaBridge::wrap_java_vm(*(JavaVM **)arg);
    default:
      return Trampoline::get_call_argument(index, arg);
  }
}

/******  GuestNativeActivity  ******/

//TODO: copy aosp header
#include "/home/Mis012/Github_and_other_sources/android_translation_layer/src/api-impl-jni/native_activity.h"

#define CALLBACK_ONSTART                        0
#define CALLBACK_ONRESUME                       1
#define CALLBACK_ONSAVEINSTANCESTATE            2
#define CALLBACK_ONPAUSE                        3
#define CALLBACK_ONSTOP                         4
#define CALLBACK_ONDESTROY                      5
#define CALLBACK_ONWINDOWFOCUSCHANGED           6
#define CALLBACK_ONNATIVEWINDOWCREATED          7
#define CALLBACK_ONNATIVEWINDOWRESIZED          8
#define CALLBACK_ONNATIVEWINDOWREDRAWNEEDED     9
#define CALLBACK_ONNATIVEWINDOWDESTROYED        10
#define CALLBACK_ONINPUTQUEUECREATED            11
#define CALLBACK_ONINPUTQUEUEDESTROYED          12
#define CALLBACK_ONCONTENTRECTCHANGED           13
#define CALLBACK_ONCONFIGURATIONCHANGED         14
#define CALLBACK_ONLOWMEMORY                    15

struct GuestNativeActivity : public ANativeActivity {
    ANativeActivity *native_;
    ANativeActivityCallbacks callbacks_;
};

static intptr_t GuestNativeActivity_getCallback(ANativeActivity *activity, int index)
{
  QemuMemory::Region<GuestNativeActivity> guest_activity((intptr_t) activity->instance);
  return (intptr_t) ((void **) guest_activity.get()->callbacks)[index];
}

#define GNA_CALLBACK(name, index) \
static void GuestNativeActivity_ ## name (ANativeActivity *activity) \
{ \
  intptr_t callback = GuestNativeActivity_getCallback(activity, index); \
  if (callback) { \
      ALOGI(#name); \
      QemuCpu::get()->call(callback, (intptr_t) activity->instance); \
  } \
}

#define GNA_CALLBACK_ARG(name, T, index) \
static void GuestNativeActivity_ ## name (ANativeActivity *activity, T arg) \
{ \
  intptr_t callback = GuestNativeActivity_getCallback(activity, index); \
  if (callback) { \
      ALOGI(#name); \
      QemuCpu::get()->call(callback, (intptr_t) activity->instance, (intptr_t) arg); \
  } \
}

GNA_CALLBACK(onStart, CALLBACK_ONSTART)
GNA_CALLBACK(onResume, CALLBACK_ONRESUME)
GNA_CALLBACK(onPause, CALLBACK_ONPAUSE)
GNA_CALLBACK(onStop, CALLBACK_ONSTOP)
GNA_CALLBACK_ARG(onWindowFocusChanged, int, CALLBACK_ONWINDOWFOCUSCHANGED)
GNA_CALLBACK_ARG(onNativeWindowCreated, ANativeWindow*, CALLBACK_ONNATIVEWINDOWCREATED)
GNA_CALLBACK_ARG(onNativeWindowResized, ANativeWindow*, CALLBACK_ONNATIVEWINDOWRESIZED)
GNA_CALLBACK_ARG(onNativeWindowRedrawNeeded, ANativeWindow*, CALLBACK_ONNATIVEWINDOWREDRAWNEEDED)
GNA_CALLBACK_ARG(onNativeWindowDestroyed, ANativeWindow*, CALLBACK_ONNATIVEWINDOWDESTROYED)
GNA_CALLBACK_ARG(onInputQueueCreated, AInputQueue*, CALLBACK_ONINPUTQUEUECREATED)
GNA_CALLBACK_ARG(onInputQueueDestroyed, AInputQueue*, CALLBACK_ONINPUTQUEUEDESTROYED)
GNA_CALLBACK_ARG(onContentRectChanged, const ARect*, CALLBACK_ONCONTENTRECTCHANGED)
GNA_CALLBACK(onConfigurationChanged, CALLBACK_ONCONFIGURATIONCHANGED)
GNA_CALLBACK(onLowMemory, CALLBACK_ONLOWMEMORY)

static void* GuestNativeActivity_onSaveInstanceState(ANativeActivity *activity, size_t *outSize)
{
  intptr_t callback = GuestNativeActivity_getCallback(activity, CALLBACK_ONSAVEINSTANCESTATE);
  if (callback) {
      ALOGW("GuestNativeActivity_onSaveInstanceState: ignoring for the time being");
      // QemuCpu::get()->call(callback, (intptr_t) activity->instance, (intptr_t) arg);
  }
  return nullptr;
}

static void GuestNativeActivity_onDestroy(ANativeActivity *activity)
{
  intptr_t callback = GuestNativeActivity_getCallback(activity, CALLBACK_ONDESTROY);
  if (callback) {
      ALOGI("GuestNativeActivity_onDestroy");
      QemuCpu::get()->call(callback, (intptr_t) activity->instance);
  }
  QemuCore::free((intptr_t) activity->instance);
  activity->instance = nullptr;
}

/******  NativeActivityTrampoline  ******/

NativeActivityTrampoline::NativeActivityTrampoline(const std::string& name, intptr_t address)
     : Trampoline(name, address, "vppI")
{
}

void NativeActivityTrampoline::call(void *ret, void **args)
{
  ANativeActivity *host_activity = *(ANativeActivity **)args[0];
  intptr_t guest_activity_p = QemuCore::malloc(sizeof(GuestNativeActivity));

  // Setup host ANativeActivity
  host_activity->instance = (void *) guest_activity_p;
  host_activity->callbacks->onStart = GuestNativeActivity_onStart;
  host_activity->callbacks->onResume = GuestNativeActivity_onResume;
  host_activity->callbacks->onSaveInstanceState = GuestNativeActivity_onSaveInstanceState;
  host_activity->callbacks->onPause = GuestNativeActivity_onPause;
  host_activity->callbacks->onStop = GuestNativeActivity_onStop;
  host_activity->callbacks->onDestroy = GuestNativeActivity_onDestroy;
  host_activity->callbacks->onWindowFocusChanged = GuestNativeActivity_onWindowFocusChanged;
  host_activity->callbacks->onNativeWindowCreated = GuestNativeActivity_onNativeWindowCreated;
  host_activity->callbacks->onNativeWindowResized = GuestNativeActivity_onNativeWindowResized;
  host_activity->callbacks->onNativeWindowRedrawNeeded = GuestNativeActivity_onNativeWindowRedrawNeeded;
  host_activity->callbacks->onNativeWindowDestroyed = GuestNativeActivity_onNativeWindowDestroyed;
  host_activity->callbacks->onInputQueueCreated = GuestNativeActivity_onInputQueueCreated;
  host_activity->callbacks->onInputQueueDestroyed = GuestNativeActivity_onInputQueueDestroyed;
  host_activity->callbacks->onContentRectChanged = GuestNativeActivity_onContentRectChanged;
  host_activity->callbacks->onConfigurationChanged = GuestNativeActivity_onConfigurationChanged;
  host_activity->callbacks->onLowMemory = GuestNativeActivity_onLowMemory;

  // Setup guest ANativeActivity
  {
      QemuMemory::Region<GuestNativeActivity> guest_activity_r(guest_activity_p);
      GuestNativeActivity *guest_activity = guest_activity_r.get();

      guest_activity->assetManager = host_activity->assetManager;
      guest_activity->callbacks = (ANativeActivityCallbacks *) QemuCore::h2g(&guest_activity->callbacks_);
      guest_activity->clazz = host_activity->clazz;
      guest_activity->env = (JNIEnv *) JavaBridge::wrap_jni_env(host_activity->env);
      guest_activity->externalDataPath = (const char *) QemuCore::h2g((void *) host_activity->externalDataPath);
      guest_activity->internalDataPath = (const char *) QemuCore::h2g((void *) host_activity->internalDataPath);
      guest_activity->obbPath = (const char *) QemuCore::h2g((void *) host_activity->obbPath);
      guest_activity->sdkVersion = host_activity->sdkVersion;
      guest_activity->vm = (JavaVM *) JavaBridge::wrap_java_vm(host_activity->vm);

      guest_activity->native_ = host_activity;
      bzero(&guest_activity->callbacks_, sizeof(ANativeActivityCallbacks));
  }

  // Call trampoline
  void *new_args[3] = { &guest_activity_p, args[1], args[2] };
  Trampoline::call(ret, new_args);
}

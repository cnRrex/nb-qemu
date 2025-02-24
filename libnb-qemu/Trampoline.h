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

#ifndef TRAMPOLINE_H_
#define TRAMPOLINE_H_

#include <string>
#include <ffi.h>

enum argProcessType {
  ARG_TYPE_UNKNOW = 0,
  ARG_TYPE_INT32_REG, //arg will store in int32 regs
  ARG_TYPE_INT32_REG_DOUBLE, //arg will store in two int32 regs
  ARG_TYPE_INT32_REG_PTR, //arg will first h2g, then store in int32 regs
  ARG_TYPE_INT64_REG, //arg will store in int64 regs
  ARG_TYPE_INT64_REG_DOUBLE, //arg will store in two int64 regs
  ARG_TYPE_INT64_REG_PTR, //arg will first h2g, then store in int64 regs
  ARG_TYPE_FLOAT32_REG, //arg will store in fp 32 regs
  ARG_TYPE_FLOAT32_REG_DOUBLE, //arg will store in two fp 32 regs
  ARG_TYPE_FLOAT64_REG, //arg will store in fp 64 regs
  ARG_TYPE_FLOAT64_REG_DOUBLE, //arg will store in two fp 64 regs
  ARG_TYPE_FLOAT128_REG, //arg will store in fp 128 regs
  ARG_TYPE_FLOAT128_REG_LOW, //64bit arg will store in the low bits in fp 128 regs
  ARG_TYPE_STACK32, //arg will store in stack and cost 4 bytes
  ARG_TYPE_STACK64, //arg will store in stack and cost 8 bytes
  ARG_TYPE_STACK128, //arg will store in stack and cost 16 bytes
  ARG_TYPE_STACK32_PTR, //arg will first h2g to 32bit pointer, then store in stack
  ARG_TYPE_STACK64_PTR, //arg will first h2g to 64bit pointer, then store in stack
  //below for struct is not implemented
  ARG_TYPE_STACK_COMPLEX, //arg will store in stack, and need to calculate its size
  ARG_TYPE_INT32_AND_STACK, //in arm32 when 16 bytes type is the 2nd/3rd regs, then it need stack to pass the rest 8 bytes
  ARG_TYPE_INT64_SPLIT_STACK // in x86_64 when uint128 is the 6th regs, it will be splited to reg and stack
};

class Trampoline
{
public:
  Trampoline(const std::string& name, intptr_t address, const std::string& signature);
  virtual ~Trampoline();

  void *get_handle() const;
  const std::string& get_name() const { return name_; }
  intptr_t get_address() const { return address_; }
  const std::string& get_signature() const { return signature_; }

protected:
  virtual void call(void *ret, void **args);
  virtual void *get_call_argument(int index, void *arg);

private:
  static void call_trampoline(ffi_cif *cif, void *ret, void **args, void *self);
  void call(intptr_t addr, struct QemuAndroidCallData *data, struct QemuAndroidCallRet *ret) const;
  QemuAndroidCallData* get_call_data();
  QemuAndroidCallRet* get_call_ret();
  static QemuAndroidCallData* alloc_call_data(int stack_size = 0, int int_regs_used = -1, int float_regs_used = -1);
  static void free_call_data(QemuAndroidCallData* data);
  static QemuAndroidCallRet* alloc_call_ret(enum retProcessType ret_type = RET_TYPE_UNKNOW);
  static void free_call_ret(QemuAndroidCallRet* ret);
  //for pthread_key_create to destruct
  static void destructor_call_data(void *ptr){ Trampoline::free_call_data( static_cast<QemuAndroidCallData*>(ptr) ); }
  static void destructor_call_ret(void *ptr){ Trampoline::free_call_ret( static_cast<QemuAndroidCallRet*>(ptr) ); }

private:
  std::string name_;
  intptr_t address_;
  std::string signature_;
  void *host_address_;
  ffi_closure *closure_;
  ffi_cif cif_;
  ffi_type *rtype_;
  ffi_type **argtypes_;
  char rtype_char_;
  int nargs_;
  int nstackargs_;
  int stacksize_;
  int *stackoffsets_;
  int int_regs_used_;
  int float_regs_used_;
  enum argProcessType *arg_store_type_; //store the args place
  int *arg_store_offsets_; //will replace stackoffsets_, because it contains
  enum retProcessType ret_type_;
  pthread_key_t call_data_key_;
  pthread_key_t call_ret_key_;
};

class JNITrampoline : public Trampoline
{
public:
  JNITrampoline(const std::string& name, intptr_t address, const std::string& shorty);

protected:
  void *get_call_argument(int index, void *arg) override;

private:
  std::string shorty_;
};

class JNILoadTrampoline : public Trampoline
{
public:
  JNILoadTrampoline(const std::string& name, intptr_t address);

protected:
  void *get_call_argument(int index, void *arg) override;
};

class NativeActivityTrampoline : public Trampoline
{
public:
  NativeActivityTrampoline(const std::string& name, intptr_t address);

protected:
  void call(void *ret, void **args) override;
};

#endif

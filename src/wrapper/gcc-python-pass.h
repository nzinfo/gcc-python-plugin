/*
   Copyright 2021 Li Monan <li.monan@gmail.com>

   This is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see
   <http://www.gnu.org/licenses/>.
*/

/* 配合 pass manger 提供 gcc-pass 有关的 module、 class */

#ifndef GCC_PYTHON_PASS_H
#define GCC_PYTHON_PASS_H


#include <Python.h>
#include <pybind11/embed.h>

namespace py = pybind11;
// using namespace py::literals;
/*
 *  Pass 的设计与改造方案
 *
 *  在 davidmalcolm 版本中，使用 如下的 Python 代码实现 自定义 Pass 的注册
 *
 *  ```
 *  # Here's the (trivial) implementation of our new pass:
 *  class MyPass(gcc.GimplePass):
 *      # This is optional.
 *      # If present, it should return a bool, specifying whether or not
 *      # to execute this pass (and any child passes)
 *      def gate(self, fun):
 *          print('gate() called for %r' % fun)
 *          return True
 *
 *      def execute(self, fun):
 *          print('execute() called for %r' % fun)
 *
 *      # We now create an instance of the class:
 *      my_pass = MyPass(name='my-pass')
 *      # ...and wire it up, after the "cfg" pass:
 *      my_pass.register_after('cfg')
 *  ```
 *
 *  其具体逻辑为 在 类型 gcc.GimplePass 的 初始化部分 tp_init ，封装、构造 `struct opt_pass` ，并使用这个结构体的内存地址作为 key 注册到全局对象 `pass_wrapper_cache` 中
 *
 *      1. 因为额外在 `pass_wrapper_cache` ，还对 gc 模块进行了特殊处理，需要对 GGC 模块额外增加引用计数
 *      2. 当调用 register_pass 时，将 MyPass 中封装好的 `struct opt_pass`  取出， 再注册到 gcc 系统中，通过 impl_register -> register_pass
 *      3. gcc 实际调用回调时，通过 PyGccPass_New -> PyGcc_LazilyCreateWrapper 从 全局对象 `pass_wrapper_cache` get 或 按需创建对应的 封装（ 依据内置的 Pass 类型表 ）
 *      4. 取到 对应的 PyGccPass 对象后，调用其 `gate` 或 `execute`
 *
 *  存在的问题：
 *
 *      - 在 Linux 上 ，gcc 默认使用 g++ 编译，并开启了 -fno-rtti , 但是 pybind11 要求必须开启 -frtti ，这导致 C++ 符号使用了不同的 Name Mangling 规则，运行时报找不到符号。
 *
 *  需要额外引入 使用 -fno-rtti 的 动态库，并提供 API ，实现
 *
 *      1. 根据输入的 gcc `struct opt_pass` 封装对应的 GccPass 对象 ， 并注册到 动态库 自行维护的对象缓存中。
 *      2. 对外暴露 C API
 *
 *          - pygcc_pass_manger_init: () -> pass_env *
 *          - pygcc_pass_manger_cleanup: pass_env* -> ()
 *          - pygcc_get_pass_lazy: struct opt_pass -> GccPass*
 *          - pygcc_set_callback:  GccPass* -> cb_impl_gate -> cb_impl_execute -> void* data
 *          - pygcc_set_opt_pass_cb: all callbacks in ipa_opt_pass_d.
 *
 *
 *   调整步骤：
 *
 *      1. 构造 对象 pass_manger , 类型为 Gcc_PassManager ， 默认只有一个对象，绑定到 module gcc.
 *      2. Gcc_PassManager 实现函数
 *          -> register_pass { after | before | replace }
 *          -> get_pass_by_name
 *      3. 在 Python 空间构造 GCC 对应 Pass 的封装
 * */
#include "python-ggc-wrapper.h"

#include <gcc-plugin.h>
#include <gimple.h>
#include <tree.h>
#include <tree-pass.h>

void ggc_marker(opt_pass*);

class PyGccPass : public GCCTraceBase<opt_pass*>{
public:
    //struct opt_pass *pass;
    explicit PyGccPass(opt_pass* v): GCCTraceBase<opt_pass*>(v), passName_(v->name) {};
public:
    const std::string &getName() const;

private:
    std::string passName_;
};


int
PyGccPass_TypeInit(py::module_& m);

#endif

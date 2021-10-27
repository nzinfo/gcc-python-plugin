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

#include "python-ggc-wrapper.h"


#include <gcc-plugin.h>
#include <tree.h>
#include <tree-pass.h>

#include "wrapper/gcc-python-pass.h"

class PyGccPassManager {
public:
    // 导入 GCC 的标准枚举
    using PassPosition = enum pass_positioning_ops;

    static PyGccPassManager& instance() {
        static PyGccPassManager theSingleton;
        return theSingleton;
    };
public:
    // Python API
    // int register(struct PyGccPass* pass, PassPosition pos, int instance_number);
    int register_pass(PyGccPass& p, PassPosition pos, int instance_number);
    PyGccPass* get(const std::string& name);

    int cleanup() {
        return 0;
    };
private:

};

/*
int PyGccPassManager::register(PyGccPass* pass, PassPosition pos, int instance_number) {
    return 0;
};
 */
int PyGccPassManager::register_pass(PyGccPass& p, PassPosition pos, int instance_number){
    return 0;
}

PyGccPass* PyGccPassManager::get(const std::string& name) {
    return NULL;
}

int
PyGcc_pass_manager_init(py::module_& m)
{
    /*
     * 创建 Pass Manager 类，并将其实例化。
     * */
    py::class_<PyGccPassManager> pass_manger(m, "PassManager");

    pass_manger
            .def("register_pass",  [](PyGccPassManager& self, PyGccPass& pass, PyGccPassManager::PassPosition pos, int instance_number) {
                    return self.register_pass(pass, pos, instance_number);
                }, py::arg().noconvert(), py::arg().noconvert(), py::arg("instance_number") = 0)
            .def("get", &PyGccPassManager::get, py::return_value_policy::reference);

    //.def_static("__new__", [](py::object) { return Singleton::instance(); },
            //            py::return_value_policy::reference_internal);
    py::enum_<PyGccPassManager::PassPosition>(pass_manger, "PassPosition")
            .value("INSERT_BEFORE", PyGccPassManager::PassPosition::PASS_POS_INSERT_BEFORE)
            .value("INSERT_AFTER", PyGccPassManager::PassPosition::PASS_POS_INSERT_AFTER)
            .value("REPLACE", PyGccPassManager::PassPosition::PASS_POS_REPLACE)
            .export_values();
    // 设置 Pass Props 作为 int 型的常量

#define DEFPROP(e) \
       pass_manger.attr(#e) = e;

    // py::enum_<plugin_event>(m, "PassProps", py::arithmetic(), "Pass properties")
    // used in cfun & pasd_data as flags.
    // PyModule_AddIntMacro(PyGcc_globals.module, PROP_gimple_any);
    /*   Pass properties.  */
    DEFPROP(PROP_gimple_any)            /* entire gimple grammar */
    DEFPROP(PROP_gimple_lcf)            /* lowered control flow */
    DEFPROP(PROP_gimple_leh)            /* lowered eh */
    DEFPROP(PROP_cfg)
    DEFPROP(PROP_ssa)
    DEFPROP(PROP_no_crit_edges)
    DEFPROP(PROP_rtl)
    DEFPROP(PROP_gimple_lomp)           /* lowered OpenMP directives */
    DEFPROP(PROP_cfglayout)             /* cfglayout mode on RTL */
    DEFPROP(PROP_gimple_lcx)            /* lowered complex */
    DEFPROP(PROP_loops)			        /* preserve loop structures */
    DEFPROP(PROP_gimple_lvec)           /* lowered vector */
    DEFPROP(PROP_gimple_eomp)           /* no OpenMP directives */
    DEFPROP(PROP_gimple_lva)            /* No va_arg internal function.  */
    // up to gcc 9.3 ?
    DEFPROP(PROP_gimple_opt_math)       /* Disable canonicalization of math functions; the current choices have been optimized.  */
    DEFPROP(PROP_gimple_lomp_dev)           /* done omp_device_lower */
    DEFPROP(PROP_rtl_split_insns)           /* RTL has insns split.  */
    DEFPROP(PROP_trees)                      /*  (PROP_gimple_any | PROP_gimple_lcf | PROP_gimple_leh | PROP_gimple_lomp) */

#if (GCC_VERSION >= 4008)
#else
    DEFPROP(PROP_referenced_vars)
#endif
# undef DEFPROP


    // 现有对象的引用，由 C++ 控制生命周期。
    m.def("passManager", []() { return PyGccPassManager::instance(); }, py::return_value_policy::reference);
    return 0;
}

void
PyGcc_pass_manager_cleanup(){
    PyGccPassManager::instance().cleanup();
}
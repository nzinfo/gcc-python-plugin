#include "python-ggc-wrapper.h"

#include <gcc-plugin.h>
#include "gcc-python.h"

#include "wrapper/gcc-python-pass.h"
#include "wrapper/gcc-python-current.h"

void PyGccCurrentEnvironment::Update(enum plugin_event ev) {
    current_event_ = ev;
    current_fun_ = cfun;
    /* Current position in real source file.  defaults is UNKNOWN_LOCATION */
    location_t input_location_;
    /* The function currently being compiled.  */
    struct function * current_fun_;
    /* 插件的概念，当期处理的插件事件 */
    enum plugin_event current_event_;
    /* This is used for debugging.  It allows the current pass to printed from anywhere in compilation.
        The variable current_pass is also used for statistics and plugins.  */
    opt_pass *current_pass_;
}


int
PyGccCurrentEnvironment_TypeInit(py::module_& m)
{
    py::class_<PyGccCurrentEnvironment> current_env(m, "CurrentEnvironment");

    // current_env.def_property_readonly("ints_property", [](py::object s) {
        /*
        current_env
                .def("register_pass",  [](PyGccPassManager& self, PyGccPass& pass, PyGccPassManager::PassPosition pos, int instance_number) {
                    return self.register_pass(pass, pos, instance_number);
                }, py::arg().noconvert(), py::arg().noconvert(), py::arg("instance_number") = 0)
                .def("get", &PyGccPassManager::get, py::return_value_policy::reference);
        */
//.def_static("__new__", [](py::object) { return Singleton::instance(); },
//            py::return_value_policy::reference_internal);

    m.def("current", []() { return PyGccCurrentEnvironment::instance(); }, py::return_value_policy::reference);
}

void
PyGccCurrentEnvironment_Cleanup(void){
    PyGccCurrentEnvironment::instance().cleanup();
}

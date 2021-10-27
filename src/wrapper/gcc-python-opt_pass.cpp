#include "python-ggc-wrapper.h"

#include <gcc-plugin.h>
#include "gcc-python.h"

#include "wrapper/gcc-python-pass.h"

void ggc_marker(opt_pass*) {
    // 推迟到 运行时
    //assert(false && "Needs Implementation.");
    // gt_ggc_mx_opt_pass(pass);
    // do nothing , opt_pass not declared as  GTY(()) , so it will NOT be garbage collected, and no such function gt_ggc_mx_opt_pass
}

const std::string &
PyGccPass::getName() const {
    return this->passName_;
}

int
PyGcc_PyGccPass_TypeInit(py::module_& m) {

    py::class_<PyGccPass> gcc_pass (m, "GccPass");

    gcc_pass.def("__repr__",
         [](const PyGccPass &a) {
             return a.getName();
         }
    );
    /*
    pass_manger
            .def("register_pass",  [](PyGccPassManager& self, PyGccPass& pass, PyGccPassManager::PassPosition pos, int instance_number) {
                return self.register_pass(pass, pos, instance_number);
            }, py::arg().noconvert(), py::arg().noconvert(), py::arg("instance_number") = 0)
            .def("get", &PyGccPassManager::get, py::return_value_policy::reference);
    */

    return 0;
}
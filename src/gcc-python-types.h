
#ifndef GCC_PYTHON_PLUGIN_GCC_PYTHON_TYPES_H
#define GCC_PYTHON_PLUGIN_GCC_PYTHON_TYPES_H

// 用于统一初始化 使用 python 对象包装的 gcc 类型
#include "wrapper/gcc-python-pass.h"
#include "wrapper/gcc-python-current.h"

int
PyGcc_TypeInit(py::module_& m) {
    int rs = 0;
    rs = PyGccPass_TypeInit(m);
    // rs = PyGccPass_TypeInit(m);
    rs = PyGccCurrentEnvironment_TypeInit(m);
    return rs;
}

void
PyGcc_TypeCleanup(void) {
    PyGccCurrentEnvironment_Cleanup();
}


#endif //GCC_PYTHON_PLUGIN_GCC_PYTHON_TYPES_H

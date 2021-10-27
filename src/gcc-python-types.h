
#ifndef GCC_PYTHON_PLUGIN_GCC_PYTHON_TYPES_H
#define GCC_PYTHON_PLUGIN_GCC_PYTHON_TYPES_H

// 用于统一初始化 使用 python 对象包装的 gcc 类型
#include "wrapper/gcc-python-pass.h"

int
PyGcc_TypeInit(py::module_& m) {
    int rs = PyGcc_PyGccPass_TypeInit(m);
    return rs;
}

#endif //GCC_PYTHON_PLUGIN_GCC_PYTHON_TYPES_H

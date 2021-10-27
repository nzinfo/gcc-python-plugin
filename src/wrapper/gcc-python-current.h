//
// Created by limn on 2021/10/27.
//

#ifndef GCC_PYTHON_PLUGIN_GCC_PYTHON_CURRENT_H
#define GCC_PYTHON_PLUGIN_GCC_PYTHON_CURRENT_H

/*
 * 在 GCC 的上下文，提供对全局变量的访问。
 *
 * - current_pass
 * - current_event
 * - current_func
 * - current_location
 *
 * */

#include "python-ggc-wrapper.h"

#include <gcc-plugin.h>
#include <gimple.h>
#include <tree.h>
#include <tree-pass.h>


class PyGccCurrentEnvironment {

public:
    explicit PyGccCurrentEnvironment() {
        Update(PLUGIN_EVENT_FIRST_DYNAMIC);
    };

    static PyGccCurrentEnvironment& instance() {
        static PyGccCurrentEnvironment theSingleton;
        return theSingleton;
    };

    // 更新要使用 gcc.current 的 值， 从 gcc 的全局变量
    void Update(enum plugin_event ev);

    // 允许 current_env 在系统退出之前 释放持有的 资源，主要可能是 python 相关的对象。
    void cleanup() {};
private:
    /* Current position in real source file.  defaults is UNKNOWN_LOCATION */
    location_t input_location_;
    /* The function currently being compiled.  */
    struct function * current_fun_;
    /* 插件的概念，当期处理的插件事件 */
    enum plugin_event current_event_;
    /* This is used for debugging.  It allows the current pass to printed from anywhere in compilation.
        The variable current_pass is also used for statistics and plugins.  */
    opt_pass *current_pass_;
};

// TODO: update input location for error reporting

int
PyGccCurrentEnvironment_TypeInit(py::module_& m);

void
PyGccCurrentEnvironment_Cleanup(void);

#endif //GCC_PYTHON_PLUGIN_GCC_PYTHON_CURRENT_H

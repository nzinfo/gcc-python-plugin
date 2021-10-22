/*
    TODO: GPL License

    Copyright 2011-2013, 2015, 2017 David Malcolm <dmalcolm@redhat.com>
    Copyright 2011-2013, 2015, 2017 Red Hat, Inc.

    Copyright 2021 Li Monan <li.monan@gmail.com>
*/
#ifndef GCC_PYTHON_PLUGIN_GGC_WRAPPER_H
#define GCC_PYTHON_PLUGIN_GGC_WRAPPER_H

#include <Python.h>
#include <pybind11/embed.h>
#include <cassert>
#include "gcc-python-config.h"
/*
 * 与 GCC 的 GC 集成，避免 PyObject 内部引用的 GCC 数据结构被 GC 掉。
 * */

/*
 * 与 之前 gcc-python-plugin 在 Python Object 的 MetaType 上构造 双向链表进行追踪不同，这一版本的 gcc-python-plugin2
 * 使用 C++ 的 继承关系 构造满足 Gcc GC 约束条件的对象。 GCCTraceRoot<GCC_T>
 *  当构造时，自动将自身放入 GC 需要 mark 的双向链表中；当析构时，在从双向链表 移除。
 * */

// 维护双向链表的基类
class GCCTraceRoot {
public:
    GCCTraceRoot *wr_prev;
    GCCTraceRoot *wr_next;
};

// 构造虚函数表
class GCCTraceMarkable : public GCCTraceRoot {
public:
    virtual void ggc_marker() = 0;
};

extern GCCTraceRoot g_Sentinel;

template<typename GCC_T>
void ggc_marker(GCC_T obj) {
    // 推迟到 运行时
    assert(false && "Needs Implementation.");
}

// 处理不同 Gcc 内部结构的模板宏, 所有包装 GCC 内部数据结构的对象，都需要从此类型继承。
template<typename GCC_T>
class GCCTraceBase : public GCCTraceMarkable {
public:
    GCCTraceBase(GCC_T value) : value_(value) {
        // add this object to tracking list.
        GCCTraceRoot* sentinel = &g_Sentinel;
        sentinel->wr_prev->wr_next = this;
        this->wr_prev = sentinel->wr_prev;
        this->wr_next = sentinel;
        sentinel->wr_prev = this;
    }
    virtual ~GCCTraceBase() {
        // remove this from tracking list.
        GCCTraceRoot* sentinel = &g_Sentinel;
        if (this->wr_prev) {
            assert(sentinel->wr_next);
            assert(sentinel->wr_prev);
            assert(this->wr_next);

            /* Remove from linked list: */
            this->wr_prev->wr_next = this->wr_next;
            this->wr_next->wr_prev = this->wr_prev;
            this->wr_prev = NULL;
            this->wr_next = NULL;
        }
    }

    void ggc_marker() override {
        ggc_marker(value_);
    }
protected:
    GCC_T value_;
};

// end of C++ TraceGC

namespace py = pybind11;

/* python-ggc-wrapper.cpp */
void
PyGcc_ggc_init(void);

PyObject *
PyGcc__force_garbage_collection(PyObject *self, PyObject *args);

PyObject *
PyGcc__gc_selftest(PyObject *self, PyObject *args);


/* gcc-python-callbacks.cpp: */
// int PyGcc_IsWithinEvent(enum plugin_event *out_event);

// 注册 script 提供的回调函数 作为 gcc 的回调 ， 返回 None | Error
bool
PyGcc_RegisterCallback(long eventEnum, py::function callback_fn, py::args args, const py::kwargs& kwargs);
//PyGcc_RegisterCallback(py::object callback_fn, py::args args, const py::kwargs& kwargs);

#endif //GCC_PYTHON_PLUGIN_GGC_WRAPPER_H

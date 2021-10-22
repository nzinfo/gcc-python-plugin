/*
   Copyright 2011 David Malcolm <dmalcolm@redhat.com>
   Copyright 2011 Red Hat, Inc.

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
#include "gcc-python.h"


typedef struct callback_closure
{
    callback_closure * next_closure_;
    py::function callback_fn;
    const py::args args;
    const py::kwargs kwargs;
    enum plugin_event event;
    /* or GCC_PYTHON_PLUGIN_BAD_EVENT if not an event */
}callback_closure;

// 使用 xxx_sentinel 构造一个环。
callback_closure g_callback_sentinel = {
        &g_callback_sentinel,
        py::none(),
        py::none(),
        py::none(),
        (enum plugin_event)GCC_PYTHON_PLUGIN_BAD_EVENT,
};

callback_closure g_plugin_finish_callback_sentinel = {
        &g_plugin_finish_callback_sentinel,
        py::none(),
        py::none(),
        py::none(),
        (enum plugin_event)GCC_PYTHON_PLUGIN_BAD_EVENT,
};

struct callback_closure *
PyGcc_closure_new_generic(py::function callback_fn, py::args args, const py::kwargs& kwargs)
{
    /*
     *  在 David 版本的 gcc-python-plugin 中，使用 PyMem_New 构造了一个 对 Python 不透明的内存数据块。
     *
     *  在这一版本中， 考虑到 callback 一旦注册无法取消。 因此对应的 callback_closure 可以存在于 plugin 的整个生命周期。
     *      因此，只需要在 plugin 卸载时 清除内存即可（避免 GCC 的编译过程 leaky memory)
     *  考虑到 PLUGIN_FINISH 事件本身 也可作为 Python 的回调注册，因此当
     *      注册过 PLUGIN_FINISH 事件时，对应的 callback_closure 应单独处理。
     * */

    callback_closure *closure = new callback_closure{
        NULL,
        callback_fn,
        args,
        kwargs,
        (enum plugin_event)GCC_PYTHON_PLUGIN_BAD_EVENT
    };
    return closure;
}

struct callback_closure *
PyGcc_Closure_NewForPluginEvent(py::function callback_fn, py::args args, const py::kwargs& kwargs, enum plugin_event event)
{
    struct callback_closure *closure = PyGcc_closure_new_generic(callback_fn, args, kwargs);
    if (closure) {
        closure->event = event;
    }
    // register callback
    if(event == PLUGIN_FINISH) {
      // plugin finished callback.
        callback_closure * iter = &g_plugin_finish_callback_sentinel;
        while(iter->next_closure_ != &g_plugin_finish_callback_sentinel)
            iter = iter->next_closure_;
        // insert closure
        closure->next_closure_ = iter->next_closure_; // to &g_plugin_finish_callback_sentinel
        iter->next_closure_ = closure;
    } else {
        // other normal callback.
        callback_closure * iter = &g_callback_sentinel;
        while(iter->next_closure_ != &g_callback_sentinel)
            iter = iter->next_closure_;
        // insert closure
        closure->next_closure_ = iter->next_closure_; // to &g_callback_sentinel
        iter->next_closure_ = closure;
    }
    return closure;
}

py::object
PyGcc_ClosureInvoke(int expect_wrapped_data, py::object  wrapped_gcc_data, void *user_data)
{
    callback_closure *closure = (callback_closure *)user_data;
    assert(closure);
    // gcc_location saved_loc = gcc_get_input_location();

    py::function callback_fn = py::reinterpret_borrow<py::function>(closure->callback_fn);
    return callback_fn(closure->args, closure->kwargs);
}

void clear_callback_closures(){
    // 清除 除 PLUGIN_FINISH 外 的回调结构。
    callback_closure * iter = &g_callback_sentinel;
    while(iter->next_closure_ != &g_callback_sentinel) {
        callback_closure * obj = iter->next_closure_;
        // remove obj from the list.
        iter->next_closure_ = obj->next_closure_;
        delete obj;
    }
}

void clear_plugin_finish_callback_closures(struct callback_closure * closure){
    assert(closure);
    assert(closure != &g_callback_sentinel);

    callback_closure * iter = &g_callback_sentinel;
    while(iter->next_closure_ != &g_callback_sentinel) {
        if (iter->next_closure_ == closure) {
            iter->next_closure_ = closure->next_closure_;
            delete closure;
        }
        iter = iter->next_closure_;
    }
}

#if 0
#include "gcc-python-closure.h"
#include "gcc-python.h"
#include "function.h"

#include "gcc-c-api/gcc-function.h"

struct callback_closure *
PyGcc_closure_new_generic(PyObject *callback, PyObject *extraargs, PyObject *kwargs)
{
    struct callback_closure *closure;

    assert(callback);
    /* extraargs can be NULL
       kwargs can also be NULL */

    closure = PyMem_New(struct callback_closure, 1);
    if (!closure) {
        return NULL;
    }
    
    closure->callback = callback;
    Py_INCREF(callback);

    // FIXME: we may want to pass in the event enum as well as the user-supplied extraargs

    if (extraargs) {
	/* Hold a reference to the extraargs for when we register it with the
	   callback: */
	closure->extraargs = extraargs;
	Py_INCREF(extraargs);
    } else {
	closure->extraargs = PyTuple_New(0);
	if (!closure->extraargs) {
	    return NULL;  // singleton, so can't happen, really
	}
    }

    closure->kwargs = kwargs;
    if (kwargs) {
	Py_INCREF(kwargs);
    }

    closure->event = (enum plugin_event)GCC_PYTHON_PLUGIN_BAD_EVENT;

    return closure;
}

struct callback_closure *
PyGcc_Closure_NewForPluginEvent(PyObject *callback, PyObject *extraargs, PyObject *kwargs,
                                        enum plugin_event event)
{
    struct callback_closure *closure = PyGcc_closure_new_generic(callback, extraargs, kwargs);
    if (closure) {
        closure->event = event;
    }
    return closure;
}

PyObject *
PyGcc_Closure_MakeArgs(struct callback_closure * closure, int add_cfun, PyObject *wrapped_gcc_data)
{
    PyObject *args = NULL;
    PyObject *cfun_obj = NULL;
    int i;

    assert(closure);
    /* wrapped_gcc_data can be NULL if there isn't one for this kind of callback */
    assert(closure->extraargs);
    assert(PyTuple_Check(closure->extraargs));
    
    if (wrapped_gcc_data) {
 	/* 
	   Equivalent to either:
	     args = (gcc_data, cfun, ) + extraargs
           or:
	     args = (gcc_data, ) + extraargs
	 */
        args = PyTuple_New((add_cfun ? 2 : 1) + PyTuple_Size(closure->extraargs));

	if (!args) {
	    goto error;
	}

        if (add_cfun) {
            cfun_obj = PyGccFunction_New(gcc_get_current_function());
            if (!cfun_obj) {
                goto error;
            }
        }

	PyTuple_SetItem(args, 0, wrapped_gcc_data);
        if (add_cfun) {
            PyTuple_SetItem(args, 1, cfun_obj);
        }
	Py_INCREF(wrapped_gcc_data);
	for (i = 0; i < PyTuple_Size(closure->extraargs); i++) {
	    PyObject *item = PyTuple_GetItem(closure->extraargs, i);
	    PyTuple_SetItem(args, i + (add_cfun ? 2 : 1), item);
	    Py_INCREF(item);
	}
	
	return args;
	
    } else {
	/* Just reuse closure's extraargs tuple */
	Py_INCREF(closure->extraargs);
	return closure->extraargs;
    }

 error:
    Py_XDECREF(args);
    Py_XDECREF(cfun_obj);
    return NULL;
}


void
PyGcc_closure_free(struct callback_closure *closure)
{
    assert(closure);

    Py_XDECREF(closure->callback);
    Py_XDECREF(closure->extraargs);
    Py_XDECREF(closure->kwargs);

    PyMem_Free(closure);
}

#endif

/*
  PEP-7  
Local variables:
c-basic-offset: 4
indent-tabs-mode: nil
End:
*/

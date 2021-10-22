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

#include "gcc-python-closure.h"

extern const char* event_name[];
extern const char* g_plugin_name;

static void
PyGcc_FinishInvokingCallback(int expect_wrapped_data, PyObject *wrapped_gcc_data,
                             void *user_data)
{
    struct callback_closure *closure = (struct callback_closure *)user_data;
    assert(closure);
    // gcc_location saved_loc = gcc_get_input_location();
    // py::function callback_fn = py::reinterpret_borrow<py::function>(closure);
    // py::object result = callback_fn(std::vector<double>{1,2,3,4,5});
}


static void
PyGcc_CallbackFor_PLUGIN_FINISH(void *gcc_data, void *user_data)
{
    // PyGILState_STATE gstate;
    py::gil_scoped_acquire acquire;
    printf("%s:%i:(%p, %p)\n", __FILE__, __LINE__, gcc_data, user_data);

    // gstate = PyGILState_Ensure();

    PyGcc_FinishInvokingCallback(0, NULL,
                                 user_data);
}


// 将封装好的 callback_closure 注册到回调。
bool
PyGcc_RegisterCallback(long eventEnum, py::function callback_fn, py::args extra_args, const py::kwargs& kwargs) {
    /*
     *  将 py::functor 封装为 gcc 识别的回调结构
     *
     *  if (!PyArg_ParseTuple(args, "iO|O:register_callback", &event, &callback, &extraargs))
     *  原 接口的封装为  "iO|O:register_callback"
     *  调整过之后， callback 无法通过 register_callback= 的方式传入
     * */

    /* Acquire GIL before calling Python code */
    py::gil_scoped_acquire acquire;

    struct callback_closure *closure = PyGcc_Closure_NewForPluginEvent(callback_fn, extra_args, kwargs,
                                              (enum plugin_event)eventEnum);

    if (!closure) {
        // TODO: some error happend. NoMemory.
        //return py::reinterpret_steal<py::object>(PyErr_NoMemory());
        PyErr_SetNone(PyExc_MemoryError);
        return false;
    }

    switch ((enum plugin_event)eventEnum) {
        case PLUGIN_FINISH:
            register_callback(g_plugin_name, (enum plugin_event)eventEnum,
                              PyGcc_CallbackFor_PLUGIN_FINISH, closure);
            break;
        default:
            // warning: required to check eventEnum < PLUGIN_INCLUDE_FILE
            // PyErr_Format(PyExc_ValueError, "event type %s(%i) invalid (or not wired up yet)", event_name[eventEnum], eventEnum);
            PyErr_Format(PyExc_ValueError, "event type %i invalid (or not wired up yet)", eventEnum);
            return false;
    }
    /*
    struct callback_closure *closure = PyGcc_closure_new_generic(callback_fn, args, kwargs);
    if (closure) {
        closure->event = event;
    }
    return closure;
    */
    return true;
}

#if 0
PyObject*
PyGcc_RegisterCallback(PyObject *self, PyObject *args, PyObject *kwargs)
{
    int event;
    PyObject *callback = NULL;
    PyObject *extraargs = NULL;
    struct callback_closure *closure;

    if (!PyArg_ParseTuple(args, "iO|O:register_callback", &event, &callback, &extraargs)) {
        return NULL;
    }

    printf("%s:%i:PyGcc_RegisterCallback\n", __FILE__, __LINE__);

    closure = PyGcc_Closure_NewForPluginEvent(callback, extraargs, kwargs,
                                              (enum plugin_event)event);
    if (!closure) {
        return PyErr_NoMemory();
    }

    switch ((enum plugin_event)event) {
        case PLUGIN_ATTRIBUTES:
            register_callback("pygcc", // FIXME
                              (enum plugin_event)event,
                              PyGcc_CallbackFor_PLUGIN_ATTRIBUTES,
                              closure);
            break;

        case PLUGIN_PRE_GENERICIZE:
            register_callback("pygcc", // FIXME
                              (enum plugin_event)event,
                              PyGcc_CallbackFor_tree,
                              closure);
            break;

        case PLUGIN_PASS_EXECUTION:
            register_callback("pygcc", // FIXME
                              (enum plugin_event)event,
                              PyGcc_CallbackFor_PLUGIN_PASS_EXECUTION,
                              closure);
            break;

        case PLUGIN_FINISH:
            register_callback("pygcc", // FIXME
                              (enum plugin_event)event,
                              PyGcc_CallbackFor_FINISH,
                              closure);
            break;

        case PLUGIN_FINISH_UNIT:
            register_callback("pygcc", // FIXME
                              (enum plugin_event)event,
                              PyGcc_CallbackFor_FINISH_UNIT,
                              closure);
            break;

        case PLUGIN_FINISH_TYPE:
            register_callback("pygcc", // FIXME
                              (enum plugin_event)event,
                              PyGcc_CallbackFor_tree,
                              closure);
            break;

        case PLUGIN_GGC_START:
            register_callback("pygcc", // FIXME
                              (enum plugin_event)event,
                              PyGcc_CallbackFor_GGC_START,
                              closure);
            break;
        case PLUGIN_GGC_MARKING:
            register_callback("pygcc", // FIXME
                              (enum plugin_event)event,
                              PyGcc_CallbackFor_GGC_MARKING,
                              closure);
            break;
        case PLUGIN_GGC_END:
            register_callback("pygcc", // FIXME
                              (enum plugin_event)event,
                              PyGcc_CallbackFor_GGC_END,
                              closure);
            break;

            /* PLUGIN_FINISH_DECL was added in gcc 4.7 onwards: */
#ifdef GCC_PYTHON_PLUGIN_CONFIG_has_PLUGIN_FINISH_DECL
            case PLUGIN_FINISH_DECL:
        register_callback("pygcc", // FIXME
			  (enum plugin_event)event,
                          PyGcc_CallbackFor_tree,
			  closure);
	break;
#endif /* GCC_PYTHON_PLUGIN_CONFIG_has_PLUGIN_FINISH_DECL */

        default:
            PyErr_Format(PyExc_ValueError, "event type %i invalid (or not wired up yet)", event);
            return NULL;
    }

    Py_RETURN_NONE;
}
#endif

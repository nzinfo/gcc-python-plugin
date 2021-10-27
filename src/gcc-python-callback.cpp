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
#include "gcc-python.h"

#include <gcc-plugin.h>
#include "gcc-python-closure.h"

#include <gimple.h>
#include <tree.h>
#include <tree-pass.h>

// 用于封装 gcc 对象的类型
#include "wrapper/gcc-python-pass.h"

extern const char* event_name[];
extern const char* g_plugin_name;

/*
  Notes on the passes

  As of 2011-03-30, http://gcc.gnu.org/onlinedocs/gccint/Plugins.html doesn't
  seem to document the type of the gcc_data passed to each callback.

  For reference, with gcc-4.6.0-0.15.fc15.x86_64 the types seem to be as
  follows:

  PLUGIN_ATTRIBUTES:
    gcc_data=0x0
    Called from: init_attributes () at ../../gcc/attribs.c:187
    However, it seems at this point to have initialized these:
      static const struct attribute_spec *attribute_tables[4];
      static htab_t attribute_hash;

  PLUGIN_PRAGMAS:
    gcc_data=0x0
    Called from: c_common_init () at ../../gcc/c-family/c-opts.c:1052

  PLUGIN_START_UNIT:
    gcc_data=0x0
    Called from: compile_file () at ../../gcc/toplev.c:573

  PLUGIN_PRE_GENERICIZE
    gcc_data is:  tree fndecl;
    Called from: finish_function () at ../../gcc/c-decl.c:8323

  PLUGIN_OVERRIDE_GATE
    gcc_data:
      &gate_status
      bool gate_status;
    Called from : execute_one_pass (pass=0x1011340) at ../../gcc/passes.c:1520

  PLUGIN_PASS_EXECUTION
    gcc_data: struct opt_pass *pass
    Called from: execute_one_pass (pass=0x1011340) at ../../gcc/passes.c:1530

  PLUGIN_ALL_IPA_PASSES_START
    gcc_data=0x0
    Called from: ipa_passes () at ../../gcc/cgraphunit.c:1779

  PLUGIN_EARLY_GIMPLE_PASSES_START
    gcc_data=0x0
    Called from: execute_ipa_pass_list (pass=0x1011fa0) at ../../gcc/passes.c:1927

  PLUGIN_EARLY_GIMPLE_PASSES_END
    gcc_data=0x0
    Called from: execute_ipa_pass_list (pass=0x1011fa0) at ../../gcc/passes.c:1930

  PLUGIN_ALL_IPA_PASSES_END
    gcc_data=0x0
    Called from: ipa_passes () at ../../gcc/cgraphunit.c:1821

  PLUGIN_ALL_PASSES_START
    gcc_data=0x0
    Called from: tree_rest_of_compilation (fndecl=0x7ffff16b1f00) at ../../gcc/tree-optimize.c:420

  PLUGIN_ALL_PASSES_END
    gcc_data=0x0
    Called from: tree_rest_of_compilation (fndecl=0x7ffff16b1f00) at ../../gcc/tree-optimize.c:425

  PLUGIN_FINISH_UNIT
    gcc_data=0x0
    Called from: compile_file () at ../../gcc/toplev.c:668

  PLUGIN_FINISH_TYPE
    gcc_data=tree
    Called from c_parser_declspecs (parser=0x7fffef559730, specs=0x15296d0, scspec_ok=1 '\001', typespec_ok=1 '\001', start_attr_ok=<optimized out>, la=cla_nonabstract_decl) at ../../gcc/c-parser.c:2111

  PLUGIN_PRAGMA
    gcc_data=0x0
    Called from: init_pragma at ../../gcc/c-family/c-pragma.c:1321
    to  "Allow plugins to register their own pragmas."
*/

static void
PyGcc_CallbackFor_PLUGIN_PASS_EXECUTION(void *gcc_data, void *user_data)
{
    //  The event data is the included file path,
    //              as a const char* pointer.
    py::gil_scoped_acquire acquire;
    auto * opt = (opt_pass *)gcc_data;
    py::object result = PyGcc_ClosureInvoke(1, py::cast(PyGccPass(opt)),
                                            user_data);
}

static void
PyGcc_CallbackFor_PLUGIN_NEW_PASS(void *gcc_data, void *user_data) {
    // Called when a pass is first instantiated.
    //      in add_pass_instance@passes.c , opt_pass *new_pass -> gcc_data

    /*
     *  When your plugin is loaded, you can inspect the various pass lists to
     *  determine what passes are available.  However, other plugins might add
     *  new passes.  Also, future changes to GCC might cause generic passes to
     *  be added after plugin loading.  When a pass is first added to one of the
     *  pass lists, the event 'PLUGIN_NEW_PASS' is invoked, with the callback
     *  parameter 'gcc_data' pointing to the new pass.
     */

    py::gil_scoped_acquire acquire;
    auto * opt = (opt_pass *)gcc_data;
    py::object result = PyGcc_ClosureInvoke(1, py::cast(PyGccPass(opt)),
                                            user_data);
}

static void
PyGcc_CallbackFor_PLUGIN_INCLUDE_FILE(void *gcc_data, void *user_data)
{
    //  The event data is the included file path,
    //              as a const char* pointer.
    py::gil_scoped_acquire acquire;
    // callback_closure *closure = (callback_closure *)user_data;
    py::object result = PyGcc_ClosureInvoke(1, py::str((const char*)gcc_data),
                                 user_data);
}


static void
PyGcc_CallbackFor_PLUGIN_DEFAULT_NOTIFICATION(void *gcc_data ATTRIBUTE_UNUSED, void *user_data)
{
    /*
     * 用于 gcc_data 始终为 NULL, 0x0 的事件。
     * */
    py::gil_scoped_acquire acquire;

    py::object result = PyGcc_ClosureInvoke(0, py::none(),
                                            user_data);
    // TODDO: check result & error status ?
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

    callback_closure *closure = PyGcc_Closure_NewForPluginEvent(callback_fn, extra_args, kwargs,
                                              (enum plugin_event)eventEnum);

    if (!closure) {
        // TODO: some error happend. NoMemory.
        //return py::reinterpret_steal<py::object>(PyErr_NoMemory());
        PyErr_SetNone(PyExc_MemoryError);
        return false;
    }
    auto ev_type = (enum plugin_event)eventEnum;

#define REGISTER_EVENT_HANDLER(event) \
    case event: register_callback(g_plugin_name, ev_type, PyGcc_CallbackFor_##event, closure); break;

#define REGISTER_EVENT_HANDLER_DEFAULT(event) \
    case event: register_callback(g_plugin_name, ev_type, PyGcc_CallbackFor_PLUGIN_DEFAULT_NOTIFICATION, closure); break;

    switch (ev_type) {
        // TODO: support more event. defined in <plugin.def>
        case PLUGIN_FINISH:
            // The PLUGIN_FINISH event is the last time that plugins can call GCC functions, notably emit diagnostics with warning, error etc.
            // PLUGIN_FINISH 较为特殊，因为 系统用于清除所有数据的 回调 也使用同样的事件，且注册时机较 后面注册的同样的事件处理回调早
            // 因此，此处的对象会泄露到 PythonVM 析构之后，导致 UAF.
            // 所以，针对 事件类型 PLUGIN_FINISH 并不直接注册到 GCC 系统，而是复用插件主系统注册的那个回调。
            //register_callback(g_plugin_name, ,
            //                  PyGcc_CallbackFor_PLUGIN_FINISH, closure);
            break;
        REGISTER_EVENT_HANDLER_DEFAULT(PLUGIN_ALL_IPA_PASSES_START)
        REGISTER_EVENT_HANDLER(PLUGIN_PASS_EXECUTION)
        REGISTER_EVENT_HANDLER_DEFAULT(PLUGIN_EARLY_GIMPLE_PASSES_START)
        REGISTER_EVENT_HANDLER_DEFAULT(PLUGIN_EARLY_GIMPLE_PASSES_END)
        REGISTER_EVENT_HANDLER(PLUGIN_NEW_PASS)
        REGISTER_EVENT_HANDLER(PLUGIN_INCLUDE_FILE)
        default:
            // warning: required to check eventEnum < PLUGIN_INCLUDE_FILE
            // PyErr_Format(PyExc_ValueError, "event type %s(%i) invalid (or not wired up yet)", event_name[eventEnum], eventEnum);
            PyErr_Format(PyExc_ValueError, "event type %i invalid (or not wired up yet)", eventEnum);
            return false;
    }
#undef REGISTER_EVENT_HANDLER
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

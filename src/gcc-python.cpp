/*
   Copyright 2011-2013, 2015 David Malcolm <dmalcolm@redhat.com>
   Copyright 2011-2013, 2015 Red Hat, Inc.

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
#define _STDC_WANT_LIB_EXT1_ 1

#include <Python.h>
#include <pybind11/embed.h>
#include <gcc-plugin.h>
#include <plugin-version.h>

// gcc structure.
#if 1
/* Ideally we wouldn't have these includes here: */
#if (GCC_VERSION >= 6000)
#include <cp/cp-tree.h> /* for cp_expr */
#endif
#include <cp/name-lookup.h> /* for global_namespace */
#include <tree.h>
#include <tree-pass.h>
#include <function.h>
#include <cgraph.h>
//#include "opts.h"

/* "maybe_get_identifier" was moved from tree.h to stringpool.h in 4.9 */
#if (GCC_VERSION >= 4009)
#include <stringpool.h>
#endif

/*
 * Use an unqualified name here and rely on dual search paths to let the
 * compiler find it.  This deals with c-pragma.h moving to a
 * subdirectory in newer versions of gcc.
 */
// #include "c-pragma.h" /* for parse_in */
#endif

#include "gcc-python-settings.h"
#include "gcc-python.h"

#include "python-ggc-wrapper.h"
#include "gcc-python-types.h"

#if 0
#include "gcc-python-closure.h"
#include "gcc-python-wrappers.h"

#include "gcc-c-api/gcc-location.h"
#include "gcc-c-api/gcc-variable.h"
#include "gcc-c-api/gcc-declaration.h"
#include "gcc-c-api/gcc-diagnostics.h"
#include "gcc-c-api/gcc-option.h"
#endif

namespace py = pybind11;
using namespace py::literals;

/*
struct
{
    py::module_ module;
    // PyObject *argument_dict;
    // PyObject *argument_tuple;
} PyGcc_globals;
*/

char g_plugin_name_buffer[256]; // 直接使用 plugin_name_args 中的名称未尝不可，但是需要确保 plugin_name_args 不会被释放。偷懒起见，复制一份吧。
const char * g_plugin_name;

static bool g_FlagPyGcc_Debug;   // 调试信息开关

#define GCC_PYTHON_TRACE_ALL_EVENTS 1

/* trace events functions */
#if GCC_PYTHON_TRACE_ALL_EVENTS
const char* event_name[] = {
#define DEFEVENT(NAME) \
  #NAME,

# include "plugin.def"
  DEFEVENT(PLUGIN_EVENT_FIRST_DYNAMIC)
# undef DEFEVENT
};

static void
trace_callback(enum plugin_event event, void *gcc_data, void *user_data)
{
    fprintf(stderr,
	    "%s:%i:trace_callback(%s, %p, %p)\n",
	    __FILE__, __LINE__, event_name[event], gcc_data, user_data);
    fprintf(stderr, "  cfun: %p\n", (void*)cfun);
}

#define DEFEVENT(NAME) \
static void trace_callback_for_##NAME(void *gcc_data, void *user_data) \
{ \
     if(g_FlagPyGcc_Debug)                                             \
        trace_callback(NAME, gcc_data, user_data);                     \
}
# include "plugin.def"
# undef DEFEVENT
#endif /* GCC_PYTHON_TRACE_ALL_EVENTS */

#if 1
#define LOG(msg) \
    {             \
        if(g_FlagPyGcc_Debug)         \
            (void)fprintf(stderr, "%s:%i:%s\n", __FILE__, __LINE__, (msg)); \
    }
#else
#define LOG(msg) ((void)0);
#endif


// define 
PYBIND11_EMBEDDED_MODULE(gcc, m) {
    // `m` is a `py::module_` which is used to bind functions and classes
    m.def("add", [](int i, int j) {
        return i + j;
    });

    m.def("register_callback", &PyGcc_RegisterCallback);
#define DEFEVENT(e) \
    .value( #e , e)

    py::enum_<plugin_event>(m, "EVENT", py::arithmetic(), "GCC Plugin Events")
            # include "plugin.def"
            DEFEVENT(PLUGIN_EVENT_FIRST_DYNAMIC);
    // .export_values();
    // give the outer-python side the upper-bound of plugin. as PLUGIN_EVENT_FIRST_DYNAMIC
# undef DEFEVENT
    m.attr("GCC_VERSION") = GCC_VERSION;
}

static int
PyGcc_process_args(py::module_& m, struct plugin_name_args *plugin_info)
{
    int i = 0;
    
    py::dict args_dict;
    py::list args_list;

    for (i=0; i<plugin_info->argc; i++) {
        struct plugin_argument *arg = &plugin_info->argv[i];
        //key = PyGccString_FromString(arg->key);
        py::object value;
        auto key = py::reinterpret_steal<py::object>(PYBIND11_FROM_STRING(arg->key));
        if (arg->value) {
            // printf("arg: key=%s, value=%s\n", arg->key, plugin_info->argv[i].value);
            value = py::reinterpret_steal<py::object>(PYBIND11_FROM_STRING(arg->value));
        }else {
            // printf("arg: key=%s \n", arg->key);
            value = py::none();
        }

        args_dict[key] = value;
        
        py::tuple tup = py::make_tuple(key, value);
        args_list.append(tup);
    }
    
    // bind to module 
    m.attr("args") = args_dict;
    m.attr("args_list") = args_list;

    return 0;
}

static void PyGcc_run_any_command(const py::module_& m)
{
    // PyObject* command_obj; /* borrowed ref */
    int result;

    const py::dict & args_dict = py::reinterpret_steal<py::dict>(m.attr("args"));
    if (!args_dict.contains("command")) {
        return;
    }

    auto command_obj = args_dict["command"];
    py::str command_s(command_obj); // use the constructor

    if (0) {
        fprintf(stderr, "Running: %s\n", std::string(command_s).c_str());
    }


    result = PyRun_SimpleString(std::string(command_s).c_str());
    if (-1 == result) {
        /* Error running the python command */
        py::finalize_interpreter();
        exit(1);
    }
}

static void PyGcc_run_any_script(const py::module_& m)
{
    FILE *fp;
    int result;

    const py::dict & args_dict = py::reinterpret_steal<py::dict>(m.attr("args"));
    if (!args_dict.contains("script")) {
        return;
    }

    auto script_name_obj = args_dict["script"];

    if (script_name_obj.is_none()) {
        return;
    }

    py::str script_name_s(script_name_obj);
    auto script_filename = std::string(script_name_s);
    fp = fopen(script_filename.c_str(), "r");
    if (!fp) {
        fprintf(stderr,
                "Unable to read python script: %s\n",
                script_filename.c_str());
        exit(1);
    }
    result = PyRun_SimpleFile(fp, script_filename.c_str());
    fclose(fp);
    if (-1 == result) {
        /* Error running the python script */
        py::finalize_interpreter();
        exit(1);
    }
}

int
setup_sys(struct plugin_name_args *plugin_info)
{
    /*

     * Sets up "sys.plugin_full_name" as plugin_info->full_name.  This is the
     path to the plugin (as specified with -fplugin=)

     * Sets up "sys.plugin_base_name" as plugin_info->base_name.  This is the
     short name, of the plugin (filename without .so suffix)

     * Add the directory containing the plugin to "sys.path", so that it can
     find modules relative to itself without needing PYTHONPATH to be set up.
     (sys.path has already been initialized by the call to Py_Initialize)

     * If PLUGIN_PYTHONPATH is defined, add it to "sys.path"

    */

    const char *program =
            "import sys;\n"
            "import os;\n"
            "sys.path.append(os.path.abspath(os.path.dirname(sys.plugin_full_name)))\n";

    py::module_ sys = py::module_::import("sys");
    sys.attr("plugin_full_name") = py::reinterpret_steal<py::object>(PYBIND11_FROM_STRING(plugin_info->full_name));
    sys.attr("plugin_base_name") = py::reinterpret_steal<py::object>(PYBIND11_FROM_STRING(plugin_info->base_name));
    // 复制 base_name 到全局变量
    {
        g_plugin_name = g_plugin_name_buffer;
        strncpy(g_plugin_name_buffer, plugin_info->base_name, sizeof(g_plugin_name_buffer)-1);
        g_plugin_name_buffer[sizeof(g_plugin_name_buffer)-1] = 0;
        //assert(st == 0);
    }
    // extend sys.path.
    return PyRun_SimpleString(program);
}

// As a gcc-plugin, plugin_is_GPL_compatible has to be defined.
PLUGIN_API_TYPEDEF int plugin_is_GPL_compatible;

/*
  Wired up to PLUGIN_FINISH, this callback handles finalization for the plugin:
*/
void
on_plugin_finish(void *gcc_data, void *user_data ATTRIBUTE_UNUSED)
{
    /*
       Clean up the python runtime.

       For python 3, this flushes buffering of sys.stdout and sys.stderr
    */
    {
        py::gil_scoped_acquire acquire;
        // call plugin finish callback chain
        PyGcc_ProcessCallback_PLUGIN_FINISH(gcc_data);
        clear_callback_closures();

        // clean pass manager
        PyGcc_pass_manager_cleanup();
        // clean type registration.
        PyGcc_TypeCleanup();
    }
    py::finalize_interpreter();
}

// gcc plugin required initialize api.
#if 0       // link test failed on Linux.
extern int
plugin_init (struct plugin_name_args *plugin_info,
             struct plugin_gcc_version *version) __attribute__((nonnull));

class PyGccGimplePass;
#if (GCC_VERSION >= 4009)
/*
  GCC 4.9 converted passes to a C++ class hierarchy, with methods for gate
  and execute.
*/

static bool impl_gate(function *fun) {
    return true;
}

static unsigned int impl_execute(function *fun) {
    return 0;
}

#if (GCC_VERSION >= 5000)
/* GCC 5 added a "fun" param to the "gate" and "execute" vfuncs of
   pass opt_pass.  */
# define PASS_DECLARE_GATE_AND_EXECUTE                                  \
    bool gate (function *fun) { return impl_gate(fun); }                \
    unsigned int execute (function *fun) { return impl_execute(fun); }
#else
/* ...whereas in GCC 4.9 they took no params, with cfun being implied.  */
# define PASS_DECLARE_GATE_AND_EXECUTE                            \
    bool gate () { return impl_gate(cfun); }                      \
    unsigned int execute () { return impl_execute(cfun); }
#endif /* #if (GCC_VERSION >= 5000) */

class PyGccGimplePass : public gimple_opt_pass
{
public:
    PyGccGimplePass(const pass_data& data, gcc::context *ctxt) :
            gimple_opt_pass(data, ctxt)
    {
    }

    PASS_DECLARE_GATE_AND_EXECUTE
    opt_pass *clone() {return this; }
};
#endif
#endif // end test of link fno-rtti

PLUGIN_API_TYPEDEF  int
plugin_init (struct plugin_name_args *plugin_info,
             struct plugin_gcc_version *version)
{
    // check debug flag
    g_FlagPyGcc_Debug = false;
    if(std::getenv("PYGCC_DEBUG"))
        g_FlagPyGcc_Debug = true;

    LOG("plugin_init started");

    if (!plugin_default_version_check (version, &gcc_version)) {
        return 1;
    }

#if PY_MAJOR_VERSION >= 3
    /*
      Python 3 added internal buffering to sys.stdout and sys.stderr, but this
      leads to unpredictable interleavings of messages from gcc, such as from calling
      gcc.warning() vs those from python scripts, such as from print() and
      sys.stdout.write()

      Suppress the buffering, to better support mixed gcc/python output:
    */
    Py_UnbufferedStdioFlag = 1;
#endif

    py::initialize_interpreter(); // start the interpreter and keep it alive
    // py::scoped_interpreter guard{};  // cann't use scoped_interpreter, the interpreter will be terminated when gcc exit, not the scope exit.
    /*
    
    // use another scope dealing python stuff.

        { // scoped
            auto hello = py::str("Hello, World!");
        } // <-- OK, hello is cleaned up properly
    
    */

    // Deprecated function which does nothing.
    // Changed in version 3.7: This function is now called by Py_Initialize(), so you don’t have to call it yourself anymore.
    // PyEval_InitThreads();
    // 教训： 所有的 pybind11:: 名字空间中类型的变量，都只能存在于 py::initialize_interpreter 与 py::finalize_interpreter() 之间
    //       因此，不能在全局变量中使用 py::module ， PyGcc_globals 被整体移除。
    // start python scope.
    {
        //PyGcc_globals.module = PyImport_ImportModule("gcc");
        py::module_ module_gcc = py::module_::import("gcc");

        /* Set up int constants for each of the enum plugin_event values: */

        if (0 != PyGcc_process_args(module_gcc, plugin_info)) {
            return 1;
        }

        if (-1 == setup_sys(plugin_info)) {
            return 1;
        }

        // 初始化 python-ggc 集成的接口。
        PyGcc_ggc_init();

        // 初始化 callbacks 需要的参数类型的 Python 封装。
        PyGcc_TypeInit(module_gcc);

        // 初始化 pass_manger
        PyGcc_pass_manager_init(module_gcc);

        /*
        {
            pass_data data;
            PyGccGimplePass* pass = new PyGccGimplePass(data, NULL);
        }
        */

        // execute user_script.
        PyGcc_run_any_command(module_gcc);
        PyGcc_run_any_script(module_gcc);
    } 
    // end python scope.

    {
#if GCC_PYTHON_TRACE_ALL_EVENTS

#if GCC_VERSION < 6000
    auto GGC_ROOTS = PLUGIN_REGISTER_GGC_CACHES;
#else
    auto GGC_ROOTS = PLUGIN_REGISTER_GGC_ROOTS;
#endif

#define DEFEVENT(NAME) \
    if (NAME != PLUGIN_PASS_MANAGER_SETUP &&         \
        NAME != PLUGIN_INFO &&                       \
        NAME != GGC_ROOTS) {        \
    register_callback(plugin_info->base_name, NAME,  \
		      trace_callback_for_##NAME, NULL); \
    }
# include <plugin.def>
# undef DEFEVENT
#endif /* GCC_PYTHON_TRACE_ALL_EVENTS */
    }

#if 0

    /* Init other modules */
    PyGcc_wrapper_init();
    if (PyErr_Occurred()) {
        LOG("PyGcc_wrapper_init failed.");
        PyErr_Print();
    }

    /* FIXME: properly integrate them within the module hierarchy */

    PyGcc_version_init(version);

    autogenerated_callgraph_init_types();  /* FIXME: error checking! */
    autogenerated_cfg_init_types();  /* FIXME: error checking! */
    autogenerated_function_init_types();  /* FIXME: error checking! */
    autogenerated_gimple_init_types();  /* FIXME: error checking! */
    autogenerated_location_init_types();  /* FIXME: error checking! */
    autogenerated_option_init_types();  /* FIXME: error checking! */

#if (GCC_VERSION < 10000)
    autogenerated_parameter_init_types();  /* FIXME: error checking! */
#endif
    autogenerated_pass_init_types();  /* FIXME: error checking! */
    autogenerated_pretty_printer_init_types();  /* FIXME: error checking! */
    autogenerated_rtl_init_types(); /* FIXME: error checking! */
    autogenerated_tree_init_types(); /* FIXME: error checking! */
    autogenerated_variable_init_types(); /* FIXME: error checking! */

    autogenerated_callgraph_add_types(PyGcc_globals.module);
    autogenerated_cfg_add_types(PyGcc_globals.module);
    autogenerated_function_add_types(PyGcc_globals.module);
    autogenerated_gimple_add_types(PyGcc_globals.module);
    autogenerated_location_add_types(PyGcc_globals.module);
    autogenerated_option_add_types(PyGcc_globals.module);
#if (GCC_VERSION < 10000)
    autogenerated_parameter_add_types(PyGcc_globals.module);
#endif

    autogenerated_pass_add_types(PyGcc_globals.module);
    autogenerated_pretty_printer_add_types(PyGcc_globals.module);
    autogenerated_rtl_add_types(PyGcc_globals.module);
    autogenerated_tree_add_types(PyGcc_globals.module);
    autogenerated_variable_add_types(PyGcc_globals.module);

#endif 

    /* Register at-exit finalization for the plugin: */
    register_callback(plugin_info->base_name, PLUGIN_FINISH,
                      on_plugin_finish, NULL);

    LOG("init_plugin finished");

    return 0;
}


#if 0
#define OS_WIN32 1
#include "util-fmemopen.h"
#define fmemopen SCFmemopen

/*
  Weakly import parse_in; it will be non-NULL in the C and C++ frontend,
  but it's not available lto1 (link-time optimization)
*/
__typeof__ (parse_in) parse_in __attribute__ ((weak));

static PyObject*
PyGcc_define_macro(PyObject *self,
                        PyObject *args, PyObject *kwargs)
{
    const char *macro;
    const char *keywords[] = {"macro",
                        NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
                                     "s:define_preprocessor_name", (char**)keywords,
                                     &macro)) {
        return NULL;
    }

    if (0) {
        fprintf(stderr, "gcc.define_macro(\"%s\")\n", macro);
    }

    if (!parse_in) {
        return PyErr_Format(PyExc_ValueError,
                            "gcc.define_macro(\"%s\") called without a compilation unit",
                            macro);
    }

    if (!PyGcc_IsWithinEvent(NULL)) {
        return PyErr_Format(PyExc_ValueError,
                            "gcc.define_macro(\"%s\") called from outside an event callback",
                            macro);
    }

    cpp_define (parse_in, macro);

    Py_RETURN_NONE;
}

static PyObject *
PyGcc_set_location(PyObject *self, PyObject *args)
{
    PyGccLocation *loc_obj;
    if (!PyArg_ParseTuple(args,
                          "O!:set_location",
                          &PyGccLocation_TypeObj, &loc_obj)) {
        return NULL;
    }

    gcc_set_input_location(loc_obj->loc);

    Py_RETURN_NONE;
}

static bool add_option_to_list(gcc_option opt, void *user_data)
{
    PyObject *result = (PyObject*)user_data;
    PyObject *obj_opt;

    obj_opt = PyGccOption_New(opt);
    if (!obj_opt) {
        return true;
    }
    if (-1 == PyList_Append(result, obj_opt)) {
        Py_DECREF(obj_opt);
        return true;
    }
    /* Success: */
    Py_DECREF(obj_opt);
    return false;
}

static PyObject *
PyGcc_get_option_list(PyObject *self, PyObject *args)
{
    PyObject *result;

    result = PyList_New(0);
    if (!result) {
        return NULL;
    }

    if (gcc_for_each_option(add_option_to_list, result)) {
        Py_DECREF(result);
        return NULL;
    }

    /* Success: */
    return result;
}

static bool add_option_to_dict(gcc_option opt, void *user_data)
{
    PyObject *dict = (PyObject*)user_data;
    PyObject *opt_obj;

    opt_obj = PyGccOption_New(opt);
    if (!opt_obj) {
        return true;
    }

    if (-1 == PyDict_SetItemString(dict,
                                   gcc_option_get_text(opt),
                                   opt_obj)) {
        Py_DECREF(opt_obj);
        return true;
    }

    Py_DECREF(opt_obj);
    return false;
}

static PyObject *
PyGcc_get_option_dict(PyObject *self, PyObject *args)
{
    PyObject *dict;

    dict = PyDict_New();
    if (!dict) {
        return NULL;
    }

    if (gcc_for_each_option(add_option_to_dict, dict)) {
        Py_DECREF(dict);
        return NULL;
    }

    /* Success: */
    return dict;
}

/* Function has been removed from GCC 10. */
#if (GCC_VERSION < 10000)
static PyObject *
PyGcc_get_parameters(PyObject *self, PyObject *args)
{
    PyObject *dict;
    size_t i;

    dict = PyDict_New();
    if (!dict) {
	goto error;
    }

    for (i = 0; i < get_num_compiler_params(); i++) {
        PyObject *param_obj = PyGccParameter_New((compiler_param)i);
        if (!param_obj) {
	    goto error;
        }
        if (-1 == PyDict_SetItemString(dict,
                                       compiler_params[i].option,
                                       param_obj)) {
	    Py_DECREF(param_obj);
	    goto error;
	}
        Py_DECREF(param_obj);
    }

    return dict;

 error:
    Py_XDECREF(dict);
    return NULL;
}
#endif

IMPL_APPENDER(add_var_to_list,
              gcc_variable,
              PyGccVariable_New)

static PyObject *
PyGcc_get_variables(PyObject *self, PyObject *args)
{
    IMPL_GLOBAL_LIST_MAKER(gcc_for_each_variable,
                           add_var_to_list)
}

static PyObject *
PyGcc_maybe_get_identifier(PyObject *self, PyObject *args)
{
    const char *str;
    tree t;

    if (!PyArg_ParseTuple(args,
			  "s:maybe_get_identifier",
			  &str)) {
	return NULL;
    }

    t = maybe_get_identifier(str);
    return PyGccTree_New(gcc_private_make_tree(t));
}

static PyObject *PyGcc_make_translation_unit_decl(gcc_translation_unit_decl decl)
{
    gcc_tree tree = gcc_translation_unit_decl_as_gcc_tree(decl);
    return PyGccTree_New(tree);
}

IMPL_APPENDER(add_translation_unit_decl_to_list,
              gcc_translation_unit_decl,
              PyGcc_make_translation_unit_decl)

static PyObject *
PyGcc_get_translation_units(PyObject *self, PyObject *args)
{
    IMPL_GLOBAL_LIST_MAKER(gcc_for_each_translation_unit_decl,
                           add_translation_unit_decl_to_list)
}


/* Weakly import global_namespace; it will be non-NULL for the C++ frontend.

   GCC 8's r247599 (aka c99e91fe8d1191b348cbc1659fbfbac2fc07e154)
   converted it from:
      extern GTY(()) tree global_namespace;
   to:
      #define global_namespace               cp_global_trees[CPTI_GLOBAL]
   so we have to access cp_global_trees instead.  */

#if (GCC_VERSION >= 8000)
__typeof__ (cp_global_trees) cp_global_trees __attribute__ ((weak));
#else
__typeof__ (global_namespace) global_namespace __attribute__ ((weak));
#endif /* #if (GCC_VERSION >= 8000) */


static PyObject *
PyGcc_get_global_namespace(PyObject *self, PyObject *args)
{
    /* (global_namespace will be NULL outside the C++ frontend, giving a
       result of None) */
    return PyGccTree_New(gcc_private_make_tree(global_namespace));
}

/* Dump files */

static PyObject *
PyGcc_dump(PyObject *self, PyObject *arg)
{
    PyObject *str_obj;
    /*
       gcc/output.h: declares:
           extern FILE *dump_file;
       This is NULL when not defined.
    */
    if (!dump_file) {
        /* The most common case; make it fast */
        Py_RETURN_NONE;
    }

    str_obj = PyObject_Str(arg);
    if (!str_obj) {
        return NULL;
    }

    /* FIXME: encoding issues */
    /* FIXME: GIL */
    if (!fwrite(PyGccString_AsString(str_obj),
                strlen(PyGccString_AsString(str_obj)),
                1,
                dump_file)) {
        Py_DECREF(str_obj);
        return PyErr_SetFromErrnoWithFilename(PyExc_IOError, dump_file_name);
    }

    Py_DECREF(str_obj);

    Py_RETURN_NONE;
}

static PyObject *
PyGcc_get_dump_file_name(PyObject *self, PyObject *noargs)
{
    /* gcc/tree-pass.h declares:
        extern const char *dump_file_name;
    */
    return PyGccStringOrNone(dump_file_name);
}

static PyObject *
PyGcc_get_dump_base_name(PyObject *self, PyObject *noargs)
{
    /*
      The generated gcc/options.h has:
          #ifdef GENERATOR_FILE
          extern const char *dump_base_name;
          #else
            const char *x_dump_base_name;
          #define dump_base_name global_options.x_dump_base_name
          #endif
    */
    return PyGccStringOrNone(dump_base_name);
}

static PyObject *
PyGcc_get_is_lto(PyObject *self, PyObject *noargs)
{
    /*
      The generated gcc/options.h has:
          #ifdef GENERATOR_FILE
          extern bool in_lto_p;
          #else
            bool x_in_lto_p;
          #define in_lto_p global_options.x_in_lto_p
          #endif
    */
    return PyBool_FromLong(in_lto_p);
}

static PyMethodDef GccMethods[] = {
    {"register_attribute",
     (PyCFunction)PyGcc_RegisterAttribute,
     (METH_VARARGS | METH_KEYWORDS),
     "Register an attribute."},

    {"register_callback",
     (PyCFunction)PyGcc_RegisterCallback,
     (METH_VARARGS | METH_KEYWORDS),
     "Register a callback, to be called when various GCC events occur."},

    {"define_macro",
     (PyCFunction)PyGcc_define_macro,
     (METH_VARARGS | METH_KEYWORDS),
     "Pre-define a named value in the preprocessor."},

    /* Diagnostics: */
    {"permerror", PyGcc_permerror, METH_VARARGS,
     NULL},
    {"error",
     (PyCFunction)PyGcc_error,
     (METH_VARARGS | METH_KEYWORDS),
     ("Report an error\n"
      "FIXME\n")},
    {"warning",
     (PyCFunction)PyGcc_warning,
     (METH_VARARGS | METH_KEYWORDS),
     ("Report a warning\n"
      "FIXME\n")},
    {"inform",
     (PyCFunction)PyGcc_inform,
     (METH_VARARGS | METH_KEYWORDS),
     ("Report an information message\n"
      "FIXME\n")},
    {"set_location",
     (PyCFunction)PyGcc_set_location,
     METH_VARARGS,
     ("Temporarily set the default location for error reports\n")},

    /* Options: */
    {"get_option_list",
     PyGcc_get_option_list,
     METH_NOARGS,
     "Get all command-line options, as a list of gcc.Option instances"},

    {"get_option_dict",
     PyGcc_get_option_dict,
     METH_NOARGS,
     ("Get all command-line options, as a dict from command-line text strings "
      "to gcc.Option instances")},

/* Function has been removed from GCC 10. */
#if (GCC_VERSION < 10000)
    {"get_parameters", PyGcc_get_parameters, METH_NOARGS,
     "Get all tunable GCC parameters.  Returns a dictionary, mapping from"
     "option name -> gcc.Parameter instance"},
#endif

    {"get_variables", PyGcc_get_variables, METH_NOARGS,
     "Get all variables in this compilation unit as a list of gcc.Variable"},

    {"maybe_get_identifier", PyGcc_maybe_get_identifier, METH_VARARGS,
     "Get the gcc.IdentifierNode with this name, if it exists, otherwise None"},

    {"get_translation_units", PyGcc_get_translation_units, METH_NOARGS,
     "Get a list of all gcc.TranslationUnitDecl"},

    {"get_global_namespace", PyGcc_get_global_namespace, METH_NOARGS,
     "C++: get the global namespace (aka '::') as a gcc.NamespaceDecl"},

    /* Version handling: */
    {"get_plugin_gcc_version", PyGcc_get_plugin_gcc_version, METH_NOARGS,
     "Get the gcc.Version that this plugin was compiled with"},

    {"get_gcc_version", PyGcc_get_gcc_version, METH_NOARGS,
     "Get the gcc.Version for this version of GCC"},

    {"get_callgraph_nodes", PyGcc_get_callgraph_nodes, METH_VARARGS,
     "Get a list of all gcc.CallgraphNode instances"},

    /* Dump files */
    {"dump", PyGcc_dump, METH_O,
     "Dump str() of the argument to the current dump file (or silently discard it when no dump file is open)"},

    {"get_dump_file_name", PyGcc_get_dump_file_name, METH_NOARGS,
     "Get the name of the current dump file (or None)"},

    {"get_dump_base_name", PyGcc_get_dump_base_name, METH_NOARGS,
     "Get the base name used when writing dump files"},

    {"is_lto", PyGcc_get_is_lto, METH_NOARGS,
     "Determine whether or not we're being invoked during link-time optimization"},

    /* Garbage collection */
    {"_force_garbage_collection", PyGcc__force_garbage_collection, METH_NOARGS,
     "Forcibly trigger a single run of GCC's garbage collector"},

    {"_gc_selftest", PyGcc__gc_selftest, METH_NOARGS,
     "Run a garbage-collection selftest"},

    /* Sentinel: */
    {NULL, NULL, 0, NULL}
};

static struct 
{
    PyObject *module;
    PyObject *argument_dict;
    PyObject *argument_tuple;
} PyGcc_globals;

#if PY_MAJOR_VERSION == 3
static struct PyModuleDef module_def = {
    PyModuleDef_HEAD_INIT,
    "gcc",   /* name of module */
    NULL,
    -1,
    GccMethods
};
#endif

PyMODINIT_FUNC PyInit_gcc(void)
{
#if PY_MAJOR_VERSION == 3
    PyObject *m;
    m = PyModule_Create(&module_def);
#else
    Py_InitModule("gcc", GccMethods);
#endif

#if PY_MAJOR_VERSION == 3
    return m;
#endif
}

static int
PyGcc_init_gcc_module(struct plugin_name_args *plugin_info)
{
    int i;

    if (!PyGcc_globals.module) {
        return 0;
    }

    /* Set up int constants for each of the enum plugin_event values: */
    #define DEFEVENT(NAME) \
       PyModule_AddIntMacro(PyGcc_globals.module, NAME);
    # include "plugin.def"
    # undef DEFEVENT

    PyGcc_globals.argument_dict = PyDict_New();
    if (!PyGcc_globals.argument_dict) {
        return 0;
    }

    PyGcc_globals.argument_tuple = PyTuple_New(plugin_info->argc);
    if (!PyGcc_globals.argument_tuple) {
        return 0;
    }

    /* PySys_SetArgvEx(plugin_info->argc, plugin_info->argv, 0); */
    for (i=0; i<plugin_info->argc; i++) {
	struct plugin_argument *arg = &plugin_info->argv[i];
        PyObject *key;
        PyObject *value;
	PyObject *pair;
      
	key = PyGccString_FromString(arg->key);
	if (arg->value) {
            value = PyGccString_FromString(plugin_info->argv[i].value);
	} else {
  	    value = Py_None;
	}
        PyDict_SetItem(PyGcc_globals.argument_dict, key, value);
	// FIXME: ref counts?

	pair = Py_BuildValue("(s, s)", arg->key, arg->value);
	if (!pair) {
  	    return 1;
	}
        PyTuple_SetItem(PyGcc_globals.argument_tuple, i, pair);

    }
    PyModule_AddObject(PyGcc_globals.module, "argument_dict", PyGcc_globals.argument_dict);
    PyModule_AddObject(PyGcc_globals.module, "argument_tuple", PyGcc_globals.argument_tuple);

    /* Pass properties: */
    PyModule_AddIntMacro(PyGcc_globals.module, PROP_gimple_any);
    PyModule_AddIntMacro(PyGcc_globals.module, PROP_gimple_lcf);
    PyModule_AddIntMacro(PyGcc_globals.module, PROP_gimple_leh);
    PyModule_AddIntMacro(PyGcc_globals.module, PROP_cfg);
#if (GCC_VERSION >= 4008)
    /* PROP_referenced_vars went away in GCC 4.8 (in r190067) */
#else
    PyModule_AddIntMacro(PyGcc_globals.module, PROP_referenced_vars);
#endif
    PyModule_AddIntMacro(PyGcc_globals.module, PROP_ssa);
    PyModule_AddIntMacro(PyGcc_globals.module, PROP_no_crit_edges);
    PyModule_AddIntMacro(PyGcc_globals.module, PROP_rtl);
    PyModule_AddIntMacro(PyGcc_globals.module, PROP_gimple_lomp);
    PyModule_AddIntMacro(PyGcc_globals.module, PROP_cfglayout);
    PyModule_AddIntMacro(PyGcc_globals.module, PROP_gimple_lcx);

    PyModule_AddIntMacro(PyGcc_globals.module, GCC_VERSION);

    /* Success: */
    return 1;
}

static void PyGcc_run_any_command(void)
{
    PyObject* command_obj; /* borrowed ref */
    int result;
    const char *command_str;

    command_obj = PyDict_GetItemString(PyGcc_globals.argument_dict, "command");
    if (!command_obj) {
        return;
    }

    command_str = PyGccString_AsString(command_obj);

    if (1) {
        fprintf(stderr, "Running: %s\n", command_str);
    }

    result = PyRun_SimpleString(command_str);
    if (-1 == result) {
        /* Error running the python command */
        Py_Finalize();
        exit(1);
    }
}

static void PyGcc_run_any_script(void)
{
    PyObject* script_name;
    FILE *fp;
    int result;
  
    script_name = PyDict_GetItemString(PyGcc_globals.argument_dict, "script");
    if (!script_name) {
        return;
    }

    fp = fopen(PyGccString_AsString(script_name), "r");
    if (!fp) {
        fprintf(stderr,
		    "Unable to read python script: %s\n",
                PyGccString_AsString(script_name));
	    exit(1);
    }
    result = PyRun_SimpleFile(fp, PyGccString_AsString(script_name));
    fclose(fp);
    if (-1 == result) {
        /* Error running the python script */
        Py_Finalize();
        exit(1);
    }
}

int
setup_sys(struct plugin_name_args *plugin_info)
{
    /*

     * Sets up "sys.plugin_full_name" as plugin_info->full_name.  This is the
     path to the plugin (as specified with -fplugin=)

     * Sets up "sys.plugin_base_name" as plugin_info->base_name.  This is the
     short name, of the plugin (filename without .so suffix)

     * Add the directory containing the plugin to "sys.path", so that it can
     find modules relative to itself without needing PYTHONPATH to be set up.
     (sys.path has already been initialized by the call to Py_Initialize)

     * If PLUGIN_PYTHONPATH is defined, add it to "sys.path"

    */
    int result = 0; /* failure */
    PyObject *full_name = NULL;
    PyObject *base_name = NULL;
    const char *program =
      "import sys;\n"
      "import os;\n"
      "sys.path.append(os.path.abspath(os.path.dirname(sys.plugin_full_name)))\n";

    /* Setup "sys.plugin_full_name" */
    full_name = PyGccString_FromString(plugin_info->full_name);
    if (!full_name) {
        goto error;
    }
    if (-1 == PySys_SetObject((char*)"plugin_full_name", full_name)) {
        goto error;
    }

    /* Setup "sys.plugin_base_name" */
    base_name = PyGccString_FromString(plugin_info->base_name);
    if (!base_name) {
        goto error;
    }
    if (-1 == PySys_SetObject((char*)"plugin_base_name", base_name)) {
        goto error;
    }

    /* Add the plugin's path to sys.path */
    if (-1 == PyRun_SimpleString(program)) {
        goto error;
    }

#ifdef PLUGIN_PYTHONPATH
    {
        /*
           Support having multiple builds of the plugin installed independently
           of each other, by supporting each having a directory for support
           files e.g. gccutils, libcpychecker, etc

           We do this by seeing if PLUGIN_PYTHONPATH was defined in the
           compile, and if so, adding it to sys.path:
        */
        const char *program2 =
            "import sys;\n"
            "import os;\n"
            "sys.path.append('" PLUGIN_PYTHONPATH "')\n";

        if (-1 == PyRun_SimpleString(program2)) {
            goto error;
        }
    }
#endif

    /* Success: */
    result = 1;

 error:
    Py_XDECREF(full_name);
    Py_XDECREF(base_name);
    return result;
}



extern int
plugin_init (struct plugin_name_args *plugin_info,
             struct plugin_gcc_version *version) __attribute__((nonnull));

__declspec(dllexport)  int
plugin_init (struct plugin_name_args *plugin_info,
             struct plugin_gcc_version *version)
{
    LOG("plugin_init started");

    if (!plugin_default_version_check (version, &gcc_version)) {
        return 1;
    }

#if PY_MAJOR_VERSION >= 3
    /*
      Python 3 added internal buffering to sys.stdout and sys.stderr, but this
      leads to unpredictable interleavings of messages from gcc, such as from calling
      gcc.warning() vs those from python scripts, such as from print() and
      sys.stdout.write()

      Suppress the buffering, to better support mixed gcc/python output:
    */
    Py_UnbufferedStdioFlag = 1;
#endif

    PyImport_AppendInittab("gcc", PyInit_gcc);

    LOG("calling Py_Initialize...");

    Py_Initialize();

    LOG("Py_Initialize finished");

    PyGcc_globals.module = PyImport_ImportModule("gcc");

    PyEval_InitThreads();
  
    if (!PyGcc_init_gcc_module(plugin_info)) {
        return 1;
    }

    if (!setup_sys(plugin_info)) {
        return 1;
    }

    /* Init other modules */
    PyGcc_wrapper_init();
    if (PyErr_Occurred()) {
        LOG("PyGcc_wrapper_init failed.");
        PyErr_Print();
    }

    /* FIXME: properly integrate them within the module hierarchy */

    PyGcc_version_init(version);

    autogenerated_callgraph_init_types();  /* FIXME: error checking! */
    autogenerated_cfg_init_types();  /* FIXME: error checking! */
    autogenerated_function_init_types();  /* FIXME: error checking! */
    autogenerated_gimple_init_types();  /* FIXME: error checking! */
    autogenerated_location_init_types();  /* FIXME: error checking! */
    autogenerated_option_init_types();  /* FIXME: error checking! */

#if (GCC_VERSION < 10000)
    autogenerated_parameter_init_types();  /* FIXME: error checking! */
#endif
    autogenerated_pass_init_types();  /* FIXME: error checking! */
    autogenerated_pretty_printer_init_types();  /* FIXME: error checking! */
    autogenerated_rtl_init_types(); /* FIXME: error checking! */
    autogenerated_tree_init_types(); /* FIXME: error checking! */
    autogenerated_variable_init_types(); /* FIXME: error checking! */

    autogenerated_callgraph_add_types(PyGcc_globals.module);
    autogenerated_cfg_add_types(PyGcc_globals.module);
    autogenerated_function_add_types(PyGcc_globals.module);
    autogenerated_gimple_add_types(PyGcc_globals.module);
    autogenerated_location_add_types(PyGcc_globals.module);
    autogenerated_option_add_types(PyGcc_globals.module);
#if (GCC_VERSION < 10000)
    autogenerated_parameter_add_types(PyGcc_globals.module);
#endif

    autogenerated_pass_add_types(PyGcc_globals.module);
    autogenerated_pretty_printer_add_types(PyGcc_globals.module);
    autogenerated_rtl_add_types(PyGcc_globals.module);
    autogenerated_tree_add_types(PyGcc_globals.module);
    autogenerated_variable_add_types(PyGcc_globals.module);
#if 0
#endif
    /* Register at-exit finalization for the plugin: */
    register_callback(plugin_info->base_name, PLUGIN_FINISH,
                      on_plugin_finish, NULL);

    PyGcc_run_any_command();
    PyGcc_run_any_script();

    printf("%s:%i:got here\n", __FILE__, __LINE__);

#if GCC_PYTHON_TRACE_ALL_EVENTS
#define DEFEVENT(NAME) \
    if (NAME != PLUGIN_PASS_MANAGER_SETUP &&         \
        NAME != PLUGIN_INFO &&                       \
	NAME != PLUGIN_REGISTER_GGC_ROOTS &&         \
	NAME != PLUGIN_REGISTER_GGC_CACHES) {        \
    register_callback(plugin_info->base_name, NAME,  \
		      trace_callback_for_##NAME, NULL); \
    }
# include "plugin.def"
# undef DEFEVENT
#endif /* GCC_PYTHON_TRACE_ALL_EVENTS */

    LOG("init_plugin finished");

    return 0;
}

PyObject *
PyGccStringOrNone(const char *str_or_null)
{
    if (str_or_null) {
	return PyGccString_FromString(str_or_null);
    } else {
	Py_RETURN_NONE;
    }
}

PyObject *
PyGcc_int_from_decimal_string_buffer(const char *buf)
{
    PyObject *long_obj;
#if PY_MAJOR_VERSION < 3
    long long_val;
    int overflow;
#endif
    long_obj = PyLong_FromString((char *)buf, NULL, 10);
    if (!long_obj) {
        return NULL;
    }
#if PY_MAJOR_VERSION >= 3
    return long_obj;
#else
    long_val = PyLong_AsLongAndOverflow(long_obj, &overflow);
    if (overflow) {
        /* Doesn't fit in a PyIntObject; use the PyLongObject: */
        return long_obj;
    } else {
        /* Fits in a PyIntObject: use that */
        PyObject *int_obj = PyInt_FromLong(long_val);
        if (!int_obj) {
            return long_obj;
        }
        Py_DECREF(long_obj);
        return int_obj;
    }
#endif
}

#if (GCC_VERSION < 5000)
/*
  "double_int" is declared in gcc/double-int.h as a pair of HOST_WIDE_INT.
  These in turn are defined in gcc/hwint.h as a #define to one of "long",
  "long long", or "__int64".

  It appears that they can be interpreted as either "unsigned" or "signed"

  How to convert this to other types?
  We "cheat", and take it through the decimal representation, then convert
  from decimal.  This is probably slow, but is (I hope) at least correct.
*/
void
PyGcc_DoubleIntAsText(double_int di, bool is_unsigned,
                              char *out, int bufsize)
{
    FILE *f;
    assert(out);
    assert(bufsize > 256); /* FIXME */

    out[0] = '\0';
    f = fmemopen(out, bufsize, "w");
    dump_double_int (f, di, is_unsigned);
    fclose(f);
}

PyObject *
PyGcc_int_from_double_int(double_int di, bool is_unsigned)
{
    char buf[512]; /* FIXME */
    PyGcc_DoubleIntAsText(di, is_unsigned, buf, sizeof(buf));
    return PyGcc_int_from_decimal_string_buffer(buf);
}

#endif /* #if (GCC_VERSION < 5000) */

/*
   GCC's headers "poison" strdup to make it unavailable, so we provide our own.

   The buffer is allocated using PyMem_Malloc
*/
char *
PyGcc_strdup(const char *str)
{
    char *result;
    char *dst;

    result = (char*)PyMem_Malloc(strlen(str) + 1);

    if (!result) {
        return NULL;
    }

    dst = result;
    while (*str) {
        *(dst++) = *(str++);
    }
    *dst = '\0';

    return result;
}

void PyGcc_PrintException(const char *msg)
{
    /* Handler for Python exceptions */
    assert(msg);

    /*
       Emit a gcc error, using GCC's "input_location"

       Typically, by the time our code is running, that's generally just the
       end of the source file.

       The value is saved and restored whenever calling into Python code, and
       within passes is initialized to the top of the function; it can be
       temporarily overridden using gcc.set_location()
    */
    gcc_error_at(gcc_get_input_location(), msg);

    /* Print the traceback: */
    PyErr_PrintEx(1);
}

PyObject *
PyGcc_GetReprOfAttribute(PyObject *obj, const char *attrname)
{
    PyObject *attr_obj;
    PyObject *attr_repr;

    attr_obj = PyObject_GetAttrString(obj, attrname);
    if (!attr_obj) {
        return NULL;
    }
    attr_repr = PyObject_Repr(attr_obj);
    if (!attr_repr) {
        Py_DECREF(attr_obj);
        return NULL;
    }

    return attr_repr;
}
#endif 
/*
  PEP-7
Local variables:
c-basic-offset: 4
indent-tabs-mode: nil
End:
*/

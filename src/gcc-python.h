/*
   Copyright 2011-2013, 2015, 2017 David Malcolm <dmalcolm@redhat.com>
   Copyright 2011-2013, 2015, 2017 Red Hat, Inc.

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

#ifndef INCLUDED__GCC_PYTHON_H
#define INCLUDED__GCC_PYTHON_H

#include <gcc-plugin.h>


#include "autogenerated-config.h"
#if 1
#include "tree.h"

#if (GCC_VERSION >= 4009)
#include "basic-block.h" /* needed by gimple.h in 4.9 */
#include "function.h" /* needed by gimple.h in 4.9 */
#include "tree-ssa-alias.h" /* needed by gimple.h in 4.9 */
#include "internal-fn.h" /* needed by gimple.h in 4.9 */
#include "is-a.h" /* needed by gimple.h in 4.9 */
#include "predict.h" /* needed by gimple.h in 4.9 */
#include "gimple-expr.h" /* needed by gimple.h in 4.9 */
#endif
#include "gimple.h"


/* gcc 10 removed this header */
#if (GCC_VERSION < 10000)
#include "params.h"
#endif

#endif

#if 0
#include "gcc-c-api/gcc-cfg.h"

/* GCC doesn't seem to give us an ID for "invalid event", so invent one: */
#define GCC_PYTHON_PLUGIN_BAD_EVENT (0xffff)

/*
  Define some macros to allow us to use cpychecker's custom attributes when
  compiling the plugin using itself (gcc-with-cpychecker), but also to turn
  them off when compiling with vanilla gcc (and other compilers).
*/
#if defined(WITH_CPYCHECKER_RETURNS_BORROWED_REF_ATTRIBUTE)
  #define CPYCHECKER_RETURNS_BORROWED_REF \
    __attribute__((cpychecker_returns_borrowed_ref))
#else
  #define CPYCHECKER_RETURNS_BORROWED_REF
#endif

#if defined(WITH_CPYCHECKER_STEALS_REFERENCE_TO_ARG_ATTRIBUTE)
  #define CPYCHECKER_STEALS_REFERENCE_TO_ARG(n) \
    __attribute__((cpychecker_steals_reference_to_arg(n)))
#else
  #define CPYCHECKER_STEALS_REFERENCE_TO_ARG(n)
#endif

#if defined(WITH_CPYCHECKER_TYPE_OBJECT_FOR_TYPEDEF_ATTRIBUTE)
  #define CPYCHECKER_TYPE_OBJECT_FOR_TYPEDEF(typename) \
     __attribute__((cpychecker_type_object_for_typedef(typename)))
#else
  #define CPYCHECKER_TYPE_OBJECT_FOR_TYPEDEF(typename)
#endif

/*
  PyObject shared header for wrapping GCC objects, for integration
  with GCC's garbage collector (so that things we wrap don't get collected
  from under us)
*/
typedef struct PyGccWrapper
{
     PyObject_HEAD

     /*
       Keep track of a linked list of all live wrapper objects,
       so that we can mark the wrapped objects for GCC's garbage
       collector:
     */
     struct PyGccWrapper *wr_prev;
     struct PyGccWrapper *wr_next;
} PyGccWrapper;

/*
  PyTypeObject subclass for PyGccWrapper, adding a GC-marking callback:
 */
typedef void (*wrtp_marker) (PyGccWrapper *wrapper);

typedef struct PyGccWrapperTypeObject
{
    PyHeapTypeObject wrtp_base;

    /* Callback for marking the wrapped objects when GCC's garbage
       collector runs: */
    wrtp_marker wrtp_mark;

} PyGccWrapperTypeObject;

/* gcc-python-wrapper.c */

/*
  Allocate a new PyGccWrapper of the given type, setting up:
    - ob_refcnt
    - ob_type
  and calling PyGccWrapper_Track() on it:
*/
#define PyGccWrapper_New(ARG_structname, ARG_typeobj) \
    ( (ARG_structname*) _PyGccWrapper_New(ARG_typeobj) )

extern PyGccWrapper *
_PyGccWrapper_New(PyGccWrapperTypeObject *typeobj);

extern void
PyGccWrapper_Track(PyGccWrapper *obj);

extern void
PyGccWrapper_Dealloc(PyObject *obj);

extern PyTypeObject PyGccWrapperMeta_TypeObj;
/*
  Macro DECLARE_SIMPLE_WRAPPER():
    ARG_structname:
      the struct tag for the resulting python wrapper class,
      e.g. "PyGccPass"

    ARG_typeobj:
      the singleton PyTypeObject for the resulting python wrapper class
      e.g. "PyGccPass_TypeObj"

    ARG_typename:
      a C identifier to use when referring to instances of the underlying
      type e.g. "pass"

    ARG_wrappedtype:
      the GCC type being wrapped e.g. "struct opt_pass *"

    ARG_fieldname:
      the name of the field within the CPython struct, containing the pointer
      to the GCC data
 */
#define DECLARE_SIMPLE_WRAPPER(ARG_structname, ARG_typeobj, ARG_typename, ARG_wrappedtype, ARG_fieldname) \
  struct ARG_structname {           \
     struct PyGccWrapper head;      \
     ARG_wrappedtype ARG_fieldname; \
  };                                \
                                    \
  typedef struct ARG_structname ARG_structname;                          \
                                                                         \
  extern PyObject *                                                      \
  ARG_structname##_New(ARG_wrappedtype ARG_fieldname); \
                                                                         \
  extern PyObject *                                                      \
  ARG_structname##_NewUnique(ARG_wrappedtype ARG_fieldname); \
                                                                         \
  extern PyGccWrapperTypeObject ARG_typeobj                              \
    CPYCHECKER_TYPE_OBJECT_FOR_TYPEDEF(#ARG_structname);                 \
                                                                         \
  extern void                                                            \
  PyGcc_WrtpMarkFor##ARG_structname(ARG_structname *wrapper);               \
  /* end of macro */

DECLARE_SIMPLE_WRAPPER(PyGccCallgraphEdge,
		       PyGccCallgraphEdge_TypeObj,
		       cgraph_edge,
                       gcc_cgraph_edge, edge)

DECLARE_SIMPLE_WRAPPER(PyGccCallgraphNode,
		       PyGccCallgraphNode_TypeObj,
		       cgraph_node,
                       gcc_cgraph_node, node)

DECLARE_SIMPLE_WRAPPER(PyGccPass,
		       PyGccPass_TypeObj,
		       pass,
		       struct opt_pass *, pass)

DECLARE_SIMPLE_WRAPPER(PyGccLocation, 
		       PyGccLocation_TypeObj,
		       location,
		       gcc_location, loc)

/* class rich_location was added to libcpp in gcc 6.  */

#if (GCC_VERSION >= 6000)

DECLARE_SIMPLE_WRAPPER(PyGccRichLocation,
		       PyGccRichLocation_TypeObj,
		       rich_location,
		       rich_location, richloc)

#endif

DECLARE_SIMPLE_WRAPPER(PyGccGimple, 
		       PyGccGimple_TypeObj,
		       gimple,
		       gcc_gimple, stmt);

DECLARE_SIMPLE_WRAPPER(PyGccEdge,
		       PyGccEdge_TypeObj,
		       edge,
		       gcc_cfg_edge, e)

DECLARE_SIMPLE_WRAPPER(PyGccBasicBlock, 
		       PyGccBasicBlock_TypeObj,
		       basic_block,
		       gcc_cfg_block, bb)

DECLARE_SIMPLE_WRAPPER(PyGccCfg, 
		       PyGccCfg_TypeObj,
		       cfg,
                       gcc_cfg, cfg)

DECLARE_SIMPLE_WRAPPER(PyGccFunction, 
		       PyGccFunction_TypeObj,
		       function,
		       gcc_function, fun)

DECLARE_SIMPLE_WRAPPER(PyGccOption,
		       PyGccOption_TypeObj,
                       option,
                       gcc_option, opt)

/* gcc 10 removed params.h */
#if (GCC_VERSION < 10000)
DECLARE_SIMPLE_WRAPPER(PyGccParameter,
		       PyGccParameter_TypeObj,
                       param_num,
		       compiler_param, param_num)
#endif

DECLARE_SIMPLE_WRAPPER(PyGccRtl,
                       PyGccRtl_TypeObj,
#if (GCC_VERSION >= 5000)
                       rtl_insn *,
#else
                       rtl,
#endif
                       gcc_rtl_insn, insn)

DECLARE_SIMPLE_WRAPPER(PyGccTree,
		       PyGccTree_TypeObj,
                       tree,
		       gcc_tree, t)

DECLARE_SIMPLE_WRAPPER(PyGccVariable,
		       PyGccVariable_TypeObj,
		       variable,
		       gcc_variable, var)

/* autogenerated-callgraph.c */
int autogenerated_callgraph_init_types(void);
void autogenerated_callgraph_add_types(PyObject *m);

/* autogenerated-cfg.c */
int autogenerated_cfg_init_types(void);
void autogenerated_cfg_add_types(PyObject *m);

/* autogenerated-function.c */
int autogenerated_function_init_types(void);
void autogenerated_function_add_types(PyObject *m);

/* autogenerated-gimple.c */
int autogenerated_gimple_init_types(void);
void autogenerated_gimple_add_types(PyObject *m);
PyGccWrapperTypeObject*
PyGcc_autogenerated_gimple_type_for_stmt(gcc_gimple stmt);

/* autogenerated-location.c */
int autogenerated_location_init_types(void);
void autogenerated_location_add_types(PyObject *m);

/* autogenerated-option.c */
int autogenerated_option_init_types(void);
void autogenerated_option_add_types(PyObject *m);

/* GCC 10 removed params.h */
#if (GCC_VERSION < 10000)
/* autogenerated-parameter.c */
int autogenerated_parameter_init_types(void);
void autogenerated_parameter_add_types(PyObject *m);
#endif

/* autogenerated-pass.c */
int autogenerated_pass_init_types(void);
void autogenerated_pass_add_types(PyObject *m);
extern PyGccWrapperTypeObject PyGccGimplePass_TypeObj;
extern PyGccWrapperTypeObject PyGccRtlPass_TypeObj;
extern PyGccWrapperTypeObject PyGccSimpleIpaPass_TypeObj;
extern PyGccWrapperTypeObject PyGccIpaPass_TypeObj;

/* autogenerated-pretty-printer.c */
int autogenerated_pretty_printer_init_types(void);
void autogenerated_pretty_printer_add_types(PyObject *m);


/* autogenerated-rtl.c */
int autogenerated_rtl_init_types(void);
void autogenerated_rtl_add_types(PyObject *m);
PyGccWrapperTypeObject *
PyGcc_autogenerated_rtl_type_for_stmt(gcc_rtl_insn insn);


/* autogenerated-tree.c */
int autogenerated_tree_init_types(void);
void autogenerated_tree_add_types(PyObject *m);

PyGccWrapperTypeObject*
PyGcc_autogenerated_tree_type_for_tree(gcc_tree t, int borrow_ref);

PyGccWrapperTypeObject*
PyGcc_autogenerated_tree_type_for_tree_code(enum tree_code code, int borrow_ref);

extern PyGccWrapperTypeObject PyGccComponentRef_TypeObj;

/* autogenerated-variable.c */
int autogenerated_variable_init_types(void);
void autogenerated_variable_add_types(PyObject *m);


PyObject *
PyGccStringOrNone(const char *str_or_null);

#if (GCC_VERSION >= 4008)
PyObject *
VEC_tree_as_PyList(vec<tree, va_gc> *vec_nodes);
#else
PyObject *
VEC_tree_as_PyList(VEC(tree,gc) *vec_nodes);
#endif

PyObject *
PyGcc_int_from_int_cst(tree int_cst);

PyObject *
PyGcc_int_from_decimal_string_buffer(const char *buf);

void
PyGcc_DoubleIntAsText(double_int di, bool is_unsigned,
                              char *out, int bufsize)  __attribute__((nonnull));

#if (GCC_VERSION >= 5000)
PyObject *
PyGcc_int_from_wide_int();
#else
PyObject *
PyGcc_int_from_double_int(double_int di, bool is_unsigned);
#endif

PyObject *
PyGcc_LazilyCreateWrapper(PyObject **cache,
				 void *ptr,
				 PyObject *(*ctor)(void *ptr));
int
PyGcc_insert_new_wrapper_into_cache(PyObject **cache,
                                         void *ptr,
                                         PyObject *obj);


/* gcc-python.c */
int PyGcc_IsWithinEvent(enum plugin_event *out_event);

char * PyGcc_strdup(const char *str) __attribute__((nonnull));

void PyGcc_PrintException(const char *msg);

/*
  Shorthand for:  repr(getattr(obj, attrname))
*/
PyObject *
PyGcc_GetReprOfAttribute(PyObject *obj, const char *attrname);

/* Python 2 vs Python 3 compat: */
#if PY_MAJOR_VERSION == 3
/* Python 3: use PyUnicode for "str" and PyLong for "int": */
#define PyGccString_FromFormat PyUnicode_FromFormat
#define PyGccString_FromString PyUnicode_FromString
#define PyGccString_FromString_and_size PyUnicode_FromStringAndSize
#define PyGccString_AsString _PyUnicode_AsString
#define PyGccInt_FromLong PyLong_FromLong
#define PyGccInt_Check PyLong_Check
#define PyGccInt_AsLong PyLong_AsLong
#else
/* Python 2: use PyString for "str" and PyInt for "int": */
#define PyGccString_FromFormat PyString_FromFormat
#define PyGccString_FromString PyString_FromString
#define PyGccString_FromString_and_size PyString_FromStringAndSize
#define PyGccString_AsString PyString_AsString
#define PyGccInt_FromLong PyInt_FromLong
#define PyGccInt_Check PyInt_Check
#define PyGccInt_AsLong PyInt_AsLong
#endif

#endif 

/*
  PEP-7
Local variables:
c-basic-offset: 4
indent-tabs-mode: nil
End:
*/

#endif /* INCLUDED__GCC_PYTHON_H */

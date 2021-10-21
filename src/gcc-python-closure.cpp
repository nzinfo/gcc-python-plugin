/*
   Copyright 2011 David Malcolm <dmalcolm@redhat.com>
   Copyright 2011 Red Hat, Inc.

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


py::object
PyGcc_RegisterCallback(py::args args, const py::kwargs& kwargs) {
    return py::none();
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

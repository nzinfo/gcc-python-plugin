/*
    TODO: GPL License
    Copyright 2021 Li Monan <li.monan@gmail.com>
    Copyright 2011-2013, 2015, 2017 David Malcolm <dmalcolm@redhat.com>
    Copyright 2011-2013, 2015, 2017 Red Hat, Inc.
*/
#ifndef GCC_PYTHON_PLUGIN_GGC_WRAPPER_H
#define GCC_PYTHON_PLUGIN_GGC_WRAPPER_H

#include <Python.h>

/*
 * 与 GCC 的 GC 集成，避免 PyObject 内部引用的 GCC 数据结构被 GC 掉。
 * */

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


/* python-ggc-wrapper.cpp */
void
PyGcc_wrapper_init(void);

PyObject *
PyGcc__force_garbage_collection(PyObject *self, PyObject *args);

PyObject *
PyGcc__gc_selftest(PyObject *self, PyObject *args);


#endif //GCC_PYTHON_PLUGIN_GGC_WRAPPER_H

/*
    TODO: GPL License
*/

#include "python-ggc-wrapper.h"
#include "gcc-python-compat.h"
#include <ggc.h>

static void
force_gcc_gc(void);

void
gcc_python_ggc_walker(void *sentinel_base);

/*
 * Some inner of Gcc GC.
 * // Structures for the easy way to mark roots.
    struct ggc_root_tab {
        void *base;             // the base address which this root table try to manage. It's might be some start address of a memory pool
        size_t nelt;            // nelt, the memory are divided into `nelt` parts , with the same size
        size_t stride;          // stride, the size refered above
        gt_pointer_walker cb;   // cleanup callback each address of base[i] will be passed to cb. sizeof(elementBase) = stride
        gt_pointer_walker pchw; // callback when needs write down some debug information to pch.
    };
    // The * base must not be NULL else it will be an end marker.
 * */

/* Maintain a circular linked list of PyGccWrapper instances: */
static struct PyGccWrapper sentinel = {
        PyObject_HEAD_INIT(NULL)
        &sentinel,
        &sentinel,
};

static struct ggc_root_tab gcc_python_root_table[] = {
        { &sentinel, 1, sizeof(struct PyGccWrapper), gcc_python_ggc_walker, NULL },
        LAST_GGC_ROOT_TAB
};

void
gcc_python_ggc_walker(void * sentinel_base) {
    /*
      Callback for use by GCC's garbage collector when marking

      Walk all the PyGccWrapper objects here and if they reference
      GCC GC objects, mark the underlying GCC objects so that they
      don't get swept
    */
    // arg will be the address of dummy , use less.
}

void
PyGcc_wrapper_init(void)
{
    /* Register our GC root-walking callback: */
    ggc_register_root_tab(gcc_python_root_table);

    //PyType_Ready(&PyGccWrapperMeta_TypeObj);
}


static void
force_gcc_gc(void)
{
    bool stored = ggc_force_collect;

    ggc_force_collect = true;
    ggc_collect();
    ggc_force_collect = stored;
}
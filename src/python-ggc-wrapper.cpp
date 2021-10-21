/*
    TODO: GPL License
*/

#include "python-ggc-wrapper.h"
#include "gcc-python-compat.h"
#include <ggc.h>

/* Debugging, for use by selftest routine */
static int debug_PyGcc_wrapper = 1;

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
// all function with modify sentinel must be locked by GIL.
GCCTraceRoot g_Sentinel = {
        &g_Sentinel,
        &g_Sentinel,
};

static struct ggc_root_tab gcc_python_root_table[] = {
        { &g_Sentinel, 1, sizeof(struct GCCTraceRoot), gcc_python_ggc_walker, NULL },
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
    struct GCCTraceRoot *iter;
    assert(sentinel_base);  // the sentinel_base must a pointer to sentinel.

    GCCTraceRoot* sentinel_p = (struct GCCTraceRoot *)sentinel_base;


    if (debug_PyGcc_wrapper) {
        printf("  walking the live PyGccWrapper objects\n");
    }
    for (iter = sentinel_p->wr_next; iter != sentinel_p; iter = iter->wr_next) {
        if (debug_PyGcc_wrapper) {
            printf("    marking inner object for: ");
            PyObject_Print((PyObject*)iter, stdout, 0);
            printf("\n");
        }
        // iter will never point to GCCTraceRoot, so it will always point to GCCTraceMarkable's subclass.
        ((GCCTraceMarkable*)iter)->ggc_marker();
    }
    if (debug_PyGcc_wrapper) {
        printf("  finished walking the live PyGccWrapper objects\n");
    }

}

void
PyGcc_wrapper_init(void)
{
    /* Register our GC root-walking callback: */
    // ggc_register_root_tab(gcc_python_root_table);
    // use register_callback are both ok.
    register_callback(PLUGIN_NAME,
                      PLUGIN_REGISTER_GGC_ROOTS,
                      NULL,
                      gcc_python_root_table);
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
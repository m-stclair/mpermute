#define PY_SSIZE_T_CLEAN
#include <stdbool.h>
#include <Python.h>

#define PYCHECK_2(FUNC, AOBJ, BOBJ) \
(bool) PyObject_IsTrue(PyObject_CallFunctionObjArgs(FUNC, AOBJ, BOBJ, NULL)) \


enum Status {
    OK,
    FAILED
};

typedef struct
UniqueResult {
    long nunique;
    PyObject **items;
    long *counts;
    enum Status status;
} UniqueResult;

static inline int
pyorder(const void *a, const void *b) {
    PyObject *aobj = *(PyObject **) a, *bobj = *(PyObject **) b;
    PyObject *lt = PyObject_GetAttrString(PyObject_Type(aobj), "__lt__");
    if (PYCHECK_2(lt, aobj, bobj)) return -1;
    if (PYCHECK_2(lt, bobj, aobj)) return 1;
    return 0;
}

bool GLOBAL_ERROR_INDICATOR = false;

static inline int
pyorder_safe(const void *a, const void *b) {
    PyObject *aobj = *(PyObject **) a, *bobj = *(PyObject **) b;
    PyObject *lt = PyObject_GetAttrString(PyObject_Type(aobj), "__lt__");
    PyObject *a_lt_b = PyObject_CallFunctionObjArgs(lt, aobj, bobj, NULL);
    if (PyErr_Occurred() || a_lt_b == NULL) {
        GLOBAL_ERROR_INDICATOR = true;
        return 0;
    }
    if (PyObject_IsTrue(a_lt_b)) return -1;
    PyObject *b_lt_a = PyObject_CallFunctionObjArgs(lt, bobj, aobj, NULL);
    if (PyErr_Occurred() || b_lt_a == NULL) {
        GLOBAL_ERROR_INDICATOR = true;  
        return 0;
    }
    if (PyObject_IsTrue(b_lt_a)) return 1;
    return 0;
}


// NOTE: assumes type consistency has already been checked and elements are
//  already sorted
static void
unique_local(PyObject **elements, const long length,
             UniqueResult *const result) {
    // TODO: length-0 case
    PyObject **items = malloc(sizeof(PyObject*) * length);
    long *counts = calloc(sizeof(long), length);
    items[0] = elements[0];
    counts[0] = 1;
    long ix = 0;
    PyObject *eq = PyObject_GetAttrString(PyObject_Type(elements[0]),
                                          "__eq__");
    if (eq == NULL || PyErr_Occurred()) {
        result->status = FAILED;
        return;
    }
    for (long i = 1; i < length; i++) {
        if (PYCHECK_2(eq, elements[i], items[ix])) {
            counts[ix] += 1;
        } else {
            items[++ix] = elements[i];
            counts[ix] = 1;
        }
    }
    result->nunique = ix + 1;
    result->items = malloc(sizeof(PyObject*) * result->nunique);
    result->counts = malloc(sizeof(long) * result->nunique);
    for (int i = 0; i < ix + 1; i++) {
        result->items[i] = items[i];
        result->counts[i] = counts[i];
    }
    free(items);
    free(counts);
}

// NOTE: assumes elements are already sorted.
unsigned long
n_uniq_pyobj_perms(PyObject **elements, const long n) {
    UniqueResult result;
    unique_local(elements, n, &result);
    // we're doing this to prevent an overflow. if you still overflow, you're
    // permuting too many things and that's on you.
    double pdouble = lgamma((double) n + 1);
    for (unsigned long a = 0; a < result.nunique; a++) {
        if (result.counts[a] > 1) {
            pdouble -= lgamma((double) result.counts[a] + 1);
        }
    }
    unsigned long permsize = (unsigned long) roundl(exp(pdouble));
    free(result.items);
    free(result.counts);
    return permsize;
}

typedef struct
MSetElement {
    const PyObject *value;
    struct MSetElement *next;
} MSetElement;

typedef struct
MsetIter {
    MSetElement *h;
    MSetElement *i;
    MSetElement *j;
} MsetIter;

typedef struct
MPRes {
    PyObject **const perms;
    unsigned short n;
    unsigned long nperm;
    MsetIter *msi;
} MPRes;

static inline MSetElement *
nth_mse(MSetElement *mse, const unsigned int n) {
    unsigned int i = 0;
    while (i < n && mse->next != NULL) {
        mse = mse->next;
        i++;
    }
    return mse;
}

static inline MSetElement *
new_mse(const PyObject *value, MSetElement *const next) {
    MSetElement *mse = malloc(sizeof(MSetElement));
    mse->value = value;
    mse->next = next;
    return mse;
}

// NOTE: assumes objs are already sorted.
static inline MsetIter *
init_mse_iter(PyObject **const objs, unsigned int size) {
    if (size < 2) return NULL;
    MSetElement *h = new_mse(objs[0], NULL);
    for (unsigned int i = 1; i < size; i++) {
        MSetElement *h0 = new_mse(objs[i], h);
        h = h0;
    }
    MsetIter *msi = malloc(sizeof(MsetIter));
    msi->h = h;
    msi->i = nth_mse(h, size - 2);
    msi->j = nth_mse(h, size - 1);
    return msi;
}

static inline PyObject *
mpvisit(const MSetElement *h, const unsigned int size) {
    PyObject *const perm = PyTuple_New(size);
    unsigned int uix = 0;
    const MSetElement *o = h;
    while (o != NULL) {
        PyTuple_SET_ITEM(perm, uix++, o->value);
        o = o->next;
    }
    return perm;
}

static inline void
msifree(MsetIter *msi) {
    MSetElement *o = msi->h; {
        while(o != NULL) {
            MSetElement *o0 = o->next;
            free(o);
            o = o0;
        }
    }
    free(msi);
}

static PyObject *
mpermute(PyObject *self, PyObject *const *args, Py_ssize_t n_args) {
    assert(n_args == 1);
    PyObject *mset = args[0];
    if (!PyTuple_Check(mset)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a tuple.");
        return NULL;
    }
    long n = PyObject_Length(mset);
    if (n < 2) {
        PyObject *output_trivial = PyTuple_New(1);
        PyTuple_SetItem(output_trivial, 0, mset);
        return output_trivial;
    }
    PyObject *first = PyTuple_GetItem(mset, 0);
    PyObject *const type = PyObject_Type(first);
    PyObject *ge = PyObject_GetAttrString(type, "__ge__");
    PyObject *lt = PyObject_GetAttrString(type, "__lt__");
    if (PyErr_Occurred()) return NULL;
    if (ge == NULL || lt == NULL) {
        PyErr_SetString(PyExc_TypeError, "Couldn't get comparison functions "
                                         "for elements.");
        return NULL;
    }
    for (long i = 1; i < n; i++) {
        PyObject *el = PyTuple_GetItem(mset, i);
        if (PyObject_Type(el) != type) {
            PyErr_SetString(PyExc_TypeError, "All elements must have the "
                                             "same type.");
            return NULL;
        }
        PyObject *res = PyObject_CallFunctionObjArgs(lt, first, el, NULL);
        if (PyErr_Occurred()) return NULL;
        if (res == NULL) {
            PyErr_SetString(PyExc_TypeError, "An element is incomparable.");
            return NULL;
        }
    }
    PyObject **objs = malloc(sizeof(PyObject*) * n);
    if (objs == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    for (long i = 0; i < n; i++) objs[i] = PyTuple_GetItem(mset, i);
    qsort(objs, n, sizeof(PyObject*), pyorder_safe);
    if (GLOBAL_ERROR_INDICATOR == true) {
        PyErr_SetString(PyExc_TypeError, "Some elements are incomparable.");
        free(objs);
        return NULL;
    }
    unsigned long nperm = n_uniq_pyobj_perms(objs, n);
    PyObject *perms;
    if (nperm > LONG_MAX || nperm == 0) perms = NULL;
    else perms = PyTuple_New((long) nperm);
    if (perms == NULL) {
        char errstring[100];
        sprintf(errstring, "Could not construct container for %li "
                           "permutations.", n);
        PyErr_SetString(PyExc_MemoryError, errstring);
        return NULL;
    }
    MsetIter *msi = init_mse_iter(objs, n);
    if (msi == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to set up iteration.");
        free(perms);
        free(objs);
        return NULL;
    }
    PyObject *output;
    PyGC_Disable();
    MSetElement *s;
    PyTuple_SetItem(perms, 0, mpvisit(msi->h, n));
    long permix = 1;
    while (msi->j->next != NULL || PYCHECK_2(lt, msi->j->value, msi->h->value))
    {
        if (msi->j->next != NULL
            && PYCHECK_2(ge, msi->i->value, msi->j->next->value)
        ) s = msi->j;
        else s = msi->i;
        MSetElement *t = s->next;
        s->next = t->next;
        t->next = msi->h;
        if (PYCHECK_2(lt, t->value, msi->h->value)) msi->i = t;
        msi->j = msi->i->next;
        msi->h = t;
        PyObject *visit = mpvisit(msi->h, n);
        PyTuple_SetItem(perms, permix++, visit);
    }
    if (permix != nperm) {
        printf("%lu != %lu\n", permix, nperm);
        PyErr_SetString(PyExc_RuntimeError, "Permutation counting error.");
        Py_SET_REFCNT(perms, 0);
        perms = NULL;
    }
    msifree(msi);
    free(objs);
    PyGC_Enable();
    return perms;
}

static PyMethodDef
MpermuteMethods[] = {
    {
        "mpermute",
        (PyCFunction) mpermute,
        METH_FASTCALL,
        "Multi-permutation function."
    },
    {NULL, NULL, 0, NULL}
};

static struct
    PyModuleDef mpermute_mod = {
    PyModuleDef_HEAD_INIT,
    "_mpermute",
    "multiset permutation",
    -1,
    MpermuteMethods
};

// NOTE: the name of this function _must_ be "PyInit_" followed immediately
//  by the Python-visible name of the module, hence the double underscore.
PyMODINIT_FUNC PyInit__mpermute(void) {
    return PyModule_Create(&mpermute_mod);
}

int
main() {
//    PyObject *one = PyLong_FromLong(1);
//    PyObject *two = PyLong_FromLong(2);
//    PyObject **things = malloc(sizeof(PyObject*) * 4);
//    things[0] = one;
//    things[1] = two;
//    things[2] = one;
//    things[3] = two;
//    unsigned long res = n_uniq_pyobj_perms(things, 4);
    return 0;
}
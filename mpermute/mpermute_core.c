#include "api_helpers.h"
//#include "group.h"
#include "unique.h"

typedef struct
MSetElement {
    PyObject *value;
    PyObject *hash;
    struct MSetElement *next;
} MSetElement;

typedef struct
MsetIter {
    MSetElement *h;
    MSetElement *i;
    MSetElement *j;
} MsetIter;

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
new_mse(PyObject *const value, MSetElement *const next, PyObject *const key) {
    MSetElement *mse = malloc(sizeof(MSetElement));
    mse->value = value;
    mse->next = next;
    if (!Py_IsNone(key)) {
        mse->hash = PyObject_CallOneArg(key, value);
        // TODO: in some cases we likely want to decref this
    }
    else mse->hash = value;
    return mse;
}

// NOTE: assumes objs are sorted.
static inline MsetIter *
init_mse_iter(PyObject *const udict, const long size, PyObject *const key) {
    if (size < 2) return NULL;
    PyObject *item;
    PyObject *const items = PyDict_Items(udict);
    PyObject *const iter = PyObject_GetIter(items);
    MSetElement *h = NULL;
    while ((item = PyIter_Next(iter)) != NULL) {
        PyObject *const k = PyTuple_GetItem(item, 0);
        const long v = PyLong_AsLong(PyTuple_GetItem(item, 1));
        for (long i = 0; i < v; i++) {
            MSetElement *h0 = new_mse(k, h, key);
            h = h0;
        }
        Py_DECREF(item);
    }
    Py_DECREF(iter);
    Py_DECREF(items);
    MsetIter *msi = malloc(sizeof(MsetIter));
    msi->h = h, msi->i = nth_mse(h, size - 2), msi->j = nth_mse(h, size - 1);
    return msi;
}

static inline PyObject *
mpvisit(const MSetElement *h, const unsigned int size) {
    // TODO: check how much overhead it introduces to explicitly error-handle
    //  NULL case -- which is absolutely possible for memory reasons -- and
    //  whether or not it is essentially handled by producing garbage output
    //  in the outer loop
    unsigned int uix = 0;
    const MSetElement *o = h;
    PyObject *const perm = PyTuple_New(size);
    while (o != NULL) {
        PyTuple_SET_ITEM(perm, uix++, o->value);
        o = o->next;
    }
    return perm;
}

static inline void
msifree(MsetIter *msi) {
    MSetElement *o = msi->h;
    while(o != NULL) {
        MSetElement *o0 = o->next;
        free(o);
        o = o0;
    }
    free(msi);
}


static inline PyObject *
do_msi_step(const long n, MsetIter *const msi) {
    MSetElement *s;
    if (
        msi->j->next != NULL
        && !PYCOMP(msi->i->hash, msi->j->next->hash, Py_LT)
    ) s = msi->j;
    else s = msi->i;
    MSetElement *t = s->next;
    s->next = t->next, t->next = msi->h;
    if (PYCOMP(t->hash, msi->h->hash, Py_LT)) msi->i = t;
    msi->j = msi->i->next, msi->h = t;
    PyObject *visit = mpvisit(msi->h, n);
    return visit;
}

typedef struct
MpermSetupState {
    PyObject *trivial_output;
    MsetIter *msi;
    char *errstring;
    PyObject *exception_class;
    PyObject *key;
    UniqueResult *uresult;
    long n;
    PyObject *key_lt;
    PyObject *key_eq;
} MpermSetupState;

static MpermSetupState *
new_mperm_setup_state() {
    MpermSetupState *msst = malloc(sizeof(MpermSetupState));
    msst->trivial_output = NULL;
    msst->errstring = NULL;
    msst->exception_class = NULL;
    msst->key = NULL;
    msst->uresult = NULL;
    msst->n = 0;
    msst->msi = NULL;
    msst->key_eq = NULL;
    msst->key_lt = NULL;
    return msst;
}

#define MSST_ERROUT(MSST, EXCTYPE, EXCMSG)               \
do {                                                     \
    MSST->exception_class = EXCTYPE;                     \
    MSST->errstring = EXCMSG;                            \
    return MSST;                                         \
} while(0)

static MpermSetupState *
mperm_setup(PyObject *const *args, Py_ssize_t n_args) {
    if (n_args < 1 || n_args > 2)
        PYRAISE(TYPEERROR, "Function accepts exactly 1 or 2 arguments.");
    PyObject *mset = args[0], *key;
    if (n_args == 1) key = Py_None;
    else key = args[1];
    long n = PyObject_Length(mset);
    MpermSetupState* msst = new_mperm_setup_state();
    msst->n = n;
    if (n < 2) {
        PyObject *output_trivial = PyTuple_New(1);
        PyTuple_SetItem(output_trivial, 0, mset);
        msst->trivial_output = output_trivial;
        return msst;
    }
    if (!Py_IsNone(key))
        if (PyCallable_Check(key) == 0)
            MSST_ERROUT(msst, TYPEERROR, "Invalid relational operator.");
    msst->key = key;
    UniqueResult *uresult = n_uniq_pyobj_perms(mset, n, key);
    if (PyErr_Occurred() || uresult->status == UNIQUE_FAILED)
        MSST_ERROUT(msst, PyExc_TypeError, "relational / sequence init err");
    if (!uresult->udict_sorted) {
        PyObject *sorted_dict = sort_dict(uresult->udict);
        Py_DECREF(uresult->udict);
        uresult->udict = sorted_dict;
        uresult->udict_sorted = true;
    }
    msst->uresult = uresult;
    MsetIter *msi = init_mse_iter(uresult->udict, n, key);
    if (msi == NULL) {
        free_uniqueresult(uresult);
        MSST_ERROUT(msst, PyExc_RuntimeError, "Failed to set up iteration.");
    }
    msst->msi = msi;
    return msst;
}

static PyObject *
mpermute(PyObject *self, PyObject *const *args, Py_ssize_t n_args) {
    MpermSetupState *const msst = mperm_setup(args, n_args);
    if (msst == NULL || PyErr_Occurred()) return NULL;  // error already set
    if (msst->exception_class != NULL) {
        PyErr_SetString(msst->exception_class, msst->errstring);
        free(msst);
        return NULL;
    }
    if (msst->trivial_output != NULL) {
        PyObject *trivial = msst->trivial_output;
        free(msst);
        return trivial;
    }
    PyObject *perms;
    const unsigned long nperm_o = msst->uresult->nperm;
    const long nperm = (long) nperm_o;
    PyGC_Disable();
    if (nperm_o > LONG_MAX || nperm_o == 0) perms = NULL;
    else perms = PyTuple_New((long) nperm);
    if (perms == NULL) {
        PyGC_Enable();
        char errstring[100];
        sprintf(
            errstring, "Couldn't allocate memory for %li permutations", nperm
        );
        free(msst);
        PYRAISE(PyExc_MemoryError, errstring);
    }
    MsetIter *msi = msst->msi;
    long n = msst->n;
    PyTuple_SetItem(perms, 0, mpvisit(msi->h, n));
    long permix = 1;
    while (
        msi->j->next != NULL || PYCOMP(msi->j->hash, msi->h->hash, Py_LT)
    ) {
        PyObject *visit = do_msi_step(n, msi);
        PyTuple_SetItem(perms, permix++, visit);
    }
    if (permix != nperm) {
        PyErr_SetString(PyExc_RuntimeError, "Permutation counting error.");
        Py_SET_REFCNT(perms, 0);
        perms = NULL;
    } else ures_incref(msst->uresult, (long) nperm);
    msifree(msi);
    free_uniqueresult(msst->uresult);
    free(msst);
    PyGC_Enable();
    return perms;
}

typedef struct {
    PyObject_HEAD
    MsetIter *msi;
    PyObject *key;
    PyObject *trivial;
    long permix;
    long n;
    long nperm;
} MPGenState;


static PyObject *
mpgen_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    MPGenState *self;
    self = (MPGenState *) type->tp_alloc(type, 0);
    self->nperm = -1;
    self->permix = 0;
    self->trivial = Py_None;
    self->key = Py_None;
    self->msi = NULL;
    self->n = -1;
    PyObject *selfreturn = (PyObject *) self;
    Py_INCREF(selfreturn);
    return selfreturn;
}

static int
mpgen_init(MPGenState *self, PyObject *args, PyObject *kwds_unused) {
    PyObject *argl[3];
    if (!PyArg_ParseTuple(args, "OO", &argl[0], &argl[1])) {
        PyErr_SetString(TYPEERROR, "Invalid arguments.");
        return -1;
    }
    MpermSetupState *const msst = mperm_setup((PyObject **) argl, 2);
    if (PyErr_Occurred()) return -1;  // error already set
    if (msst->trivial_output != NULL)
        self->trivial = msst->trivial_output;
    self->nperm = (long) msst->uresult->nperm;
    self->n = msst->n;
    self->msi = msst->msi;
    self->key = msst->key;
    Py_INCREF(self->key);
    free_uniqueresult(msst->uresult);
    free(msst);
    return 0;
}

static PyObject *
mpgen_iternext(MPGenState *self) {
    if (++self->permix > self->nperm) {
        PyErr_SetNone(STOPITERATION);
        if (self->msi != NULL) {
            msifree(self->msi);
            self->msi = NULL;
        }
        return NULL;
    }
    if (Py_IsNone(self->trivial) != 1) return self->trivial;
    PyObject *visit = do_msi_step(self->n, self->msi);
    for (long i = 0; i < self->n; i++) Py_INCREF(PyTuple_GetItem(visit, i));
    return visit;
}

static inline PyObject *
mpgen_iter(PyObject *self) { return self; }

// TODO: Needs a destructor
static PyTypeObject MPGeneratorType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "mpermute_core.MPGenerator",
    .tp_basicsize = sizeof(MPGenState),
    .tp_itemsize = 0,
    .tp_new = (newfunc) mpgen_new,
    .tp_init = (initproc) mpgen_init,
    .tp_iternext = (iternextfunc) mpgen_iternext,
    .tp_iter = mpgen_iter,
    .tp_flags = Py_TPFLAGS_DEFAULT
};


static PyMethodDef
MpermuteCoreMethods[] = {
    {
        "mpermute",
        (PyCFunction) mpermute,
        METH_FASTCALL,
        "Multi-permutation function."
    },
    {
        "unique",
        (PyCFunction) unique,
        METH_FASTCALL,
        "Unique-counting function."
    },
    {
        "unique_2",
        (PyCFunction) unique_2,
        METH_FASTCALL,
        "Unique-counting function."
    },
//    {
//        "groupby",
//        (PyCFunction) groupby,
//        METH_FASTCALL,
//        "Group-by-key function."
//    },
//    {
//        "dictbuild",
//        (PyCFunction) dictbuild,
//        METH_FASTCALL,
//        "Pointlessly build dict."
//    },
    {NULL, NULL, 0, NULL}
};

static struct
    PyModuleDef mpermute_core_mod = {
    PyModuleDef_HEAD_INIT,
    "mpermute_core",
    "multiset permutation",
    -1,
    MpermuteCoreMethods
};

// NOTE: the name of this function _must_ be "PyInit_" followed immediately
//  by the Python-visible name of the module, hence the double underscore.
PyMODINIT_FUNC PyInit_mpermute_core(void) {
    PyObject *m;
    if (PyType_Ready(&MPGeneratorType) < 0) return NULL;
    m = PyModule_Create(&mpermute_core_mod);
    if (m == NULL) return NULL;
    if (PyModule_AddObjectRef(
        m, "MPGenerator", (PyObject *) &MPGeneratorType) < 0)
    {
        Py_DECREF(m);
        return NULL;
    }
    return m;
}

// this function is only present for debugging purposes
int
main() {
    return 0;
}

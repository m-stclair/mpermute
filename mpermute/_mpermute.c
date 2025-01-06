#include <Python.h>
#define PY_SSIZE_T_CLEAN
#include <stdbool.h>

#define STOPITERATION PyExc_StopIteration
#define TYPEERROR PyExc_TypeError
#define RUNTIMEERROR PyExc_RuntimeError
#define PYCALL PyObject_CallFunctionObjArgs
#define GETATTR PyObject_GetAttrString
#define PYREPR(PYOBJ) PyUnicode_AsUTF8(PyObject_Repr(PYOBJ))
#define PYPRINT(PYOBJ) printf("%s\n", PYREPR(PYOBJ))
#define IMPORT PyImport_ImportModule

#define PYCOMP(AOBJ, BOBJ, COMP)                       \
(bool) PyObject_RichCompareBool(AOBJ, BOBJ, COMP)      \

#define PYCHECK_2(FUNC, AOBJ, BOBJ)                    \
(bool) PyObject_IsTrue(PYCALL(FUNC, AOBJ, BOBJ, NULL))

#define PYRAISE(EXCTYPE, MSG)      \
do {                               \
    PyErr_SetString(EXCTYPE, MSG); \
    return NULL;                   \
} while(0)

enum Status { OK, FAILED };

typedef struct
UniqueResult {
    unsigned long nunique;
    unsigned long nperm;
    PyObject *udict;
    enum Status status;
    char *errmsg;
    PyObject *exctype;
    bool udict_sorted;
} UniqueResult;


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
    if (!Py_IsNone(key)) mse->hash = PyObject_CallOneArg(key, value);
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
    }
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
    MSetElement *o = msi->h; {
        while(o != NULL) {
            MSetElement *o0 = o->next;
            free(o);
            o = o0;
        }
    }
    free(msi);
}

static inline void
free_uniqueresult(UniqueResult *const ures) {
    PyObject *keys = PyDict_Keys(ures->udict);
    PyObject *iter = PyObject_GetIter(keys);
    PyObject *key;
    while ((key = PyIter_Next(iter)) != NULL) Py_DECREF(key);
    Py_DECREF(ures->udict);
    Py_DECREF(keys);
    Py_DECREF(iter);
    free(ures);
}

// incrementing references each time we construct a permutation is slow,
// and because we're always holding at least one reference to each input
// object (and holding the GIL!), there's little risk of anything sketchy
// happening while we're constructing permutations. it's better to just
// batch-set the refcount here at the end based on the fact that each
// permutation tuple should hold a reference to each of its children for
// each time an element of the child's equivalence class appears in the input.
static inline void
ures_incref(const UniqueResult *const ures, const long n_inc) {
    PyObject *items = PyDict_Items(ures->udict);
    PyObject *iter = PyObject_GetIter(items);
    PyObject *item;
    while ((item = PyIter_Next(iter)) != NULL) {
        PyObject *k = PyTuple_GetItem(item, 0);
        PyObject *v = PyTuple_GetItem(item, 1);
        // -2 because both PyDict_Items and PyTuple_GetItem create refs
        Py_SET_REFCNT(k, Py_REFCNT(k) + n_inc * PyLong_AsLong(v) - 2);
    }
    Py_DECREF(items);
    Py_DECREF(iter);
}

static PyObject *sort_dict(PyObject *const dict) {
    PyObject *const sorted = GETATTR(IMPORT("builtins"), "sorted");
    PyObject *const keys = PyDict_Keys(dict);
    PyObject *const list = PYCALL(sorted, keys, NULL);
    PyObject *const fastlist = PySequence_Fast(list, "");
    PyObject *const sorted_dict = PyDict_New();
    for (long i = 0; i < PyObject_Length(list); i++) {
        PyObject *k = PySequence_Fast_GET_ITEM(fastlist, i);
        PyDict_SetItem(sorted_dict, k, PyDict_GetItem(dict, k));
    }
    Py_DECREF(fastlist);
    Py_DECREF(keys);
    Py_DECREF(list);
    return sorted_dict;
}


#define UL_SEQERR(RESULT, ELEMENTS, UDICT, DO_DECREF)                  \
do {                                                                   \
    Py_DECREF(UDICT);                                                  \
    result->status = FAILED;                                           \
    if (!PyErr_Occurred()) {                                           \
        result->exctype = TYPEERROR;                                   \
        result->errmsg = "Object not compatible with sequence";        \
    }                                                                  \
    if (DO_DECREF) Py_DECREF(ELEMENTS);                                \
    return;                                                            \
} while(0)

static void
unique_local(PyObject *elements, const long n, UniqueResult *const result,
             PyObject *const key) {
    result->udict_sorted = false;
    PyObject *const udict = PyDict_New();
    bool do_decref_elements = false;
    if (Py_IsNone(key) > 0) {
        if (!(PyList_Check(elements) || PyTuple_Check(elements))) {
            elements = PySequence_Tuple(elements);
            if (elements == NULL || PyErr_Occurred())
                UL_SEQERR(result, elements, udict, false);
            do_decref_elements = true;
        }
        PyObject *const fastel = PySequence_Fast(elements, "");
        PyObject *const oset = PySet_New(elements);
        if (PyErr_Occurred() || fastel == NULL || oset == NULL)
            UL_SEQERR(result, elements, udict, do_decref_elements);
        PyObject *const oset_iter = PyObject_GetIter(oset);
        PyObject *item;
        while((item = PyIter_Next(oset_iter)) != NULL) {
            long count = 0;
            for (long ix = 0; ix < n; ix++) {
                PyObject *el = PySequence_Fast_GET_ITEM(fastel, ix);
                if (PYCOMP(el, item, Py_EQ)) count += 1;
            }
            PyDict_SetItem(udict, item, PyLong_FromLong(count));
            // both PySet_New and PyIter_Next create a strong reference. We
            // are allowing one to survive to represent the reference held by
            // udict.
            Py_DECREF(item);
        }
        Py_DECREF(fastel);
        Py_DECREF(oset);
        Py_DECREF(oset_iter);
        if (PyErr_Occurred()) {
            UL_SEQERR(result, elements, udict, do_decref_elements);
        }
    } else {
        PyObject *const sorted = GETATTR(IMPORT("builtins"), "sorted");
        PyObject *args = PyTuple_New(1);
        PyObject *kwargs = PyDict_New();
        PyTuple_SetItem(args, 0, elements);
        PyDict_SetItemString(kwargs, "key", key);
        PyObject *const list = PyObject_Call(sorted, args, kwargs);
        Py_DECREF(args);
        Py_DECREF(kwargs);
        if (!list) UL_SEQERR(result, elements, udict, false);
        PyObject *const fastel = PySequence_Fast(list, "");
        if (!fastel) {
            Py_DECREF(list);
            UL_SEQERR(result, elements, udict, false);
        }
        long count = 0;
        PyObject *last = PySequence_Fast_GET_ITEM(fastel, 0);
        PyObject *hash = PYCALL(key, last, NULL);
        for (long ix = 1; ix < PySequence_Fast_GET_SIZE(fastel); ix++) {
            PyObject *const next = PySequence_Fast_GET_ITEM(fastel, ix);
            PyObject *const nexthash = PYCALL(key, next, NULL);
            if (!PYCOMP(nexthash, hash, Py_EQ)) {
                PyDict_SetItem(udict, last, PyLong_FromLong(++count));
                Py_INCREF(last);
                count = 0;
            } else count++;
            last = next, hash = nexthash;
        }
        PyDict_SetItem(udict, last, PyLong_FromLong(++count));
        Py_DECREF(list);
        result->udict_sorted = true;
    }
    if (do_decref_elements) Py_DECREF(elements);
    // TODO: length-0 case
    result->udict = udict;
    result->nunique = PyObject_Length(udict);
}

// NOTE: assumes objs are already sorted and operators are compatible.
UniqueResult *
n_uniq_pyobj_perms(PyObject *elements, const long n, PyObject *const lt) {
    UniqueResult *result = malloc(sizeof(UniqueResult));
    result->status = OK, result->nunique = 0;
    unique_local(elements, n, result, lt);
    if (result->status == FAILED) return result;
    double pdouble = lgamma((double) n + 1);
    PyObject *vals = PyDict_Values(result->udict);
    PyObject *iter = PyObject_GetIter(vals);
    PyObject *val;
    while ((val = PyIter_Next(iter)) != NULL) {
        const long count = PyLong_AsLong(val);
        if (count > 1) pdouble -= lgamma((double) count + 1);
    }
    // TODO: check for overflow by examining pdouble before constructing
    //  permsize
    unsigned long permsize = (unsigned long) roundl(exp(pdouble));
    result->nperm = permsize;
    Py_DECREF(vals);
    Py_DECREF(iter);
    return result;
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
    if (n_args != 2)
        PYRAISE(TYPEERROR, "Function accepts exactly 2 arguments.");
    PyObject *mset = args[0], *key = args[1];
    long n = PyObject_Length(mset);
    MpermSetupState* msst = new_mperm_setup_state();
    msst->n = n;
    if (n < 2) {
        PyObject *output_trivial = PyTuple_New(1);
        PyTuple_SetItem(output_trivial, 0, mset);
        msst->trivial_output = output_trivial;
        return msst;
    }
    if (PyErr_Occurred()) return msst;
    if (!Py_IsNone(key)) {
        if (PyCallable_Check(key) == 0)
            MSST_ERROUT(msst, TYPEERROR, "Invalid relational operator.");
    }
    msst->key = key;
    UniqueResult *uresult = n_uniq_pyobj_perms(mset, n, key);
    if (PyErr_Occurred() || uresult->status == FAILED)
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
do_mpermute(PyObject *self, PyObject *const *args, Py_ssize_t n_args) {
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
    if (msst->uresult == NULL) PYRAISE(RUNTIMEERROR, "Bad uniqueness setup");
    PyObject *perms;
    unsigned long nperm = msst->uresult->nperm;
    PyGC_Disable();
    if (nperm > LONG_MAX || nperm == 0) perms = NULL;
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
    free_uniqueresult(msst->uresult);
    msifree(msi);
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

static PyObject *
mpgen_iter(PyObject *self) {
    return (PyObject *) self;
}

static PyTypeObject MPGeneratorType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_mpermute.MPGenerator",
    .tp_basicsize = sizeof(MPGenState),
    .tp_itemsize = 0,
    .tp_new = (newfunc) mpgen_new,
    .tp_init = (initproc) mpgen_init,
    .tp_iternext = (iternextfunc) mpgen_iternext,
    .tp_iter = mpgen_iter,
    .tp_flags = Py_TPFLAGS_DEFAULT
};


static PyMethodDef
MpermuteMethods[] = {
    {
        "do_mpermute",
        (PyCFunction) do_mpermute,
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
    PyObject *m;
    if (PyType_Ready(&MPGeneratorType) < 0) return NULL;
    m = PyModule_Create(&mpermute_mod);
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
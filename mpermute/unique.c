#include "unique.h"

void
free_uniqueresult(UniqueResult *const ures) {
    PyObject *keys = PyDict_Keys(ures->udict);
    PyObject *iter = PyObject_GetIter(keys);
    PyObject *key;
//     Note: values should always be python ints, i.e., immortal
    while ((key = PyIter_Next(iter)) != NULL) { Py_DECREF(key); }
    Py_DECREF(iter);
    Py_DECREF(keys);
    Py_DECREF(ures->udict);
    free(ures);
}

// incrementing references each time we construct a permutation is slow,
// and because we're always holding at least one reference to each input
// object (and holding the GIL!), there's little risk of anything sketchy
// happening while we're constructing permutations. it's better to just
// batch-set the refcount here at the end based on the fact that each
// permutation tuple should hold a reference to each of its children for
// each time an element of the child's equivalence class appears in the input.
void
ures_incref(const UniqueResult *const ures, const long n_inc) {
    PyObject *items = PyDict_Items(ures->udict);
    PyObject *iter = PyObject_GetIter(items);
    PyObject *item;
    while ((item = PyIter_Next(iter)) != NULL) {
        PyObject *k = PyTuple_GetItem(item, 0);
        PyObject *v = PyTuple_GetItem(item, 1);
        // ignore immortal objects
        if (Py_REFCNT(k) < 4000000) { ;
            Py_SET_REFCNT(k, Py_REFCNT(k) + n_inc * PyLong_AsLong(v));
        }
        Py_DECREF(item);
    }
    Py_DECREF(items);
    Py_DECREF(iter);
}

PyObject *
sort_dict(PyObject *const dict) {
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
    result->status = UNIQUE_FAILED;                                           \
    if (!PyErr_Occurred()) {                                           \
        result->exctype = TYPEERROR;                                   \
        result->errmsg = "Object not compatible with sequence";        \
    }                                                                  \
    if (DO_DECREF) Py_DECREF(ELEMENTS);                                \
    return;                                                            \
} while(0)

static inline bool
rich_eq(const richcmpfunc f, PyObject *const a, PyObject *const b) {
    PyObject *const res = (*f)(a, b, Py_EQ);
    if (res == Py_True) return true;
    Py_DECREF(res);  // can be an exception or something
    return false;
}

static inline PyObject *
identity_proxy(PyObject *const keyfunc_unused, PyObject *const *args,
               const size_t n_unused, PyObject *const kwargs_unused) {
    return args[0];
}

static void
unique_local(PyObject *elements, const long n, UniqueResult *const result,
             PyObject *const key) {
    result->udict_sorted = false;
    PyObject *const udict = PyDict_New();
    if (n == 0) { result->udict = udict; return; } // nothing to do
    PyObject *const sorted = GETATTR(IMPORT("builtins"), "sorted");
    PyObject *args = PyTuple_New(1);
    PyObject *kwargs = PyDict_New();
    PyTuple_SetItem(args, 0, elements);
    Py_INCREF(elements);
    PyDict_SetItemString(kwargs, "key", key);
    PyObject *const list = PyObject_Call(sorted, args, kwargs);
    Py_DECREF(args);
    Py_DECREF(kwargs);
    if (!list) UL_SEQERR(result, elements, udict, false);
    long count = 0;
    PyObject *(*kf)(PyObject *, PyObject * const*, size_t, PyObject *);
    if (Py_IsNone(key)) kf = identity_proxy;
    else kf = PyObject_Vectorcall;
    PyObject *last = PySequence_Fast_GET_ITEM(list, 0);
    PyObject *vargs[1] = {last};
    PyObject *hash = kf(key, vargs, 1, NULL);
    const richcmpfunc rcf = Py_TYPE(hash)->tp_richcompare;
    if (rcf == NULL) {
        Py_DECREF(hash);
        PyErr_SetString(TYPEERROR, "Bad equality operator");
        result->status = UNIQUE_FAILED;
        return;
    }
    for (long ix = 1; ix < PySequence_Fast_GET_SIZE(list); ix++) {
        PyObject *const next = PySequence_Fast_GET_ITEM(list, ix);
        vargs[0] = next;
        PyObject *const nexthash = kf(key, vargs, 1, NULL);
        if (!rich_eq(rcf, nexthash, hash)) {
//            if (!PYCOMP(nexthash, hash, Py_EQ)) {
            PyDict_SetItem(udict, last, PyLong_FromLong(++count));
            Py_INCREF(last);
            count = 0;
        } else count++;
        last = next, hash = nexthash;
    }
    PyDict_SetItem(udict, last, PyLong_FromLong(++count));
    Py_DECREF(list);
    result->udict_sorted = true;
    result->udict = udict;
    result->nunique = PyObject_Length(udict);
}

UniqueResult *
n_uniq_pyobj_perms(PyObject *elements, const long n, PyObject *const lt) {
    UniqueResult *result = malloc(sizeof(UniqueResult));
    result->status = UNIQUE_OK, result->nunique = 0;
    unique_local(elements, n, result, lt);
    if (result->status == UNIQUE_FAILED) return result;
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


PyObject *
unique(PyObject *self, PyObject *const *args, Py_ssize_t n_args) {
    if (n_args != 2)
        PYRAISE(TYPEERROR, "Function accepts exactly 2 arguments.");
    PyObject *const mset = args[0], *const key = args[1];
    const long n = PyObject_Length(mset);
    if (!Py_IsNone(key)) {
        if (PyCallable_Check(key) == 0)
            PYRAISE(TYPEERROR, "Invalid relational operator.");
    }
    UniqueResult *uresult = n_uniq_pyobj_perms(mset, n, key);
    if (PyErr_Occurred()) return NULL;
    if (uresult->status == UNIQUE_FAILED)
        PYRAISE(TYPEERROR, "relational / sequence init err");
    PyObject *udict = uresult->udict;
    free(uresult);
    return udict;
}

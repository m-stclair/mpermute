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
        Py_DECREF(oset);
        Py_DECREF(oset_iter);
        Py_DECREF(fastel);
        if (PyErr_Occurred()) {
            UL_SEQERR(result, elements, udict, do_decref_elements);
        }
    } else {
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
        PyObject *const fastel = PySequence_Fast(list, "");
        if (!fastel) {
            Py_DECREF(list);
            UL_SEQERR(result, elements, udict, false);
        }
        long count = 0;
        PyObject *last = PySequence_Fast_GET_ITEM(fastel, 0);
        PyObject *vargs[1] = {last};
        PyObject *dkw = PyDict_New();
        PyObject *hash = PyObject_Vectorcall(key, vargs, 1, dkw);
        for (long ix = 1; ix < PySequence_Fast_GET_SIZE(fastel); ix++) {
            PyObject *const next = PySequence_Fast_GET_ITEM(fastel, ix);
            vargs[0] = next;
            PyObject *const nexthash = PyObject_Vectorcall(key, vargs, 1, dkw);
            if (!PYCOMP(nexthash, hash, Py_EQ)) {
                PyDict_SetItem(udict, last, PyLong_FromLong(++count));
                Py_INCREF(last);
                count = 0;
            } else count++;
            last = next, hash = nexthash;
        }
        Py_DECREF(dkw);
        PyDict_SetItem(udict, last, PyLong_FromLong(++count));
        Py_DECREF(list);
        Py_DECREF(fastel);
        result->udict_sorted = true;
    }
    if (do_decref_elements) Py_DECREF(elements);
    // TODO: length-0 case
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

#include "group.h"


static inline bool
set_group_tuple(const PyObject *const fastel, PyObject *const groupdict,
                const long ix, const long gstart, PyObject *const key,
                PyObject *const slist) {
    PyObject *gtup = PyTuple_New(ix - gstart);
    // note that this must be populated first.
    for (long gix = 0; gix < ix - gstart; gix++) {
        PyObject *member = FGETITEM(fastel, gstart + gix);
        PyTuple_SetItem(gtup, gix, member);
        Py_INCREF(member);
    }
    PyDict_SetItem(groupdict, key, gtup);
    if (PyErr_Occurred()) {
        // keyfunc most likely produced a non-hashable, but a memory
        //  error is possible
        Py_DECREF(gtup);
        Py_DECREF(groupdict);
        Py_DECREF(slist);
        Py_DECREF(fastel);
        return false;
    }
    return true;
}

PyObject *
groupby_thing(PyObject *self, PyObject *const *args, const Py_ssize_t n_args) {
    if (n_args != 2)
        PYRAISE(TYPEERROR, "Function accepts exactly 2 arguments.");
    PyObject *const elements = args[0], *const keyfunc = args[1];
    if (!PyCallable_Check(keyfunc)) PYRAISE(TYPEERROR, "Second argument must "
                                                       "be callable.");
    const long n = PyObject_Length(elements);
    PyObject *const sorted = GETATTR(IMPORT("builtins"), "sorted");
    PyObject *const sargs = PyTuple_New(1);
    PyObject *const skwargs = PyDict_New();
    PyTuple_SetItem(sargs, 0, elements);
    Py_INCREF(elements);
    PyDict_SetItemString(skwargs, "key", keyfunc);
    PyObject *const slist = PyObject_Call(sorted, sargs, skwargs);
    Py_DECREF(sargs);
    Py_DECREF(skwargs);
    if (!slist) return NULL;
    PyObject *const fastel = PySequence_Fast(slist, "");
    if (!fastel) {
        Py_DECREF(slist);
        return NULL;
    }
    long gstart = 0;
    PyObject *key = PYCALL(keyfunc, FGETITEM(fastel, 0), NULL);
    PyObject *groupdict = PyDict_New();
    if (groupdict == NULL) return NULL;
    for (long ix = 1; ix < n; ix++) {
        PyObject *el = FGETITEM(fastel, ix);
        PyObject *const nextkey = PYCALL(keyfunc, el, NULL);
        if (!PYCOMP(nextkey, key, Py_EQ)) {
            bool success = set_group_tuple(fastel, groupdict, ix, gstart, key,
                                           slist);
            if (success == false) return NULL;
            gstart = ix;
        }
        key = nextkey;
    }
    bool success = set_group_tuple(fastel, groupdict, n, gstart, key, slist);
    if (success == false) return NULL;
    return groupdict;
}

static inline bool
add_to_groupdict(
    PyObject *const groupdict, PyObject *const key, PyObject *const value
) {
    PyObject *glist = PyDict_GetItem(groupdict, key);
    if (PyErr_Occurred()) return false;
    if (glist == NULL) {
        glist = PyList_New(1);
        if (glist == NULL) return false;
        PyList_SetItem(glist, 0, value);
        if (PyErr_Occurred()) return false;
        PyDict_SetItem(groupdict, key, glist);
        if (PyErr_Occurred()) return false;
        Py_SET_REFCNT(glist, 1);
        if (PyErr_Occurred()) return false;
    }
    else {
        PyList_Append(glist, value);
        // note that PyList_Append adds a reference, while PyList_SetItem
        // steals one
        Py_DECREF(value);
    }
    return true;
}

PyObject *
groupby(PyObject *self, PyObject *const *args, const Py_ssize_t n_args) {
    if (n_args != 2)
        PYRAISE(TYPEERROR, "Function accepts exactly 2 arguments.");
    PyObject *const elements = args[0], *const keyfunc = args[1];
    if (!PyCallable_Check(keyfunc)) PYRAISE(TYPEERROR, "Second argument must "
                                                       "be callable.");
    PyGC_Disable();
    PyObject *const groupdict = PyDict_New();
    for (long i = 0; i < PyObject_Length(elements); i++) {
        PyObject *el = PySequence_GetItem(elements, i);
        PyObject *const kargs[1] = {el};
        PyObject *const key = PyObject_Vectorcall(keyfunc, kargs, 1, NULL);
        if (PyErr_Occurred()) goto loop_errout;
        bool res = add_to_groupdict(groupdict, key, el);
        if (res == false) goto loop_errout;
        Py_DECREF(key);
        continue;

        loop_errout:
            if (key != NULL) Py_DECREF(key);
            Py_DECREF(groupdict);
            if (!PyErr_Occurred())
                PyErr_SetString(RUNTIMEERROR, "Unclassified grouping error.");
            PyGC_Enable();
            return NULL;
    }
    PyGC_Enable();
    return groupdict;
}

typedef struct
GroupC {
    PyObject **objs;
    long n;
    long sz;
    Py_hash_t hash;
    PyObject *key;
    long hix;
} GroupC;

static inline int
gc_comp(const void *a, const void *b) {
    const Py_hash_t comp = (*(GroupC **) a)->hash - (*(GroupC **) b)->hash;
    if (comp > 0) return 1;
    if (comp < 0) return -1;
    return 0;
}

static inline GroupC *
new_groupc(PyObject *key) {
    GroupC *gr = malloc(sizeof(GroupC));
    gr->objs = calloc(sizeof(PyObject *), 1024);
    gr->n = 0;
    gr->sz = 1024;
    gr->hix = -1;
    gr->hash = PyObject_Hash(key);
    gr->key = key;
    return gr;
}

typedef struct
GroupCList {
    GroupC **gcs;
    long maxsz;
    long n;
    Py_hash_t maxhash;
    Py_hash_t minhash;
    bool valid;
} GroupCList;

static GroupCList *
new_gcl(long maxsz) {
    GroupCList *gcl = malloc(sizeof(GroupCList));
    gcl->n = 0;
    gcl->gcs = calloc(sizeof(GroupC *), maxsz);
    gcl->valid = (gcl->gcs != NULL);
    gcl->maxsz = maxsz;
    gcl->maxhash = LONG_LONG_MIN;
    gcl->minhash = LONG_LONG_MAX;
    return gcl;
}

static inline void
ordershift(PyObject *const key, PyObject *const match,
           long result[static 2]) {
    result[0] = PYCOMP(key, match, Py_EQ) > 0;
    if (result[0] == 0 && PYCOMP(key, match, Py_GT) > 0) result[1] += 1;
}

static void
find_gc_insert_index(const GroupCList *gcl, const Py_hash_t gch,
                    PyObject *const key, long result[static 2]) {
    long high = gcl->n - 1, low = 0, mid;
    bool pivot_done = false;
    while (pivot_done == false) {
        mid = (high + low) / 2;
        Py_hash_t comphash = gcl->gcs[mid]->hash;
        Py_hash_t comp = gch - comphash;
        if (comp > 0) high = mid - 1;
        else if (comp < 0) low = mid + 1;
        else {
            result[0] = 1;
            result[1] = mid;
            break;
        }
        if (low > high) pivot_done = true;
    }
    if (result[1] == -1) {
        result[1] = low == mid ? mid : low;
        return;
    }
    PyObject *const hashmatch = gcl->gcs[result[1]]->key;
    ordershift(key, hashmatch, result);
}

static void
find_in_gcl(const GroupCList *const gcl, const Py_hash_t khash,
            PyObject *const key, long result[static 2]) {
    result[0] = 0;
    if (khash == gcl->minhash) {
        PyObject *const minkey = gcl->gcs[0]->key;
        result[1] = 0;
        ordershift(key, minkey, result);
        return;
    }
    else if (khash < gcl->minhash) result[1] = 0;
    else if (khash == gcl->maxhash) {
        result[1] = gcl->n - 1;
        PyObject *const maxkey = gcl->gcs[gcl->n - 1]->key;
        ordershift(key, maxkey, result);
    }
    else if (khash > gcl->maxhash) result[1] = gcl->n;
    else find_gc_insert_index(gcl, khash, key, result);
}

static char
grow_gcl(GroupCList *gcl, const long size) {
    if (size <= gcl->maxsz) return 0;
    GroupC **newbuf = (GroupC **) realloc(gcl->gcs, size * sizeof(long));
    if (newbuf == NULL) return -1;
    gcl->gcs = newbuf;
    gcl->maxsz = size;
    return 0;
}

static inline char
autogrow_gcl(GroupCList *gcl) {
    if (gcl->n < gcl->maxsz - 10) return 0;
    return grow_gcl(gcl, gcl->maxsz * 2);
}

int
gcladd(GroupCList *const gcl, PyObject *const key, long result[2]) {
    const Py_hash_t khash = PyObject_Hash(key);
    if (PyErr_Occurred()) return -1;
    if (gcl->n == 0) {
//        printf("Initialized gcl with %s: %li\n", PYREPR(key), khash);
        gcl->gcs[0] = new_groupc(key);
        gcl->minhash = khash;
        gcl->maxhash = khash;
        gcl->n = 1;
        result[0] = 0, result[1] = 0;
        return 1;
    }
    find_in_gcl(gcl, khash, key, result);
//    printf("%s: %li -- {%li, %li}\n", PYREPR(key), khash, result[0], result[1]);
    if (result[0] == 1) return 0;
    if (result[1] == 0) gcl->minhash = khash;
    else if (result[1] == gcl->n) gcl->maxhash = khash;
    if (autogrow_gcl(gcl) != 0) return -1;
    if (result[1] != gcl->n)
        for (long i = gcl->n - 1; i > result[1] - 1; i--)
            gcl->gcs[i + 1] = gcl->gcs[i];
    gcl->gcs[result[1]] = new_groupc(key);
    gcl->n += 1;
    return 1;
}

static inline void
free_gcl(GroupCList *const gcl) {
    if (gcl->gcs != NULL) {
        for (long gcix = 0; gcix < gcl->n; gcix++) {
            GroupC *const gc = gcl->gcs[gcix];
            // n.b. none of these guard conditions should ever happen
            if (gc == NULL) continue;
            if (gc->key != NULL) Py_DECREF(gc->key);
            if (gc->objs != NULL) free(gc->objs);
            free(gc);
        }
    }
    free(gcl->gcs);
    free(gcl);
}

PyObject *
dictbuild(PyObject *self, PyObject *const *args, const Py_ssize_t n_args) {
    if (n_args != 2)
        PYRAISE(TYPEERROR, "This function takes exactly 2 arguments.");
    PyObject *const elements = args[0], *const keyfunc = args[1];
    if (!PySequence_Check(elements))
        PYRAISE(TYPEERROR, "First argument must be a sequence.");
    if (!PyCallable_Check(keyfunc))
        PYRAISE(TYPEERROR, "Second argument must be callable.");
    PyGC_Disable();
    const long n = PyObject_Size(elements);
    if (PyErr_Occurred()) return NULL;
    PyObject *fastel = PySequence_Fast(elements, "");
    if (PyErr_Occurred()) return NULL;
    GroupCList *const gcl = new_gcl(n / 10);
    if (gcl->valid == false) {
        free(gcl);
        Py_DECREF(fastel);
        PyGC_Enable();
        PyErr_NoMemory();
        return NULL;
    }
    PyObject *gdict = PyDict_New();
    for (long i = 0; i < n; i++) {
        PyObject *const el = FGETITEM(fastel, i);
        PyObject *const key = pycall_v1(keyfunc, el);
//        printf("generated key %s w/refcount %li [id %li]\n", PYREPR(key),
//               Py_REFCNT(key), (uintptr_t) key);
        if (PyErr_Occurred()) goto loop_1_errout;
//        printf("element %li: %s [%s]\n", i, PYREPR(el), PYREPR(key));
        long iresult[2];
        int added = gcladd(gcl, key, iresult);
//        printf("iresult: {%li, %li}\n", iresult[0], iresult[1]);
        if (added == -1) goto loop_1_errout;
        GroupC *const gr = gcl->gcs[iresult[1]];
//        printf("added? %i, group_p is %lu w/key %s at addr %lu\n", added,
//               (uintptr_t) gr, PYREPR(key), (uintptr_t) gr->key);
        if (gr->n >= gr->sz) {
            gr->sz *= 2;
            PyObject **newbuf = realloc(gr->objs,
                                        gr->sz * sizeof(PyObject *));
            if (newbuf == NULL) goto loop_1_errout;
            gr->objs = newbuf;
        }
//        printf("Setting element %li of group w/key %s to %s\n",
//               gr->n, PYREPR(gr->key), PYREPR(el));
        gr->objs[gr->n++] = el;
        if (added == 0) {
//            printf("decrefing %s w/refcount %li [id %li]\n",
//                   PYREPR(key), Py_REFCNT(key), (uintptr_t) key);
            Py_DECREF(key);
        }
        continue;

        loop_1_errout:
            if (key != NULL) Py_DECREF(key);
            goto errout;
    }
    for (long i = 0; i < gcl->n; i++) {
        GroupC *const gr = gcl->gcs[i];
        if (gr == NULL || gr->objs == NULL) continue;  // should never happen
        PyObject *outtup = PyTuple_New(gr->n);
        if (outtup == NULL) goto errout;
        for (long eix = 0; eix < gr->n; eix++) {
            PyObject *const groupel = gr->objs[eix];
            if (groupel == NULL) {
                char errstring[100];
                sprintf(errstring, "element %li of group w/key %s missing\n",
                        i, PYREPR(gr->key));
                PyErr_SetString(PyExc_RuntimeError, errstring);
                goto errout;
            }
            PyTuple_SetItem(outtup, eix, groupel);
            Py_INCREF(groupel);
        }
        PyObject *const key = gr->key;
        PyDict_SetItem(gdict, key, outtup);
//        Py_DECREF(key);
        Py_DECREF(outtup);
    }
    Py_DECREF(fastel);
    free_gcl(gcl);
    PyGC_Enable();
    return gdict;

    errout:
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_MemoryError, "Could not allocate buffers.");
        free_gcl(gcl);
        Py_DECREF(fastel);
        Py_DECREF(gdict);
        PyGC_Enable();
        return NULL;
}

//    const long n = PyObject_Length(elements);
//    for (long i = 0; i < n; i++) {
//        PyObject *const el = PySequence_GetItem(elements, i);
//        PyObject *const count = PyDict_GetItem(cdict, el);
//        if (count == NULL) PyDict_SetItem(cdict, el, one);
//        else PyDict_SetItem(cdict, el, PyNumber_Add(count, one));
//    }
//    PyObject *key, *value;
//    Py_ssize_t pos = 0;
//    PyObject *gdict = PyDict_New();
//    while (PyDict_Next(cdict, &pos, &key, &value)) {
//        PyObject *emptytup = PyTuple_New(PyLong_AsLong(value));
//        PyDict_SetItem(gdict, key, emptytup);
//    }
//    const long ngroups = PyObject_Length(cdict);
//    long **gixes = calloc(sizeof(long), ngroups);
//    for (long i = 0; i < n; i++) {
//        PyTuple_SET_ITEM(PyDict_GetItem())
//    }
//
//    free(gixes);
//    return Py_None;
//}

//
//PyObject *
//groupby_so_many_loops(PyObject *self, PyObject *const *args, const Py_ssize_t n_args) {
//    if (n_args != 2)
//        PYRAISE(TYPEERROR, "Function accepts exactly 2 arguments.");
//    PyObject *const elements = args[0], *const keyfunc = args[1];
//    if (!PyCallable_Check(keyfunc)) PYRAISE(TYPEERROR, "Second argument must "
//                                                       "be callable.");
//    const long n = PyObject_Length(elements);
//    PyObject *const iter = PyObject_GetIter(elements);
//    if (PyErr_Occurred()) return NULL;
//    PyObject * *const keyarr = malloc(sizeof(PyObject *) * n);
//    PyObject *const countdict = PyDict_New(), *el;
//    PyObject *const one = PyLong_FromLong(1);
//    long kix = 0;
//    while ((el = PyIter_Next(iter)) != NULL) {
//        PyObject *const key = PYCALL(keyfunc, el);
//        PyObject *countobj = PyDict_GetItem(countdict, key);
//        if (countobj == NULL) PyDict_SetItem(countdict, key, one);
//        else PyDict_SetItem(countdict, key, PyNumber_Add(countobj, one));
//        keyarr[kix++] = key;
////        if (PyErr_Occurred()) {
////            Py_DECREF(groupdict);
////            if (key != NULL) Py_DECREF(key);
////            Py_DECREF(iter);
////            return NULL;
////        }
//    }
//    Py_DECREF(iter);
//    PyObject *const groupdict = PyDict_New();
//    PyObject *const countitems = PyDict_Items(countdict);
//    PyObject *const countiter = PyObject_GetIter(countitems);
//    while ((el = PyIter_Next(countiter)) != NULL) {
//        PyObject *const ckey = PyTuple_GetItem(el, 0);
//        PyObject *const cval = PyTuple_GetItem(el, 1);
//        PyObject *glist = PyList_New(PyLong_AsLong(cval));
//        PyDict_SetItem(groupdict, ckey, glist);
//    }
//    free(countiter);
//    free(countitems);
//    long *const glix = calloc(sizeof(long), n);
//    PyObject *const iter_2 = PyObject_GetIter(elements);
//    long eix = 0;
//    while ((el = PyIter_Next(iter_2)) != NULL) {
//        PyObject *const key = keyarr[eix++];
//
//    }
//    Py_DECREF(iter);
//    return groupdict;
//}

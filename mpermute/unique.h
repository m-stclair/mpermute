#ifndef MPERMUTE_UNIQUE_H
#define MPERMUTE_UNIQUE_H

#include "api_helpers.h"

enum UniqueStatus { UNIQUE_OK, UNIQUE_FAILED };

typedef struct
UniqueResult {
    unsigned long nunique;
    unsigned long nperm;
    PyObject *udict;
    enum UniqueStatus status;
    char *errmsg;
    PyObject *exctype;
    bool udict_sorted;
} UniqueResult;

UniqueResult *n_uniq_pyobj_perms(PyObject *, long, PyObject *);
void free_uniqueresult(UniqueResult *);
PyObject *sort_dict(PyObject *);
PyObject *unique(PyObject *, PyObject *const *, Py_ssize_t);
PyObject *unique_2(PyObject *, PyObject *const *, Py_ssize_t);
void ures_incref(const UniqueResult *, long);

#endif //MPERMUTE_UNIQUE_H

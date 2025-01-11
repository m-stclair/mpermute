#ifndef MPERMUTE_GROUP_H
#define MPERMUTE_GROUP_H

#include "api_helpers.h"

PyObject *groupby(PyObject *, PyObject *const *, Py_ssize_t);
PyObject *dictbuild(PyObject *, PyObject *const *, Py_ssize_t);

#endif //MPERMUTE_GROUP_H

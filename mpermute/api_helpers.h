#ifndef MPERMUTE_API_HELPERS_H
#define MPERMUTE_API_HELPERS_H

#include <stdbool.h>

#include <Python.h>
#define PY_SSIZE_T_CLEAN

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


#define PYRAISE(EXCTYPE, MSG)      \
do {                               \
    PyErr_SetString(EXCTYPE, MSG); \
    return NULL;                   \
} while(0)

#endif //MPERMUTE_API_HELPERS_H

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
#define FGETITEM PySequence_Fast_GET_ITEM
#define INT2VOID PyLong_AsVoidPtr
#define VOID2INT PyLong_FromVoidPtr

#define PYCOMP(AOBJ, BOBJ, COMP)                       \
(bool) PyObject_RichCompareBool(AOBJ, BOBJ, COMP)      \
\

#define PYRAISE(EXCTYPE, MSG)      \
do {                               \
    PyErr_SetString(EXCTYPE, MSG); \
    return NULL;                   \
} while(0)

static inline PyObject *
pycall_v1(PyObject *const func, PyObject *const arg) {
    PyObject *const vec_args[1] = {arg};
    return PyObject_Vectorcall(func, vec_args, 1, NULL);
}
static inline PyObject *
pycall_v2(PyObject *const func, PyObject *const arg1, PyObject *const arg2) {
    PyObject *const vec_args[2] = {arg1, arg2};
    return PyObject_Vectorcall(func, vec_args, 2, NULL);
}


#endif //MPERMUTE_API_HELPERS_H

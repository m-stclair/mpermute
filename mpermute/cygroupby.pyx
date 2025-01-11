from cpython.dict cimport PyDict_GetItem, PyDict_SetItem
from cpython.list cimport PyList_Append, PyList_GET_ITEM, PyList_GET_SIZE
from cpython.ref cimport PyObject, Py_INCREF
from cpython.tuple cimport PyTuple_GET_ITEM, PyTuple_GetSlice, PyTuple_New, PyTuple_SET_ITEM

cdef inline object _groupby_core(dict d, object key, object item):
    cdef PyObject *obj = PyDict_GetItem(d, key)
    if obj is NULL:
        val = []
        PyList_Append(val, item)
        PyDict_SetItem(d, key, val)
    else:
        PyList_Append(<object>obj, item)


cpdef dict groupby(object key, object seq):
    """
    Group a collection by a key function

    >>> names = ['Alice', 'Bob', 'Charlie', 'Dan', 'Edith', 'Frank']
    >>> groupby(len, names)  # doctest: +SKIP
    {3: ['Bob', 'Dan'], 5: ['Alice', 'Edith', 'Frank'], 7: ['Charlie']}

    >>> iseven = lambda x: x % 2 == 0
    >>> groupby(iseven, [1, 2, 3, 4, 5, 6, 7, 8])  # doctest: +SKIP
    {False: [1, 3, 5, 7], True: [2, 4, 6, 8]}

    Non-callable keys imply grouping on a member.

    >>> groupby('gender', [{'name': 'Alice', 'gender': 'F'},
    ...                    {'name': 'Bob', 'gender': 'M'},
    ...                    {'name': 'Charlie', 'gender': 'M'}]) # doctest:+SKIP
    {'F': [{'gender': 'F', 'name': 'Alice'}],
     'M': [{'gender': 'M', 'name': 'Bob'},
           {'gender': 'M', 'name': 'Charlie'}]}

    Not to be confused with ``itertools.groupby``

    See Also:
        countby
    """
    cdef dict d = {}
    cdef object item, keyval
    cdef Py_ssize_t i, N
    if callable(key):
        for item in seq:
            keyval = key(item)
            _groupby_core(d, keyval, item)
    elif isinstance(key, list):
        N = PyList_GET_SIZE(key)
        for item in seq:
            keyval = PyTuple_New(N)
            for i in range(N):
                val = <object>PyList_GET_ITEM(key, i)
                val = item[val]
                Py_INCREF(val)
                PyTuple_SET_ITEM(keyval, i, val)
            _groupby_core(d, keyval, item)
    else:
        for item in seq:
            keyval = item[key]
            _groupby_core(d, keyval, item)
    return d

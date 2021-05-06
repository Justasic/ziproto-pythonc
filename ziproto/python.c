#include "common.h"

// define the python functions we create
// doc: https://docs.python.org/3/c-api/structures.html#METH_O
// SO: https://stackoverflow.com/a/56217044
static PyMethodDef module_methods[] = {
    { "decode",  (PyCFunction) ziproto_decode, METH_O },
    { "encode",  (PyCFunction) ziproto_encode, METH_O },
    {0}
};


static struct PyModuleDef ziproto_module = {
    PyModuleDef_HEAD_INIT,
    "ziproto",
    "Protocol Buffer used to serialize and compress data",
    -1,
    module_methods
};

PyMODINIT_FUNC
PyInit_ziproto(void)
{
	// Our python object is pretty simplistic.
	// Just one object with 2 functions, both entirely C
	return PyModule_Create(&ziproto_module);
	// Py_InitModule("ziproto", module_methods);
}

#include "common.h"
#include <stdarg.h>

void PrintObject(PyObject *obj, const char *format, ...)
{
	PyObject *namestr_obj = PyObject_ASCII(obj);
	Py_INCREF(namestr_obj);
	// Now try and get the string'd version of that object
	puts(PyUnicode_AsUTF8(namestr_obj));
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
	Py_DECREF(namestr_obj);
}

PyObject *ziproto_decode(PyObject *self, PyObject *bytes_obj)
{
	if (PyBytes_Check(bytes_obj) || PyByteArray_Check(bytes_obj))
	{
		Py_INCREF(bytes_obj);
		if (PyObject_CheckBuffer(bytes_obj))
		{
			PyObject *memview = PyMemoryView_FromObject(bytes_obj);
			Py_buffer *data = PyMemoryView_GET_BUFFER(memview);
// 			void *data = PyBuffer_GetPointer(data, data->indices);
			printf("provided %d or %d bytes of data in %s format\n", data->len, data->itemsize, data->format);
			for (Py_ssize_t  i = 0; i < data->len; ++i)
			{
				uint8_t *d = (uint8_t*)data->buf;
				printf("0x%x", d[i]);
				if (i+1 >= data->len || i % 10 == 0)
					puts("\n");
			}
// 			PyBuffer_Release(data);
			Py_DECREF(memview);
		}
		else
		{
			PyObject *namestr_obj = PyObject_ASCII(bytes_obj);
			Py_INCREF(namestr_obj);
			// Now try and get the string'd version of that object
			PyObject *retval = PyErr_Format(PyExc_OverflowError, "Decode failed. %s", PyUnicode_AsUTF8(namestr_obj));
			Py_DECREF(namestr_obj);
			return retval;
		}
		Py_DECREF(bytes_obj);
	}
	else
	{
		PyObject *namestr_obj = PyObject_ASCII(bytes_obj);
		Py_INCREF(namestr_obj);
		// Now try and get the string'd version of that object
		PyObject *retval = PyErr_Format(PyExc_OverflowError, "Decode failed. %s", PyUnicode_AsUTF8(namestr_obj));
		Py_DECREF(namestr_obj);
		return retval;
	}
	// Decode the bytes object passed into ziproto.decode()
	Py_RETURN_NOTIMPLEMENTED;
}

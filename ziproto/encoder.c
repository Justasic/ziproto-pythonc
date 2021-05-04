#include "common.h"
#include <stdbool.h>

// Resources:
// 1. https://stackoverflow.com/questions/56214129/python-c-api-how-to-check-if-an-object-is-an-instance-of-a-type
// 2. https://docs.python.org/3/c-api/concrete.html
// 3. https://docs.python.org/3/c-api/index.html
// 4. https://realpython.com/build-python-c-extension-module/#pylong_fromlongbytes_copied
// 5. https://stackoverflow.com/a/29732914
// 6. https://docs.python.org/3/extending/extending.html

PyObject *EncodeAndDestroy(ZiHandle_t *handle)
{
	//EncodeTypeSingle;
	if (!handle) [[unlikely]]
		return PyErr_Format(PyExc_RuntimeError, "Encoding failure from C");

	PyObject *ret = PyBytes_FromStringAndSize((const char *)GetZiData(handle), GetZiSize(handle));
	FreeZiHandle(handle);
	return ret;
}

PyObject *EncodeFailure(PyObject *obj)
{
	PyObject *namestr_obj = PyObject_ASCII(obj);
	Py_INCREF(namestr_obj);
	// Now try and get the string'd version of that object
	PyObject *retval = PyErr_Format(PyExc_OverflowError, "Encode failed. %s", PyUnicode_AsUTF8(namestr_obj));
	Py_DECREF(namestr_obj);
	return retval;
}

PyObject *ziproto_encode(PyObject *self, PyObject *obj)
{
	// Encode "None" from Python
	// https://stackoverflow.com/a/29732914
	if (obj == Py_None)
		return EncodeAndDestroy(EncodeTypeSingle(NULL, NIL_TYPE, NULL, 0));
	else if (PyBool_Check(obj))
	{
		bool istrue = PyObject_IsTrue(obj);
		if (istrue)
			return EncodeAndDestroy(EncodeTypeSingle(NULL, BOOL_TYPE, &istrue, sizeof(bool)));
		else
			return EncodeAndDestroy(EncodeTypeSingle(NULL, BOOL_TYPE, &istrue, sizeof(bool)));
	}
	else if (PyLong_Check(obj))
	{
		int overflow = 0;
		long long svalue = PyLong_AsLongLongAndOverflow(obj, &overflow);

		// signed long long will overflow but we have to support numbers
		// in the range of unsigned long long. Really big negative numbers
		// are not supported though so error out early.
		if (overflow == -1)
			return EncodeFailure(obj);

		// Convert our value
		if (svalue >= 0 || overflow == 1)
		{
			unsigned long long uvalue = PyLong_AsUnsignedLongLong(obj);
			// We can't handle really big integers.
			if (uvalue == -1ULL && PyErr_Occurred())
				return EncodeFailure(obj);

			return EncodeAndDestroy(EncodeTypeSingle(NULL, UINT_TYPE, &uvalue, sizeof(uvalue)));
		}
		else
			return EncodeAndDestroy(EncodeTypeSingle(NULL, INT_TYPE, &svalue, sizeof(svalue)));
	}
	else if (PyFloat_Check(obj))
	{
		double value = PyFloat_AsDouble(obj);
		// If there was an error getting the value for some reason.
		if (value == -1.0f && PyErr_Occurred())
			return EncodeFailure(obj);

		return EncodeAndDestroy(EncodeTypeSingle(NULL, FLOAT_TYPE, &value, sizeof(value)));
	}
	else if (PyBytes_Check(obj) || PyByteArray_Check(obj))
	{
		Py_INCREF(obj);
		const char *buffer = 0;
		Py_ssize_t length = 0;
		if (PyBytes_Check(obj))
		{
			buffer = PyBytes_AsString(obj);
			length = PyBytes_Size(obj);
		}
		else
		{
			buffer = PyByteArray_AsString(obj);
			length = PyByteArray_Size(obj);
		}

		if (PyErr_Occurred())
		{
			Py_DECREF(obj);
			return EncodeFailure(obj);
		}
		PyObject *ret = EncodeAndDestroy(EncodeTypeSingle(NULL, BIN_TYPE, buffer, length));
		Py_DECREF(obj);
		return ret;
	}
	else if (PyUnicode_Check(obj))
	{
		Py_ssize_t length = 0;
		const char *text = PyUnicode_AsUTF8AndSize(obj, &length);
		if (text)
			return EncodeAndDestroy(EncodeTypeSingle(NULL, STR_TYPE, text, length));
		else
			return EncodeFailure(obj);
	}
	// else if (PyDict_Check(obj))
	// {
	// 	// Dictionaries are a bit weird, we'll iterate the dictionary
	// 	// then encode the data type as a byte object.

	// 	// TODO: this is really a cheap hack, we should do our own
	// 	// implementation to use internal buffers instead of recursive
	// 	// calls to this function.
	// 	Py_ssize_t length = PyDict_Size(obj);

	// 	ZiProtoFormat_t type;
	// 	if (length <= 0xF)
	// 		type = FIXMAP;
	// 	else if (length <= 0xFFFF)
	// 		type = ARRAY16;
	// 	else if (length <= 0xFFFFFFFF)
	// 		type = ARRAY32;
	// 	else
	// 		return EncodeFailure(obj);

	// 	PyObject *key_bytes, *value_bytes;
	// 	Py_ssize_t pos = 0;
	// 	while (PyDict_Next(obj, &pos, &key_bytes, &value_bytes))
	// 	{
	// 		PyObject *key_bytes = ziproto_encode(self, value_bytes);
	// 		Py_INCREF(key_bytes);
	// 		PyObject *value_bytes = ziproto_encode(self, value_bytes);
	// 		Py_INCREF(value_bytes);

	// 		if (!PyBytes_Check(key_bytes))
	// 			return key_bytes;
	// 		if (!PyBytes_Check(value_bytes))
	// 			return value_bytes;

			
	// 	}
	// }

	// If ZiProto is upgraded in the future to support additional types
	// return not implemented which I guess is safe enough.
	Py_RETURN_NOTIMPLEMENTED;
}
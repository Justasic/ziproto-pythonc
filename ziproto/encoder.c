#include "common.h"
#include <stdbool.h>

// Resources:
// 1. https://stackoverflow.com/questions/56214129/python-c-api-how-to-check-if-an-object-is-an-instance-of-a-type
// 2. https://docs.python.org/3/c-api/concrete.html
// 3. https://docs.python.org/3/c-api/index.html
// 4. https://realpython.com/build-python-c-extension-module/#pylong_fromlongbytes_copied
// 5. https://stackoverflow.com/a/29732914
// 6. https://docs.python.org/3/extending/extending.html

void PrintObject(PyObject *obj)
{
	if (!obj)
	{
		printf("Object: <NULL>\n");
		return;
	}

	PyObject *namestr_obj = PyObject_ASCII(obj);
	Py_INCREF(namestr_obj);
	printf("Object: %s\n", PyUnicode_AsUTF8(namestr_obj));
	Py_DECREF(namestr_obj);
}

PyObject *EncodeAndDestroy(ZiHandle_t *handle)
{
	//EncodeTypeSingle;
	if (!handle) [[unlikely]]
		return PyErr_Format(PyExc_RuntimeError, "Encoding failure from C");

	PyObject *memobj = PyMemoryView_FromMemory(handle->EncodedData, handle->szEncodedData, PyBUF_READ);
	PyObject *ret = PyBytes_FromObject(memobj);

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

ZiHandle_t *EncodePySingle(ZiHandle_t *handle, PyObject *obj)
{
	// Encode "None" from Python
	// https://stackoverflow.com/a/29732914
	if (obj == Py_None)
		return EncodeTypeSingle(handle, NIL_TYPE, NULL, 0);
	else if (PyBool_Check(obj))
	{
		bool istrue = PyObject_IsTrue(obj);
		if (istrue)
			return EncodeTypeSingle(handle, BOOL_TYPE, &istrue, sizeof(bool));
		else
			return EncodeTypeSingle(handle, BOOL_TYPE, &istrue, sizeof(bool));
	}
	else if (PyLong_Check(obj))
	{
		int		  overflow = 0;
		long long svalue   = PyLong_AsLongLongAndOverflow(obj, &overflow);

		// signed long long will overflow but we have to support numbers
		// in the range of unsigned long long. Really big negative numbers
		// are not supported though so error out early.
		if (overflow == -1)
			return NULL;

		// Convert our value
		if (svalue >= 0 || overflow == 1)
		{
			unsigned long long uvalue = PyLong_AsUnsignedLongLong(obj);
			// We can't handle really big integers.
			if (uvalue == -1ULL && PyErr_Occurred())
				return NULL;

			return EncodeTypeSingle(handle, UINT_TYPE, &uvalue, sizeof(uvalue));
		}
		else
			return EncodeTypeSingle(handle, INT_TYPE, &svalue, sizeof(svalue));
	}
	else if (PyFloat_Check(obj))
	{
		double value = PyFloat_AsDouble(obj);
		// If there was an error getting the value for some reason.
		if (value == -1.0f && PyErr_Occurred())
			return NULL;

		return EncodeTypeSingle(handle, FLOAT_TYPE, &value, sizeof(value));
	}
	else if (PyBytes_Check(obj) || PyByteArray_Check(obj))
	{
		Py_INCREF(obj);
		const char *buffer = 0;
		Py_ssize_t	length = 0;
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
			return NULL;
		}
		ZiHandle_t *ret = EncodeTypeSingle(handle, BIN_TYPE, buffer, length);
		Py_DECREF(obj);
		return ret;
	}
	else if (PyUnicode_Check(obj))
	{
		Py_ssize_t	length = 0;
		const char *text   = PyUnicode_AsUTF8AndSize(obj, &length);
		if (text)
			return EncodeTypeSingle(handle, STR_TYPE, text, length);
		else
			return NULL;
	}
	return NULL;
}



PyObject *ziproto_encode(PyObject *self, PyObject *obj)
{
	// TODO: idk can this be better?
	if (obj == Py_None || PyBool_Check(obj) || PyLong_Check(obj) || PyFloat_Check(obj) ||
			PyBytes_Check(obj) || PyByteArray_Check(obj) || PyUnicode_Check(obj))
	{
		return EncodeAndDestroy(EncodePySingle(NULL, obj));
	}
	else if (PyObject_HasAttrString(obj, "__iter__"))
	{
		// It's an array-like object (or so we hope)
		Py_ssize_t length = PyObject_Length(obj);
		if (length == -1)
			return EncodeFailure(obj);

		printf("Length of %ld\n", length);

		// We start an array
		ZiHandle_t *data = EncodeTypeSingle(NULL, ARRAY_TYPE, &length, sizeof(length));

		PyObject *iter = PyObject_GetIter(obj);
		if (!iter || PyCallIter_Check(iter))
			return PyErr_Format(PyExc_RuntimeError, "Was given an object without an __iter__ method (this shoudn't happen????)");

		PyObject *item = NULL;

		while ((item = PyIter_Next(iter)))
		{
			PrintObject(item);
			ZiHandle_t *nextdata = EncodePySingle(data, item);
			if (!nextdata)
			{
				printf("Failed to encode: ");
				PrintObject(item);
			}
			else
				data = nextdata;
			Py_DECREF(item);
		}
		Py_DECREF(iter);

		if (PyErr_Occurred())
		{
			printf("An error happened!\n");
		}
		printf("Returning data!\n");

		return EncodeAndDestroy(data);
	}
	else if (PyDict_Check(obj))
	{
		// Dictionaries are a bit weird, we'll iterate the dictionary
		// then encode the data type as a byte object.

		// TODO: this is really a cheap hack, we should do our own
		// implementation to use internal buffers instead of recursive
		// calls to this function.
		Py_ssize_t length = PyDict_Size(obj);

		ZiProtoFormat_t type;
		if (length <= 0xF)
			type = FIXMAP;
		else if (length <= 0xFFFF)
			type = ARRAY16;
		else if (length <= 0xFFFFFFFF)
			type = ARRAY32;
		else
			return EncodeFailure(obj);

		PyObject *key_bytes, *value_bytes;
		Py_ssize_t pos = 0;
		while (PyDict_Next(obj, &pos, &key_bytes, &value_bytes))
		{
			PyObject *key_bytes = ziproto_encode(self, value_bytes);
			Py_INCREF(key_bytes);
			PyObject *value_bytes = ziproto_encode(self, value_bytes);
			Py_INCREF(value_bytes);

			if (!PyBytes_Check(key_bytes))
				return key_bytes;
			if (!PyBytes_Check(value_bytes))
				return value_bytes;
		}
	}
	// else
	// 	return EncodeFailure(obj);

	// If ZiProto is upgraded in the future to support additional types
	// return not implemented which I guess is safe enough.
	Py_RETURN_NOTIMPLEMENTED;
}
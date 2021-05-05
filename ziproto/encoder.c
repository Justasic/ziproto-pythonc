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

ZiHandle_t *EncodePyType(ZiHandle_t *handle, PyObject *obj)
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
		int       overflow = 0;
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
		Py_ssize_t  length = 0;
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
		Py_ssize_t  length = 0;
		const char *text   = PyUnicode_AsUTF8AndSize(obj, &length);
		if (text)
			return EncodeTypeSingle(handle, STR_TYPE, text, length);
		else
			return NULL;
	}
	else if (PyDict_Check(obj))
	{
		Py_ssize_t length = PyDict_Size(obj);
		if (length == -1)
			return NULL;

		ZiHandle_t *data = EncodeTypeSingle(handle, MAP_TYPE, &length, sizeof(length));
		if (!data)
			return NULL;

		PyObject *key_obj, *value_obj;
		Py_ssize_t pos = 0;
		while (PyDict_Next(obj, &pos, &key_obj, &value_obj))
		{
			printf("Dict Key: ");
			PrintObject(key_obj);
			printf("Dict value: ");
			PrintObject(value_obj);
			
			ZiHandle_t *newhand = EncodePyType(handle, key_obj);
			if (!newhand)
			{
				Py_DECREF(key_obj);
				return NULL;
			}
			
			newhand = EncodePyType(newhand, value_obj);
			if (!newhand)
			{
				Py_DECREF(value_obj);
				return NULL;
			}
			Py_DECREF(key_obj);
			Py_DECREF(value_obj);
			handle = newhand;
		}
		return data;
	}
	else if (PyObject_HasAttrString(obj, "__iter__"))
	{
		// It's an array-like object (or so we hope)
		Py_ssize_t length = PyObject_Length(obj);
		if (length == -1)
			return NULL;

		// We start an array
		ZiHandle_t *data = EncodeTypeSingle(handle, ARRAY_TYPE, &length, sizeof(length));
		// Array too big!
		if (!data)
			return NULL;

		PyObject *iter = PyObject_GetIter(obj);
		if (!iter || PyCallIter_Check(iter))
			return NULL;
			//return PyErr_Format(PyExc_RuntimeError, "Was given an object without an __iter__ method (this shoudn't happen????)");

		PyObject *item = NULL;
		while ((item = PyIter_Next(iter)))
		{
			// Call recursively
			ZiHandle_t *nextdata = EncodePyType(data, item);
			if (!nextdata || PyErr_Occurred())
			{
				FreeZiHandle(data);
				Py_DECREF(item);
				Py_DECREF(iter);
				return NULL;
			}
			else
				data = nextdata;
			Py_DECREF(item);
		}
		Py_DECREF(iter);
		
		return data;
	}
	return NULL;
}

PyObject *ziproto_encode(PyObject *self, PyObject *obj)
{
	ZiHandle_t *data = EncodePyType(NULL, obj);
	if (!data)
	{
		PyObject *namestr_obj = PyObject_ASCII(obj);
		Py_INCREF(namestr_obj);
		// Now try and get the string'd version of that object
		PyObject *retval = PyErr_Format(PyExc_OverflowError, "Encode failed. %s", PyUnicode_AsUTF8(namestr_obj));
		Py_DECREF(namestr_obj);
		return retval;
	}
	
	if (unlikely(!data))
		return PyErr_Format(PyExc_RuntimeError, "Encoding failure from C");

	PyObject *memobj = PyMemoryView_FromMemory(GetZiData(data), GetZiSize(data), PyBUF_READ);
	PyObject *ret = PyBytes_FromObject(memobj);
	FreeZiHandle(data);
	
	return ret;
}

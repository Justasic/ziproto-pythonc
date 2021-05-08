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

// Handle everything but arrays and dictionaries
PyObject *DecodeBytesToPySimple(void *bytes, size_t len)
{
	static const int _sizes[] = {
		sizeof(int8_t),
		sizeof(int16_t),
		sizeof(int32_t),
		sizeof(int64_t)
	};
	static const int _usizes[] = {
		sizeof(uint8_t),
		sizeof(uint16_t),
		sizeof(uint32_t),
		sizeof(uint64_t)
	};
	
	uint8_t *data = (uint8_t*)bytes;
	for (size_t i = 0; i < len; ++i)
	{
		uint8_t byte = data[i];
		if (byte < FIXMAP)
		{
			printf("Type is a POSITIVE_FIXINT! %d\n", byte);
			PyObject *obj = PyLong_FromLong(byte);
			return obj;
		}
		
		if (byte >= FIXMAP && byte < FIXARRAY)
		{
			// TODO: decode this?
			printf("Byte is a FIXMAP! %d\n", byte - FIXMAP);
		}
		
		if (byte >= FIXARRAY && byte < FIXSTR)
		{
			uint8_t len = byte - FIXSTR;
			printf("Byte is a FIXSTR! %d\n", len);
			i += len;
		}
		
		if (byte == NIL)
		{
			Py_RETURN_NONE;
		}
		
		if (byte == TRUE || byte == FALSE)
		{
			printf("Byte is boolean! %s\n", byte - TRUE ? "False" : "True");
			if (byte == TRUE)
			{
				Py_RETURN_TRUE;
			}
			else
			{
				Py_RETURN_FALSE;
			}
		}
		
		if (byte == BIN8 || byte == BIN16 || BIN8 == BIN32)
		{
			printf("Found binary data!\n");
			uint8_t *d = data + i + 1;
			uint32_t len = 0;
			if (byte == BIN8)
				len = *d, d++;
			else if (byte == BIN16)
				memrev(&len, d, sizeof(uint16_t)), d+=sizeof(uint16_t);
			else
				memrev(&len, d, sizeof(uint32_t)), d+=sizeof(uint32_t);
			
			printf("Binary data is length %d\n", len);
			i += len;
		}
		
		if (byte == FLOAT32 || byte == FLOAT64)
		{
			// Python only accepts doubles for floating point values
			double value = 0.0;
			
			if (byte == FLOAT32)
			{
				float fvalue = 0.0;
				memrev(&value, data + i + 1, sizeof(float));
				value = fvalue;
			}
			else
				memrev(&value, data+i+1, sizeof(value));
			
			return PyFloat_FromDouble(value);
		}
		
		if (byte >= UINT8 && byte <= UINT64)
		{
			uint64_t value = 0;
			memrev(value, data + i + 1, _usizes[byte - UINT8]);
			return PyLong_FromUnsignedLongLong(value);
		}
		
		if (byte >= INT8 && byte <= INT64)
		{
			int64_t value = 0;
			memrev(&value, data + i + 1, _sizes[byte - INT8]);
			return PyLong_FromLongLong(value);
		}
		
		if (byte >= STR8 && byte <= STR32)
		{
			uint64_t len = 0;
			memrev(&len, data + i + 1, _usizes[byte - STR8]);
			const char *bad_str = (const char*)(data + i + 1 + _sizes[byte - STR8]);
			return PyUnicode_FromStringAndSize(bad_str, len);
		}
		
		if (byte >= NEGATIVE_FIXINT)
		{
			PyObject *obj = PyLong_FromLong(-32 + (byte - NEGATIVE_FIXINT));
			return obj;
		}
	}
	Py_RETURN_NOTIMPLEMENTED;
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
			printf("provided %d or %d bytes of data in %s format\n", data->len, data->itemsize, data->format);
			PyObject *obj = DecodeBytesToPySimple(data->buf, data->len);
			Py_DECREF(memview);
			return obj;
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

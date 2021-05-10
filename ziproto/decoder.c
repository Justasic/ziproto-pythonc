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

PyObject *DecodeNext(ZiHandle_t *bytedata)
{
	uint8_t *data = bytedata->EncodedData + bytedata->_cursor + 1;
	uint8_t byte = *(bytedata->EncodedData + bytedata->_cursor);
	// Increment past our type byte
	bytedata->_cursor += 1;
	
	printf("Decoding byte: 0x%x\n", byte);
	
	if (byte < FIXMAP)
	{
		return PyLong_FromLong(byte);
	}
	else if (byte >= FIXMAP && byte < FIXARRAY) // Handle FIXMAP
	{
		// TODO: decode this?
		printf("Byte is a FIXMAP! %d\n", byte - FIXMAP);
	}
	else if (byte >= FIXARRAY && byte < FIXSTR) // Handle FIXARRAY
	{
		printf("Byte is a FIXARRAY! %d\n", byte - FIXMAP);
	}
	else if (byte >= FIXSTR && byte < NIL) // Handle FIXSTR
	{
		// FIXSTRs can only be 32 bytes big, 1 byte for null terminator
		char buffer[33] = {0};
		uint8_t len = byte - FIXSTR;
		memcpy(buffer, data, len);
		buffer[len + 1] = 0;
		bytedata->_cursor += len;
		return PyUnicode_FromStringAndSize(buffer, len);
	}
	else if (byte == NIL)
	{
		Py_RETURN_NONE;
	}
	else if (byte == TRUE || byte == FALSE)
	{
		if (byte == TRUE)
		{
			Py_RETURN_TRUE;
		}
		else
		{
			Py_RETURN_FALSE;
		}
	}
	else if (byte == BIN8 || byte == BIN16 || BIN8 == BIN32)
	{
		uint32_t len = 0;
		
		// Copy our length byte (from big endianness)
		memrev(&len, data, _usizes[byte - BIN8]);
		// Advance our cursor
		bytedata->_cursor += _usizes[byte - BIN8];
		// Pointer to the actual data.
		uint8_t *d = data + _usizes[byte - BIN8];
		// Advance our cursor again
		bytedata->_cursor += len;
		
		char *text= malloc(len);
		memset(text, 0, len);
		memcpy(text, d, len);
		
		return PyBytes_FromObject(PyMemoryView_FromMemory(text, len, PyBUF_READ));
	}
	else if (byte == FLOAT32 || byte == FLOAT64)
	{
		// Python only accepts doubles for floating point values
		double value = 0.0;
		
		if (byte == FLOAT32)
		{
			float fvalue = 0.0;
			memrev(&value, data, sizeof(float));
			value = fvalue;
			bytedata->_cursor += sizeof(float);
		}
		else
		{
			memrev(&value, data, sizeof(value));
			bytedata->_cursor += sizeof(value);
		}
		
		return PyFloat_FromDouble(value);
	}
	else if (byte >= UINT8 && byte <= UINT64)
	{
		uint64_t value = 0;
		memrev(&value, data, _usizes[byte - UINT8]);
		bytedata->_cursor += _usizes[byte - UINT8];
		return PyLong_FromUnsignedLongLong(value);
	}
	else if (byte >= INT8 && byte <= INT64)
	{
		int64_t value = 0;
		memrev(&value, data, _sizes[byte - INT8]);
		bytedata->_cursor += _sizes[byte - INT8];
		return PyLong_FromLongLong(value);
	}
	else if (byte >= STR8 && byte <= STR32)
	{
		uint64_t len = 0;
		memrev(&len, data, _usizes[byte - STR8]);
		bytedata->_cursor += _usizes[byte - STR8] + len;
		const char *bad_str = (const char*)(data + _usizes[byte - STR8]);
		return PyUnicode_FromStringAndSize(bad_str, len);
	}
	else if (byte == ARRAY16 || byte == ARRAY32)
	{
		
	}
	else if (byte == MAP16 || byte == MAP32)
	{
		
	}
	else if (byte >= NEGATIVE_FIXINT)
	{
		PyObject *obj = PyLong_FromLong(-32 + (byte - NEGATIVE_FIXINT));
		return obj;
	}
	Py_RETURN_NOTIMPLEMENTED;
}

PyObject *ziproto_decode(PyObject *self, PyObject *bytes_obj)
{
	PyObject *namestr_obj = NULL;
	if (PyBytes_Check(bytes_obj) || PyByteArray_Check(bytes_obj))
	{
		Py_INCREF(bytes_obj);
		if (PyObject_CheckBuffer(bytes_obj))
		{
			PyObject *memview = PyMemoryView_FromObject(bytes_obj);
			Py_buffer *data = PyMemoryView_GET_BUFFER(memview);
			printf("provided %d or %d bytes of data in %s format\n", data->len, data->itemsize, data->format);
			
			// Allocate a ZiHandle_t object
			ZiHandle_t *netkas = malloc(sizeof(ZiHandle_t));
			if (!netkas)
				goto failure;
			
			memset(netkas, 0, sizeof(ZiHandle_t));
			
			netkas->EncodedData = data->buf;
			netkas->szEncodedData = data->len;
			
			PyObject *obj = DecodeNext(netkas);
			
			free(netkas);
			Py_DECREF(memview);
			return obj;
		}
		else
			goto failure;
		
		Py_DECREF(bytes_obj);
	}
	else
		goto failure;
	
	// Decode the bytes object passed into ziproto.decode()
	Py_RETURN_NOTIMPLEMENTED;
failure:
	namestr_obj = PyObject_ASCII(bytes_obj);
	Py_INCREF(namestr_obj);
	// Now try and get the string'd version of that object
	PyObject *retval = PyErr_Format(PyExc_OverflowError, "Decode failed. %s", PyUnicode_AsUTF8(namestr_obj));
	Py_DECREF(namestr_obj);
	return retval;
}

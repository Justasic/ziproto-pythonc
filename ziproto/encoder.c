#include "common.h"

// Resources:
// 1. https://stackoverflow.com/questions/56214129/python-c-api-how-to-check-if-an-object-is-an-instance-of-a-type
// 2. https://docs.python.org/3/c-api/concrete.html
// 3. https://docs.python.org/3/c-api/index.html
// 4. https://realpython.com/build-python-c-extension-module/#pylong_fromlongbytes_copied
// 5. https://stackoverflow.com/a/29732914
// 6. https://docs.python.org/3/extending/extending.html



PyObject *EncodeFailure(PyObject *obj)
{
	PyObject *namestr_obj = PyObject_ASCII(obj);
	Py_INCREF(namestr_obj);
	// Now try and get the string'd version of that object
	PyObject *retval = PyErr_Format(PyExc_OverflowError, "Encode failed. %s", PyUnicode_AsUTF8(namestr_obj));
	Py_DECREF(namestr_obj);
	return retval;
}

// Small stack buffer to avoid the heap, we'll use this
// for the initial bytes to add into Python later
static char _buf[1024];
// A basic function to encode each type as required.
PyObject *EncodeType(ZiProtoFormat_t type, const void *data, size_t size)
{
	char *buf = _buf;
	// size of the type byte plus the data length
	size_t bufsz = sizeof(uint8_t) + size;
	// If we need to malloc or not.
	if (bufsz > sizeof(_buf))
		buf	  = malloc(bufsz);

	// Null the buffer (only the amount required)
	memset(buf, 0, bufsz);
	// Now copy the type byte
	memcpy(buf, &type, sizeof(uint8_t));
	// Now copy the data itself
	if (data)
		memcpy((buf + sizeof(uint8_t)), data, size);

	// We return a bytes object with the encoded data.
	PyObject *ret = PyBytes_FromStringAndSize(buf, bufsz);

	// Free our ram for this buffer
	if (bufsz > sizeof(_buf))
		free(buf);

	// :wave:
	return ret;
}

PyObject *EncodeTypeBigEndian(ZiProtoFormat_t type, void *data, size_t size)
{
	bigendian(data, data, size);
	return EncodeType(type, data, size);
}

PyObject *EncodeTypeArbitraryLength(ZiProtoFormat_t type, const void *data, size_t size)
{
	// This encodes an arbitrary sized type (aka BIN and STR formats)
	if (size == 0)
		return EncodeType(type, 0, 0);
	else if (size < 32 && type == FIXSTR)
		return EncodeType(FIXSTR + size, data, size);
	else if (size <= 0xFF)
	{
		// Copy the size of the data plus size of char * 2,
		// one for data len byte, one for type byte
		size_t bufsz = size + (sizeof(uint8_t) * 2);
		if (bufsz < sizeof(_buf))
		{
			// We can still be pretty cheap with allocations for small
			// objects and just stack allocate.
			memset(_buf, 0, bufsz);
			// Copy type byte
			memcpy(_buf, &type, sizeof(uint8_t));
			// Copy the length size as a byte
			memcpy(_buf + sizeof(uint8_t), &size, sizeof(uint8_t));
			// Copy the data
			memcpy(_buf + (sizeof(uint8_t) * 2), data, size);
			return PyBytes_FromStringAndSize(_buf, bufsz);
		}
		else
		{
			// We have to malloc, sorry :(
			char *buf = malloc(bufsz);
			memset(buf, 0, bufsz);
			// Copy type byte
			memcpy(buf, &type, sizeof(uint8_t));
			// Copy the length size as a byte
			memcpy(buf + sizeof(uint8_t), &size, sizeof(uint8_t));
			// Copy the data
			memcpy(buf + (sizeof(uint8_t) * 2), data, size);
			PyObject *obj = PyBytes_FromStringAndSize(buf, bufsz);
			free(buf);
			return obj;
		}
	}
	else if (size <= 0xFFFFFFFF)
	{
		// Just malloc, it's too much, we might as well encode the byte
		// array ourselves as well, this means we only make one malloc
		// but at the cost of duplicating code.
		size_t bufsz = size + sizeof(char) + ((size <= 0xFFFF) ? sizeof(uint16_t) : sizeof(uint32_t));
		char *buf = malloc(bufsz);
		memset(buf, 0, bufsz);
		// Copy type
		memcpy(buf, &type, sizeof(uint8_t));
		// Copy the length of the data as a 16 or 32 bit integer
		memcpy(buf + sizeof(uint8_t), &size, (size <= 0xFFFF) ? sizeof(uint16_t) : sizeof(uint32_t));
		// Copy the data itself.
		memcpy(buf + (bufsz - size), data, size);
		PyObject *obj = PyBytes_FromStringAndSize(buf, bufsz);
		free(buf);
		return obj;
	}
	else // TODO: Handle this better?
		return EncodeFailure(Py_None);
}

PyObject *ziproto_encode(PyObject *self, PyObject *obj)
{
	// Encode "None" from Python
	// https://stackoverflow.com/a/29732914
	if (obj == Py_None)
		return EncodeType(NIL, 0, 0);
	else if (PyBool_Check(obj))
	{
		if (PyObject_IsTrue(obj))
			return EncodeType(TRUE, 0, 0);
		else
			return EncodeType(FALSE, 0, 0);
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

			// If we've overflowed, encode the unsigned value.
			if (overflow == 1)
				return EncodeTypeBigEndian(UINT64, &uvalue, sizeof(uint64_t));

			// I wish there was a bitwise way to do this but
			// I'm too dumb and google returns useless answers
			// like telling me to sizeof() the integer or use an
			// inefficient loop. At least we can avoid declaring
			// more stack space by "casting" the integer with
			// smaller byte sizes provided.
			if (uvalue <= 0x7F) // This is weird, the type is the int
				return EncodeTypeBigEndian(uvalue, 0, 0);
			else if (uvalue <= 0xFF)
				return EncodeTypeBigEndian(UINT8, &uvalue, sizeof(uint8_t));
			else if (uvalue <= 0xFFFF)
				return EncodeTypeBigEndian(UINT16, &uvalue, sizeof(uint16_t));
			else if (uvalue <= 0xFFFFFFFF)
				return EncodeTypeBigEndian(UINT32, &uvalue, sizeof(uint32_t));
			else
				return EncodeFailure(obj);
		}
		else
		{
			if (svalue >= -32)
				return EncodeTypeBigEndian(svalue, 0, 0);
			else if (svalue >= -128)
				return EncodeTypeBigEndian(INT8, &svalue, sizeof(int8_t));
			else if (svalue >= -32768)
				return EncodeTypeBigEndian(INT16, &svalue, sizeof(int16_t));
			else if (svalue >= -2147483648L)
				return EncodeTypeBigEndian(INT32, &svalue, sizeof(int32_t));
			else if (svalue >= -9223372036854775808) // NOTE: This is technically invalid.
				return EncodeTypeBigEndian(INT64, &svalue, sizeof(int64_t));
			else
				return EncodeFailure(obj);
		}
	}
	else if (PyFloat_Check(obj))
	{
		double value = PyFloat_AsDouble(obj);
		// If there was an error getting the value for some reason.
		if (value == -1.0f && PyErr_Occurred())
			return EncodeFailure(obj);

		// Do some floating point bit hacking to check the
		// type will fit in a FLOAT32 or has to be FLOAT64
		// This is probably a really bad/cheap hack.
		unsigned long long ieee = *(unsigned long long *)&value;
		if (ieee >= UINT_MAX)
			return EncodeTypeBigEndian(FLOAT64, &value, sizeof(value));
		else
		{
			// Convert the value back to a 4 byte float
			float fvalue = *(float *)&ieee;
			return EncodeTypeBigEndian(FLOAT32, &fvalue, sizeof(fvalue));
		}
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

		ZiProtoFormat_t t;
		if (length <= 0xFF)            t = BIN8;
		else if (length <= 0xFFFF)     t = BIN16;
		else if (length <= 0xFFFFFFFF) t = BIN32;
		else
			return EncodeFailure(obj);

		PyObject *ret = EncodeTypeArbitraryLength(t, buffer, length);
		Py_DECREF(obj);
		return ret;
	}
	else if (PyUnicode_Check(obj))
	{
		Py_ssize_t length = 0;
		const char *text = PyUnicode_AsUTF8AndSize(obj, &length);
		if (text)
		{
			ZiProtoFormat_t t;
			if (length < 32)               t = FIXSTR;
			else if (length <= 0xFF)       t = STR8;
			else if (length <= 0xFFFF)     t = STR16;
			else if (length <= 0xFFFFFFFF) t = STR32;
			else
				return EncodeFailure(obj);

			return EncodeTypeArbitraryLength(t, text, length);
		}
		else
			return EncodeFailure(obj);
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
		while (PyDict_Next(obj, &pos, &key, &value))
		{
			PyObject *key_bytes = ziproto_encode(self, key);
			Py_INCREF(key_bytes);
			PyObject *value_bytes = ziproto_encode(self, value);
			Py_INCREF(value_bytes);

			if (!PyBytes_Check(key_bytes))
				return key_bytes;
			if (!PyBytes_Check(value_bytes))
				return value_bytes;

			
		}
	}

		// If ZiProto is upgraded in the future to support additional types
		// return not implemented which I guess is safe enough.
		Py_RETURN_NOTIMPLEMENTED;
}
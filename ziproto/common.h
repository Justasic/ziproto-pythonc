#pragma once
#define PY_SSIZE_T_CLEAN
#include <Python.h>

// Value types as defined in:
// https://github.com/Netkas/ZiProto-Python/blob/master/ziproto/ValueType.py
typedef enum
{
	NIL_TYPE,
	BOOL_TYPE,
	INT_TYPE,
	FLOAT_TYPE,
	BIN_TYPE,
	STR_TYPE,
	ARRAY_TYPE,
	MAP_TYPE,
	// in the Python version of ZiProto this
	// does not exist, I had to create it becase
	// there is no way to differentiate a signed
	// or unsigned type in C based on value.
	UINT_TYPE
} ValueType_t;

typedef enum
{
	POSITIVE_FIXINT = 0x00,
	FIXMAP			= 0x80,
	FIXARRAY		= 0x90,
	FIXSTR			= 0xA0,
	NIL				= 0xC0,
	FALSE			= 0xC2,
	TRUE			= 0xC3,
	BIN8			= 0xC4,
	BIN16			= 0xC5,
	BIN32			= 0xC6,
	FLOAT32			= 0xCA,
	FLOAT64			= 0xCB,
	UINT8			= 0xCC,
	UINT16			= 0xCD,
	UINT32			= 0xCE,
	UINT64			= 0xCF,
	INT8			= 0xD0,
	INT16			= 0xD1,
	INT32			= 0xD2,
	INT64			= 0xD3,
	STR8			= 0xD9,
	STR16			= 0xDA,
	STR32			= 0xDB,
	ARRAY16			= 0xDC,
	ARRAY32			= 0xDD,
	MAP16			= 0xDE,
	MAP32			= 0xDF,
	NEGATIVE_FIXINT = 0xE0
} ZiProtoFormat_t;

inline void *memrev(void *dest, const void *src, size_t n)
{
	// Iterators, s is beginning, e is end.
	unsigned char *s = (unsigned char *)dest, *e = ((unsigned char *)dest) + n - 1;

	// Copy to out buffer for our work
	memcpy(dest, src, n);

	// Iterate and reverse copy the bytes
	for (; s < e; ++s, --e)
	{
		unsigned char t = *s;
		*s				= *e;
		*e				= t;
	}

	// Return provided buffer
	return dest;
}

/**
 * @struct ZiHandle_t
 * @brief Handle to ZiProto state and encoded data
 */
typedef struct 
{
	/*@{*/
	size_t szEncodedData;   /**< Size of the raw ZiProto data buffer */
	size_t _allocsz;        /**< Allocated size of the EncodedData object */
	size_t _cursor;         /**< Current position in the EncodedData buffer */
	uint8_t EncodedData[1]; /**< Raw ZiProto encoded data */
	/*@}*/
} ZiHandle_t;

extern ZiHandle_t [[nodiscard]] *EncodeTypeSingle(ZiHandle_t *handle, ValueType_t vType, const void *TypeBuffer, size_t szTypeBuffer);

// Macros to make things seem function-like
#define FreeZiHandle(x) free(x)
#define GetZiSize(x) (x->szEncodedData)
#define GetZiData(x) (x->EncodedData)

// Define the bigendian function to either be
// memcpy or memrev depending on the arctiecture
// we're compiling on. This is used elsewhere
// since ziproto is big endian by default.
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
# define bigendian memrev
#else
# define bigendian memcpy
#endif

extern PyObject *ziproto_decode(PyObject *self, PyObject *args);
extern PyObject *ziproto_encode(PyObject *self, PyObject *args);
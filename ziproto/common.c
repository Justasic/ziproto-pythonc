#include "common.h"
#include <stdbool.h>
#include <tgmath.h> // For fabs()

/**
 * @brief Encodes POD types to ZiProto bytes.
 *
 * This function encodes different Plain-ol-Data (POD) types into the ZiProto format.
 * These types are ints, floats, bools, strings, and "Null" but not the array and map
 * types which will be encoded in a separate function that calls this function
 *
 * @param[in] handle       The ZiHandle object with current encoding state
 * @param[in] Type         The type of the object about to be encoded
 * @param[in] TypeBuffer   The raw platform-dependent data being encoded
 * @param[in] szTypeBuffer Size of the data in TypeBuffer.
 * @returns ZiHandle_t object with the updated state (may be reallocated) or null on failure.
 */
ZiHandle_t NODISCARD *EncodeTypeSingle(ZiHandle_t *handle, ValueType_t vType, const void *TypeBuffer, size_t szTypeBuffer)
{
	// Small buffer for simple types. This will be used
	// for avoiding a malloc call for types most small
	// types, we will have to call malloc for the BIN/STR types
	uint8_t _localbuf[8];
	// Some types (specifically BIN and STR) have extra data they must write
	// to the buffer for the lenths.
	void * ExtraData   = 0;
	size_t szExtraData = 0;
	// The ZiProto data type to be encoded as
	ZiProtoFormat_t ZiType = 0;

	switch (vType)
	{ 
		case NIL_TYPE:
			ZiType = NIL;
			TypeBuffer = szTypeBuffer = 0;
			break;
		case BOOL_TYPE:
		{
			// read our type
			if ((*(bool*)TypeBuffer))
				ZiType = TRUE;
			else
				ZiType = FALSE;
			TypeBuffer = szTypeBuffer = 0;
			break;
		}
		case UINT_TYPE:
		{
			uint64_t value = *(uint64_t *)TypeBuffer;
			if (value <= 0x7F)
			{
				ZiType = (ZiProtoFormat_t)((uint8_t)value);
				TypeBuffer = szTypeBuffer = 0;
				break;
			}
			else if (value <= 0xFF)
				ZiType = UINT8, szExtraData = sizeof(uint8_t);
			else if (value <= 0xFFFF)
				ZiType = UINT16, szExtraData = sizeof(uint16_t);
			else if (value <= 0xFFFFFFFF)
				ZiType = UINT32, szExtraData = sizeof(uint32_t);
			else
				ZiType = UINT64, szExtraData = sizeof(uint64_t);

			// We're encoding data in big endian
			ExtraData = TypeBuffer;
			TypeBuffer = szTypeBuffer = 0;
			break;
		}
		case INT_TYPE:
		{
			int64_t value = *(int64_t *)TypeBuffer;

			if (value >= -32)
			{
				ZiType = (ZiProtoFormat_t)((uint8_t)(0x100 + value));
				TypeBuffer = szTypeBuffer = 0;
				break;
			}
			else if (value >= -128)
				ZiType = INT8, szExtraData = sizeof(int8_t);
			else if (value >= -32768)
				ZiType = INT16, szExtraData = sizeof(int16_t);
			else if (value >= -2147483648)
				ZiType = INT32, szExtraData = sizeof(int32_t);
			else //if (value >= -9223372036854775808)
				ZiType = INT64, szExtraData = sizeof(int64_t);

			ExtraData = TypeBuffer;
			TypeBuffer = szTypeBuffer = 0;
			break;
		}
		case FLOAT_TYPE:
		{
			// Fun: https://evanw.github.io/float-toy/
			if (szTypeBuffer > sizeof(float))
			{
				double value = *(double *)TypeBuffer;
				// Maybe some day this comparison can be done with
				// integer values or something to make it faster but oh well.
				// ref: https://stackoverflow.com/a/16857716
				if (fabs(value) <= 340282346638528859811704183484516925440.000000)
				{
					// This can be a float32.
					float val = (float)value;
					ZiType = FLOAT32;
					memcpy(_localbuf, &val, sizeof(float));
					ExtraData = _localbuf;
					szExtraData = sizeof(float);
				}
				else
				{
					ZiType		= FLOAT64;
					ExtraData   = TypeBuffer;
					szExtraData = szTypeBuffer;
				}
			}
			else
			{
				ZiType		= FLOAT64;
				ExtraData   = TypeBuffer;
				szExtraData = szTypeBuffer;
			}
			TypeBuffer = szTypeBuffer = 0;
			break;
		}
		case BIN_TYPE:
		{
			if (!TypeBuffer || !szTypeBuffer)
			{
				ZiType = BIN8;
				_localbuf[0] = 0;
				ExtraData = _localbuf;
				szExtraData = sizeof(uint8_t);
				break;
			}

			// Handle size first
			if (szTypeBuffer <= 0x7F)
				ZiType = BIN8, szExtraData = sizeof(uint8_t);
			else if (szTypeBuffer <= 0xFF)
				ZiType = BIN8, szExtraData = sizeof(uint8_t);
			else if (szTypeBuffer <= 0xFFFF)
				ZiType = BIN16, szExtraData = sizeof(uint16_t);
			else if (szTypeBuffer <= 0xFFFFFFFF)
				ZiType = BIN32, szExtraData = sizeof(uint32_t);
			else
				return NULL;
			ExtraData = _localbuf;
			memcpy(ExtraData, &szTypeBuffer, szExtraData);
			break;
		}
		case STR_TYPE:
		{
			if (!TypeBuffer || !szTypeBuffer)
			{
				ZiType = FIXSTR;
				break;
			}

			// Handle size first
			if (szTypeBuffer < 32)
				ZiType = FIXSTR + szTypeBuffer;
			else if (szTypeBuffer <= 0xFF)
				ZiType = STR8, szExtraData = sizeof(uint8_t);
			else if (szTypeBuffer <= 0xFFFF)
				ZiType = STR16, szExtraData = sizeof(uint16_t);
			else if (szTypeBuffer <= 0xFFFFFFFF)
				ZiType = STR32, szExtraData = sizeof(uint32_t);
			else
				return NULL;
			ExtraData = _localbuf;
			memcpy(ExtraData, &szTypeBuffer, szExtraData);
			break;
		}
		// In both the array and map types, we pack the type needed
		// defined by the caller by ORing them together.
		case ARRAY_TYPE:
		{
			uint64_t length = *(uint64_t *)TypeBuffer;
			if (length <= 0xF)
				ZiType = FIXARRAY + (uint8_t)length;
			else if (length <= 0xFFFF)
				ZiType = ARRAY16, szExtraData = sizeof(uint16_t);
			else if (length <= 0xFFFFFFFF)
				ZiType = ARRAY32, szExtraData = sizeof(uint32_t);
			else
				return NULL;

			if (ZiType != FIXARRAY)
			{
				ExtraData = _localbuf;
				memcpy(ExtraData, TypeBuffer, szTypeBuffer);
			}
			TypeBuffer = szTypeBuffer = 0;
			break;
		}
		case MAP_TYPE:
		{
			uint64_t length = *(uint64_t *)TypeBuffer;
			if (length <= 0xF)
				ZiType = FIXMAP + (uint8_t)length;
			else if (length <= 0xFFFF)
				ZiType = MAP16, szExtraData = sizeof(uint16_t);
			else if (length <= 0xFFFFFFFF)
				ZiType = MAP32, szExtraData = sizeof(uint32_t);
			else
				return NULL;

			if (ZiType != FIXMAP)
			{
				ExtraData = _localbuf;
				memcpy(ExtraData, TypeBuffer, szTypeBuffer);
			}
			TypeBuffer = szTypeBuffer = 0;
			break;
		}
		default:
			return NULL;
	}

	// If the handle doesn't exist, create one.
	// The size of the handle includes the length of the data as well
	// In this instance we allocate the size of the ZiHandle_t struct
	// plus the size of the byte we're about to encode as well as the
	// size of the ZiProtoFormat_t type byte.
	size_t szNextSize = szExtraData + szTypeBuffer + sizeof(uint8_t);
	bool wasallocated = false;
	if (unlikely(!handle))
	{
		handle = malloc(sizeof(ZiHandle_t));
		memset(handle, 0, sizeof(ZiHandle_t));
		handle->EncodedData = malloc(szNextSize);
		memset(handle->EncodedData, 0, szNextSize);
		handle->_allocsz = szNextSize;
		wasallocated = true;
	}
	
	// We'll need to realloc if this is true, it's likely this will happen.
	if (likely(handle->_allocsz < (handle->szEncodedData + szNextSize)))
	{
		size_t addl_bytes = sizeof(uint8_t) + szTypeBuffer + szExtraData;
		size_t newsz      = handle->_allocsz + addl_bytes;
		newsz += newsz + (newsz & 7);
		// Add 8 byte alignment to help reduce the number of realloc calls.
		void *newhandle = realloc(handle->EncodedData, newsz);
		// our allocation failed, return null I guess.
		// Might want to handle this situation better.
		if (unlikely(!newhandle))
		{
			// Don't memleak on failure
			if (wasallocated)
				free(handle);
			return NULL;
		}
			

		// Null out the new space
		memset(((uint8_t*)newhandle) + handle->_allocsz, 0, newsz - handle->_allocsz);

		// Update our handle object.
		handle->EncodedData = newhandle;
		handle->_allocsz    = newsz;
	}

	// Write our type byte (will always be first)
	memcpy(handle->EncodedData + handle->_cursor, &ZiType, sizeof(uint8_t));
	handle->_cursor += sizeof(uint8_t);
	handle->szEncodedData += sizeof(uint8_t);

	// Write our extra data if needed
	if (ExtraData)
	{
		// since our "extra data" will always be big endian
		bigendian(handle->EncodedData + handle->_cursor, ExtraData, szExtraData);
		handle->_cursor += szExtraData;
		handle->szEncodedData += szExtraData;
	}

	// Now write our data itself (if applicable)
	if (szTypeBuffer && TypeBuffer)
	{
		memcpy(handle->EncodedData + handle->_cursor, TypeBuffer, szTypeBuffer);
		handle->_cursor += szTypeBuffer;
		handle->szEncodedData += szTypeBuffer;
	}


	// We're done!
	return handle;
}

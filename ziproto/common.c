#include "common.h"
#include <float.h> // for FLT_MAX

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
ZiHandle_t [[nodiscard]] *EncodeTypeSingle(ZiHandle_t *handle, ValueType_t vType, const void *TypeBuffer, size_t szTypeBuffer)
{
	// Small buffer for different things
	uint8_t _localbuf[256];
	// Some types (specifically BIN and STR) have extra data they must write
	// to the buffer for the lenths.
	void * ExtraData   = 0;
	size_t szExtraData = 0;
	// The ZiProto data type to be encoded as
	ZiProtoFormat_t ZiType = 0;

	switch ((uint8_t)vType)
	{
		case NIL_TYPE:
			ZiType = NIL;
			break;
		case BOOL_TYPE:
		{
			// read our type
			if ((*(bool*)TypeBuffer))
				ZiType = TRUE;
			else
				ZiType = FALSE;
		}
		case UINT_TYPE:
		{
			uint64_t value = *(uint64_t *)TypeBuffer;
			if (value <= 0x7F)
			{
				ZiType = (ZiProtoFormat_t)((uint8_t)value);
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
			TypeBuffer = 0;
			szTypeBuffer = 0;
		}
		case INT_TYPE:
		{
			int64_t value = *(int64_t *)TypeBuffer;

			if (value >= -32)
			{
				ZiType = (ZiProtoFormat_t)((uint8_t)(0x100 + value));
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

		}
		case FLOAT_TYPE:
		{
			// Fun: https://evanw.github.io/float-toy/
			if (szTypeBuffer > sizeof(float))
			{
				double value = *(double *)TypeBuffer;
				// Maybe some day this comparison can be done with
				// integer values or something to make it faster but oh well.
				if (value < FLT_MAX) [[unlikely]]
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
					ZiType = FLOAT64;
					ExtraData = TypeBuffer;
					szExtraData = szTypeBuffer;
				}
			}
			else
			{
				ZiType = FLOAT32;
				ExtraData = TypeBuffer;
				szExtraData = szTypeBuffer;
			}
			TypeBuffer = szTypeBuffer = 0;
		}
		case BIN_TYPE:
		{
			if (!TypeBuffer || !szTypeBuffer)
			{
				ZiType = BIN8;
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
			memcpy(ExtraData, &szTypeBuffer, szExtraData);
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
			memcpy(ExtraData, &szTypeBuffer, szExtraData);
		}
		// In both the array and map types, we pack the type needed
		// defined by the caller by ORing them together.
		case ARRAY_TYPE:
		{
			// Get the size they specify.
			ZiType = (vType >> 8);
			szExtraData = szTypeBuffer;
			memcpy(ExtraData, &szTypeBuffer, szTypeBuffer);
			TypeBuffer = szTypeBuffer = 0;
		}
		case MAP_TYPE:
		{
			// Get the size they specify.
			ZiType		= (vType >> 8);
			szExtraData = szTypeBuffer;
			memcpy(ExtraData, &szTypeBuffer, szTypeBuffer);
			TypeBuffer = szTypeBuffer = 0;
		}
		default:
			return NULL;
	}

	// If the handle doesn't exist, create one.
	// The size of the handle includes the length of the data as well
	// In this instance we allocate the size of the ZiHandle_t struct
	// plus the size of the byte we're about to encode as well as the
	// size of the ZiProtoFormat_t type byte.
	if (!handle) [[unlikely]]
	{
		handle = malloc(sizeof(ZiHandle_t) + szExtraData + szTypeBuffer + sizeof(uint8_t));
		memset(handle, 0, sizeof(ZiHandle_t) + szExtraData + szTypeBuffer + sizeof(uint8_t));
		handle->_allocsz = szExtraData + szTypeBuffer + sizeof(uint8_t);
	}

	// We'll need to realloc if this is true, it's likely this will happen.
	if (handle->_allocsz < szExtraData + szTypeBuffer + sizeof(uint8_t)) [[likely]]
	{
		size_t		newsz	  = handle->_allocsz + sizeof(uint8_t) + szTypeBuffer + szExtraData;
		ZiHandle_t *newhandle = realloc(handle, newsz);
		// our allocation failed, return null I guess.
		// Might want to handle this situation better.
		if (!newhandle) [[unlikely]]
			return NULL;

		// Null out the new space
		memset(newhandle + newhandle->_allocsz, 0, sizeof(uint8_t) + szTypeBuffer + szExtraData);

		// Update our handle object.
		handle			 = newhandle;
		handle->_allocsz = newsz;
	}

	// Write our type byte (will always be first)
	memcpy(handle->EncodedData + handle->_cursor, &Type, sizeof(uint8_t));
	handle->_cursor += sizeof(uint8_t);

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
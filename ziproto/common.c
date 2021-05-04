#include "common.h"

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
				// This is weird, the type is the int
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
			double value = *(double*)
			if (szTypeBuffer > sizeof(float))
			{

			}

			#if 0
			// Rationale:
			// Some systems may not have Floating point support but wish
			// to use ZiProto and if we can still avoid using 4 more bytes
			// then lets do it. This attempts to "safely" convert a double
			// precision number to a single precision number if the number
			// will fit in single precision. Since software-emulated FPUs
			// can be quite costly to do conversions, we do these as
			// integer operations in C code (which all computers are fast
			// at), it's ugly but it's fast and that's all I care about.
			// Fun: https://evanw.github.io/float-toy/
			uint64_t value = *(uint64_t *)TypeBuffer;

			// 0x7F7FFFFF

			// ignore the sign bit in our comparisons
			value &= 0x80000000;

			uint32_t exponent = value >> 23;
			uint32_t fraction = value & 0x7FFFFF;

			if ()
			#endif

			// Encode a 32 bit or 64 bit floating point value.
		}
		case BIN_TYPE:
		{

		}
		case STR_TYPE:
		{

		}
		// In both the array and map types, we simply
		// make a decision based on the szTypeBuffer size
		// Subsequent calls to this function should yield
		// the correct formatting. This just encodes the
		// type byte.
		case ARRAY_TYPE:
		{
			if (szTypeBuffer == FIXARRAY)
				ZiType = FIXARRAY;
			if (szTypeBuffer == ARRAY16)
				ZiType = ARRAY16;
			if (szTypeBuffer == ARRAY32)
				ZiType = ARRAY32;
			TypeBuffer = 0;
			szTypeBuffer = 0;
		}
		case MAP_TYPE:
		{
			if (szTypeBuffer == FIXMAP)
				ZiType = FIXMAP;
			if (szTypeBuffer == MAP16)
				ZiType = MAP16;
			if (szTypeBuffer == MAP32)
				ZiType = MAP32;
			TypeBuffer = 0;
			szTypeBuffer = 0;
		}
		default:
			break;
	}

	// Write the actual byte data
	switch (Type)
	{
		case POSITIVE_FIXINT:
		{
			break;
		}
		case FIXMAP:
		{
			break;
		}
		case FIXARRAY:
		{
			break;
		}
		case FIXSTR:
		{
			break;
		}
		case NIL:  [[fallthrough]]
		case 0xC2: [[fallthrough]] // ZiProto FALSE
		case 0xC3: [[fallthrough]] // ZiProto TRUE
			break;
		case BIN8:
		{
			break;
		}
		case BIN16:
		{
			break;
		}
		case BIN32:
		{
			break;
		}
		case FLOAT32: [[fallthrough]]
		case FLOAT64:
			break;
		case UINT8:  [[fallthrough]]
		case UINT16: [[fallthrough]]
		case UINT32: [[fallthrough]]
		case UINT64: [[fallthrough]]
		case INT8:   [[fallthrough]]
		case INT16:  [[fallthrough]]
		case INT32:  [[fallthrough]]
		case INT64:
		{
			// We want reverse endianness.
			ExtraData = TypeBuffer;
			szExtraData = szTypeBuffer;
			TypeBuffer = 0;
			szTypeBuffer = 0;
			break;
		}
		case STR8:
		{
			break;
		}
		case STR16:
		{
			break;
		}
		case STR32:
		{
			break;
		}
		case ARRAY16:
		{
			break;
		}
		case ARRAY32:
		{
			break;
		}
		case MAP16:
		{
			break;
		}
		case MAP32:
		{
			break;
		}
		case NEGATIVE_FIXINT:
		{
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
	if (szTypeBuffer)
	{
		memcpy(handle->EncodedData + handle->_cursor, TypeBuffer, szTypeBuffer);
		handle->_cursor += szTypeBuffer;
		handle->szEncodedData += szTypeBuffer;
	}

	// We're done!
	return handle;
}

ZiHandle_t [[nodiscard]] *EncodeArray(ZiHandle_t *handle, ZiProtoFormat_t Type, const void *Array, size_t szArray)
{
	// TODO: Handle array types
}

ZiHandle_t [[nodiscard]] *EncodeMap(ZiHandle_t *handle, ZiProtoFormat_t Type)
{
	// TODO: Handle array types
}
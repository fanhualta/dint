#pragma once
#include <iosfwd>
#include <xmmintrin.h>

//#include "basic_types.h"
//#include "constants.h"

// A collection of classes to encode and decode data in tagged group coding
// Tagged Group Coding - TGC in short - is a byte-oriented compression scheme.
//  - Like variable byte coding, TGC compresses 64-bit integers
//    by removing leading zero bytes of a 64-bit integer.
//  - Unlike variable byte coding, TGC encodes a group of integers
//    into a tag and following series of bytes.
//  - A TGC group can have zero to 10 values.
//
// A Two-Byte Tag
//  - Every TGC group has a leading two-byte tag.
//    The tag can be extended by one or two more bytes
//    in exceptional groups.
//
// Escape Code and Group Types
//  - The first byte of a 2-byte tag has the escape bit and the type of a group.
//    Using the first byte for the escape and exception types
//    makes the compiler generate code faster than using high-order bits.
//  - Base-zero bit positions are used here like the following.
//    MSB - 7, 6, 5, 4, 3, 2, 1, 0 - LSB
//  - Bit 6 is the escape bit for exceptional groups.
//    Bit 7 (MSB) is for 64-bit-value groups when the escape bit is set.
//    Two bits following the escape bit (bit 5 and 4) are
//    for 32-bit-value groups and VBC-coded groups.
//
// A regular group has Ten 24-bit integer values.
//  - a two-byte tag.
//  - The sizes of a pair of values are encoded in three bits.
//  - There is a mapping table to map a 3-bit size tag
//    to maks, shifts, and sizes. See DecoderMappingTable.
//  - Bit 6 of a tag is zero.
//    Bit 6 of a tag is used to minimize shift operations while decoding.
//    The other 15 bits are used to encode the size tags of 5 pairs or 10 values.
//
// A 32-bit exceptional group has Ten 32-bit integer values.
//  - a three-byte tag
//  - The size of each value is encoded in two bits.
//  - 00: 1 byte, 01: 2 bytes, 10: 3 bytes, 11: 4 bytes
//  - Bit 6 of a tag is one, and 3 bits (bit 7, 5, and 4) are zeros.
//    The other 20 bits are sizes for 10 32-bit values.
//
// A 64-bit exceptional group has Ten 64-bit values,
//  - a four-byte tag
//  - The size of each value is encoded in three bits.
//    * 000: 1 byte, 001: 2 bytes, 010: 3 bytes, 011: 4 bytes
//    * 100: 5 byte, 101: 6 bytes, 110: 7 bytes, 111: 8 bytes
//  - Bit 7 and Bit 6 of a tag are ones.
//    The other 30 bits are sizes for 10 64-bit values.
//
// A VBC exceptional group has ten or less number of 64-bit values.
//  - a two-byte tag.
//    It is followed by a data block of values compressed in variable byte coding.
//  - Bit 6 and bit 5 of a tag is one, and bit 7 is zero. 3 bits (bit 7, 6, and 5) are 011;
//    Then, bit 4 is used for VBC exception types.
//  - There are VbcType1 and VbcType2 types.
//    Users assign them for their purpose.
//    FYI: TgcIndex assigned VbcType1 to the end-of-page
//         and VbcType2 to the end-of-term.
//  - The low-order 4 bits are the number values encoded.
//  - The high-order 8 bits are the size of Vbc coded data
//
// TGC
//  - Encoded a group of values.
//  - Regular group: a two-byte tag. Bit 6 is zero. Ten 24-bit Values.
//  - Exceptoinal group: Bit 6 of a two-byte tag is one.
//    * 32-bit exceptional group: a three-byte tag and Ten 32-bit Values.
//    * 64-bit exceptional group: a four-byte tag and Ten 64-bit Values.
//    * vbc exceptional group: a two-byte tag and VBC coded Values.
//

// TigerCoding (have doc index instead of position posting) is almost the same as TgcCoding
// if a term is not a tiger term, the encoding is exactly the same as tgc coding.
// if a term is tiger term:
//  1. doc index is stored rather than location.  aka: term location is removed.  doc index is stored instead.
//  2. a term frequency value (TF) is associated with each doc index.
//  3. meta data for each location is removed.


//  TF encoding details:
//  - 10 TFs are stored as a group.
//  - each TF takes 2 bits:
//      - if TF = 1 or 2, it is stored as 01 or 10
//      - if TF = 0 or 3 <= TF < 256, it is stored as 00 with 1 byte real value in exception field
//      - if 256 <= TF <= UInt32(-1),  it is stored as 11 with 4 bytes real value in exception field
//  - the rest 4 bits are not used so far

typedef uint8_t UInt8;
typedef uint16_t UInt16;
typedef uint32_t UInt32;
typedef uint64_t UInt64;
typedef uint32_t DWORD;
typedef unsigned int DWORD32;
typedef unsigned long long DWORD64;

namespace TgcCoding
{


// class to encode and decode a tag
    struct Tag
    {
        // size of a regular group in the number of values
        static const UInt8 c_groupSize = 10; // Ten-Value Group

        // The bit 6 of the first byte of a tag is the escape code to excaptional groups
        static const UInt16 c_escapeCode = 0x0040;

        // A 4-bit mask to get an exception type.
        // - Bit 6 is one, and 3 bits (bit 7, 5, and 4) are used.
        //   The escape bit is included.
        static const UInt16 c_exceptionTypeFullMask = 0x00F0;

        // A 3-bit mask to get an exception type.
        // - Bit 6 is one, and 3 bits (bit 7, 5, and 4) are used.
        // - Bit 5 is zero, bit 7 is zero
        // - Bit 4 is zero for TGC 32Bit, one for Tiger 32Bit
        //   The escape bit is not included.
        static const UInt16 c_exceptionType32BitMask = 0x00A0;

        // 32-bit exception: encoded values are 32-bit integers
        // - Bit 6 is one, and 3 bits (bit 7, 5, and 4) are zeros.
        // - A two byte tag is followed by another byte
        //   to complete a 20-bit size tag for 10 32-bit values.
        // 32-bit Exception Tag= 0100xxxx
        static const UInt16 c_exceptionType32Bit = 0x0000;

        // a single-bit mask to get an exception type for 64-bit values.
        // - Bit 7 and Bit 6 are ones.
        //   The escape bit is not included.
        // - A two byte tag is followed by two bytes
        //   to complete a 30-bit size tag for 10 64-bit values.
        // 64-bit Exception Tag= 11xxxxxx
        static const UInt16 c_exceptionType64Bit = 0x0080;

        // a 2-bit mask for a vbc-coded-value group
        // - The 3-bit pattern of bit 7, 6, and 5 is 011x.
        //   Bit 4 is a type of VBC exceptional groups.
        // - The low-order 4 bits are the number of values compressed in VBC.
        // - The highest byte of a tag (8-bit) is the size of VBC-coded data.
        // VBC Exception Tag= 011xxxxx
        static const UInt16 c_exceptionTypeVbc = 0x0060;
        static const UInt16 c_exceptionTypeVbc1 = 0x0060; // 0110xxxx
        static const UInt16 c_exceptionTypeVbc2 = 0x0070; // 0111xxxx

        // a 3-bit mask for VBC exception types
        static const UInt16 c_exceptionTypeVbcMask = (c_exceptionTypeVbc | 0x0010);

        // The lowest-order bit of VBC exception types
        static const UInt16 c_exceptionTypeVbcLSB = 0x0010;

        // unused exception type
        static const UInt16 c_exceptionTypeUnused1 = 0x0010;

        // an invalid exception type
        // 0 is a tag for a regular group
        static const UInt16 c_invalidExceptionType = UInt16(0);

        // a 2-bit mask for a 32-bit size
        static const UInt16 c_uint32SizeMask = 3;

        // a 3-bit mask for a 64-bit size
        static const UInt16 c_uint64SizeMask = 7;

        // a 2-byte tag for a VBC data
        static const UInt16 c_vbcTagSize = 2;

        // a shift size to get the number of VBC values
        static const UInt16 c_shiftVbcNumValues = 0x0000;

        // a 4-bit mask to get the number of VBC values
        static const UInt16 c_maskVbcNumValues = 0x000F;

        // a shift size to get the size of VBC data
        static const UInt16 c_shiftVbcDataSize = 0x0008;

        // a mask to get the size of VBC data
        static const UInt16 c_maskVbcDataSize = 0xFFU;// 8 bits

        // The maximum size of VBC data
        static const UInt16 c_maxVbcSize = ((1 << 8) -1); // 8 bits

        // The maximum number of values a VBC group can have
        static const UInt16 c_maxNumVbcValues = c_groupSize;

        // return true if the given tag is a regular tag
        inline static bool IsRegular(UInt16 p_tag)
        {
            return (p_tag & c_escapeCode) == 0;
        }

        // return true if the given tag is an excepional tag
        inline static bool IsException(UInt16 p_tag)
        {
            return (p_tag & c_escapeCode) != 0;
        }

        // return true if the given tag is an 32-bit-value excepional tag
        inline static bool Is32BitValueException(UInt16 p_exceptionTag)
        {
            return IsException(p_exceptionTag) &&
                   ((p_exceptionTag & c_exceptionType32BitMask) == c_exceptionType32Bit);
        }

        // return true if the given tag is an 64-bit-value excepional tag
        inline static bool Is64BitValueException(UInt16 p_exceptionTag)
        {
            return IsException(p_exceptionTag) &&
                   ((p_exceptionTag & c_exceptionType64Bit) != 0);
        }

        // return true if the given tag is an VBC excepional tag
        inline static bool IsVbcException(UInt16 p_exceptionTag)
        {
            UInt16 exceptionTagType = p_exceptionTag & c_exceptionTypeFullMask;
            return ((exceptionTagType == c_exceptionTypeVbc1) ||
                    (exceptionTagType == c_exceptionTypeVbc2));
        }

        // return an exception tag type for a VBC coded group
        inline static UInt16 ExceptionVbcType(UInt16 p_exceptionTag)
        {
            return p_exceptionTag & c_exceptionTypeVbcMask;
        }

        // return the number of VBC coded values
        inline static UInt16 GetVbcNumValues(UInt16 p_exceptionTag)
        {
            return (p_exceptionTag >> c_shiftVbcNumValues) & c_maskVbcNumValues;
        }

        // return the size of VBC data
        inline static UInt16 GetVbcDataSize(UInt16 p_exceptionTag)
        {
            return (p_exceptionTag >> c_shiftVbcDataSize) & c_maskVbcDataSize;
        }

        // read 2 bytes and return a 16-bit integer
        inline static UInt16 Get16BitTag(const UInt8* p_ptr)
        {
            return *reinterpret_cast<const UInt16*>(p_ptr);
        }

        // store a 16-bit integer in 2 bytes
        inline static void Store16BitTag(UInt8* p_ptr, UInt16 p_exceptionTag)
        {
            *reinterpret_cast<UInt16*>(p_ptr) = p_exceptionTag;
        }

        // encode a vbc exception tag in 2 bytes
        inline static UInt8 EncodeVbcExceptionWithSize(
                UInt8* p_ptr, UInt16 p_exceptionType, UInt8 p_dataSize, UInt8 p_numValues)
        {
            UInt16 tag = p_exceptionType & c_exceptionTypeVbcMask;
            tag |= (p_numValues & c_maskVbcNumValues) << c_shiftVbcNumValues;
            tag |= (p_dataSize & c_maskVbcDataSize) << c_shiftVbcDataSize;
            tag |= Tag::c_escapeCode | c_exceptionTypeVbc;
            Store16BitTag(p_ptr, tag);
            return 2;
        }

        // return the total size of the given tag - 2, 3, and 4 bytes.
        static UInt8 ComputeTotalTagSize(const UInt8* p_tag);

        // return the minimal number of bytes to store a value
        static UInt8 ComputeMinimalSizeInBytes(UInt64 p_value);
    };


// encode 64-bit values in TGC
    class UInt64Encoder
    {
    public:
        // a constructor
        UInt64Encoder();

        // encode 10 values
        void EncodeTenValues(const UInt64* p_values, bool p_isPositionLessTFTerm);

        // encode 10 term frequency values
        UInt8 EncodeTenTFs(const UInt32* p_tfs);

        // encode 64 bit values in VBC with an exception type
        // p_numData is less than or equal to Tag::c_groupSize
        // return false if data is too many to encode
        bool EncodeValuesInVbc(
                const UInt64* p_values,
                unsigned p_numData,
                UInt16 p_exceptionType);

        // return the size of data
        UInt8 Size() const;

        // return the size of data plus the overhead to access the last value
        UInt8 SafeSize() const;

        // return the number of encoded values
        UInt8 NumberOfEncodedValues() const;

        // copy data
        UInt8 CopyEncodedData(UInt8* p_dest) const;

        // clear internal states
        void Clear();

        // print internal states
        std::ostream& Print(std::ostream& p_os) const;

    private:
        // computes minimal size of each value in bytes
        // return true if the group is a regular group.
        void ComputeValueSize(const UInt64* p_values, unsigned p_numData);

        // computes the overhead to load the last value in either 32-bit or 64-bit value
        // return the size of a buffer overrun
        UInt8 ComputeSafetyOverhead();

        // Store Ten 64-bit values as few bytes as possible
        // return the number of bytes used.
        UInt8 StoreTenValuesInCompactFormat(UInt8* p_ptr, const UInt64* p_values);

        // Encode Ten 24-bit values
        // return the number of bytes used.
        UInt8 EncodeTen24BitValues(UInt8* p_ptr, const UInt64* p_values);

        // Encode Ten 32-bit values
        // return the number of bytes used.
        UInt8 EncodeTen32BitValues(UInt8* p_ptr, const UInt64* p_values, bool p_isPositionLessTFTerm);

        // Encode Ten 64-bit values
        // return the number of bytes used.
        UInt8 EncodeTen64BitValues(UInt8* p_ptr, const UInt64* p_values);

        // Store a 32-bit value to p_ptr
        void StoreUInt32Value(UInt8* p_ptr, UInt64 p_value);

        // Store a 64-bit value to p_ptr
        void StoreUInt64Value(UInt8* p_ptr, UInt64 p_value);

        // data size
        UInt8 m_size;

        // size including the overhead to load the last value
        // in either a 32-bit or 64-bit value integer.
        // It guarantees that a decoder will not overrun a buffer while loading values.
        UInt8 m_safeSize;

        // number of encoded values
        UInt8 m_numValues;

        // minimal size of values in bytes
        UInt8 m_valueSizes[Tag::c_groupSize];

        // buffer to store compressed values
        // a 3-byte tag, 4 8-byte values, Two Dummy 8-byte integer just in case
        UInt8 m_buffer[3 + (Tag::c_groupSize * 2 + 2) * sizeof(UInt64) + Tag::c_maxVbcSize];
        static const UInt8 m_maskIndexes[8][8];
        static const UInt8 m_maskEncodeType[8][8];
        static const UInt8 m_maskMinimalSizeInBytes[64];
    };


// a mapping table to map a 3-bit tag to maks, shifts, and sizes of a pair of values.
// Some of the methods are critical to the performance, so they are forced to be inlined.
    struct DecoderMappingTable
    {
        // decode a pair of delta values based on the given tag (p_tag)
        // Decoded delta values are added to p_base and stored to p_values
        inline void
        DecodeTwo24BitDeltas(UInt64& p_base, UInt16 p_tag, UInt64* p_values, const UInt8*& p_ptr)
        {
            UInt16 dualSize = p_tag & 7;
            UInt64 delta = *(UInt64*)p_ptr;
            UInt64 pairMasks = s_masksOfPairs[dualSize];

            p_base = (p_values[0] = p_base + (delta & (pairMasks & 0xFFFFFFU))); // 24 bits
            pairMasks >>= 24;
            delta >>= pairMasks & 0xFFU; // 8 bits
            pairMasks >>= 8;
            p_base = (p_values[1] = p_base + (delta & (pairMasks & 0xFFFFFFU))); // 24 bits
            p_ptr += pairMasks >> 24;
        }

        // decode the last pair of delta values based on the given tag (p_tag)
        // Decoded delta values are added to p_base and stored to p_values
        inline void
        DecodeLastTwo24BitDeltas(UInt64& p_base, UInt16 p_tag, UInt64* p_values, const UInt8*& p_ptr)
        {
            UInt16 dualSize = p_tag; // Not necessary "& 7;"
            UInt64 delta = *(UInt64*)p_ptr;
            UInt64 pairMasks = s_masksOfPairs[dualSize];

            p_base = (p_values[0] = p_base + (delta & (pairMasks & 0xFFFFFFU))); // 24 bits
            pairMasks >>= 24;
            delta >>= pairMasks & 0xFFU; // 8 bits
            pairMasks >>= 8;
            p_base = (p_values[1] = p_base + (delta & (pairMasks & 0xFFFFFFU))); // 24 bits
            p_ptr += pairMasks >> 24;
        }

        // the mask of the first value of a pair
        template<typename IDXT>
        inline static UInt64
        GetMaskOfFisrtValue(IDXT p_idx)
        {
            return (s_masksOfPairs[p_idx] & 0xFFFFFFU); // 24 bits
        }


        // the shift count to get the second value of a pair
        template<typename IDXT>
        inline static UInt64
        GetShiftForSecondValue(IDXT p_idx)
        {
            return ((s_masksOfPairs[p_idx] >> 24) & 0xFFU); // 8 bits
        }


        // the mask of the second value of a pair
        template<typename IDXT>
        inline static UInt64
        GetMaskOfSecondValue(IDXT p_idx)
        {
            return ((s_masksOfPairs[p_idx] >> 32) & 0xFFFFFFU); // 24 bits
        }


        // the total size of a pair of values
        template<typename IDXT>
        inline static UInt64
        GetTotalSizeOfPair(IDXT p_idx)
        {
            return (s_masksOfPairs[p_idx] >> 56); // 8 bits
        }


    private:
        // a mapping table of 3-bit indexes for the masks, shifts, and sizes of a pair of encoded values.
        static const UInt64 s_masksOfPairs[8];
    };


// decode 64-bit values in TGC
// Decoding speed of regular TGC groups is the most important thing in this class
    class UInt64DeltaDecoder
    {
    public:
        // a constructor
        UInt64DeltaDecoder()
                : m_ptr(NULL)
        {}

        // reset to decode a group at p_ptr
        inline void Reset(const UInt8* p_ptr)
        {
            m_ptr = p_ptr;
        }

        // the current decoding point
        inline const UInt8* Current() const
        {
            return m_ptr;
        }

        // skip p_size without decoding
        inline void SkipData(unsigned p_size)
        {
            m_ptr += p_size;
        }

        // return a 16-bit exception tag
        inline UInt16 Peek16BitTag()
        {
            return Tag::Get16BitTag(m_ptr);
        }

        // return a 16-bit exception tag, and move the current decoding pointer
        inline UInt16 Get16BitTagAndAdvance()
        {
            UInt16 tag16 = Tag::Get16BitTag(m_ptr);
            m_ptr += 2;
            return tag16;
        }

        // decode Ten 24-bit delta compressed values by p_tag and store them to p_values
        // return the last value
        inline UInt64 DecodeRegularDeltas(UInt64 p_base, UInt16 p_tag, UInt64* p_values)
        {
            return DecodeTen24BitDeltas(p_base, p_tag, p_values);
        }

        // decode Ten 32-bit delta compressed values by p_tag and store them to p_values
        // return the last value
        inline UInt64 Decode32BitDelta(UInt64 p_base, UInt16 p_tag, UInt64* p_values)
        {
            _mm_prefetch((const char*)m_ptr + 64, _MM_HINT_T0);
            _mm_prefetch((const char*)m_ptr + 96, _MM_HINT_T0);
            const UInt8* ptr = m_ptr;
            UInt32 tag24 = p_tag | (UInt32(*ptr) << 16);
            ptr += 1;

            // 2 * 10 = 20 bits = 10 values, a 2-bit tag for each value
            UInt32 tag = tag24 & Tag::c_uint32SizeMask;
            p_base = (p_values[0] = p_base + (*(UInt32*)ptr & s_sizeTo64BitMask[tag]));
            ptr += tag + 1;
            tag24 >>= 2;
            tag = tag24 & Tag::c_uint32SizeMask;
            p_base = (p_values[1] = p_base + (*(UInt32*)ptr & s_sizeTo64BitMask[tag]));
            ptr += tag + 1;
            tag24 >>= 6;

            tag = tag24 & Tag::c_uint32SizeMask;
            p_base = (p_values[2] = p_base + (*(UInt32*)ptr & s_sizeTo64BitMask[tag]));
            ptr += tag + 1;
            tag24 >>= 2;
            tag = tag24 & Tag::c_uint32SizeMask;
            p_base = (p_values[3] = p_base + (*(UInt32*)ptr & s_sizeTo64BitMask[tag]));
            ptr += tag + 1;
            tag24 >>= 2;

            tag = tag24 & Tag::c_uint32SizeMask;
            p_base = (p_values[4] = p_base + (*(UInt32*)ptr & s_sizeTo64BitMask[tag]));
            ptr += tag + 1;
            tag24 >>= 2;
            tag = tag24 & Tag::c_uint32SizeMask;
            p_base = (p_values[5] = p_base + (*(UInt32*)ptr & s_sizeTo64BitMask[tag]));
            ptr += tag + 1;
            tag24 >>= 2;

            tag = tag24 & Tag::c_uint32SizeMask;
            p_base = (p_values[6] = p_base + (*(UInt32*)ptr & s_sizeTo64BitMask[tag]));
            ptr += tag + 1;
            tag24 >>= 2;
            tag = tag24 & Tag::c_uint32SizeMask;
            p_base = (p_values[7] = p_base + (*(UInt32*)ptr & s_sizeTo64BitMask[tag]));
            ptr += tag + 1;
            tag24 >>= 2;

            tag = tag24 & Tag::c_uint32SizeMask;
            p_base = (p_values[8] = p_base + (*(UInt32*)ptr & s_sizeTo64BitMask[tag]));
            ptr += tag + 1;
            tag24 >>= 2;
            tag = tag24 & Tag::c_uint32SizeMask;
            p_base = (p_values[9] = p_base + (*(UInt32*)ptr & s_sizeTo64BitMask[tag]));
            ptr += tag + 1;

            m_ptr = ptr;

            return p_base;
        }


        // decode Ten 64-bit delta compressed values by p_tag16 and store them to p_values
        // return the last value
        inline UInt64 Decode64BitDelta(UInt64 p_base, UInt16 p_tag16, UInt64* p_values)
        {
            return DecodeTenUInt64Deltas(p_base, p_tag16, p_values);
        }

        // decode an VBC-coded TGC group of delta compressed values
        // and store them to p_values
        // CAUTION: return the number of values. It does *NOT* return the last value.
        unsigned DecodeVbcCodedDeltas(UInt64 p_base, UInt16 p_exceptionTag, UInt64* p_values);

    protected:
        // the current decoding pointer
        // This class has no other member for fast initialization and decoding.
        const UInt8* m_ptr;

    private:
        // decode Ten 24-bit delta compressed values by p_tag and store them to p_values
        // return the last value
        UInt64 DecodeTen24BitDeltas(UInt64 p_base, UInt16 p_tag, UInt64* p_values);

        // decode Ten 32-bit delta compressed values by p_tag and store them to p_values
        // return the last value
        UInt64 DecodeTen32BitDeltas(UInt64 p_base, UInt16 p_tag, UInt64* p_values);

        // decode Ten 64-bit delta compressed values by p_tag16 and store them to p_values
        // return the last value
        UInt64 DecodeTenUInt64Deltas(UInt64 p_base, UInt16 p_tag, UInt64* p_values);

        // return a 32-bit value at p_ptr
        static UInt32 GetUInt32Value(UInt32 p_tag, const UInt8* p_ptr);

        // return a 64-bit value at p_ptr
        static UInt64 GetUInt64Value(UInt32 p_tag, const UInt8* p_ptr);

        // an array of bit masks for two-bit and three-bit sizes.
        static const UInt64 s_sizeTo64BitMask[8];
    };

// decode 64-bit doc index and term frequency in TGC
// Decoding speed of regular TGC groups is the most important thing in this class
    class UInt64DocIndexDeltaWithTFDecoder : public UInt64DeltaDecoder
    {
    public:
        // a constructor
        UInt64DocIndexDeltaWithTFDecoder() {}

        // skip tf fields
        inline void SkipTF()
        {
            SkipData(GetTFSize(m_ptr));
        }

        // decode an VBC-coded TGC group of delta compressed values
        // and store them to p_values
        // CAUTION: return the number of values. It does *NOT* return the last value.
        unsigned DecodeVbcCodedDeltas(UInt64 p_base, UInt16 p_exceptionTag, UInt64* p_values, UInt32* p_tfs);

        // decode 10 tf values
        void DecodeTenTFs(UInt32* p_tfs);

        // decode 10 tf values
        void DecodeTenTFs(UInt32* p_tfs, UInt64 p_target, const UInt64* p_locations);

        // compute and return the number of bytes of a TGC group not including the tag
        // returns the size of the encoded group at p_ptr in bytes
        static UInt32 ComputeGroupSize(const UInt8* p_ptr, UInt16 p_tag16);

        // get size of tf
        static UInt32 GetTFSize(const UInt8* ptr);

    private:

        // decode one tf value
        static inline UInt32 DecodeTF(UInt32 p_tf, const UInt8* &p_ptr)
        {
            static const UInt32 s_cTFLen[4] = {1, 0, 0, 4};
            static const UInt32 s_cTFMask[4] = {0xff, 0, 0, 0xffffffff};
            static const UInt32 s_cTFVal[4] = {0, 1, 2, 0};
            UInt32 tf = s_cTFVal[p_tf] + ((*reinterpret_cast<const UInt32*>(p_ptr)) & (s_cTFMask[p_tf]));
            p_ptr += s_cTFLen[p_tf];
            return tf;
        }

        // a precomputed map from 4 bits flag to overflow bytes.
        // this is used for TF value decoding
        static const UInt8 s_sizeTFTable[16];

        // a precomputed map from 8 bits flag to overflow bytes.
        // this is used for TF value decoding
        static const UInt8 s_sizeTFTable256[256];
    };


    inline UInt32
    UInt64DocIndexDeltaWithTFDecoder::GetTFSize(const UInt8* p_ptr)
    {
        UInt32 size = 3;
        UInt8 leading = *(p_ptr++);
        size += s_sizeTFTable256[leading];
        leading = *(p_ptr++);
        size += s_sizeTFTable256[leading];
        leading = *p_ptr;
        size += s_sizeTFTable[leading & 0xF];
        return size;
    }


    inline void
    UInt64DocIndexDeltaWithTFDecoder::DecodeTenTFs(UInt32* p_tfs)
    {
        UInt8 leading = *(m_ptr++);
        p_tfs[0] = leading >> 6;
        p_tfs[1] = (leading >> 4) & 3;
        p_tfs[2] = (leading >> 2) & 3;
        p_tfs[3] = leading & 3;
        leading = *(m_ptr++);
        p_tfs[4] = leading >> 6;
        p_tfs[5] = (leading >> 4) & 3;
        p_tfs[6] = (leading >> 2) & 3;
        p_tfs[7] = leading & 3;
        leading = *(m_ptr++);
        p_tfs[8] = (leading >> 2) & 3;
        p_tfs[9] = leading & 3;
        // extract 1 byte or 4 bytes tf from exception array
        for (int i = 0; i < 10; ++i)
        {
            if (p_tfs[i] == 0) // 1 byte
            {
                p_tfs[i] = *m_ptr;
                m_ptr++;
            }
            else if (p_tfs[i] == 3) // 4 bytes
            {
                p_tfs[i] = *reinterpret_cast<const UInt32*>(m_ptr);
                m_ptr += 4;
            }
        }
    }

    inline void
            UInt64DocIndexDeltaWithTFDecoder::DecodeTenTFs(UInt32* p_tfs, UInt64 p_target, const UInt64* p_locations)
{
    const UInt8 *p_leading = m_ptr;
    m_ptr += 3;
    UInt8 leading = *(p_leading++);
    if (p_target > p_locations[3])
{
    m_ptr += s_sizeTFTable256[leading];
}
else
{
p_tfs[0] = leading >> 6;
p_tfs[1] = (leading >> 4) & 3;
p_tfs[2] = (leading >> 2) & 3;
p_tfs[3] = leading & 3;
if (s_sizeTFTable256[leading] != 0)
{
p_tfs[0] = DecodeTF(p_tfs[0], m_ptr);
p_tfs[1] = DecodeTF(p_tfs[1], m_ptr);
p_tfs[2] = DecodeTF(p_tfs[2], m_ptr);
p_tfs[3] = DecodeTF(p_tfs[3], m_ptr);
}
}
leading = *(p_leading++);
if (p_target > p_locations[7])
{
m_ptr += s_sizeTFTable256[leading];
}
else
{
p_tfs[4] = leading >> 6;
p_tfs[5] = (leading >> 4) & 3;
p_tfs[6] = (leading >> 2) & 3;
p_tfs[7] = leading & 3;
if (s_sizeTFTable256[leading] != 0)
{
p_tfs[4] = DecodeTF(p_tfs[4], m_ptr);
p_tfs[5] = DecodeTF(p_tfs[5], m_ptr);
p_tfs[6] = DecodeTF(p_tfs[6], m_ptr);
p_tfs[7] = DecodeTF(p_tfs[7], m_ptr);
}
}
leading = *p_leading;
p_tfs[8] = (leading >> 2) & 3;
p_tfs[9] = leading & 3;
if (s_sizeTFTable[leading & 0xf] != 0)
{
p_tfs[8] = DecodeTF(p_tfs[8], m_ptr);
p_tfs[9] = DecodeTF(p_tfs[9], m_ptr);
}
}


// need the boundary condition guaranteed for the last 32-bit integer
inline UInt32
UInt64DeltaDecoder::GetUInt32Value(UInt32 p_tag, const UInt8* p_ptr)
{
const UInt32* ptr = reinterpret_cast<const UInt32*>(p_ptr);
return (*ptr & s_sizeTo64BitMask[p_tag & Tag::c_uint32SizeMask]);
}

} // namespace TgcCoding


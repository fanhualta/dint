#include <iostream>

#include "TgcCoding.h"
#include <assert.h>
#include "VariableByteCoding.h"
//#include "logging.h"
//#include "BitOperationUtils.h"


namespace TgcCoding
{
    //Returns the number of leading 0-bits in x, starting at the most significant bit position.
    // If x is 0, the result is undefined.
    int builtin_clz(unsigned int type)
    {
        int num = 0;
        type |= 1; //防止type为0时，出现无限循环infinite loop，type为0时的计算结果为31。
        while (!(type & 0x80000000)) //检测最高位是不是1。
        {
            num += 1;
            type <<= 1;
        }
        return num;
    }

    unsigned char _BitScanReverse(uint32_t * Index, uint32_t Mask)
    {
        if (Mask == 0)
        {
            *Index = 0;
            return 0;
        }
        *Index = 31 - builtin_clz(Mask);
        return 1;
    }

    bool _BitScanReverse64( DWORD *Index,  DWORD64 Mask)
    {
        DWORD index1, index2;
        bool rt2 = _BitScanReverse(&index2, (DWORD)(Mask >> 32));
        if (rt2)
        {
            *Index = index2 + 32;
            return rt2;
        }
        bool rt1 = _BitScanReverse(&index1, (DWORD)Mask);
        *Index = index1;
        return rt1;
    }



// a mapping table to map a 2/3-bit size to a 64bit mask.
alignas(64) const UInt64 UInt64DeltaDecoder::s_sizeTo64BitMask[8] =
        {
                0xFFULL, 0xFFFFULL, 0xFFFFFFULL, 0xFFFFFFFFULL,
                0xFFFFFFFFFFULL, 0xFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL,
        };



// The table of indexes into s_masksOfPairs for the 8 most popular pairs
// - (2,2), (1,1), (3,3), (1,2), (2,1), (2,3), (3,2), (1,3) not including 4,4
// The other cells have the sum of value sizes plus 8.
//
// the statistics of sizes of delta pairs on a 5,853,790-doc primary index
// format: the first delta size, the second delta size, the frequency of the pair
// 1, 1, 576027707
// 1, 2, 223945543
// 1, 3,  58003897
// 1, 4,   9258382
// 1, 5,    174894
// 2, 1, 225461997
// 2, 2, 667953120
// 2, 3, 149796025
// 2, 4,  15390116
// 2, 5,    355356
// 3, 1,  57875928
// 3, 2, 150688302
// 3, 3, 340441923
// 3, 4,  24777682
// 3, 5,    102457
// 3, 6,         1
// 4, 1,   9954469
// 4, 2,  16287039
// 4, 3,  25252937
// 4, 4,  70693888
// 4, 5,   1222909
// 5, 1,    798164
// 5, 2,   1020461
// 5, 3,    447167
// 5, 4,   1839010
// 5, 5,    996192
// 5, 6,        21
// 6, 3,         4
// 6, 4,        10
// 6, 5,         8
// Index Size was 3,058,016 pages = 12,525,633,536 bytes
// 21,136,170 words on the primary index
//
const UInt8 UInt64Encoder::m_maskIndexes[8][8] =
        {
                // 1,  2,  3,  4,  5,  6,  7,  8
                {  0,  1,  2, 13, 14, 15, 16, 17 }, // 1
                {  3,  4,  5, 14, 15, 16, 17, 18 }, // 2
                { 12,  6,  7, 15, 16, 17, 18, 19 }, // 3
                { 13, 14, 15, 16, 17, 18, 19, 20 }, // 4
                { 14, 15, 16, 17, 18, 19, 20, 21 }, // 5
                { 15, 16, 17, 18, 19, 20, 21, 22 }, // 6
                { 16, 17, 18, 19, 20, 21, 22, 23 }, // 7
                { 17, 18, 19, 20, 21, 22, 23, 24 }  // 8
        };

const UInt8 UInt64Encoder::m_maskEncodeType[8][8] =
        {
                // 1,  2,  3,  4,  5,  6,  7,  8
                {  1,  1,  1,  2,  4,  4,  4,  4 }, // 1
                {  1,  1,  1,  2,  4,  4,  4,  4 }, // 2
                {  2,  1,  1,  2,  4,  4,  4,  4 }, // 3
                {  2,  2,  2,  2,  4,  4,  4,  4 }, // 4
                {  4,  4,  4,  4,  4,  4,  4,  4 }, // 5
                {  4,  4,  4,  4,  4,  4,  4,  4 }, // 6
                {  4,  4,  4,  4,  4,  4,  4,  4 }, // 7
                {  4,  4,  4,  4,  4,  4,  4,  4 }, // 8
        };

const UInt8 UInt64Encoder::m_maskMinimalSizeInBytes[64] =
        {
                1, 1, 1, 1, 1, 1, 1, 1,
                2, 2, 2, 2, 2, 2, 2, 2,
                3, 3, 3, 3, 3, 3, 3, 3,
                4, 4, 4, 4, 4, 4, 4, 4,
                5, 5, 5, 5, 5, 5, 5, 5,
                6, 6, 6, 6, 6, 6, 6, 6,
                7, 7, 7, 7, 7, 7, 7, 7,
                8, 8, 8, 8, 8, 8, 8, 8,
        };


// a mapping table of 3-bit indexes for the masks, shifts, and sizes of a pair of encoded values.
// the first value = (64-bit Data) & mask1;
// the second value = ((64-bit Data) >> shift1 ) & mask2
// the total size = size2
const UInt64 DecoderMappingTable::s_masksOfPairs[8] =
        {
                // mask1(3B)| shift1 (1B) |            mask2 (3B) |      size2 (1B)
                (     0xFFU | (  8 << 24) | (     0xFFULL << 32) | ( 2ULL << 56)), // (1,1)
                (     0xFFU | (  8 << 24) | (   0xFFFFULL << 32) | ( 3ULL << 56)), // (1,2)
                (     0xFFU | (  8 << 24) | ( 0xFFFFFFULL << 32) | ( 4ULL << 56)), // (1,3)
                (   0xFFFFU | ( 16 << 24) | (     0xFFULL << 32) | ( 3ULL << 56)), // (2,1)
                (   0xFFFFU | ( 16 << 24) | (   0xFFFFULL << 32) | ( 4ULL << 56)), // (2,2)
                (   0xFFFFU | ( 16 << 24) | ( 0xFFFFFFULL << 32) | ( 5ULL << 56)), // (2,3)
                ( 0xFFFFFFU | ( 24 << 24) | (   0xFFFFULL << 32) | ( 5ULL << 56)), // (3,2)
                ( 0xFFFFFFU | ( 24 << 24) | ( 0xFFFFFFULL << 32) | ( 6ULL << 56)), // (3,3)
        };

//this table is a map from 4 bits flag (for 2 TF) to overflow bytes.
//  0: 1 extra byte for TF value 3~255
//  1: for TF value 1
//  2: for TF value 2
//  3: 4 extra byte for TF value 256~0xffffffff
const UInt8 UInt64DocIndexDeltaWithTFDecoder::s_sizeTFTable[16] =
        {
                2, 1, 1, 5,
                1, 0, 0, 4,
                1, 0, 0, 4,
                5, 4, 4, 8,
        };

//s_sizeTFTable256[i] = s_sizeTFTable[i>>4]+s_sizeTFTable[i&15];
const UInt8 UInt64DocIndexDeltaWithTFDecoder::s_sizeTFTable256[256] =
        {
                4, 3, 3, 7, 3, 2, 2, 6, 3, 2, 2, 6, 7, 6, 6, 10,
                3, 2, 2, 6, 2, 1, 1, 5, 2, 1, 1, 5, 6, 5, 5, 9,
                3, 2, 2, 6, 2, 1, 1, 5, 2, 1, 1, 5, 6, 5, 5, 9,
                7, 6, 6, 10, 6, 5, 5, 9, 6, 5, 5, 9, 10, 9, 9, 13,
                3, 2, 2, 6, 2, 1, 1, 5, 2, 1, 1, 5, 6, 5, 5, 9,
                2, 1, 1, 5, 1, 0, 0, 4, 1, 0, 0, 4, 5, 4, 4, 8,
                2, 1, 1, 5, 1, 0, 0, 4, 1, 0, 0, 4, 5, 4, 4, 8,
                6, 5, 5, 9, 5, 4, 4, 8, 5, 4, 4, 8, 9, 8, 8, 12,
                3, 2, 2, 6, 2, 1, 1, 5, 2, 1, 1, 5, 6, 5, 5, 9,
                2, 1, 1, 5, 1, 0, 0, 4, 1, 0, 0, 4, 5, 4, 4, 8,
                2, 1, 1, 5, 1, 0, 0, 4, 1, 0, 0, 4, 5, 4, 4, 8,
                6, 5, 5, 9, 5, 4, 4, 8, 5, 4, 4, 8, 9, 8, 8, 12,
                7, 6, 6, 10, 6, 5, 5, 9, 6, 5, 5, 9, 10, 9, 9, 13,
                6, 5, 5, 9, 5, 4, 4, 8, 5, 4, 4, 8, 9, 8, 8, 12,
                6, 5, 5, 9, 5, 4, 4, 8, 5, 4, 4, 8, 9, 8, 8, 12,
                10, 9, 9, 13, 9, 8, 8, 12, 9, 8, 8, 12, 13, 12, 12, 16,
        };

UInt8
Tag::ComputeTotalTagSize(const UInt8* p_tag)
{
    UInt16 tag = Tag::Get16BitTag(p_tag);
    UInt8 tagSize;
    if (Tag::IsRegular(tag))
    {
        tagSize = 2; // a regular group - 2-byte tag
    }
    else if (Tag::Is32BitValueException(tag))
    {
        tagSize = 3; // a 32-bit group - 3-byte tag
    }
    else if (Tag::Is64BitValueException(tag))
    {
        tagSize = 4; // a 64-bit group - 4-byte tag
    }
    else
    {
        tagSize = 2; // a Vbc group - 2-byte tag
    }
    return tagSize;
}


UInt8
Tag::ComputeMinimalSizeInBytes(UInt64 p_value)
{
    UInt8 tgcSize = 1;

    p_value >>= 8;
    for (; p_value > 0; ++tgcSize)
    {
        p_value >>= 8;
    }
    return tgcSize;
}


UInt64Encoder::UInt64Encoder()
        : m_size(0),
          m_safeSize(0),
          m_numValues(0)
{}


UInt8
UInt64Encoder::Size() const
{
    return m_size;
}


UInt8
UInt64Encoder::SafeSize() const
{
    return m_safeSize;
}


UInt8
UInt64Encoder::NumberOfEncodedValues() const
{
    return m_numValues;
}


UInt8
UInt64Encoder::CopyEncodedData(UInt8* p_dest) const
{
    memcpy(p_dest, m_buffer, m_size);
    return m_size;
}


void
UInt64Encoder::Clear()
{
    m_size = 0;
    m_safeSize = 0;
    m_numValues = 0;
}

void
UInt64Encoder::ComputeValueSize(const UInt64* p_values, unsigned /* p_numData */)
{

    DWORD index = 0;
    _BitScanReverse64(&index, p_values[0]);
    m_valueSizes[0] = static_cast<UInt8>((index >> 3) + 1);
    _BitScanReverse64(&index, p_values[1]);
    m_valueSizes[1] = static_cast<UInt8>((index >> 3) + 1);
    _BitScanReverse64(&index, p_values[2]);
    m_valueSizes[2] = static_cast<UInt8>((index >> 3) + 1);
    _BitScanReverse64(&index, p_values[3]);
    m_valueSizes[3] = static_cast<UInt8>((index >> 3) + 1);
    _BitScanReverse64(&index, p_values[4]);
    m_valueSizes[4] = static_cast<UInt8>((index >> 3) + 1);
    _BitScanReverse64(&index, p_values[5]);
    m_valueSizes[5] = static_cast<UInt8>((index >> 3) + 1);
    _BitScanReverse64(&index, p_values[6]);
    m_valueSizes[6] = static_cast<UInt8>((index >> 3) + 1);
    _BitScanReverse64(&index, p_values[7]);
    m_valueSizes[7] = static_cast<UInt8>((index >> 3) + 1);
    _BitScanReverse64(&index, p_values[8]);
    m_valueSizes[8] = static_cast<UInt8>((index >> 3) + 1);
    _BitScanReverse64(&index, p_values[9]);
    m_valueSizes[9] = static_cast<UInt8>((index >> 3) + 1);
}


UInt8
UInt64Encoder::ComputeSafetyOverhead()
{
    size_t safeSize;

    UInt16 tag = Tag::Get16BitTag(m_buffer);
    if (Tag::IsRegular(tag))
    {
        // to guarantee loading 64 bits for the last two values does not overrun a buffer.
        safeSize = (sizeof(UInt64) - m_valueSizes[m_numValues-2] - m_valueSizes[m_numValues-1]);
    }
    else if (Tag::Is32BitValueException(tag))
    {
        // to guarantee loading 32 bits for the last value does not overrun a buffer.
        safeSize = (sizeof(UInt32) - m_valueSizes[m_numValues - 1]);
    }
    else if (Tag::Is64BitValueException(tag))
    {
        // to guarantee loading 64 bits for the last value does not overrun a buffer.
        safeSize = (sizeof(UInt64) - m_valueSizes[m_numValues - 1]);
    }
    else
    {
        assert(Tag::IsVbcException(tag));
        // VBC doesn't overrun
        safeSize = 0;
    }

    return static_cast<UInt8>(safeSize);
}


void
UInt64Encoder::EncodeTenValues(const UInt64* p_values, bool p_isPositionLessTFTerm)
{
    m_numValues = Tag::c_groupSize;

    ComputeValueSize(p_values, Tag::c_groupSize);
    if (p_isPositionLessTFTerm)
    {
        m_size = EncodeTen32BitValues(m_buffer, p_values, true);
    }
    else
    {
        UInt8 schema = 1;
        schema |= (m_maskEncodeType[m_valueSizes[0] - 1][m_valueSizes[1] - 1]);
        schema |= (m_maskEncodeType[m_valueSizes[2] - 1][m_valueSizes[3] - 1]);
        schema |= (m_maskEncodeType[m_valueSizes[4] - 1][m_valueSizes[5] - 1]);
        schema |= (m_maskEncodeType[m_valueSizes[6] - 1][m_valueSizes[7] - 1]);
        schema |= (m_maskEncodeType[m_valueSizes[8] - 1][m_valueSizes[9] - 1]);

        if (schema == 1)
        {
            m_size = EncodeTen24BitValues(m_buffer, p_values);
        }
        else if (schema <= 3)
        {
            m_size = EncodeTen32BitValues(m_buffer, p_values, false);
        }
        else
        {
            m_size = EncodeTen64BitValues(m_buffer, p_values);
        }
    }

    m_safeSize = m_size + ComputeSafetyOverhead();
}


bool
UInt64Encoder::EncodeValuesInVbc(
        const UInt64* p_values,
        unsigned p_numData,
        UInt16 p_exceptionType)
{
    // PositionLess index (aka Tiger) will encode TF as well as deltas. So both TF and delta are counted in m_numValues for tiger.
    if (m_numValues > Tag::c_groupSize*2)
    {
        // too many data
        return false;
    }

    m_numValues = static_cast<UInt8>(p_numData);

    if (m_numValues == 0)
    {
        m_size = Tag::EncodeVbcExceptionWithSize(m_buffer, p_exceptionType, 0, 0);
        m_safeSize = m_size;
        return true;
    }

    UInt8* ptr = m_buffer + Tag::c_vbcTagSize;
    VariableByteCoding::Encoder vbcEncoder(ptr, m_buffer + sizeof(m_buffer));
    for (unsigned numData = m_numValues; numData > 0; --numData)
    {
        if (vbcEncoder.Encode(*p_values) == false)
        {
            // too many data
            return false;
        }
        ++p_values;
    }

    ptr = vbcEncoder.Current();
    size_t totalSize = ptr - m_buffer;
    m_size = static_cast<UInt8>(totalSize);
    m_safeSize = m_size;
    assert(totalSize <= sizeof(m_buffer));
    UInt8 vbcSize = static_cast<UInt8>(vbcEncoder.Size());
    assert(vbcSize < Tag::c_maxVbcSize);

    Tag::EncodeVbcExceptionWithSize(
            m_buffer, p_exceptionType, vbcSize, UInt8(p_numData));
    return true;
}


UInt8
UInt64Encoder::StoreTenValuesInCompactFormat(UInt8* p_ptr, const UInt64* p_values)
{
    UInt8* ptr = p_ptr;

    StoreUInt64Value(ptr, p_values[0]);
    ptr += m_valueSizes[0];
    StoreUInt64Value(ptr, p_values[1]);
    ptr += m_valueSizes[1];

    StoreUInt64Value(ptr, p_values[2]);
    ptr += m_valueSizes[2];
    StoreUInt64Value(ptr, p_values[3]);
    ptr += m_valueSizes[3];

    StoreUInt64Value(ptr, p_values[4]);
    ptr += m_valueSizes[4];
    StoreUInt64Value(ptr, p_values[5]);
    ptr += m_valueSizes[5];

    StoreUInt64Value(ptr, p_values[6]);
    ptr += m_valueSizes[6];
    StoreUInt64Value(ptr, p_values[7]);
    ptr += m_valueSizes[7];

    StoreUInt64Value(ptr, p_values[8]);
    ptr += m_valueSizes[8];
    StoreUInt64Value(ptr, p_values[9]);
    ptr += m_valueSizes[9];

    return static_cast<UInt8>(ptr - p_ptr);
}


UInt8
UInt64Encoder::EncodeTen24BitValues(UInt8* p_ptr, const UInt64* p_values)
{
    /*
    UInt16 debugTag = 0;
    debugTag |= (m_maskIndexes[m_valueSizes[0] - 1][m_valueSizes[1] -1]);
    debugTag |= (m_maskIndexes[m_valueSizes[2] - 1][m_valueSizes[3] -1]);
    debugTag |= (m_maskIndexes[m_valueSizes[4] - 1][m_valueSizes[5] -1]);
    debugTag |= (m_maskIndexes[m_valueSizes[6] - 1][m_valueSizes[7] -1]);
    debugTag |= (m_maskIndexes[m_valueSizes[8] - 1][m_valueSizes[9] -1]);
    assert(debugTag < 8);
    */

    UInt8* ptr = p_ptr;

    // 16-bit tag: 3 * 5 = 15 bits = 10 values
    UInt16 tag = 0;
    tag |= (m_maskIndexes[m_valueSizes[0] - 1][m_valueSizes[1] -1]) << 0; // 0,1,2
    tag |= (m_maskIndexes[m_valueSizes[2] - 1][m_valueSizes[3] -1]) << 3; // 3,4,5
    // 6 - type
    tag |= (m_maskIndexes[m_valueSizes[4] - 1][m_valueSizes[5] -1]) << 7; // 7,8,9
    tag |= (m_maskIndexes[m_valueSizes[6] - 1][m_valueSizes[7] -1]) << 10; // 10,11,12
    tag |= (m_maskIndexes[m_valueSizes[8] - 1][m_valueSizes[9] -1]) << 13; // 13,14,15
    Tag::Store16BitTag(ptr, tag);

    return 2 + StoreTenValuesInCompactFormat(ptr + 2, p_values);
}



UInt8
UInt64Encoder::EncodeTen32BitValues(UInt8* p_ptr, const UInt64* p_values, bool p_isPositionLessTFTerm)
{
    /*
    UInt16 debugTag = 0;
    if (!p_isPositionLessTFTerm)
    {
        debugTag |= (m_maskIndexes[m_valueSizes[0] - 1][m_valueSizes[1] - 1]);
        debugTag |= (m_maskIndexes[m_valueSizes[2] - 1][m_valueSizes[3] - 1]);
        debugTag |= (m_maskIndexes[m_valueSizes[4] - 1][m_valueSizes[5] - 1]);
        debugTag |= (m_maskIndexes[m_valueSizes[6] - 1][m_valueSizes[7] - 1]);
        debugTag |= (m_maskIndexes[m_valueSizes[8] - 1][m_valueSizes[9] - 1]);
        assert(debugTag >= 8);
    }
    */

    UInt8* ptr = p_ptr;

    // 24-bit tag: 2 * 10 = 20 bits = 10 values
    // Bits at bit position 4,5,6, and 7 are 0100 for a 32-bit exception tag.
    UInt32 tag = UInt32(TgcCoding::Tag::c_escapeCode | TgcCoding::Tag::c_exceptionType32Bit);
    tag |= (UInt16(m_valueSizes[0] - 1)) << (0 * 2);
    tag |= (UInt16(m_valueSizes[1] - 1)) << (1 * 2);

    // 4 bits (4,5,6,7) are used for the exception type
    tag |= (UInt16(m_valueSizes[2] - 1)) << (2 * 2 + 4);
    tag |= (UInt16(m_valueSizes[3] - 1)) << (3 * 2 + 4);
    tag |= (UInt16(m_valueSizes[4] - 1)) << (4 * 2 + 4);
    tag |= (UInt16(m_valueSizes[5] - 1)) << (5 * 2 + 4);
    tag |= (UInt16(m_valueSizes[6] - 1)) << (6 * 2 + 4);
    tag |= (UInt16(m_valueSizes[7] - 1)) << (7 * 2 + 4);
    tag |= (UInt16(m_valueSizes[8] - 1)) << (8 * 2 + 4);
    tag |= (UInt16(m_valueSizes[9] - 1)) << (9 * 2 + 4);

    if (p_isPositionLessTFTerm)
    {
        tag |= 0x10; // Use bit position 4 to mark it is a position less index (aka Tiger) 32bits values
    }

    Tag::Store16BitTag(ptr, UInt16(tag)); // low two bytes
    *(ptr + 2) = UInt8(tag >> 16);        // high one byte

    return 3 + StoreTenValuesInCompactFormat(ptr + 3, p_values);
}


UInt8
UInt64Encoder::EncodeTen64BitValues(UInt8* p_ptr, const UInt64* p_values)
{
    /*
    UInt16 debugTag = 0;
    debugTag |= (m_maskIndexes[m_valueSizes[0] - 1][m_valueSizes[1] -1]);
    debugTag |= (m_maskIndexes[m_valueSizes[2] - 1][m_valueSizes[3] -1]);
    debugTag |= (m_maskIndexes[m_valueSizes[4] - 1][m_valueSizes[5] -1]);
    debugTag |= (m_maskIndexes[m_valueSizes[6] - 1][m_valueSizes[7] -1]);
    debugTag |= (m_maskIndexes[m_valueSizes[8] - 1][m_valueSizes[9] -1]);
    assert(debugTag >= 8);
    */

    UInt8* ptr = p_ptr;

    // 32-bit tag: 3 * 10 = 30 bits = 10 values
    // MSB two bits are ones.
    UInt32 tag = UInt32(TgcCoding::Tag::c_escapeCode | TgcCoding::Tag::c_exceptionType64Bit);
    tag |= (UInt16(m_valueSizes[0] - 1)) << (0 * 3);
    tag |= (UInt16(m_valueSizes[1] - 1)) << (1 * 3);

    // 2 bits (6,7) go for the exception type.
    tag |= (UInt16(m_valueSizes[2] - 1)) << (2 * 3 + 2);
    tag |= (UInt16(m_valueSizes[3] - 1)) << (3 * 3 + 2);
    tag |= (UInt16(m_valueSizes[4] - 1)) << (4 * 3 + 2);
    tag |= (UInt16(m_valueSizes[5] - 1)) << (5 * 3 + 2);
    tag |= (UInt16(m_valueSizes[6] - 1)) << (6 * 3 + 2);
    tag |= (UInt16(m_valueSizes[7] - 1)) << (7 * 3 + 2);
    tag |= (UInt16(m_valueSizes[8] - 1)) << (8 * 3 + 2);
    tag |= (UInt16(m_valueSizes[9] - 1)) << (9 * 3 + 2);


    Tag::Store16BitTag(ptr, UInt16(tag));           // low two bytes
    Tag::Store16BitTag(ptr + 2, UInt16(tag >> 16)); // high two bytes

    return 4 + StoreTenValuesInCompactFormat(ptr + 4, p_values);
}


void
UInt64Encoder::StoreUInt32Value(UInt8* p_ptr, UInt64 p_value)
{
    *reinterpret_cast<UInt32*>(p_ptr) = static_cast<UInt32>(p_value);
}


void
UInt64Encoder::StoreUInt64Value(UInt8* p_ptr, UInt64 p_values)
{
    *reinterpret_cast<UInt64*>(p_ptr) = static_cast<UInt64>(p_values);
}


std::ostream&
UInt64Encoder::Print(std::ostream& p_os) const
{
    p_os << "UInt64Encoder::m_numValues= " << NumberOfEncodedValues() << std::endl;
    p_os << "UInt64Encoder::m_size= " << Size() << std::endl;
    p_os << "UInt64Encoder::m_buffer= ";
    std::ios_base::fmtflags flags = p_os.flags();
    p_os << std::hex;
    for (unsigned u = 0; u < Size(); ++u) {
        p_os << m_buffer[u] << " ";
    }
    p_os.flags(flags);
    p_os << std::endl;
    return p_os;
}

UInt8
UInt64Encoder::EncodeTenTFs(const UInt32* p_tfs)
{
    UInt8* p_ptr = m_buffer + m_size;
    UInt8* ptr = p_ptr + 3;
    UInt8 leading = 0;
    for (int i = 0; i < 10; ++i)
    {
        if (1 == p_tfs[i] || 2 == p_tfs[i])
        {
            leading = (leading << 2) | (UInt8)p_tfs[i];
        }
        else if (p_tfs[i] < 256)
        {
            leading = (leading << 2);
            *ptr++ = (UInt8)p_tfs[i];
        }
        else
        {
            leading = (leading << 2) | 3;
            *(UInt32*)ptr = p_tfs[i];
            ptr += 4;
        }
        if (i == 3 || i == 7 || i == 9)
        {
            *(p_ptr + (i / 4)) = leading;
            leading = 0;
        }
    }
    UInt8 size = (UInt8)(ptr - p_ptr);
    m_size += size;
    m_safeSize += size;
    return size;
}


// need the boundary condtion guaranteed for the last 64-bit integer
inline UInt64
UInt64DeltaDecoder::GetUInt64Value(UInt32 p_tag, const UInt8* p_ptr)
{
    const UInt64* ptr = reinterpret_cast<const UInt64*>(p_ptr);
    UInt64 value = (*ptr & s_sizeTo64BitMask[p_tag & Tag::c_uint64SizeMask]);
    return value;
}


UInt64
UInt64DeltaDecoder::DecodeTenUInt64Deltas(UInt64 p_base, UInt16 p_tag, UInt64* p_values)
{
    const UInt8* ptr = m_ptr;
    UInt32 tag32 = p_tag | (UInt32(Tag::Get16BitTag(ptr)) << 16);
    ptr += 2;

    // 3 * 10 = 30 bits = 10 values, a 3-bit tag for each value
    p_base = (p_values[0] = p_base + GetUInt64Value(tag32, ptr));
    ptr += (tag32 & Tag::c_uint64SizeMask) + 1;
    tag32 >>= 3;
    p_base = (p_values[1] = p_base + GetUInt64Value(tag32, ptr));
    ptr += (tag32 & Tag::c_uint64SizeMask) + 1;
    tag32 >>= 5; // one three-bit sizes and a 2-bit group type

    p_base = (p_values[2] = p_base + GetUInt64Value(tag32, ptr));
    ptr += (tag32 & Tag::c_uint64SizeMask) + 1;
    tag32 >>= 3;
    p_base = (p_values[3] = p_base + GetUInt64Value(tag32, ptr));
    ptr += (tag32 & Tag::c_uint64SizeMask) + 1;
    tag32 >>= 3;

    p_base = (p_values[4] = p_base + GetUInt64Value(tag32, ptr));
    ptr += (tag32 & Tag::c_uint64SizeMask) + 1;
    tag32 >>= 3;
    p_base = (p_values[5] = p_base + GetUInt64Value(tag32, ptr));
    ptr += (tag32 & Tag::c_uint64SizeMask) + 1;
    tag32 >>= 3;

    p_base = (p_values[6] = p_base + GetUInt64Value(tag32, ptr));
    ptr += (tag32 & Tag::c_uint64SizeMask) + 1;
    tag32 >>= 3;
    p_base = (p_values[7] = p_base + GetUInt64Value(tag32, ptr));
    ptr += (tag32 & Tag::c_uint64SizeMask) + 1;
    tag32 >>= 3;

    p_base = (p_values[8] = p_base + GetUInt64Value(tag32, ptr));
    ptr += (tag32 & Tag::c_uint64SizeMask) + 1;
    tag32 >>= 3;
    p_base = (p_values[9] = p_base + GetUInt64Value(tag32, ptr));
    ptr += (tag32 & Tag::c_uint64SizeMask) + 1;
    // tag32 >>= 3; is not necessary here

    m_ptr = ptr;

    return p_base;
}


// decode Ten 24-bit delta compressed values by p_tag and store them to p_values
// return the last value
// This functions is critical to the performance, so it is forced to be inlined.
UInt64
UInt64DeltaDecoder::DecodeTen24BitDeltas(UInt64 p_base, UInt16 p_tag, UInt64* p_values)
{
    const UInt8* ptr = m_ptr;
    DecoderMappingTable tableDecoder;

    // 3 bits a pair * 5 pairs = 15 bits = 10 values, a 3-bit tag for a pair of values
    tableDecoder.DecodeTwo24BitDeltas(p_base, p_tag, p_values + 0, ptr);
    p_tag >>= 3;
    tableDecoder.DecodeTwo24BitDeltas(p_base, p_tag, p_values + 2, ptr);
    p_tag >>= 4; // a 3-bit tag and one bit escape code at bit 6
    tableDecoder.DecodeTwo24BitDeltas(p_base, p_tag, p_values + 4, ptr);
    p_tag >>= 3;
    tableDecoder.DecodeTwo24BitDeltas(p_base, p_tag, p_values + 6, ptr);
    p_tag >>= 3;
    tableDecoder.DecodeLastTwo24BitDeltas(p_base, p_tag, p_values + 8, ptr);
    // p_tag >>= 3;

    m_ptr = ptr;
    return p_base;
}

unsigned
UInt64DeltaDecoder::DecodeVbcCodedDeltas(UInt64 p_base, UInt16 p_exceptionTag, UInt64* p_values)
{
    UInt16 exceptionTag = p_exceptionTag;

    assert(Tag::IsVbcException(exceptionTag));

    const UInt8* ptrEnd = m_ptr + Tag::GetVbcDataSize(exceptionTag);
    VariableByteCoding::Decoder vbcDecoder(m_ptr, ptrEnd);
    m_ptr = ptrEnd;

    unsigned numDeltas = 0;
    while (vbcDecoder.Decode(*p_values))
    {
        p_base = (*p_values += p_base);
        ++p_values;
        ++numDeltas;
        assert(numDeltas <= Tag::c_groupSize);
    }
    assert(numDeltas == Tag::GetVbcNumValues(exceptionTag));
    return numDeltas;
}


UInt64
UInt64DeltaDecoder::DecodeTen32BitDeltas(UInt64 p_base, UInt16 p_tag, UInt64* p_values)
{
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


UInt32
UInt64DocIndexDeltaWithTFDecoder::ComputeGroupSize(const UInt8* p_ptr, UInt16 p_tag16)
{
    const UInt8* orgPtr = p_ptr;
    const UInt8* ptr = p_ptr;

    UInt16 tag = p_tag16;
    unsigned numDeltas;
    if (Tag::IsRegular(tag))
    {
        numDeltas = Tag::c_groupSize;

        ptr += DecoderMappingTable::GetTotalSizeOfPair(tag & 7);
        tag >>= 3;
        ptr += DecoderMappingTable::GetTotalSizeOfPair(tag & 7);
        tag >>= 4; // a 3-bit tag and one bit escape code at bit 6
        ptr += DecoderMappingTable::GetTotalSizeOfPair(tag & 7);
        tag >>= 3;
        ptr += DecoderMappingTable::GetTotalSizeOfPair(tag & 7);
        tag >>= 3;
        ptr += DecoderMappingTable::GetTotalSizeOfPair(tag & 7);
        // tag >>= 3; not necessary here.

        return static_cast<UInt32>(ptr - orgPtr);
    }

    UInt16 exceptionTag = tag;

    if (Tag::Is32BitValueException(exceptionTag))
    {
        UInt32 sizeTag = exceptionTag | (UInt32(*ptr) << 16);
        ptr += 1;

        numDeltas = Tag::c_groupSize;
        ptr += numDeltas; // is the number of bytes in addtion to the encoded size

        ptr += (sizeTag & Tag::c_uint32SizeMask);
        sizeTag >>= 2;
        ptr += (sizeTag & Tag::c_uint32SizeMask);
        sizeTag >>= 6; // one two-bit sizes and a 4-bit group type
        for (int i = 2; i < Tag::c_groupSize; ++i)
        {
            ptr += (sizeTag & Tag::c_uint32SizeMask);
            sizeTag >>= 2;
        }

        UInt32 rt = static_cast<UInt32>(ptr - orgPtr);
        if ((tag & 0x10) == 0x10)
        {
            rt += GetTFSize(ptr);
        }
        return rt;
    }

    if (Tag::Is64BitValueException(exceptionTag))
    {
        UInt32 sizeTag = exceptionTag | (UInt32(Tag::Get16BitTag(ptr)) << 16);
        ptr += 2;

        numDeltas = Tag::c_groupSize;
        ptr += numDeltas; // is the number of bytes in addtion to the encoded size

        ptr += (sizeTag & Tag::c_uint64SizeMask);
        sizeTag >>= 3;
        ptr += (sizeTag & Tag::c_uint64SizeMask);
        sizeTag >>= 5; // one three-bit sizes and a 2-bit group type
        for (int i = 2; i < Tag::c_groupSize; ++i)
        {
            ptr += (sizeTag & Tag::c_uint64SizeMask);
            sizeTag >>= 3;
        }

        return static_cast<UInt32>(ptr - orgPtr);
    }

    ptr += Tag::GetVbcDataSize(exceptionTag);
    return static_cast<UInt32>(ptr - orgPtr);
}


unsigned
UInt64DocIndexDeltaWithTFDecoder::DecodeVbcCodedDeltas(UInt64 p_base, UInt16 p_exceptionTag, UInt64* p_values, UInt32* p_tfs)
{
    UInt16 exceptionTag = p_exceptionTag;

    assert(Tag::IsVbcException(exceptionTag));

    const UInt8* ptrEnd = m_ptr + Tag::GetVbcDataSize(exceptionTag);
    VariableByteCoding::Decoder vbcDecoder(m_ptr, ptrEnd);
    m_ptr = ptrEnd;

    unsigned numDeltas = 0;
    UInt64 delta;
    while (vbcDecoder.Decode(delta))
    {
        *p_tfs = delta & 0x3;
        *p_values = delta >> 2;
        p_base = (*p_values += p_base);
        ++p_values;
        ++numDeltas;
        assert(numDeltas <= Tag::c_groupSize);

        if (*p_tfs == 0)
        {
            vbcDecoder.Decode(delta); // tf
            *p_tfs = (UInt32)delta;
        }
        ++p_tfs;
    }
    return numDeltas;
}

} // namespace TgcCoding


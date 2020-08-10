#pragma once


// a collection of classes to encode and decode data in variable byte coding
// varaible byte coding - VBC in short - is a byte-oriented compression scheme.
// VBC compresses an integer value into minimal number of bytes
// by removing leading zero bytes with a flag.
// Here are examples.
//  - 7-bit integer: a single byte - 0xxxxxxx
//  - 14-bit integer: two bytes - 1xxxxxxx (low 7 bits) 0xxxxxxx (highest 7 bites)
//  - 21-bit integer: three bytes - 1xxxxxxx 1xxxxxxx 0xxxxxxx (highest 7 bites)
// The MSB of a byte is a continuation bit.
// VBC does not require any other bits other than a stream of bytes.
//  - easy to write, easy to use, easy to manage, easy to debug
//  - efficient for encoding and decoding small numbers in a couple of bytes.
//  - slow for encoding and decoding big numbers in many bytes.
namespace VariableByteCoding
{

// a decoder class
// decodes variable byte coded value
    class Decoder
    {
    public:
        // a constructer
        //
        // p_ptr is a pointer to the current compressed data.
        // p_end is the end of a byte stream.
        Decoder(const UInt8* p_ptr, const UInt8* p_end)
                : m_ptr(p_ptr),
                  m_end(p_end)
        {}


        // decode a VBC coded number
        //
        // p_value is a storage for a decoded data.
        // return true if a number is decoded successfully.
        //        false if p_end is hit while decoding.
        inline bool Decode(UInt64 &p_value)
        {
            if (m_ptr >= m_end)
            {
                return false; // no more value
            }
            UInt8 u8 = *m_ptr++;
            p_value = u8 & 0x7F;
            int    shift = 7;
            while (u8 & 0x80)
            {
                if (m_ptr >= m_end)
                {
                    return false; // no more value
                }
                u8 = *m_ptr++;
                p_value += UInt64(u8 & 0x7f) << shift;
                shift += 7;
            }
            return true;
        }


        // reduce the buffer size by p_amount.
        void ReduceSizeBy(unsigned p_amount)
        {
            m_end -= p_amount;
        }


        // retrun the current decoding position
        inline const UInt8* Current() const
        {
            return m_ptr;
        }

    private:
        // the current decoding position
        const UInt8* m_ptr;

        // the end of a byte stream
        const UInt8* m_end;
    };


/// Encode 64 bit values in VBC
/// Don't check buffer boundary here.
    class Encoder
    {
    public:
        // a constructor
        Encoder(UInt8* p_ptr, UInt8* p_end)
                : m_ptr(p_ptr),
                  m_end(p_end),
                  m_orgPtr(p_ptr)
        {}

        // encode a value in VBC
        // return a new current pointer
        bool Encode(UInt64 p_value)
        {
            while (p_value >= (1 << 7))
            {
                if (m_ptr >= m_end)
                {
                    return false; // no more space
                }
                *m_ptr = UInt8((p_value & 0x7F) + 0x80);
                ++m_ptr;
                p_value >>= 7;
            }
            if (m_ptr >= m_end)
            {
                return false; // no more space
            }
            *m_ptr = UInt8(p_value);
            ++m_ptr;
            return true;
        }

        // return the size of data
        UInt32 Size() const
        {
            return UInt32(m_ptr - m_orgPtr);
        }

        // retrun the current encoding position
        inline UInt8* Current() const
        {
            return m_ptr;
        }

    private:
        // the current encoding position
        UInt8* m_ptr;

        // the end of a buffer
        UInt8* m_end;

        // the beginning of a buffer
        UInt8* m_orgPtr;
    };

} //namespace VariableByteCoding



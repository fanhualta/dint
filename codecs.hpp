#pragma once

#include "FastPFor/headers/optpfor.h"
#include "FastPFor/headers/variablebyte.h"
#include "streamvbyte/include/streamvbyte.h"
#include "MaskedVByte/include/varintencode.h"
#include "MaskedVByte/include/varintdecode.h"
#include "VarIntG8IU.h"
#include "varintgb.h"
#include "interpolative_coding.hpp"
#include "qmx.hpp"

#include "succinct/util.hpp"
#include "util.hpp"
#include "dictionary.hpp"

namespace ds2i {

    // workaround: VariableByte::decodeArray needs the buffer size, while we
    // only know the number of values. It also pads to 32 bits. We need to
    // rewrite
    class TightVariableByte {
    public:
      template <uint32_t i> static uint8_t extract7bits(const uint32_t val) {
        return static_cast<uint8_t>((val >> (7 * i)) & ((1U << 7) - 1));
      }

      template <uint32_t i>
      static uint8_t extract7bitsmaskless(const uint32_t val) {
        return static_cast<uint8_t>((val >> (7 * i)));
      }

      static void encode(const uint32_t *in, const size_t length, uint8_t *out,
                         size_t &nvalue) {
        uint8_t *bout = out;
        for (size_t k = 0; k < length; ++k) {
          const uint32_t val(in[k]);
          /**
           * Code below could be shorter. Whether it could be faster
           * depends on your compiler and machine.
           */
          if (val < (1U << 7)) {
            *bout = static_cast<uint8_t>(val | (1U << 7));
            ++bout;
          } else if (val < (1U << 14)) {
            *bout = extract7bits<0>(val);
            ++bout;
            *bout = extract7bitsmaskless<1>(val) | (1U << 7);
            ++bout;
          } else if (val < (1U << 21)) {
            *bout = extract7bits<0>(val);
            ++bout;
            *bout = extract7bits<1>(val);
            ++bout;
            *bout = extract7bitsmaskless<2>(val) | (1U << 7);
            ++bout;
          } else if (val < (1U << 28)) {
            *bout = extract7bits<0>(val);
            ++bout;
            *bout = extract7bits<1>(val);
            ++bout;
            *bout = extract7bits<2>(val);
            ++bout;
            *bout = extract7bitsmaskless<3>(val) | (1U << 7);
            ++bout;
          } else {
            *bout = extract7bits<0>(val);
            ++bout;
            *bout = extract7bits<1>(val);
            ++bout;
            *bout = extract7bits<2>(val);
            ++bout;
            *bout = extract7bits<3>(val);
            ++bout;
            *bout = extract7bitsmaskless<4>(val) | (1U << 7);
            ++bout;
          }
        }
        nvalue = bout - out;
      }

      static void encode_single(uint32_t val, std::vector<uint8_t> &out) {
        uint8_t buf[5];
        size_t nvalue;
        encode(&val, 1, buf, nvalue);
        out.insert(out.end(), buf, buf + nvalue);
      }

      static uint8_t const *decode(const uint8_t *in, uint32_t *out, size_t n) {
        const uint8_t *inbyte = in;
        for (size_t i = 0; i < n; ++i) {
          unsigned int shift = 0;
          for (uint32_t v = 0;; shift += 7) {
            uint8_t c = *inbyte++;
            v += ((c & 127) << shift);
            if ((c & 128)) {
              *out++ = v;
              break;
            }
          }
        }
        return inbyte;
      }
    };

    struct header {
        static void write(uint32_t n, uint32_t universe,
                          std::vector<uint8_t>& out)
        {
            TightVariableByte::encode_single(n, out);
            TightVariableByte::encode_single(universe, out);
        }

        static uint8_t const* read(uint8_t const* in,
                                   uint32_t* n, uint32_t* universe)
        {
            uint8_t const* inbuf = in;
            inbuf = TightVariableByte::decode(inbuf, n, 1);
            inbuf = TightVariableByte::decode(inbuf, universe, 1);
            return inbuf;
        }
    };

    struct interpolative {
        static void encode(uint32_t const* in,
                           uint32_t universe, uint32_t n,
                           std::vector<uint8_t>& out,
                           dictionary::builder const* builder = nullptr)
        {
            (void) builder;
            std::vector<uint32_t> inbuf(n);
            std::vector<uint32_t> outbuf;
            inbuf[0] = *in;
            for (size_t i = 1; i < n; ++i) {
                inbuf[i] = inbuf[i - 1] + in[i];
            }

            bit_writer bw(outbuf);
            bw.write_interpolative(inbuf.data(), n - 1, 0, universe);
            uint8_t const *bufptr = (uint8_t const *)outbuf.data();
            out.insert(out.end(), bufptr,
                       bufptr + succinct::util::ceil_div(bw.size(), 8));
        }

        static uint8_t const* DS2I_NOINLINE decode(uint8_t const* in,
                                                   uint32_t* out,
                                                   uint32_t universe, uint32_t n,
                                                   dictionary const* dict = nullptr)
        {
            (void) dict;
            uint8_t const* inbuf = in;
            out[n - 1] = universe;
            size_t read_interpolative = 0;
            if (n > 1) {
                bit_reader br((uint32_t const *)inbuf);
                br.read_interpolative(out, n - 1, 0, universe);
                for (size_t i = n - 1; i > 0; --i) {
                    out[i] -= out[i - 1];
                }
                read_interpolative = succinct::util::ceil_div(br.position(), 8);
            }

            return inbuf + read_interpolative;
        }
    };

    struct optpfor {

        struct codec_type : FastPFor::OPTPFor<4, FastPFor::Simple16<false>> {

            uint8_t const* force_b;

            uint32_t findBestB(const uint32_t *in, uint32_t len) {
                // trick to force the choice of b from a parameter
                if (force_b) {
                    return *force_b;
                }

                // this is mostly a cut&paste from FastPFor, but we stop the
                // optimization early as the b to test becomes larger than maxb
                uint32_t b = 0;
                uint32_t bsize = std::numeric_limits<uint32_t>::max();
                const uint32_t mb = FastPFor::maxbits(in, in + len);
                uint32_t i = 0;
                while (mb > 28 + possLogs[i])
                    ++i; // some schemes such as Simple16 don't code numbers greater than 28

                for (; i < possLogs.size(); i++) {
                    if (possLogs[i] > mb && possLogs[i] >= mb)
                        break;
                    const uint32_t csize = tryB(possLogs[i], in, len);

                    if (csize <= bsize) {
                        b = possLogs[i];
                        bsize = csize;
                    }
                }

                return b;
            }

            const uint32_t* decodeArray(
            #ifndef NDEBUG
                    const uint32_t *in, const size_t len, uint32_t *out, size_t & nvalue) {
            #else
                    const uint32_t *in, const size_t    , uint32_t *out, size_t & nvalue) {
            #endif
            #ifndef NDEBUG
                const uint32_t * const initin(in);
            #endif
                const uint32_t * const initout(out);

                const uint32_t numBlocks = *in++;
                size_t out_len = 0;
                for (uint32_t i = 0; i < numBlocks; i++) {
                    in = decodeBlock(in, out, out_len);
                    out += out_len;
                }
                nvalue = out - initout;
                assert(nvalue == numBlocks * BlockSize);
                return in;
            }
        };

        static void encode(uint32_t const* in,
                           uint32_t /*universe*/, uint32_t n,
                           std::vector<uint8_t>& out,
                           dictionary::builder const* /*builder*/,
                           uint8_t const* b = nullptr) // if non-null forces b
        {
            thread_local codec_type optpfor_codec;
            thread_local std::vector<uint8_t> buf(2 * 4 * n);
            size_t out_len = buf.size();
            optpfor_codec.force_b = b;
            if (n % 128) {
                n = (n / 128 + 1) * 128;
            }
            optpfor_codec.encodeArray(in, n, reinterpret_cast<uint32_t *>(buf.data()), out_len); // encodeBlock
            out_len *= 4;
            out.insert(out.end(), buf.data(), buf.data() + out_len);
        }

        static uint8_t const* DS2I_NOINLINE decode(uint8_t const* in,
                                                   uint32_t* out,
                                                   uint64_t /*universe*/, uint32_t n,
                                                   dictionary const* /*dict*/)
        {
            thread_local codec_type optpfor_codec; // pfor decoding is *not* thread-safe
            size_t out_len = n;
            uint8_t const *ret;
            if (n % 128) {
                n = (n / 128 + 1) * 128;
            }
            ret = reinterpret_cast<uint8_t const *>(optpfor_codec.decodeArray(
                        reinterpret_cast<uint32_t const *>(in), n, out, out_len
                    ));
            assert(out_len == n);
            return ret;
        }
    };

    struct varintg8iu {

        struct codec_type : VarIntG8IU {

            // rewritten version of decodeBlock optimized for when the output
            // size is known rather than the input
            // the buffers pointed by src and dst must be respectively at least
            // 9 and 8 elements large
            uint32_t decodeBlock(uint8_t const *&src, uint32_t *dst) const
            {
                uint8_t desc = *src;
                src += 1;
                const __m128i data =
                    _mm_lddqu_si128(reinterpret_cast<__m128i const *>(src));
                src += 8;
                const __m128i result = _mm_shuffle_epi8(data, vecmask[desc][0]);
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dst), result);
                int readSize = maskOutputSize[desc];

                if (readSize > 4) {
                    const __m128i result2 = _mm_shuffle_epi8(
                        data, vecmask[desc][1]);
                    _mm_storeu_si128(
                        reinterpret_cast<__m128i *>(dst + 4),
                        result2);
                }

                return readSize;
            }
        };

        static void encode(uint32_t const* in,
                           uint32_t /*universe*/, uint32_t n,
                           std::vector<uint8_t>& out,
                           dictionary::builder const* /*builder*/)
        {
            thread_local codec_type varint_codec;
            thread_local std::vector<uint8_t> buf(2 * 4 * n);
            size_t out_len = buf.size();
            const uint32_t *src = in;
            unsigned char *dst = buf.data();
            size_t srclen = n * 4;
            size_t dstlen = out_len;
            out_len = 0;
            while (srclen > 0 && dstlen >= 9) {
                out_len += varint_codec.encodeBlock(src, srclen, dst, dstlen);
            }
            assert(srclen == 0);
            out.insert(out.end(), buf.data(), buf.data() + out_len);
        }

        // we only allow varint to be inlined (others have DS2I_NOILINE)
        static uint8_t const* decode(uint8_t const* in,
                                     uint32_t* out,
                                     uint32_t /*universe*/, uint32_t n,
                                     dictionary const* /*dict*/)
        {
            static codec_type varint_codec; // decodeBlock is thread-safe

            size_t out_len = 0;
            uint8_t const *src = in;
            uint32_t *dst = out;
            while (out_len <= (n - 8)) {
                out_len += varint_codec.decodeBlock(src, dst + out_len);
            }

            // decodeBlock can overshoot, so we decode the last blocks in a
            // local buffer
            while (out_len < n) {
                uint32_t buf[8];
                size_t read = varint_codec.decodeBlock(src, buf);
                size_t needed = std::min(read, n - out_len);
                memcpy(dst + out_len, buf, needed * 4);
                out_len += needed;
            }
            assert(out_len == n);
            return src;
        }
    };

    struct qmx {

        static void encode(uint32_t const* in,
                           uint32_t /*universe*/, uint32_t n,
                           std::vector<uint8_t>& out,
                           dictionary::builder const* /*builder*/)
        {
            static const uint64_t overflow = 512;
            thread_local std::vector<uint8_t> buf( (overflow * 4) + 2 * 4 * n);
            QMX::codec qmx_codec(n);
            size_t compressed_bytes = qmx_codec.encode(buf.data(), in);
            TightVariableByte::encode_single(compressed_bytes, out);
            out.insert(out.end(), buf.data(), buf.data() + compressed_bytes);
        }

        static uint8_t const* decode(uint8_t const* in,
                                     uint32_t* out,
                                     uint32_t /*universe*/, uint32_t n,
                                     dictionary const* /*dict*/)
        {
            uint32_t compressed_bytes = 0;
            in = TightVariableByte::decode(in, &compressed_bytes, 1);
            QMX::codec qmx_codec(n);
            qmx_codec.decode(out, in, compressed_bytes);
            return in + compressed_bytes;
        }
    };

    struct vbyte {

        static void encode(uint32_t const* in,
                           uint32_t /*universe*/, uint32_t n,
                           std::vector<uint8_t>& out,
                           dictionary::builder const* /*builder*/)
        {
            std::vector<uint8_t> buf(2 * 4 * n);
            size_t out_len = buf.size();
            TightVariableByte::encode(in, n, buf.data(), out_len);
            out.insert(out.end(), buf.data(), buf.data() + out_len);
        }

        static uint8_t const* decode(uint8_t const* in,
                                     uint32_t* out,
                                     uint32_t /*universe*/, size_t n,
                                     dictionary const* /*dict*/)
        {
            return TightVariableByte::decode(in, out, n);
        }
    };

    struct u32 {

        static void encode(uint32_t const* in,
                           uint32_t /*universe*/, uint32_t n,
                           std::vector<uint8_t>& out,
                           dictionary::builder const* /*builder*/)
        {
            size_t srclen = n * sizeof(uint32_t);
            const uint8_t* src = (const uint8_t*)in;
            out.insert(out.end(), src, src + srclen);
        }

        static uint8_t const* decode(uint8_t const* in,
                                     uint32_t* out,
                                     uint32_t /*universe*/, size_t n,
                                     dictionary const* /*dict*/)
        {
            const uint8_t* src = (const uint8_t*)in;
            uint8_t* dst = (uint8_t*)out;
            size_t n4 = n * 4;
            for (size_t i = 0; i < n4; i++) {
                *dst++ = *src++;
            }
            return src;
        }
    };

    struct simple16 {

        using codec_type = FastPFor::Simple16<false>;

        static void encode(uint32_t const* in,
                           uint32_t /*universe*/, uint32_t n,
                           std::vector<uint8_t>& out,
                           dictionary::builder const* /*builder*/)
        {
            thread_local codec_type simple16_codec;
            std::vector<uint8_t> buf(2 * 8 * n);
            size_t out_len = buf.size();
            simple16_codec.encodeArray(in, n, reinterpret_cast<uint32_t*>(buf.data()), out_len);
            out_len *= 4;
            out.insert(out.end(), buf.data(), buf.data() + out_len);
        }

        static uint8_t const* decode(uint8_t const* in,
                                     uint32_t* out,
                                     uint32_t /*universe*/, size_t n,
                                     dictionary const* /*dict*/)
        {
            uint8_t const* ret;
            thread_local codec_type simple16_codec;
            ret = reinterpret_cast<uint8_t const*>(simple16_codec.decodeArray(reinterpret_cast<uint32_t const*>(in), 1,
                out, n));
            return ret;
        }
    };

    struct streamvbyte {

        static void encode(uint32_t const* in,
                           uint32_t /*universe*/, uint32_t n,
                           std::vector<uint8_t>& out,
                           dictionary::builder const* /*builder*/)
        {
            uint32_t *src = const_cast<uint32_t*>(in);
            std::vector<uint8_t> buf(streamvbyte_max_compressedbytes(n));
            size_t out_len = streamvbyte_encode(src, n, buf.data());
            out.insert(out.end(), buf.data(), buf.data() + out_len);
        }

        static uint8_t const* decode(uint8_t const* in,
                                     uint32_t* out,
                                     uint32_t /*universe*/, size_t n,
                                     dictionary const* /*dict*/)
        {
            auto read = streamvbyte_decode(in, out, n);
            return in + read;
        }
    };

    struct maskedvbyte {

        static void encode(uint32_t const* in,
                           uint32_t /*universe*/, uint32_t n,
                           std::vector<uint8_t>& out,
                           dictionary::builder const* /*builder*/)
        {
            uint32_t* src = const_cast<uint32_t*>(in);
            std::vector<uint8_t> buf(2 * n * sizeof(uint32_t));
            size_t out_len = vbyte_encode(src, n, buf.data());
            out.insert(out.end(), buf.data(), buf.data() + out_len);
        }

        static uint8_t const* decode(uint8_t const* in,
                                     uint32_t* out,
                                     uint32_t /*universe*/, size_t n,
                                     dictionary const* /*dict*/)
        {
            auto read = masked_vbyte_decode(in, out, n);
            return in + read;
        }
    };

    struct varintgb {

        static void encode(uint32_t const* in,
                           uint32_t /*universe*/, uint32_t n,
                           std::vector<uint8_t>& out,
                           dictionary::builder const* /*builder*/)
        {
            thread_local VarIntGB<false> varintgb_codec;
            thread_local std::vector<uint8_t> buf(2 * n * sizeof(uint32_t));
            size_t out_len = varintgb_codec.encodeArray(in, n, buf.data());
            out.insert(out.end(), buf.data(), buf.data() + out_len);
        }

        static uint8_t const* decode(uint8_t const* in,
                                     uint32_t* out,
                                     uint32_t /*universe*/, size_t n,
                                     dictionary const* /*dict*/)
        {
            thread_local VarIntGB<false> varintgb_codec;
            auto read = varintgb_codec.decodeArray(in, n, out);
            return read + in;
        }
    };

    struct dint_statistics
    {
        dint_statistics()
            : ints(3, 0)
            , codewords(3, 0)
            , codewords_distr(5, 0)
        {}

        std::vector<uint64_t> ints;       // 0:runs; 1:table; 2:exceptions
        std::vector<uint64_t> codewords;  // 0:runs; 1:table; 2:exceptions

        std::vector<uint64_t> codewords_distr;  // 0:16+; 1:8; 2:4; 3:2; 4:1
    };

    struct dint {

        static void encode(uint32_t const* in,
                           uint32_t /*universe*/, uint32_t n,
                           std::vector<uint8_t>& out,
                           dictionary::builder const* builder)
        {
            uint32_t const* begin = in;
            uint32_t const* end = begin + n;

            while (begin < end) {
                // first, try runs of sizes 256, 128, 64, 32 and 16
                uint32_t longest_run_size = 0;
                uint32_t run_size = 256;
                uint32_t index = 1;

                for (uint32_t const* ptr  = begin;
                                     ptr != begin + std::min<uint64_t>(run_size, end - begin);
                                   ++ptr)
                {
                    if (*ptr == 1) {
                        ++longest_run_size;
                    } else {
                        break;
                    }
                }

                while (longest_run_size < run_size and run_size != 8) {
                    run_size /= 2;
                    ++index;
                }

                if (index < dictionary::reserved) {
                    auto ptr = reinterpret_cast<uint8_t const*>(&index);
                    out.insert(out.end(), ptr, ptr + 2);
                    begin += std::min<uint64_t>(run_size, end - begin);
                } else {
                    for (uint32_t sub_block_size  = builder->entry_size();
                                  sub_block_size != 0; sub_block_size /= 2)
                    {
                        uint32_t len = std::min<uint32_t>(sub_block_size, end - begin);
                        index = builder->lookup(begin, len);
                        if (index != dictionary::invalid_index) {
                            auto ptr = reinterpret_cast<uint8_t const*>(&index);
                            out.insert(out.end(), ptr, ptr + 2);
                            begin += len;
                            break;
                        }
                    }

                    if (index == dictionary::invalid_index) {
                        out.insert(out.end(), 0);
                        out.insert(out.end(), 0);
                        uint32_t exception = *begin;
                        auto ptr = reinterpret_cast<uint8_t const*>(&exception);
                        out.insert(out.end(), ptr, ptr + 4);
                        begin += 1;
                    }
                }
            }
        }

        static uint8_t const* decode(uint8_t const* in,
                                     uint32_t* out,
                                     uint32_t /*universe*/, size_t n,
                                     dictionary const* dict,
                                     dint_statistics& stats)
        {
            uint16_t const* ptr = reinterpret_cast<uint16_t const*>(in);
            for (size_t i = 0; i != n; ++ptr) {
                uint32_t index = *ptr;
                uint32_t decoded_ints = 1;

                if (DS2I_LIKELY(index > 5)) {
                    decoded_ints = dict->copy(index, out);

                    // stats.ints[1] += decoded_ints;
                    // stats.codewords[1] += 1;
                } else {
                    static const uint32_t run_lengths[6] = {1, // exception
                                                            256, 128, 64, 32, 16};
                    decoded_ints = run_lengths[index]; // runs of 256, 128, 64, 32 or 16 ints
                    if (DS2I_UNLIKELY(decoded_ints == 1)) {
                        ++ptr;
                        *out = *(reinterpret_cast<uint32_t const*>(ptr));
                        ++ptr;

                        // stats.ints[2] += 1;
                        // stats.codewords[2] += 3;
                        // stats.codewords_distr[4] += 2;
                    }
                    // else {

                    //     stats.ints[0] += decoded_ints;
                    //     stats.codewords[0] += 1;
                    // }
                }

                out += decoded_ints;
                i += decoded_ints;

                // if (decoded_ints >= 16) {
                //     stats.codewords_distr[0] += 1;
                // } else if (decoded_ints == 8) {
                //     stats.codewords_distr[1] += 1;
                // } else if (decoded_ints == 4) {
                //     stats.codewords_distr[2] += 1;
                // } else if (decoded_ints == 2) {
                //     stats.codewords_distr[3] += 1;
                // } else if (decoded_ints == 1) {
                //     stats.codewords_distr[4] += 1;
                // }
            }

            return reinterpret_cast<uint8_t const*>(ptr);
        }
    };

    #define CODECS (interpolative)(optpfor)(varintg8iu)(qmx)(vbyte)(u32)(simple16)(streamvbyte)(maskedvbyte)(varintgb)(dint)
}

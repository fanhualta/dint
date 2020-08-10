#define BOOST_TEST_MODULE strict_elias_fano

#include "test_generic_sequence.hpp"

#include "TgcCoding.h"
#include "strict_elias_fano.hpp"
#include "dictionary_types.hpp"
#include <vector>
#include <cstdlib>
#include <iostream>
#include "uniform_partitioned_sequence.hpp"
#include "strict_sequence.hpp"
#include <time.h>
#include <sstream>
#include <string>
#include <index_types.hpp>

BOOST_AUTO_TEST_CASE(strict_elias_fano)
{
    ds2i::global_parameters params;
    using ds2i::indexed_sequence;
    using ds2i::strict_sequence;

    double start, end;
    double cost;
    //uint64_t n = 100000;
    uint64_t max_n = 113597521;

    std::ofstream outFile;
    outFile.open("data.txt", std::ios::out);
    for (uint64_t n = 37620000; n <= max_n ; )
    {
        std::cout << n << std::endl;

        std::string result;
        result += std::to_string(n);
        result += "\t";
        uint64_t universe = uint64_t(2 * n);
        auto seq = random_sequence(universe, n, true);

        UInt32 size = 0;
        UInt64 deltas[10] = { 0 };
        UInt64 base = 0;
        std::vector<UInt8*> buffers;
        for (int i = 0; i < n / 10; i++)
        {
            UInt8* buffer = new UInt8[400];
            buffers.push_back(buffer);
        }

        start = clock();
        TgcCoding::UInt64Encoder encoder;

        for (int i = 0; i < n/10; i++)
        {
            encoder.Clear();
            for (int j = 0; j < 10; j++)
            {
                deltas[j] = seq[i * 10 + j] - seq[i];
            }

            if (i == 0) deltas[0] = seq[0];
            encoder.EncodeTenValues(deltas, true);
            size += (UInt32)encoder.CopyEncodedData(buffers[i]);
        }

        end = clock();
        cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
        std::cout << "encode cost: " << cost << std::endl;

        std::cout << "tgc size:" << size << std::endl;
        result += std::to_string((int)cost);
        result += "\t";
        result += std::to_string(size);
        result += "\t";

        start = clock();
        TgcCoding::UInt64DeltaDecoder decoder;
        UInt64 value = 0;
        for (int i = 0; i < n / 10; i++)
        {
            decoder.Reset(buffers[i] + 2);
            decoder.Decode32BitDelta(value, *(unsigned short*)buffers[i], deltas);

            for (int i = 0; i < 10; i++)
            {
                value += deltas[i];
            }
            //std::cout << value << std::endl;
            value = deltas[9];
//            UInt8* buffer = buffers[i];
//            delete[] buffer;
        }
        end = clock();
        cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
        std::cout << "decode cost: " << cost << std::endl;
        result += std::to_string((int)cost);
        result += "\t";

        std::cout << "compact elias_fano " << std::endl;
        out_test_sequence_ef(ds2i::compact_elias_fano(), params, universe, seq,
                             result);

//        std::cout << "uniform elias_fano " << std::endl;
//        out_test_sequence_ef(ds2i::uniform_partitioned_sequence<>(), params,
//                             universe, seq, result);

//        out_test_sequence_dint<ds2i::single_rectangular_builder, ds2i::opt_dint_single_dict_block>(seq, result);
//        std::cout << "single_dint " << std::endl;
//        out_test_sequence_ef(ds2i::single_dictionary_packed_type(), params, universe, seq, result);

//        std::cout << "partitioned elias_fano " << std::endl;
//        out_test_sequence_ef(ds2i::uniform_partitioned_sequence<strict_sequence>(), params, universe, seq, result);

//        std::cout << result;
        outFile << result << std::endl;

        n = n + 1000000;
    }
    outFile.close();
}

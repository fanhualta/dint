#define BOOST_TEST_MODULE strict_elias_fano

#include "test_generic_sequence.hpp"

#include "TgcCoding.h"
#include "strict_elias_fano.hpp"
#include "dictionary_types.hpp"
#include <vector>
#include <cstdlib>
#include <iostream>
#include "uniform_partitioned_sequence.hpp"
#include <time.h>
#include <sstream>
#include <string>
#include <index_types.hpp>

inline void out_test_sequence_tgc(uint64_t n, std::vector<uint64_t> seq, std::vector<uint64_t> seeks, std::string& result)
{
    int granularity = 4;
    double start, end;
    double cost;
    UInt32 size = 0;
    UInt64 deltas[10] = { 0 };
    std::vector<UInt8*> buffers;
    std::vector<UInt64> skips;
    for (int i = 0; i < n / 10; i++)
    {
        UInt8* buffer = new UInt8[400];
        buffers.push_back(buffer);
    }

    TgcCoding::UInt64Encoder encoder;

    // create indexs
    start = clock();
    int pre = 0, id = 0;
    for (int i = 0; i < n / 10; i++)
    {
        encoder.Clear();
        if(i % granularity == 0)
        {
            skips.push_back(pre);
        }
        for (int j = 0; j < 10; j++)
        {
            deltas[j] = seq[id] - pre;
            pre = seq[id++];
        }

        encoder.EncodeTenValues(deltas, true); // 3 byte tag
        size += (UInt32)encoder.CopyEncodedData(buffers[i]);
    }
    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "encode cost: " << cost << std::endl;

    std::cout << "encode size:" << size + 8 * skips.size() << std::endl;
    result += std::to_string((int)cost);
    result += ",";
    result += std::to_string(size + 8 * skips.size());
    result += ",";

    // decode sequence query
    start = clock();
    TgcCoding::UInt64DeltaDecoder decoder;
    UInt64 base = 0;
    for (int i = 0; i < n / 10; i++)
    {
        decoder.Reset(buffers[i] + 2);
        decoder.Decode32BitDelta(base, *(unsigned short*)buffers[i], deltas);

//            for (int i = 0; i < 10; i++)
//            {
//                std::cout << deltas[i] << " ";
//            }
        base = deltas[9];
    }
//        std::cout << std::endl;
    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "sequence decode cost: " << cost << std::endl;
    result += std::to_string((int)cost);
    result += ",";

    // decode seek query
    start = clock();
    int group = 0, granId = 0, threshold = n / 10 / granularity;
    id = 0;
    pre = -1;
    for(int i = 0; i < seeks.size(); i++)
    {
        uint64_t cur = seeks[i];
        while(granId + 1 < threshold && skips[granId + 1] < cur) {
            granId++;
            group = granId * granularity;
        }
        if(granId != pre)
        {
            base = skips[granId];
            decoder.Reset(buffers[group] + 2);
            decoder.Decode32BitDelta(base, *(unsigned short*)buffers[group], deltas);
            id = 0;
            pre = granId;
        }
        while(deltas[id] < cur){
            id++;
            if(id == 10){
                base = deltas[9];
                decoder.Reset(buffers[++group] + 2);
                decoder.Decode32BitDelta(base, *(unsigned short*)buffers[group], deltas);
                id = 0;
            }
        }
//        std::cout << deltas[id] << " ";
    }
//    std::cout << std::endl;
    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "seek decode cost: " << cost << std::endl;
    result += std::to_string((int)cost);
    result += ",";
}

inline void single_posting_test(uint64_t begin, uint64_t end)
{
    ds2i::global_parameters params;
    using ds2i::indexed_sequence;
    using ds2i::strict_sequence;

    //uint64_t n = 100000;
//    uint64_t max_n = 1000;

    std::ofstream outFile;
    outFile.open("result.csv", std::ios::out);
    outFile << "size,tgc encode time,tgc encode size,tgc sequence decode time,tgc seek decode time,ef encode time,ef encode size,ef sequence decode time,ef seek decode time,partitioned ef encode time,partitioned ef encode size,partitioned ef sequence decode time,partitioned ef seek decode time,dint build time,dint encode time,dint dictionary size,dint encode size,dint sequence decode time,dint seek decode time,packed dint build time,packed dint encode time,packed dint dictionary size,packed dint encode size,packed dint sequence decode time,packed dint seek decode time" << std::endl;
    for (uint64_t n = begin; n <= end; )
    {
        std::cout << n << std::endl;

        std::string result;
        result += std::to_string(n);
        result += ",";
        uint64_t universe = uint64_t(2 * n);
        auto seq = random_sequence(universe, n, true);
        auto seeks = seek_sequence(seq);

        //        for(auto num: seq) {
        //            std::cout << num << " ";
        //        }
        //        std::cout << std::endl;
        //        for(auto num: seeks) {
        //            std::cout << num << " ";
        //        }
        //        std::cout << std::endl;

//        std::cout << "tgc " << std::endl;
//        out_test_sequence_tgc(n, seq, seeks, result);
//
//        std::cout << "compact elias_fano " << std::endl;
//        out_test_sequence_ef(ds2i::compact_elias_fano(), params, universe, seq, seeks,
//                             result);
//
//        std::cout << "partitioned elias_fano " << std::endl;
//        out_test_sequence_ef(ds2i::uniform_partitioned_sequence<>(), params,
//                             universe, seq, seeks, result);

        std::cout << "single_dint " << std::endl;
        typedef ds2i::dict_posting_list<ds2i::single_rectangular_builder::dictionary_type, ds2i::opt_dint_single_dict_block> sequence_type1;
        out_test_sequence_dint(ds2i::single_rectangular_builder(), sequence_type1(), universe, seq, seeks, result);

        std::cout << "single_packed_dint " << std::endl;
        typedef ds2i::dict_posting_list<ds2i::single_packed_builder::dictionary_type , ds2i::opt_dint_single_dict_block> sequence_type2;
        out_test_sequence_dint(ds2i::single_packed_builder(), sequence_type2(), universe, seq, seeks, result);

        std::cout << "multi_packed_dint " << std::endl;
        typedef ds2i::dict_posting_list<ds2i::multi_packed_builder::dictionary_type , ds2i::opt_dint_multi_dict_block> sequence_type3;
        out_test_sequence_dint(ds2i::multi_packed_builder(), sequence_type3(), universe, seq, seeks, result);

        std::cout << result;
        outFile << result << std::endl;

        n += 10000000;
    }
    outFile.close();
}

BOOST_AUTO_TEST_CASE(strict_elias_fano)
{
    single_posting_test(10000000, 100000000);
}

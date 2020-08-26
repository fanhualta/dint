#pragma once

#include "succinct/test_common.hpp"
#include "succinct/bit_vector.hpp"
#include "util.hpp"
#include "../include/ds2i/bitvector_collection.hpp"

std::vector<uint64_t> random_sequence(size_t universe, size_t n,
                                      bool strict = true)
{
    srand(42);
    std::vector<uint64_t> seq;

    uint64_t u = strict ? (universe - n) : universe;
    for (size_t i = 0; i < n; ++i) {
        seq.push_back(rand() % u);
    }
    std::sort(seq.begin(), seq.end());

    if (strict) {
        for (size_t i = 0; i < n; ++i) {
            seq[i] += i;
        }
    }

    return seq;
}

std::vector<std::vector<uint64_t>> multi_random_sequence(size_t term_num, size_t universe, size_t n,
                                      bool strict = true)
{
    std::vector<std::vector<uint64_t>> seqs;
    for(size_t term = 0; term < term_num; term++) {
        srand(42 + term);
        std::vector<uint64_t> seq;

        uint64_t u = strict ? (universe - n) : universe;
        for (size_t i = 0; i < n; ++i) {
            seq.push_back(rand() % u);
        }
        std::sort(seq.begin(), seq.end());

        if (strict) {
            for (size_t i = 0; i < n; ++i) {
                seq[i] += i;
            }
        }
        seqs.push_back(seq);
    }

    return seqs;
}

std::vector<uint64_t> seek_sequence(std::vector<uint64_t> seq)
{
    srand(42);
    std::vector<uint64_t> seeks;

    uint64_t u = seq.size();
    for (size_t i = 0; i < u / 25; ++i) { // /1000
        seeks.push_back(seq[rand() % u]);
    }

    std::sort(seeks.begin(), seeks.end());

    return seeks;
}

std::vector<std::vector<uint64_t>> multi_seek_sequence(std::vector<std::vector<uint64_t>> seqs)
{
    std::vector<std::vector<uint64_t>> seeks;
    for(size_t term = 0 ; term < seqs.size(); term++) {
        srand(42 + term);
        std::vector<uint64_t> seek;

        uint64_t u = seqs[term].size();
        for (size_t i = 0; i < u / 25; ++i) {
            seek.push_back(seqs[term][rand() % u]);
        }

        std::sort(seek.begin(), seek.end());
        seeks.push_back(seek);
    }
    return seeks;
}

template <typename SequenceReader>
void test_move_next(SequenceReader r, std::vector<uint64_t> const& seq)
{
    BOOST_REQUIRE_EQUAL(seq.size(), r.size());
    if (seq.empty()) {
        // just check that move works
        BOOST_REQUIRE_EQUAL(seq.size(), r.move(seq.size()).first);
        return;
    }

    typename SequenceReader::value_type val;

    // test random access and enumeration
    for (uint64_t i = 0; i < seq.size(); ++i) {
        val = r.move(i);
        MY_REQUIRE_EQUAL(i, val.first,
                         "i = " << i);
        MY_REQUIRE_EQUAL(seq[i], val.second,
                         "i = " << i);

        if (i) {
            MY_REQUIRE_EQUAL(seq[i - 1], r.prev_value(),
                             "i = " << i);
        } else {
            MY_REQUIRE_EQUAL(0, r.prev_value(),
                             "i = " << i);
        }
    }
    r.move(seq.size());
    BOOST_REQUIRE_EQUAL(seq.back(), r.prev_value());

    val = r.move(0);
    for (uint64_t i = 0; i < seq.size(); ++i) {
        MY_REQUIRE_EQUAL(seq[i], val.second,
                         "i = " << i);

        if (i) {
            MY_REQUIRE_EQUAL(seq[i - 1], r.prev_value(),
                             "i = " << i);
        } else {
            MY_REQUIRE_EQUAL(0, r.prev_value(),
                             "i = " << i);
        }
        val = r.next();
    }
    BOOST_REQUIRE_EQUAL(r.size(), val.first);
    BOOST_REQUIRE_EQUAL(seq.back(), r.prev_value());

    // test small skips
    for (size_t i = 0; i < seq.size(); ++i) {
        for (size_t skip = 1; skip < seq.size() - i; skip <<= 1) {
            auto rr = r;
            rr.move(i);
            auto val = rr.move(i + skip);
            MY_REQUIRE_EQUAL(i + skip, val.first,
                             "i = " << i << " skip = " << skip);
            MY_REQUIRE_EQUAL(seq[i + skip], val.second,
                             "i = " << i << " skip = " << skip);
        }
    }
}

template <typename SequenceReader>
void test_next_geq(SequenceReader r, std::vector<uint64_t> const& seq)
{
    BOOST_REQUIRE_EQUAL(seq.size(), r.size());
    if (seq.empty()) {
        // just check that next_geq works
        BOOST_REQUIRE_EQUAL(seq.size(), r.next_geq(1).first);
        return;
    }

    typename SequenceReader::value_type val;

    // test successor
    uint64_t last = 0;
    for (size_t i = 0; i < seq.size(); ++i) {
        if (seq[i] == last) continue;

        auto rr = r;
        for (size_t t = 0; t < 10; ++t) {
            uint64_t p = 0;
            switch (i) {
            case 0:
                p = last + 1; break;
            case 1:
                p = seq[i]; break;
            default:
                p = last + 1 + (rand() % (seq[i] - last));
            }

            val = rr.next_geq(p);
            BOOST_REQUIRE_EQUAL(i, val.first);
            MY_REQUIRE_EQUAL(seq[i], val.second,
                             "p = " << p);

            if (val.first) {
                MY_REQUIRE_EQUAL(seq[val.first - 1], rr.prev_value(),
                                 "i = " << i);
            } else {
                MY_REQUIRE_EQUAL(0, rr.prev_value(),
                                 "i = " << i);
            }
        }
        last = seq[i];
    }

    val = r.next_geq(seq.back() + 1);
    BOOST_REQUIRE_EQUAL(r.size(), val.first);
    BOOST_REQUIRE_EQUAL(seq.back(), r.prev_value());

    // check next_geq beyond universe
    val = r.next_geq(2 * seq.back() + 1);
    BOOST_REQUIRE_EQUAL(r.size(), val.first);

    // test small skips
    for (size_t i = 0; i < seq.size(); ++i) {
        for (size_t skip = 1; skip < seq.size() - i; skip <<= 1) {
            size_t exp_pos = i + skip;
            // for weakly monotone sequences, next_at returns the first of the
            // run of equal values
            while ((exp_pos > 0) && seq[exp_pos - 1] == seq[i + skip]) {
                exp_pos -= 1;
            }

            auto rr = r;
            rr.move(i);
            val = rr.next_geq(seq[i + skip]);
            MY_REQUIRE_EQUAL(exp_pos, val.first,
                             "i = " << i << " skip = " << skip
                             << " value expected = " << seq[i + skip]
                             << " got = " << val.second);
            MY_REQUIRE_EQUAL(seq[i + skip], val.second,
                             "i = " << i << " skip = " << skip);
        }
    }
}

// oh, C++
struct no_next_geq_tag {};
struct next_geq_tag : no_next_geq_tag {};

template <typename SequenceReader>
void test_sequence(SequenceReader r, std::vector<uint64_t> const& seq,
                   no_next_geq_tag const&)
{
    test_move_next(r, seq);
}

template <typename SequenceReader>
typename std::enable_if<ds2i::has_next_geq<SequenceReader>::value, void>::type
test_sequence(SequenceReader r, std::vector<uint64_t> const& seq,
              next_geq_tag const&)
{
    test_move_next(r, seq);
    test_next_geq(r, seq);
}

template <typename SequenceReader>
void test_sequence(SequenceReader r, std::vector<uint64_t> const& seq)
{
    test_sequence(r, seq, next_geq_tag());
}

template <typename ParamsType, typename SequenceType>
inline void test_sequence(SequenceType,
                          ParamsType const& params,
                          uint64_t universe,
                          std::vector<uint64_t> const& seq)
{
    succinct::bit_vector_builder bvb;
    SequenceType::write(bvb, seq.begin(), universe, seq.size(), params);
    succinct::bit_vector bv(&bvb);
    typename SequenceType::enumerator r(bv, 0, universe, seq.size(), params);
    test_sequence(r, seq);
}

template <typename ParamsType, typename SequenceType>
inline void out_test_sequence_ef(SequenceType,
                                 ParamsType const& params,
                                 uint64_t universe,
                                 std::vector<uint64_t> const& seq,
                                 std::vector<uint64_t> const& seeks,
                                 std::string& result)
{
    double start, end;
    double cost;

    // build index
    start = clock();
    succinct::bit_vector_builder bvb;
    SequenceType::write(bvb, seq.begin(), universe, seq.size(), params);
    succinct::bit_vector bv(&bvb);
    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "encode cost: " << cost << std::endl;
    std::cout << "size:" << bv.size() << std::endl;
    //std::cout << seq[seq.size() - 1] << std::endl;
    result += std::to_string((int)cost);
    result += ",";
    result += std::to_string(bv.size());
    result += ",";

    start = clock();

    typename SequenceType::enumerator r(bv, 0, universe, seq.size(), params);

    // seq query test
//    test_move_next(r, seq);
    typename SequenceType::enumerator::value_type val;
    val = r.move(0);
    for (uint64_t i = 0; i < seq.size(); ++i) {
//        std::cout << val.second << " ";
        val = r.next();
    }
//    std::cout << std::endl;
    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "seq decode cost: " << cost << std::endl;
    result += std::to_string((int)cost);
    result += ",";


    // seek query test
    start = clock();
    val = r.move(0);
    for (uint64_t i = 0; i < seeks.size(); ++i) {
        val = r.next_geq(seeks[i]);
//        std::cout << val.second << " ";
    }
//    std::cout << std::endl;
    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "seek decode cost: " << cost << std::endl;
    result += std::to_string((int)cost);
    result += ",";
}

template <typename ParamsType, typename SequenceType>
inline void out_test_sequence_multi_ef(SequenceType,
                                 ParamsType const& params,
                                 uint64_t universe,
                                 std::vector<std::vector<uint64_t>> const& seqs,
                                 std::vector<std::vector<uint64_t>> const& seeks,
                                 std::string& result)
{
    double start, end;
    double cost;

    // build index
    start = clock();
    ds2i::bitvector_collection::builder m_sequence_builder(params);
    uint32_t size = 0;
    for(int term = 0; term < seqs.size(); term++) {
        succinct::bit_vector_builder bvb;
        SequenceType::write(bvb, seqs[term].begin(), universe, seqs[term].size(), params);
        m_sequence_builder.append(bvb);
        size += bvb.size();
    }
    ds2i::bitvector_collection m_sequence;
    m_sequence_builder.build(m_sequence);
    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "encode cost: " << cost << std::endl;
    std::cout << "size:" << start << std::endl;
    result += std::to_string((int)cost);
    result += ",";
    result += std::to_string(size);
    result += ",";

    start = clock();

    typename SequenceType::enumerator::value_type val;
    for(int term = 0; term < seqs.size(); term++) {
        typename SequenceType::enumerator r(m_sequence.bits(), m_sequence.get(params, term).position(), universe, seqs[term].size(),
                                            params);

        // seq query test
        //    test_move_next(r, seq);
        val = r.move(0);
        for (uint64_t i = 0; i < seqs[term].size(); ++i) {
//            std::cout << val.second << " ";
            val = r.next();
        }
//        std::cout << std::endl;
    }
    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "seq decode cost: " << cost << std::endl;
    result += std::to_string((int)cost);
    result += ",";


    // seek query test
    start = clock();
    for(int term = 0; term < seqs.size(); term++) {
        typename SequenceType::enumerator r(m_sequence.bits(), m_sequence.get(params, term).position(), universe,
                                            seqs[term].size(), params);
        val = r.move(0);
        for (uint64_t i = 0; i < seeks[term].size(); ++i) {
            val = r.next_geq(seeks[term][i]);
//            std::cout << val.second << " ";
        }
//        std::cout << std::endl;
    }
    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "seek decode cost: " << cost << std::endl;
    result += std::to_string((int)cost);
    result += ",";
}


template <typename DictionaryBuilder, typename CollectionsType>
inline void out_test_sequence_dint(DictionaryBuilder, CollectionsType,
                                   uint64_t universe,
                                   std::vector<uint64_t> const& seq,
                                   std::vector<uint64_t> const& seeks, std::string& result)
{
    typedef typename CollectionsType::document_enumerator document_enumerator;
    using dictionary_type = typename DictionaryBuilder::dictionary_type;
    typename dictionary_type::builder m_docs_dict_builder;

    double start, end;
    double cost;
    start = clock();

    // create dict
    using statistics_type = typename DictionaryBuilder::statistics_type;
    statistics_type stats(seq, true, DictionaryBuilder::filter());
    DictionaryBuilder::build(m_docs_dict_builder, stats);
    m_docs_dict_builder.prepare_for_encoding();
    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "build dictionary cost: " << cost << std::endl;
    m_docs_dict_builder.try_store_to_file("./dint.dictionary");
    std::ifstream ifs("./dint.dictionary");
    ifs.seekg(0, std::ios::end);
    std::cout << "dictionary size: " << ifs.tellg() << std::endl;
    result += std::to_string((int)cost);
    result += ",";

    // build index
    start = clock();
    std::vector<uint8_t> lists;
    CollectionsType::writeOmitFreq(m_docs_dict_builder, lists, seq.size(), seq.begin());
    succinct::mapper::mappable_vector<uint8_t> m_lists;
    m_lists.steal(lists);

    dictionary_type dictionary;
    m_docs_dict_builder.build(dictionary);

    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "encode cost: " << cost << std::endl;
    std::cout << "encode size: " << m_lists.size() << std::endl;
    result += std::to_string((int)cost);
    result += ",";
    result += std::to_string((int)ifs.tellg());
    result += ",";
    result += std::to_string(m_lists.size());
    result += ",";

    // seq query dict
    start = clock();
    document_enumerator enumerator(&dictionary, m_lists.data(), universe, 0);
    std::cout << "blocks num: " <<  enumerator.num_blocks() << std::endl;
    std::cout << "avg blocks size: " << m_lists.size() / enumerator.num_blocks() << std::endl;
    for (uint64_t i = 0; i < seq.size(); ++i) {
//        std::cout << enumerator.docid() << " ";
        enumerator.next();
    }
//    std::cout << std::endl;
    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "sequence decode cost: " << cost << std::endl;
    result += std::to_string((int)cost);
    result += ",";

    // seek query dict
    enumerator.reset();
    start = clock();
    for (uint64_t i = 0; i < seeks.size(); ++i) {
        enumerator.next_geq(seeks[i]);
//        std::cout << enumerator.docid() << " ";
    }
//    std::cout << std::endl;
    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "seek decode cost: " << cost << std::endl;
    result += std::to_string((int)cost);
    result += ",";
}

template <typename DictionaryBuilder, typename CollectionsType>
inline void out_test_sequence_multi_dint(DictionaryBuilder, CollectionsType,
                                   ds2i::global_parameters const& params,
                                   uint64_t universe,
                                   std::vector<std::vector<uint64_t>> const& seqs,
                                   std::vector<std::vector<uint64_t>> const& seeks, std::string& result)
{
    typedef typename CollectionsType::document_enumerator document_enumerator;
    using dictionary_type = typename DictionaryBuilder::dictionary_type;
    typename dictionary_type::builder m_docs_dict_builder;

    double start, end;
    double cost;
    start = clock();

    // create dict
    using statistics_type = typename DictionaryBuilder::statistics_type;
    statistics_type stats(seqs, true, DictionaryBuilder::filter());
    DictionaryBuilder::build(m_docs_dict_builder, stats);
    m_docs_dict_builder.prepare_for_encoding();
    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "build dictionary cost: " << cost << std::endl;
    m_docs_dict_builder.try_store_to_file("./dint.dictionary");
    std::ifstream ifs("./dint.dictionary");
    ifs.seekg(0, std::ios::end);
    std::cout << "dictionary size: " << ifs.tellg() << std::endl;
    result += std::to_string((int)cost);
    result += ",";

    // build index
    /// builder
    start = clock();
    std::vector<uint64_t> m_endpoints;
    m_endpoints.push_back(0);
    std::vector<uint8_t> m_lists;
    dictionary_type dictionary;
    uint64_t size = 0;
    for(int term = 0; term < seqs.size(); term++) {
        std::vector<uint8_t> lists;
        CollectionsType::writeOmitFreq(m_docs_dict_builder, lists, seqs[term].size(),
                                       seqs[term].begin());
        m_lists.insert(m_lists.end(), lists.begin(), lists.end());
        m_endpoints.push_back(m_lists.size());

        size += lists.size();
    }

    /// Index
    succinct::mapper::mappable_vector<uint8_t> index_lists;
    succinct::bit_vector index_endpoints;
    index_lists.steal(m_lists);
    size_t m_size = m_endpoints.size() - 1;
    succinct::bit_vector_builder bvb;
    ds2i::compact_elias_fano::write(bvb, m_endpoints.begin(),
                              index_lists.size(), m_size, params);
    succinct::bit_vector(&bvb).swap(index_endpoints);
    m_docs_dict_builder.build(dictionary);

    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "encode cost: " << cost << std::endl;
    std::cout << "encode size: " << size << std::endl;
    result += std::to_string((int)cost);
    result += ",";
    result += std::to_string((int)ifs.tellg());
    result += ",";
    result += std::to_string(size);
    result += ",";

    // seq query dict
    start = clock();
    size_t block_num = 0;
    ds2i::compact_elias_fano::enumerator endpoints(index_endpoints, 0, index_lists.size(), m_size, params);
    for(int term = 0; term < seqs.size(); term++) {
        document_enumerator enumerator(&dictionary, index_lists.data() + endpoints.move(term).second, universe, term);
        block_num += enumerator.num_blocks();
//        std::cout << "blocks num: " << enumerator.num_blocks() << std::endl;
//        std::cout << "avg blocks size: "
//                  << m_listss[term].size() / enumerator.num_blocks() << std::endl;
        for (uint64_t i = 0; i < seqs[term].size(); ++i) {
//            std::cout << enumerator.docid() << " ";
            enumerator.next();
        }
//        std::cout << std::endl;
    }
    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "avg blocks size: " << index_lists.size() / block_num << std::endl;
    std::cout << "sequence decode cost: " << cost << std::endl;
    result += std::to_string((int)cost);
    result += ",";

    // seek query dict
    start = clock();
    for(int term = 0; term < seqs.size(); term++) {
        document_enumerator enumerator(&dictionary, index_lists.data() + endpoints.move(term).second, universe, term);
        for (uint64_t i = 0; i < seeks[term].size(); ++i) {
            enumerator.next_geq(seeks[term][i]);
//            std::cout << enumerator.docid() << " ";
        }
//        std::cout << std::endl;
    }
    end = clock();
    cost = (end - start) / CLOCKS_PER_SEC * 1000.0;
    std::cout << "seek decode cost: " << cost << std::endl;
    result += std::to_string((int)cost);
    result += ",";
}
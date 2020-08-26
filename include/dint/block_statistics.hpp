#pragma once

#include "dint_configuration.hpp"
#include "binary_collection.hpp"
#include "hash_utils.hpp"
#include "util.hpp"
#include "statistics_collectors.hpp"

#include <boost/filesystem.hpp>
#include <boost/progress.hpp>

#include <unordered_map>

namespace ds2i {

template <typename Collector>
struct block_statistics { // 用于生成dic的统计信息
    static_assert(is_power_of_two(Collector::max_block_size), "");

    static std::string type() {
        return "block_statistics-" + std::to_string(Collector::max_block_size) +
               "-" + Collector::type();
    }

    template <typename Filter>
    static block_statistics create_or_load(std::string prefix_name,
                                           data_type dt, Filter const& filter) {
        std::string file_name = prefix_name + extension(dt);
        using namespace boost::filesystem;
        path p(file_name);
        std::string block_stats_filename =
            "./" + p.filename().string() + "." + type();
        std::cout << block_stats_filename << std::endl;

        if (boost::filesystem::exists(block_stats_filename)) {
            return block_statistics(block_stats_filename);
        }

        binary_collection input(file_name.c_str());
        bool compute_gaps = dt == data_type::docs;
        block_statistics stats(input, compute_gaps, filter);
        stats.try_to_store(block_stats_filename);
        return stats;
    }

    template <typename Filter>
    static block_statistics create_or_load(std::vector<std::vector<uint64_t>> const& lists,
                                           data_type dt, Filter const& filter) {
        bool compute_gaps = dt == data_type::docs;
        block_statistics stats(lists, compute_gaps, filter);
        return stats;
    }

    template <typename Filter>
    static block_statistics create_or_load(std::vector<uint64_t> const& list,
                                           data_type dt, Filter const& filter) {
        bool compute_gaps = dt == data_type::docs;
        block_statistics stats(list, compute_gaps, filter);
        return stats;
    }

    template <typename Filter>
    block_statistics(binary_collection& input, bool compute_gaps,
                     Filter const& filter) {
        logger() << "creating block stats (type = " << type() << ")"
                 << std::endl;

        map_type block_map;  // 存的统计结果, key是采样的哈希值，value是频率和数据
        boost::progress_display progress(input.num_postings() -
                                         (compute_gaps ? 2 : 0));  // 一共有多少个uint_32
        total_integers = 0; // 统计一共有多少个doc，含重复
        std::vector<uint32_t> buf; // 记录每一个posting的delta结果

        auto it = input.begin();
        if (compute_gaps) {
            ++it;  // skip first singleton sequence, containing # of docs
        }

        // 进行统计操作
        for (; it != input.end(); ++it) {
            auto const& list = *it;
            size_t n = list.size();
            if (n > constants::min_size) {
                total_integers += n;
                progress += n + 1;
                buf.reserve(n);
                uint32_t prev = compute_gaps ? -1 : 0;
                auto it = list.begin();
                for (uint32_t i = 0; i < n; ++i, ++it) {
                    buf.push_back(*it - prev - 1); // delta是后一个减去前一个再减一
                    if (compute_gaps) {
                        prev = *it;
                    }
                }
                Collector::collect(buf, block_map);  // 对统计出来的数据进行更新，按粒度进行采样并更新block_map
                buf.clear();
            }
        }

        // 根据统计出来的值进行字典编码
        logger() << "selecting entries..." << std::endl;
        uint64_t num_singletons = 0;  // 单元素的个数，因为在l = 16时，采样粒度为{1,2,4,8,16}
        blocks.resize(1);
        blocks.front().reserve(block_map.size());

        for (auto& pair : block_map) {
            auto& freq_block = pair.second;
            if (freq_block.data.size() == 1) {
                ++num_singletons;
            }
            if (filter(freq_block, total_integers) or  // 当这个item项满足filter的过滤条件或者item项的长度为1时，进行更新
                freq_block.data.size() == 1) {
                block_type block;
                block.freq = freq_block.freq;
                block.data.swap(freq_block.data);
                blocks.front().push_back(std::move(block));
            }
        }

        logger() << "DONE" << std::endl;

        freq_length_sorter sorter;
        logger() << "sorting..." << std::endl;
        logger() << num_singletons << " singletons" << std::endl;
        std::sort(blocks.front().begin(), blocks.front().end(), sorter);

        logger() << "DONE" << std::endl;
    }

    template <typename Filter>
    block_statistics(std::vector<uint64_t> const& list, bool compute_gaps,
                     Filter const& filter) {
//        logger() << "creating block stats (type = " << type() << ")"
//                 << std::endl;

        map_type block_map;

        total_integers = 0;
        std::vector<uint32_t> buf;

        size_t n = list.size();
        if (n > constants::min_size) {
            total_integers += n;
            buf.reserve(n);
            uint32_t prev = compute_gaps ? -1 : 0;
            auto it = list.begin();
            for (uint32_t i = 0; i < n; ++i, ++it) {
                buf.push_back(*it - prev - 1);
                if (compute_gaps) {
                    prev = *it;
                }
            }
            Collector::collect(buf, block_map);  // 对统计出来的数据进行更新
            buf.clear();
        }

        // 根据统计出来的值进行字典编码
//        logger() << "selecting entries..." << std::endl;
        uint64_t num_singletons = 0;
        blocks.resize(1);
        blocks.front().reserve(block_map.size());

        for (auto& pair : block_map) {
            auto& freq_block = pair.second;
            if (freq_block.data.size() == 1) {
                ++num_singletons;
            }
            if (filter(freq_block, total_integers) or
                freq_block.data.size() == 1) {
                block_type block;
                block.freq = freq_block.freq;
                block.data.swap(freq_block.data);
                blocks.front().push_back(std::move(block));
            }
        }

//        logger() << "DONE" << std::endl;

        freq_length_sorter sorter;
//        logger() << "sorting..." << std::endl;
//        logger() << num_singletons << " singletons" << std::endl;
        std::sort(blocks.front().begin(), blocks.front().end(), sorter);

//        logger() << "DONE" << std::endl;
    }


    template <typename Filter>
    block_statistics(std::vector<std::vector<uint64_t>> const& lists, bool compute_gaps,
                     Filter const& filter) {
//        logger() << "creating block stats (type = " << type() << ")"
//                 << std::endl;

        map_type block_map;

        total_integers = 0;
        std::vector<uint32_t> buf;

        for(std::vector<uint64_t> list: lists) {
            size_t n = list.size();
            if (n > constants::min_size) {
                total_integers += n;
                buf.reserve(n);
                uint32_t prev = compute_gaps ? -1 : 0;
                auto it = list.begin();
                for (uint32_t i = 0; i < n; ++i, ++it) {
                    buf.push_back(*it - prev - 1);
                    if (compute_gaps) {
                        prev = *it;
                    }
                }
                Collector::collect(buf, block_map);  // 对统计出来的数据进行更新
                buf.clear();
            }
        }

        // 根据统计出来的值进行字典编码
//        logger() << "selecting entries..." << std::endl;
        uint64_t num_singletons = 0;
        blocks.resize(1);
        blocks.front().reserve(block_map.size());

        for (auto& pair : block_map) {
            auto& freq_block = pair.second;
            if (freq_block.data.size() == 1) {
                ++num_singletons;
            }
            if (filter(freq_block, total_integers) or
                freq_block.data.size() == 1) {
                block_type block;
                block.freq = freq_block.freq;
                block.data.swap(freq_block.data);
                blocks.front().push_back(std::move(block));
            }
        }

//        logger() << "DONE" << std::endl;

        freq_length_sorter sorter;
//        logger() << "sorting..." << std::endl;
//        logger() << num_singletons << " singletons" << std::endl;
        std::sort(blocks.front().begin(), blocks.front().end(), sorter);

//        logger() << "DONE" << std::endl;
    }



    block_statistics(std::string file_name) {
        std::ifstream in(file_name.c_str());
        std::streamsize bytes = sizeof(uint32_t);
        uint32_t num_blocks, size, freq;
        in.read(reinterpret_cast<char*>(&total_integers), sizeof(uint64_t));
        in.read(reinterpret_cast<char*>(&num_blocks), bytes);
        logger() << "reading block stats (num_blocks = " << num_blocks << ")"
                 << std::endl;
        blocks.resize(1);

        // NOTE: load only the needed entries
        num_blocks = std::min<uint32_t>(constants::num_entries, num_blocks);

        blocks.front().reserve(num_blocks);
        uint32_t num_singletons = 0;
        for (uint32_t i = 0; i < num_blocks; ++i) {
            in.read(reinterpret_cast<char*>(&size), bytes);
            in.read(reinterpret_cast<char*>(&freq), bytes);
            block_type block;
            block.freq = freq;
            block.data.resize(size);
            in.read(reinterpret_cast<char*>(block.data.data()), size * bytes);
            if (size == 1) {
                ++num_singletons;
            }
            blocks.front().push_back(std::move(block));
        }
        logger() << "\t" << num_singletons << " singletons" << std::endl;
        logger() << "DONE" << std::endl;
    }

    void try_to_store(std::string file_name) {
        std::ofstream out(file_name.c_str());
        if (out) {
            logger() << "storing stats to disk..." << std::endl;
            out.write(reinterpret_cast<char const*>(&total_integers),
                      sizeof(uint64_t));
            uint32_t num_blocks = blocks.front().size();
            std::streamsize bytes = sizeof(uint32_t);
            out.write(reinterpret_cast<char const*>(&num_blocks), bytes);
            for (auto const& block : blocks.front()) {
                uint32_t size = block.data.size();
                uint32_t freq = block.freq;
                out.write(reinterpret_cast<char const*>(&size), bytes);
                out.write(reinterpret_cast<char const*>(&freq), bytes);
                out.write(reinterpret_cast<char const*>(block.data.data()),
                          size * bytes);
            }
            out.close();
        } else {
            logger() << "Cannot write block statistics to disk. Collection "
                        "directory not writeable."
                     << std::endl;
        }
    }

    uint64_t total_integers; // 所有的term里的posting doc数之和
    std::vector<std::vector<block_type>> blocks;
};

template <typename Collector>
struct block_multi_statistics {
    static_assert(is_power_of_two(Collector::max_block_size), "");

    static std::string type() {
        return "block_multi_statistics-" +
               std::to_string(Collector::max_block_size) + "-" +
               Collector::type() + ".multi";
    }

    template <typename Filter>
    static block_multi_statistics create_or_load(std::string prefix_name,
                                                 data_type dt,
                                                 Filter const& filter) {
        std::string file_name = prefix_name + extension(dt);
        using namespace boost::filesystem;
        path p(file_name);
        std::string block_stats_filename =
            "./" + p.filename().string() + "." + type();

        if (boost::filesystem::exists(block_stats_filename)) {
            return block_multi_statistics(block_stats_filename);
        }

        binary_collection input(file_name.c_str());
        bool compute_gaps = dt == data_type::docs;
        block_multi_statistics stats(input, compute_gaps, filter);
        stats.try_to_store(block_stats_filename);
        return stats;
    }


    template <typename Filter>
    static block_multi_statistics create_or_load(std::vector<uint64_t> const& list,
                                           data_type dt, Filter const& filter) {
        using namespace boost::filesystem;

        bool compute_gaps = dt == data_type::docs;
//        block_multi_statistics stats(list, compute_gaps, filter);
        return nullptr; // TODO
    }

    template <typename Filter>
    block_multi_statistics(binary_collection& input, bool compute_gaps,
                           Filter const& filter) {
        logger() << "creating block stats (type = " << type() << ")"
                 << std::endl;
        logger() << "using " << constants::num_selectors << " contexts"
                 << std::endl;
        if (constants::context == constants::block_selector::max) {
            logger() << "using context MAX" << std::endl;
        } else {
            throw std::runtime_error("Unknown context.");
        }

        std::vector<map_type> block_maps(constants::num_selectors);
        boost::progress_display progress(input.num_postings() -
                                         (compute_gaps ? 2 : 0));
        total_integers = 0;
        std::vector<uint32_t> buf;

        auto it = input.begin();
        if (compute_gaps) {
            ++it;  // skip first singleton sequence, containing # of docs
        }

        for (; it != input.end(); ++it) {
            auto const& list = *it;
            size_t n = list.size();
            if (n > constants::min_size) {
                total_integers += n;
                progress += n + 1;
                buf.reserve(n);
                uint32_t prev = compute_gaps ? -1 : 0;
                auto it = list.begin();
                for (uint32_t i = 0; i < n; ++i, ++it) {
                    buf.push_back(*it - prev - 1);
                    if (compute_gaps) {
                        prev = *it;
                    }
                }
                Collector::collect(buf, block_maps);
                buf.clear();
            }
        }

        logger() << "selecting entries..." << std::endl;
        std::vector<uint32_t> num_singletons(constants::num_selectors, 0);
        blocks.resize(constants::num_selectors);
        for (int s = 0; s != constants::num_selectors; ++s) {
            blocks[s].reserve(block_maps[s].size());
        }

        for (int s = 0; s != constants::num_selectors; ++s) {
            auto& block_map = block_maps[s];
            for (auto& pair : block_map) {
                auto& freq_block = pair.second;
                if (freq_block.data.size() == 1) {
                    ++num_singletons[s];
                }
                if (filter(freq_block, total_integers) or
                    freq_block.data.size() == 1) {
                    block_type block;
                    block.freq = freq_block.freq;
                    block.data.swap(freq_block.data);
                    blocks[s].push_back(std::move(block));
                }
            }
        }

        logger() << "DONE" << std::endl;

        freq_length_sorter sorter;
        logger() << "sorting..." << std::endl;
        for (int s = 0; s != constants::num_selectors; ++s) {
            logger() << num_singletons[s]
                     << " singletons for blocks of context "
                     << constants::selector_codes[s] << std::endl;
            logger() << "\tsaved " << blocks[s].size() << " blocks"
                     << std::endl;
            std::sort(blocks[s].begin(), blocks[s].end(), sorter);
        }

        logger() << "DONE" << std::endl;
    }

    template <typename Filter>
    block_multi_statistics(std::vector<uint64_t> const& list, bool compute_gaps,
                           Filter const& filter) {
        if (constants::context == constants::block_selector::max) {
            logger() << "using context MAX" << std::endl;
        } else {
            throw std::runtime_error("Unknown context.");
        }

        std::vector<map_type> block_maps(constants::num_selectors);

        total_integers = 0;
        std::vector<uint32_t> buf;

        size_t n = list.size();
        if (n > constants::min_size) {
            total_integers += n;
            buf.reserve(n);
            uint32_t prev = compute_gaps ? -1 : 0;
            auto it = list.begin();
            for (uint32_t i = 0; i < n; ++i, ++it) {
                buf.push_back(*it - prev - 1);
                if (compute_gaps) {
                    prev = *it;
                }
            }
            Collector::collect(buf, block_maps);
            buf.clear();
        }

        logger() << "selecting entries..." << std::endl;
        std::vector<uint32_t> num_singletons(constants::num_selectors, 0);
        blocks.resize(constants::num_selectors);
        for (int s = 0; s != constants::num_selectors; ++s) {
            blocks[s].reserve(block_maps[s].size());
        }

        for (int s = 0; s != constants::num_selectors; ++s) {
            auto& block_map = block_maps[s];
            for (auto& pair : block_map) {
                auto& freq_block = pair.second;
                if (freq_block.data.size() == 1) {
                    ++num_singletons[s];
                }
                if (filter(freq_block, total_integers) or
                    freq_block.data.size() == 1) {
                    block_type block;
                    block.freq = freq_block.freq;
                    block.data.swap(freq_block.data);
                    blocks[s].push_back(std::move(block));
                }
            }
        }
        freq_length_sorter sorter;
        for (int s = 0; s != constants::num_selectors; ++s) {
            std::sort(blocks[s].begin(), blocks[s].end(), sorter);
        }
    }


    template <typename Filter>
    block_multi_statistics(std::vector<std::vector<uint64_t>> const& lists, bool compute_gaps,
                           Filter const& filter) {
        if (constants::context == constants::block_selector::max) {
            logger() << "using context MAX" << std::endl;
        } else {
            throw std::runtime_error("Unknown context.");
        }

        std::vector<map_type> block_maps(constants::num_selectors);

        total_integers = 0;
        std::vector<uint32_t> buf;

        for (auto const& list: lists) {
            size_t n = list.size();
            if (n > constants::min_size) {
                total_integers += n;
                buf.reserve(n);
                uint32_t prev = compute_gaps ? -1 : 0;
                auto it = list.begin();
                for (uint32_t i = 0; i < n; ++i, ++it) {
                    buf.push_back(*it - prev - 1);
                    if (compute_gaps) {
                        prev = *it;
                    }
                }
                Collector::collect(buf, block_maps);
                buf.clear();
            }
        }

        std::vector<uint32_t> num_singletons(constants::num_selectors, 0);
        blocks.resize(constants::num_selectors);
        for (int s = 0; s != constants::num_selectors; ++s) {
            blocks[s].reserve(block_maps[s].size());
        }

        for (int s = 0; s != constants::num_selectors; ++s) {
            auto& block_map = block_maps[s];
            for (auto& pair : block_map) {
                auto& freq_block = pair.second;
                if (freq_block.data.size() == 1) {
                    ++num_singletons[s];
                }
                if (filter(freq_block, total_integers) or
                    freq_block.data.size() == 1) {
                    block_type block;
                    block.freq = freq_block.freq;
                    block.data.swap(freq_block.data);
                    blocks[s].push_back(std::move(block));
                }
            }
        }
        freq_length_sorter sorter;
        for (int s = 0; s != constants::num_selectors; ++s) {
            std::sort(blocks[s].begin(), blocks[s].end(), sorter);
        }
    }

    block_multi_statistics(std::string file_name) {
        std::ifstream in(file_name.c_str());
        std::streamsize bytes = sizeof(uint32_t);
        uint32_t num_blocks, size, freq;
        in.read(reinterpret_cast<char*>(&total_integers), sizeof(uint64_t));
        std::cout << "total_integers " << total_integers << std::endl;
        blocks.resize(constants::num_selectors);

        for (int s = 0; s != constants::num_selectors; ++s) {
            in.read(reinterpret_cast<char*>(&num_blocks), bytes);
            logger() << "reading block stats for context "
                     << constants::selector_codes[s]
                     << " (num_blocks = " << num_blocks << ")" << std::endl;

            // TODO
            // // NOTE: load only the needed entries
            // num_blocks = std::min<uint32_t>(constants::num_entries,
            // num_blocks);

            blocks[s].reserve(num_blocks);
            uint32_t num_singletons = 0;
            for (uint32_t i = 0; i < num_blocks; ++i) {
                in.read(reinterpret_cast<char*>(&size), bytes);
                in.read(reinterpret_cast<char*>(&freq), bytes);
                block_type block;
                block.freq = freq;
                block.data.resize(size);
                in.read(reinterpret_cast<char*>(block.data.data()),
                        size * bytes);
                if (size == 1) {
                    ++num_singletons;
                }
                blocks[s].push_back(std::move(block));
            }
            logger() << "\t" << num_singletons << " singletons" << std::endl;
        }

        logger() << "DONE" << std::endl;
    }

    void try_to_store(std::string file_name) {
        std::ofstream out(file_name.c_str());
        if (out) {
            logger() << "storing stats to disk..." << std::endl;
            out.write(reinterpret_cast<char const*>(&total_integers),
                      sizeof(uint64_t));

            std::streamsize bytes = sizeof(uint32_t);
            for (int s = 0; s != constants::num_selectors; ++s) {
                uint32_t num_blocks = blocks[s].size();
                out.write(reinterpret_cast<char const*>(&num_blocks), bytes);
                for (auto const& block : blocks[s]) {
                    uint32_t size = block.data.size();
                    uint32_t freq = block.freq;
                    out.write(reinterpret_cast<char const*>(&size), bytes);
                    out.write(reinterpret_cast<char const*>(&freq), bytes);
                    out.write(reinterpret_cast<char const*>(block.data.data()),
                              size * bytes);
                }
                logger() << "written statistics for context "
                         << constants::selector_codes[s] << std::endl;
            }

            out.close();
        } else {
            logger() << "Cannot write block statistics to disk. Collection "
                        "directory not writeable."
                     << std::endl;
        }
    }

    uint64_t total_integers;
    std::vector<std::vector<block_type>> blocks;
};
}  // namespace ds2i

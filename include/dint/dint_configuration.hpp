#pragma once

#include <cmath>
#include <limits>

#define EXCEPTIONS 2 // 这个应该指的是不存在于字典的数
#define INF std::numeric_limits<uint32_t>::max()

namespace ds2i {

namespace constants {

enum block_selector {
    max = 0
    // median = 1,
    // mode = 2
};

static const int context = block_selector::max;
static const uint32_t num_selectors = 6;
static const uint32_t selector_codes[] = {0, 1, 2, 3, 4, 5};

// b = 16, l = 16
static const uint32_t max_entry_size = 16;
static const uint32_t target_sizes[] = {16, 8, 4, 2, 1};  // 采样率，按照相应的粒度分别采样
static const uint32_t num_entries = 65536;
static const uint32_t log2_num_entries = 16;
static const uint32_t num_target_sizes = std::log2(max_entry_size) + 1; // 一共有多少个采样率
}  // namespace constants
}  // namespace ds2i

#pragma once

#include <cstdint>
#include <limits>
#include <random>

namespace tce {

/**
 * 单状态伪随机（SplitMix64 变体），64 位状态便于写入存档 JSON/二进制。
 * 满足 UniformRandomBitGenerator，可与 std::shuffle / uniform_int_distribution 配合。
 * 读档后 set_state 再继续抽牌/发奖，即可用 SL 复现同一随机序列。
 */
class RunRng {
public:
    using result_type = uint64_t;

    static constexpr result_type min() { return std::numeric_limits<result_type>::min(); }
    static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }

    explicit RunRng(uint64_t seed = 0x9E3779B97F4A7C15ULL) { seed_u64(seed); }

    void seed_u64(uint64_t s) {
        state_ = (s == 0) ? 0x9E3779B97F4A7C15ULL : s;
    }

    uint64_t get_state() const { return state_; }
    void     set_state(uint64_t s) { seed_u64(s); }

    result_type operator()() { return next_u64(); }

    /** 闭区间 [lo, hi] */
    int uniform_int(int lo, int hi) {
        if (hi <= lo) return lo;
        std::uniform_int_distribution<int> d(lo, hi);
        return d(*this);
    }

    /** 闭区间 [lo, hi] */
    size_t uniform_size(size_t lo, size_t hi) {
        if (hi <= lo) return lo;
        std::uniform_int_distribution<size_t> d(lo, hi);
        return d(*this);
    }

private:
    uint64_t state_;

    uint64_t next_u64() {
        uint64_t z = (state_ += 0x9e3779b97f4a7c15ULL);
        z        = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z        = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }
};

} // namespace tce

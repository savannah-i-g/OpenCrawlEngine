#pragma once
// Deterministic dice and the attribute modifier shared across the rules.

#include <cstdint>
#include <random>

namespace oce {

// A seedable random source. Seed it for reproducible play and tests; the rules
// take an Rng by reference so nothing reaches for a global generator.
class Rng {
public:
    explicit Rng(uint64_t seed = 0u) : engine_(static_cast<std::mt19937::result_type>(seed)) {}

    // Uniform integer in [lo, hi] inclusive (bounds are swapped if reversed).
    int between(int lo, int hi) {
        if (hi < lo) {
            const int tmp = lo;
            lo = hi;
            hi = tmp;
        }
        std::uniform_int_distribution<int> dist(lo, hi);
        return dist(engine_);
    }

    // Sum of `n` dice, each in [1, sides].
    int roll(int n, int sides) {
        int total = 0;
        for (int i = 0; i < n; ++i) {
            total += between(1, sides);
        }
        return total;
    }

private:
    std::mt19937 engine_;
};

// D&D-style attribute modifier: floor((value - 10) / 2), correct for negatives.
inline int modifier(int attribute_value) {
    const int v = attribute_value - 10;
    return (v >= 0) ? (v / 2) : -(((-v) + 1) / 2);
}

} // namespace oce

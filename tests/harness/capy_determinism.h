#ifndef CAPY_DETERMINISM_H
#define CAPY_DETERMINISM_H

/*
 * Deterministic primitives for CapyBrowser host tests.
 *
 * - capy_rng:   a seeded splitmix64 PRNG. Same seed -> same sequence.
 * - capy_clock: a monotonic clock that only advances by explicit ticks, so
 *   parse-time limits can be exercised without depending on wall time.
 *
 * No global state, no real randomness, no real clock.
 */

#include <stdint.h>

struct capy_rng {
  uint64_t state;
};

static inline void capy_rng_seed(struct capy_rng *rng, uint64_t seed) {
  rng->state = seed;
}

static inline uint64_t capy_rng_next_u64(struct capy_rng *rng) {
  uint64_t z;
  rng->state += 0x9E3779B97F4A7C15u;
  z = rng->state;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9u;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBu;
  return z ^ (z >> 31);
}

struct capy_clock {
  uint64_t now_ns;
};

static inline void capy_clock_init(struct capy_clock *clk, uint64_t start_ns) {
  clk->now_ns = start_ns;
}

static inline uint64_t capy_clock_now_ns(const struct capy_clock *clk) {
  return clk->now_ns;
}

static inline void capy_clock_advance_ns(struct capy_clock *clk,
                                         uint64_t delta_ns) {
  clk->now_ns += delta_ns;
}

#endif /* CAPY_DETERMINISM_H */

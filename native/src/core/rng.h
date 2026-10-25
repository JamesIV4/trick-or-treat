#pragma once
// PCG32 random generator plus seed hashing. Used everywhere instead of Godot's global random so a
// seed always produces the same neighborhood on every platform.

#include <cstdint>
#include <vector>

namespace tot {

inline uint64_t hash64(uint64_t x) {
	x ^= x >> 33;
	x *= 0xff51afd7ed558ccdULL;
	x ^= x >> 33;
	x *= 0xc4ceb9fe1a85ec53ULL;
	x ^= x >> 33;
	return x;
}

// Seed tree: seed -> stage -> lot -> zone. Re-rolling one branch never disturbs its siblings.
inline uint64_t sub_seed(uint64_t seed, uint64_t a, uint64_t b = 0, uint64_t c = 0) {
	return hash64(seed ^ hash64(a * 0x9E3779B97F4A7C15ULL + hash64(b * 0xC2B2AE3D27D4EB4FULL + c + 1)));
}

class Rng {
public:
	explicit Rng(uint64_t seed = 0x853c49e6748fea9bULL, uint64_t seq = 0xda3e39cb94b95bdbULL) {
		state_ = 0;
		inc_ = (seq << 1u) | 1u;
		next();
		state_ += seed;
		next();
	}

	uint32_t next() {
		uint64_t old = state_;
		state_ = old * 6364136223846793005ULL + inc_;
		uint32_t xorshifted = uint32_t(((old >> 18u) ^ old) >> 27u);
		uint32_t rot = uint32_t(old >> 59u);
		return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
	}

	float f() { return float(next() >> 8) * (1.0f / 16777216.0f); }
	float range(float a, float b) { return a + (b - a) * f(); }
	// Inclusive integer range.
	int irange(int a, int b) { return b <= a ? a : a + int(next() % uint32_t(b - a + 1)); }
	bool chance(float p) { return f() < p; }
	float sign() { return chance(0.5f) ? 1.0f : -1.0f; }

	template <class T>
	const T &pick(const std::vector<T> &v) { return v[next() % v.size()]; }

	template <class T>
	void shuffle(std::vector<T> &v) {
		for (size_t i = v.size(); i > 1; --i) {
			size_t j = next() % i;
			std::swap(v[i - 1], v[j]);
		}
	}

	int weighted(const std::vector<float> &w) {
		float total = 0.0f;
		for (float x : w) {
			total += x;
		}
		if (total <= 0.0f) {
			return 0;
		}
		float r = f() * total;
		for (size_t i = 0; i < w.size(); ++i) {
			r -= w[i];
			if (r <= 0.0f) {
				return int(i);
			}
		}
		return int(w.size()) - 1;
	}

private:
	uint64_t state_;
	uint64_t inc_;
};

} // namespace tot

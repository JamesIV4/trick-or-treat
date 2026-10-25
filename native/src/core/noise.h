#pragma once
// Deterministic 2D gradient noise and fractal sums for terrain and scatter density.

#include <cmath>
#include <cstdint>

#include "core/rng.h"

namespace tot::noise {

inline float fade(float t) {
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline float grad(uint64_t seed, int ix, int iz, float dx, float dz) {
	const uint64_t h = hash64(seed ^ (uint64_t(uint32_t(ix)) * 0x9E3779B97F4A7C15ull) ^ (uint64_t(uint32_t(iz)) * 0xC2B2AE3D27D4EB4Full));
	const float a = float(h % 4096) / 4096.0f * 6.2831853f;
	return std::cos(a) * dx + std::sin(a) * dz;
}

// Gradient noise in roughly [-0.7, 0.7].
inline float perlin(uint64_t seed, float x, float z) {
	const int ix = int(std::floor(x)), iz = int(std::floor(z));
	const float fx = x - float(ix), fz = z - float(iz);
	const float u = fade(fx), v = fade(fz);
	const float n00 = grad(seed, ix, iz, fx, fz);
	const float n10 = grad(seed, ix + 1, iz, fx - 1.0f, fz);
	const float n01 = grad(seed, ix, iz + 1, fx, fz - 1.0f);
	const float n11 = grad(seed, ix + 1, iz + 1, fx - 1.0f, fz - 1.0f);
	const float a = n00 + (n10 - n00) * u;
	const float b = n01 + (n11 - n01) * u;
	return a + (b - a) * v;
}

// Fractal sum normalised to roughly [-1, 1].
inline float fbm(uint64_t seed, float x, float z, int octaves, float lacunarity = 2.0f, float gain = 0.5f) {
	float sum = 0.0f, amp = 1.0f, norm = 0.0f, f = 1.0f;
	for (int o = 0; o < octaves; ++o) {
		sum += perlin(seed + uint64_t(o) * 7919u, x * f, z * f) * amp;
		norm += amp;
		amp *= gain;
		f *= lacunarity;
	}
	return sum / norm * 1.45f;
}

} // namespace tot::noise

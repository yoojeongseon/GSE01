#pragma once
#include <cstdint>

namespace WorldGeneration
{
    constexpr int ChunkSize = 8;

    // Version 1: changing this changes existing procedural discoveries.
    inline std::uint64_t Hash(std::int64_t x, std::int64_t y)
    {
        std::uint64_t v = static_cast<std::uint64_t>(x) * 0x9E3779B185EBCA87ULL;
        v ^= static_cast<std::uint64_t>(y) * 0xC2B2AE3D27D4EB4FULL;
        v ^= 20260908ULL;
        v ^= v >> 30;
        v *= 0xBF58476D1CE4E5B9ULL;
        v ^= v >> 27;
        v *= 0x94D049BB133111EBULL;
        return v ^ (v >> 31);
    }

    inline bool Road(std::int64_t x, std::int64_t y)
    {
        return x == 0 || y == 0;
    }
} // namespace WorldGeneration

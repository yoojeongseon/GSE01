#pragma once
#include <array>
#include <cstdint>
#include <string>

// Versioned, bounded canonical geometry. Screen positions are never cache keys.
struct PrimitiveMeshCache
{
    static constexpr std::uint32_t Version = 1;
    static constexpr std::uint32_t Slots = 48;
    static constexpr std::uint32_t MeshCount = 3;

    struct Vertex
    {
        float x, y;
    };

    std::array<Vertex, MeshCount * Slots> vertices{};
    std::array<std::uint32_t, MeshCount> counts{3, 6, 48};
    std::string status;
    std::uint32_t generated = 0;
    std::uint32_t loaded = 0;

    static PrimitiveMeshCache LoadOrCreate();
    bool Read(const std::wstring& path);
    bool Write(const std::wstring& path) const;
    void Generate();
};

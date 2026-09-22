#include "stdafx.h"
#include "PrimitiveMeshCache.h"
#include "RuntimeFiles.h"
#include <cmath>
#include <fstream>
#include <iostream>

namespace
{
    constexpr std::uint32_t Magic = 0x314D5347;

    std::uint32_t Checksum(const PrimitiveMeshCache& cache)
    {
        std::uint32_t hash = 2166136261u;
        const auto* bytes = reinterpret_cast<const unsigned char*>(cache.vertices.data());
        for (size_t i = 0; i < sizeof(cache.vertices); ++i)
        {
            hash = (hash ^ bytes[i]) * 16777619u;
        }
        return hash;
    }
} // namespace

bool PrimitiveMeshCache::Read(const std::wstring& path)
{
    static_assert(sizeof(Vertex) == sizeof(float) * 2, "Cache layout changed; bump version");
    std::ifstream file(path.c_str(), std::ios::binary);
    std::array<std::uint32_t, 8> header{};
    if (!file.read(reinterpret_cast<char*>(header.data()), sizeof(header)) || header[0] != Magic ||
        header[1] != Version || header[2] != Slots || header[3] != MeshCount || header[4] != 3 ||
        header[5] != 6 || header[6] != 48 ||
        !file.read(reinterpret_cast<char*>(vertices.data()), sizeof(vertices)) ||
        file.peek() != std::char_traits<char>::eof() || header[7] != Checksum(*this))
    {
        return false;
    }
    for (const auto& vertex : vertices)
    {
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || vertex.x < 0 || vertex.x > 1 ||
            vertex.y < 0 || vertex.y > 1)
        {
            return false;
        }
    }
    // Padding must remain degenerate when unlike meshes share a draw.
    for (size_t mesh = 0; mesh < MeshCount; ++mesh)
    {
        for (size_t i = counts[mesh]; i < Slots; ++i)
        {
            const auto& vertex = vertices[mesh * Slots + i];
            if (vertex.x != 0 || vertex.y != 0)
            {
                return false;
            }
        }
    }
    return true;
}

bool PrimitiveMeshCache::Write(const std::wstring& path) const
{
    std::wstring temp = path + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
    std::ofstream file(temp.c_str(), std::ios::binary | std::ios::trunc);
    const std::array<std::uint32_t, 8> header{
        Magic, Version, Slots, MeshCount, counts[0], counts[1], counts[2], Checksum(*this)};
    file.write(reinterpret_cast<const char*>(header.data()), sizeof(header));
    file.write(reinterpret_cast<const char*>(vertices.data()), sizeof(vertices));
    file.flush();
    bool ok = file.good();
    file.close();
    ok = ok && !file.fail();
    if (ok &&
        MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        return true;
    }
    DeleteFileW(temp.c_str());
    return false;
}

void PrimitiveMeshCache::Generate()
{
    vertices.fill({0, 0});
    vertices[0] = {0, 0};
    vertices[1] = {1, 0};
    vertices[2] = {1, 1};
    const Vertex quad[] = {{0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1}};
    for (size_t i = 0; i < 6; ++i)
    {
        vertices[Slots + i] = quad[i];
    }
    for (int i = 0; i < 16; ++i)
    {
        float a = i * 6.2831853f / 16, b = (i + 1) * 6.2831853f / 16;
        vertices[Slots * 2 + i * 3] = {.5f, .5f};
        vertices[Slots * 2 + i * 3 + 1] = {.5f + std::cos(a) * .5f, .5f + std::sin(a) * .5f};
        vertices[Slots * 2 + i * 3 + 2] = {.5f + std::cos(b) * .5f, .5f + std::sin(b) * .5f};
    }
}

PrimitiveMeshCache PrimitiveMeshCache::LoadOrCreate()
{
    PrimitiveMeshCache cache;
    std::wstring directory = RuntimeDirectory(L"Cache");
    std::wstring path = directory.empty() ? L"" : directory + L"\\primitives_v1.meshcache";
    if (!path.empty() && cache.Read(path))
    {
        cache.loaded = MeshCount;
        cache.status = "disk_hit";
    }
    else
    {
        cache.Generate();
        cache.generated = MeshCount;
        cache.status =
            !path.empty() && cache.Write(path) ? "generated_and_saved" : "memory_fallback";
    }
    std::cout << "[MeshCache] version=" << Version << " status=" << cache.status
              << " loaded=" << cache.loaded << " generated=" << cache.generated << '\n';
    return cache;
}

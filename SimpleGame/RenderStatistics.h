#pragma once
#include <cstdint>

struct RenderStatistics
{
    std::uint64_t worldDraws = 0, uiDraws = 0, postDraws = 0;
    std::uint64_t worldInstances = 0, uiInstances = 0;
    std::uint64_t legacyRuns = 0, flushes = 0;
    std::uint64_t usefulVertices = 0, submittedVertices = 0;
    std::uint64_t uploadBytes = 0, bufferGrowths = 0, queueGrowths = 0;
    std::uint64_t glyphHits = 0, glyphMisses = 0;
    std::uint64_t meshGenerations = 0;
    double uploadCpuMs = 0, submitCpuMs = 0;
};

#pragma once
#include "Renderer.h"
#include <array>
#include <chrono>
#include <fstream>
#include <string>
#include <vector>

class PerformanceLog
{
public:
    explicit PerformanceLog(const Renderer& renderer);
    ~PerformanceLog();
    void BeginFrame();
    void EndFrame();
    void Record(const Renderer& renderer,
                const char* scene,
                double updateMs,
                unsigned updates,
                double renderMs,
                double swapMs);

private:
    using Clock = std::chrono::steady_clock;

    struct Query
    {
        GLuint id = 0;
        bool pending = false;
        std::uint64_t frame = 0;
    };

    struct Sample
    {
        std::uint64_t frame = 0, draws = 0;
        double seconds = 0, frameMs = 0, updateMs = 0, renderMs = 0, swapMs = 0;
        double gpuMs = -1;
        std::uint64_t gpuFrame = 0;
        unsigned updates = 0;
        bool gpuSkipped = false;
        bool batching = true, effects = true, bloom = true, edgeBlur = true, vignette = true;
        int width = 0, height = 0;
        float exposure = 1;
        std::string scene;
        RenderStatistics stats;
    };

    std::array<Query, 4> queries_{};
    int activeQuery_ = -1;
    std::uint64_t frame_ = 0, gpuFrame_ = 0;
    double gpuMs_ = -1;
    Clock::time_point start_ = Clock::now(), previous_ = start_, reportStart_ = start_;
    std::vector<Sample> samples_;
    std::ofstream csv_, events_;
    std::string lastConfiguration_;
    bool outputError_ = false;
    double lastLoggingMs_ = 0;
    unsigned segment_ = 0;
    void Flush(Clock::time_point now);
};

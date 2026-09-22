#include "stdafx.h"
#include "PerformanceLog.h"
#include "RuntimeFiles.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>

namespace
{
    double Milliseconds(std::chrono::steady_clock::duration elapsed)
    {
        return std::chrono::duration<double, std::milli>(elapsed).count();
    }

    const char* GLText(GLenum name)
    {
        const auto* value = glGetString(name);
        return value ? reinterpret_cast<const char*>(value) : "unavailable";
    }
} // namespace

PerformanceLog::PerformanceLog(const Renderer& renderer)
{
    samples_.reserve(512);
    const std::wstring directory = RuntimeDirectory(L"Logs");
    SYSTEMTIME utc{};
    GetSystemTime(&utc);
    std::wostringstream name;
    name << L"renderer_" << utc.wYear << L'_' << utc.wMonth << L'_' << utc.wDay << L'_' << utc.wHour
         << L'_' << utc.wMinute << L'_' << utc.wSecond << L'_' << utc.wMilliseconds << L'_'
         << GetCurrentProcessId();
    if (!directory.empty())
    {
        const std::wstring base = directory + L"\\" + name.str();
        csv_.open((base + L".csv").c_str(), std::ios::out | std::ios::app);
        events_.open((base + L".log").c_str(), std::ios::out | std::ios::app);
        csv_.imbue(std::locale::classic());
        events_.imbue(std::locale::classic());
        std::wcout << L"[Performance] session=" << base << L" (.csv + .log)\n";
    }
    if (!csv_.is_open() || !events_.is_open())
    {
        std::cerr << "[Performance] File logging unavailable; console logging remains active.\n";
        outputError_ = true;
    }
    csv_
        << "schema,segment,frame,seconds,scene,width,height,batching,hdr,bloom,edge_blur,vignette,exposure,"
           "frame_ms,update_cpu_ms,update_ticks,render_cpu_ms,swap_cpu_ms,gpu_ms,gpu_frame,gpu_query_skipped,"
           "draws,world_draws,ui_draws,post_draws,legacy_geometry_runs,world_instances,ui_instances,"
           "useful_vertices,submitted_vertices,upload_bytes,upload_cpu_ms,submit_cpu_ms,buffer_growths,"
           "queue_growths,flushes,glyph_hits,glyph_misses,mesh_generations,last_log_flush_ms\n";
    events_
        << "schema=1 algorithm=ordered_mesh_atlas_v1 cache_version=1\n"
        << "build=" << __DATE__ << ' ' << __TIME__ << " compiler_msvc=" << _MSC_FULL_VER
        << " pointer_bits=" << sizeof(void*) * 8 << '\n'
        << "vendor=" << GLText(GL_VENDOR) << "\nrenderer=" << GLText(GL_RENDERER)
        << "\nOpenGL=" << GLText(GL_VERSION) << "\nGLSL=" << GLText(GL_SHADING_LANGUAGE_VERSION)
        << "\nmesh_cache=" << renderer.MeshCacheStatus()
        << " startup_loaded=" << renderer.StartupMeshesLoaded()
        << " startup_generated=" << renderer.StartupMeshesGenerated()
        << "\nlogical_resolution=1280x800 timer_request_ms=16 swap_interval=unqueried\n"
        << "GPU=GL_TIME_ELAPSED, delayed nonblocking samples, excludes swap; gpu_frame identifies source\n"
        << "CPU durations are wall clock; render includes queueing, GL calls and driver waits\n"
        << "F10 switches batching; files buffered once per second; no glFinish\n";
#ifdef _DEBUG
    events_ << "configuration=Debug\n";
#else
    events_ << "configuration=Release\n";
#endif
    for (auto& query : queries_)
    {
        glGenQueries(1, &query.id);
    }
    events_.flush();
}

PerformanceLog::~PerformanceLog()
{
    EndFrame();
    Flush(Clock::now());
    for (auto& query : queries_)
    {
        if (query.id)
        {
            glDeleteQueries(1, &query.id);
        }
    }
}

void PerformanceLog::BeginFrame()
{
    ++frame_;
    // Never wait on the GPU for profiling. Full rings skip a timing sample.
    for (auto& query : queries_)
    {
        if (!query.pending)
        {
            continue;
        }
        GLint ready = GL_FALSE;
        glGetQueryObjectiv(query.id, GL_QUERY_RESULT_AVAILABLE, &ready);
        if (!ready)
        {
            continue;
        }
        GLuint64 nanoseconds = 0;
        glGetQueryObjectui64v(query.id, GL_QUERY_RESULT, &nanoseconds);
        if (query.frame > gpuFrame_)
        {
            gpuMs_ = static_cast<double>(nanoseconds) / 1000000.0;
            gpuFrame_ = query.frame;
        }
        query.pending = false;
    }
    activeQuery_ = -1;
    for (size_t i = 0; i < queries_.size(); ++i)
    {
        auto& query = queries_[i];
        if (query.id && !query.pending)
        {
            query.frame = frame_;
            glBeginQuery(GL_TIME_ELAPSED, query.id);
            activeQuery_ = static_cast<int>(i);
            break;
        }
    }
}

void PerformanceLog::EndFrame()
{
    if (activeQuery_ >= 0)
    {
        glEndQuery(GL_TIME_ELAPSED);
        queries_[activeQuery_].pending = true;
        activeQuery_ = -1;
    }
}

void PerformanceLog::Record(const Renderer& renderer,
                            const char* scene,
                            double updateMs,
                            unsigned updates,
                            double renderMs,
                            double swapMs)
{
    const auto now = Clock::now();
    Sample sample;
    sample.frame = frame_;
    sample.seconds = std::chrono::duration<double>(now - start_).count();
    sample.frameMs = Milliseconds(now - previous_);
    sample.updateMs = updateMs;
    sample.updates = updates;
    sample.renderMs = renderMs;
    sample.swapMs = swapMs;
    sample.gpuMs = gpuMs_;
    sample.gpuFrame = gpuFrame_;
    sample.gpuSkipped = true;
    for (const auto& query : queries_)
    {
        if (query.frame == frame_ && query.pending)
        {
            sample.gpuSkipped = false;
        }
    }
    sample.scene = scene;
    sample.width = renderer.Width();
    sample.height = renderer.Height();
    sample.batching = renderer.BatchingEnabled();
    const auto& fx = renderer.Effects();
    sample.effects = renderer.PostProcessingAvailable() && fx.enabled;
    sample.bloom = fx.bloom;
    sample.edgeBlur = fx.edgeBlur;
    sample.vignette = fx.vignette;
    sample.exposure = fx.exposure;
    sample.stats = renderer.Statistics();
    sample.draws = renderer.FrameDrawCalls();

    std::ostringstream config;
    config.imbue(std::locale::classic());
    config << scene << ' ' << sample.width << 'x' << sample.height << " batch=" << sample.batching
           << " hdr=" << sample.effects << " bloom=" << sample.bloom << " edge=" << sample.edgeBlur
           << " vignette=" << sample.vignette << " exposure=" << sample.exposure;
    if (config.str() != lastConfiguration_)
    {
        // Keep summaries homogeneous across toggles and scene transitions.
        Flush(previous_);
        ++segment_;
        lastConfiguration_ = config.str();
        events_ << "event=config seconds=" << sample.seconds << " segment=" << segment_ << ' '
                << lastConfiguration_ << '\n';
    }
    samples_.push_back(std::move(sample));
    previous_ = now;
    if (now - reportStart_ >= std::chrono::seconds(1) || samples_.size() >= 4096)
    {
        Flush(now);
    }
}

void PerformanceLog::Flush(Clock::time_point now)
{
    if (samples_.empty())
    {
        reportStart_ = now;
        return;
    }
    const auto loggingStart = Clock::now();
    double elapsedMs = 0, draws = 0, render = 0, update = 0, swap = 0;
    std::uint64_t maxDraws = 0;
    std::vector<double> frameTimes;
    frameTimes.reserve(samples_.size());
    csv_ << std::fixed << std::setprecision(4);
    for (const auto& sample : samples_)
    {
        const auto& s = sample.stats;
        elapsedMs += sample.frameMs;
        draws += static_cast<double>(sample.draws);
        maxDraws = std::max(maxDraws, sample.draws);
        render += sample.renderMs;
        update += sample.updateMs;
        swap += sample.swapMs;
        frameTimes.push_back(sample.frameMs);
        csv_ << "1," << segment_ << ',' << sample.frame << ',' << sample.seconds << ','
             << sample.scene << ',' << sample.width << ',' << sample.height << ','
             << sample.batching << ',' << sample.effects << ',' << sample.bloom << ','
             << sample.edgeBlur << ',' << sample.vignette << ',' << sample.exposure << ','
             << sample.frameMs << ',' << sample.updateMs << ',' << sample.updates << ','
             << sample.renderMs << ',' << sample.swapMs << ',' << sample.gpuMs << ','
             << sample.gpuFrame << ',' << sample.gpuSkipped << ',' << sample.draws << ','
             << s.worldDraws << ',' << s.uiDraws << ',' << s.postDraws << ',' << s.legacyRuns << ','
             << s.worldInstances << ',' << s.uiInstances << ',' << s.usefulVertices << ','
             << s.submittedVertices << ',' << s.uploadBytes << ',' << s.uploadCpuMs << ','
             << s.submitCpuMs << ',' << s.bufferGrowths << ',' << s.queueGrowths << ',' << s.flushes
             << ',' << s.glyphHits << ',' << s.glyphMisses << ',' << s.meshGenerations << ','
             << lastLoggingMs_ << '\n';
    }
    std::sort(frameTimes.begin(), frameTimes.end());
    const auto& last = samples_.back();
    const double n = static_cast<double>(samples_.size());
    const size_t p95 = static_cast<size_t>(std::ceil(n * .95)) - 1;
    std::ostringstream line;
    line.imbue(std::locale::classic());
    line << std::fixed << std::setprecision(2)
         << "[Performance] FPS=" << n * 1000 / std::max(.001, elapsedMs)
         << " | DrawCalls/frame: last=" << last.draws << ", avg=" << draws / n
         << ", max=" << maxDraws << " | W/UI/Post=" << last.stats.worldDraws << '/'
         << last.stats.uiDraws << '/' << last.stats.postDraws
         << " | CPU update/render/swap ms=" << update / n << '/' << render / n << '/' << swap / n
         << " | GPU(last)=" << last.gpuMs << "ms@" << last.gpuFrame
         << " | frame p95=" << frameTimes[p95] << "ms | batch=" << last.batching;
    std::cout << line.str() << '\n';
    events_ << "summary seconds=" << last.seconds << " segment=" << segment_
            << " frames=" << samples_.size() << ' ' << line.str() << '\n';
    csv_.flush();
    events_.flush();
    if ((!csv_ || !events_) && !outputError_)
    {
        std::cerr << "[Performance] Log write failed; check Logs directory/disk space.\n";
        outputError_ = true;
    }
    samples_.clear();
    reportStart_ = now;
    lastLoggingMs_ = Milliseconds(Clock::now() - loggingStart);
}

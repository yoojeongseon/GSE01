#include "stdafx.h"
#include "Renderer.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>

namespace
{
    std::string ReadShader(const wchar_t* name)
    {
        wchar_t executable[32768] = {};
        DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
        if (length == 0 || length >= 32768)
        {
            return {};
        }
        std::wstring path(executable, length);
        path = path.substr(0, path.find_last_of(L"\\/") + 1) + L"Shaders\\" + name;
        // The MSVC runtime supports wide file paths, including Korean directories.
        std::ifstream file(path.c_str(), std::ios::binary);
        if (!file)
        {
            std::wcerr << L"Missing shader: " << path
                       << L"\nBuild the project to copy Shaders next to the executable.\n";
            return {};
        }
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    GLuint Shader(GLenum type, const std::string& source)
    {
        if (source.empty())
        {
            return 0;
        }
        GLuint shader = glCreateShader(type);
        if (!shader)
        {
            return 0;
        }
        const char* data = source.c_str();
        glShaderSource(shader, 1, &data, nullptr);
        glCompileShader(shader);
        GLint ok = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[4096] = {};
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::cerr << "Shader compilation failed: " << log << '\n';
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    GLuint Program(const wchar_t* vertexFile, const wchar_t* fragmentFile)
    {
        GLuint vs = Shader(GL_VERTEX_SHADER, ReadShader(vertexFile));
        GLuint fs = Shader(GL_FRAGMENT_SHADER, ReadShader(fragmentFile));
        GLuint program = 0;
        if (vs && fs)
        {
            program = glCreateProgram();
            if (program)
            {
                glAttachShader(program, vs);
                glAttachShader(program, fs);
                glLinkProgram(program);
                GLint ok = GL_FALSE;
                glGetProgramiv(program, GL_LINK_STATUS, &ok);
                if (!ok)
                {
                    char log[4096] = {};
                    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
                    std::wcerr << L"Program link failed: " << fragmentFile << L'\n';
                    std::cerr << log << '\n';
                    glDeleteProgram(program);
                    program = 0;
                }
            }
        }
        if (vs)
        {
            glDeleteShader(vs);
        }
        if (fs)
        {
            glDeleteShader(fs);
        }
        return program;
    }
} // namespace

// Rasterize installed Windows Unicode glyphs once, then reuse coverage runs in
// the existing ordered geometry batch. No legacy OpenGL text APIs are needed.
struct Renderer::FontData
{
    struct Run
    {
        float x, y, width, alpha;
    };

    struct Glyph
    {
        float advance = 12;
        std::vector<Run> runs;
    };

    HDC dc = nullptr;
    HFONT handle = nullptr;
    HGDIOBJ previous = nullptr;
    int ascent = 24;
    bool valid = false;
    std::map<wchar_t, Glyph> cache;

    FontData()
    {
        dc = CreateCompatibleDC(nullptr);
        handle = CreateFontW(-24,
                             0,
                             0,
                             0,
                             FW_NORMAL,
                             FALSE,
                             FALSE,
                             FALSE,
                             DEFAULT_CHARSET,
                             OUT_TT_PRECIS,
                             CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY,
                             DEFAULT_PITCH,
                             L"Malgun Gothic");
        if (!dc || !handle)
        {
            return;
        }
        previous = SelectObject(dc, handle);
        if (!previous || previous == HGDI_ERROR)
        {
            return;
        }
        TEXTMETRICW metrics = {};
        if (!GetTextMetricsW(dc, &metrics))
        {
            return;
        }
        ascent = metrics.tmAscent;
        valid = true;
    }

    ~FontData()
    {
        if (dc && previous && previous != HGDI_ERROR)
        {
            SelectObject(dc, previous);
        }
        if (handle)
        {
            DeleteObject(handle);
        }
        if (dc)
        {
            DeleteDC(dc);
        }
    }

    const Glyph& Get(wchar_t character)
    {
        auto existing = cache.find(character);
        if (existing != cache.end())
        {
            return existing->second;
        }
        GLYPHMETRICS metrics = {};
        MAT2 transform = {};
        transform.eM11.value = 1;
        transform.eM22.value = 1;
        DWORD bytes =
            GetGlyphOutlineW(dc, character, GGO_GRAY8_BITMAP, &metrics, 0, nullptr, &transform);
        Glyph glyph;
        if (bytes == GDI_ERROR)
        {
            // Keep missing characters visible rather than dropping them silently.
            glyph.advance = 24;
            glyph.runs = {{2, 4, 18, 1}, {2, 23, 18, 1}};
            for (int row = 5; row < 23; ++row)
            {
                glyph.runs.push_back({2, float(row), 1, 1});
                glyph.runs.push_back({19, float(row), 1, 1});
            }
        }
        else
        {
            glyph.advance = float(metrics.gmCellIncX);
            std::vector<unsigned char> bitmap(bytes);
            if (bytes &&
                GetGlyphOutlineW(
                    dc, character, GGO_GRAY8_BITMAP, &metrics, bytes, bitmap.data(), &transform) !=
                    GDI_ERROR)
            {
                DWORD stride = (metrics.gmBlackBoxX + 3) & ~3UL;
                for (DWORD row = 0; row < metrics.gmBlackBoxY; ++row)
                {
                    for (DWORD column = 0; column < metrics.gmBlackBoxX;)
                    {
                        unsigned char coverage = bitmap[row * stride + column];
                        DWORD end = column + 1;
                        while (end < metrics.gmBlackBoxX && bitmap[row * stride + end] == coverage)
                        {
                            ++end;
                        }
                        if (coverage)
                        {
                            glyph.runs.push_back({float(metrics.gmptGlyphOrigin.x) + column,
                                                  float(ascent - metrics.gmptGlyphOrigin.y) + row,
                                                  float(end - column),
                                                  coverage / 64.f});
                        }
                        column = end;
                    }
                }
            }
        }
        return cache.emplace(character, std::move(glyph)).first->second;
    }
};

bool Renderer::IsInitialized() const
{
    return program_ && vao_ && meshBuffer_ && instanceBuffer_ && font_ && font_->valid;
}

Renderer::Renderer(int width, int height)
{
    font_.reset(new FontData());
    if (!font_->valid)
    {
        std::cerr << "Could not initialize the Windows Unicode font.\n";
        return;
    }
    Resize(width, height);
    program_ = Program(L"SolidRect.vs", L"SolidRect.fs");
    if (!program_)
    {
        return;
    }
    InitializeMeshes();
    instances_.reserve(32768);
    drawRuns_.reserve(4096);
    worldUniform_ = glGetUniformLocation(program_, "u_World");
    InitializePostProcessing();
}

Renderer::~Renderer()
{
    ReleasePostProcessing();
    if (instanceBuffer_)
    {
        glDeleteBuffers(1, &instanceBuffer_);
    }
    if (meshBuffer_)
    {
        glDeleteBuffers(1, &meshBuffer_);
    }
    if (vao_)
    {
        glDeleteVertexArrays(1, &vao_);
    }
    if (program_)
    {
        glDeleteProgram(program_);
    }
}

void Renderer::Resize(int w, int h)
{
    width_ = std::max(w, 1);
    height_ = std::max(h, 1);
}

void Renderer::Begin()
{
    frameDrawCalls_ = 0;
    instances_.clear();
    drawRuns_.clear();
    inInterface_ = false;
    hdrFrame_ = postReady_ && effects_.enabled;
    glDisable(GL_SCISSOR_TEST);
    // Composite shader writes display-encoded RGB; prevent a second conversion.
    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
    glClearColor(.025f, .035f, .045f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (hdrFrame_)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, scene_.framebuffer);
        glViewport(0, 0, scene_.width, scene_.height);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    else
    {
        WindowViewport();
    }
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::WindowViewport()
{
    float fit = std::min(width_ / 1280.f, height_ / 800.f);
    int w = std::max(1, static_cast<int>(1280 * fit));
    int h = std::max(1, static_cast<int>(800 * fit));
    glViewport((width_ - w) / 2, (height_ - h) / 2, w, h);
}

void Renderer::End()
{
    if (!inInterface_)
    {
        BeginInterface();
    }
    Flush();
}

void Renderer::BeginInterface()
{
    if (inInterface_)
    {
        return;
    }
    Flush();
    if (hdrFrame_)
    {
        Composite();
    }
    inInterface_ = true;
}

void Renderer::Flush()
{
    if (!IsInitialized() || instances_.empty())
    {
        return;
    }
    glUseProgram(program_);
    glUniform1i(worldUniform_, hdrFrame_ && !inInterface_ ? 1 : 0);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer_);
    size_t bytes = instances_.size() * sizeof(Instance);
    if (bytes > instanceCapacity_)
    {
        instanceCapacity_ = std::max<size_t>(65536, bytes * 2);
        glBufferData(GL_ARRAY_BUFFER, instanceCapacity_, nullptr, GL_STREAM_DRAW);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, instances_.data());
    // Merge adjacent instances only. Sorting by mesh would break alpha/painter order.
    for (const DrawRun& run : drawRuns_)
    {
        size_t base = run.first * sizeof(Instance);
        glVertexAttribPointer(1,
                              4,
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(Instance),
                              reinterpret_cast<void*>(base + offsetof(Instance, a)));
        glVertexAttribPointer(2,
                              4,
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(Instance),
                              reinterpret_cast<void*>(base + offsetof(Instance, c)));
        glVertexAttribPointer(3,
                              4,
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(Instance),
                              reinterpret_cast<void*>(base + offsetof(Instance, color)));
        const MeshRange& mesh = meshes_[static_cast<size_t>(run.mesh)];
        glDrawArraysInstanced(GL_TRIANGLES, mesh.first, mesh.count, run.count);
        ++frameDrawCalls_;
    }
    glBindVertexArray(0);
    glUseProgram(0);
    instances_.clear();
    drawRuns_.clear();
}

void Renderer::InitializeMeshes()
{
    std::vector<Point> vertices;
    auto append = [&](MeshKind kind, const std::vector<Point>& points)
    {
        meshes_[static_cast<size_t>(kind)] = {static_cast<GLint>(vertices.size()),
                                              static_cast<GLsizei>(points.size())};
        vertices.insert(vertices.end(), points.begin(), points.end());
    };
    append(MeshKind::Triangle, {{0, 0}, {1, 0}, {1, 1}});
    append(MeshKind::Quad, {{0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1}});
    std::vector<Point> circle;
    for (int i = 0; i < 16; ++i)
    {
        float a = i * 6.2831853f / 16;
        float b = (i + 1) * 6.2831853f / 16;
        circle.push_back({.5f, .5f});
        circle.push_back({.5f + std::cos(a) * .5f, .5f + std::sin(a) * .5f});
        circle.push_back({.5f + std::cos(b) * .5f, .5f + std::sin(b) * .5f});
    }
    append(MeshKind::Circle, circle);
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &meshBuffer_);
    glGenBuffers(1, &instanceBuffer_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, meshBuffer_);
    // Immutable geometry upload: no primitive mesh is rebuilt on subsequent frames.
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Point), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Point), nullptr);
    glVertexAttribDivisor(0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer_);
    const size_t offsets[] = {
        offsetof(Instance, a), offsetof(Instance, c), offsetof(Instance, color)};
    for (GLuint attribute = 1; attribute <= 3; ++attribute)
    {
        glEnableVertexAttribArray(attribute);
        glVertexAttribPointer(attribute,
                              4,
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(Instance),
                              reinterpret_cast<void*>(offsets[attribute - 1]));
        glVertexAttribDivisor(attribute, 1);
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer::QueueMesh(MeshKind mesh, Point a, Point b, Point c, Point d, Color color)
{
    if (drawRuns_.empty() || drawRuns_.back().mesh != mesh)
    {
        drawRuns_.push_back({mesh, instances_.size(), 0});
    }
    ++drawRuns_.back().count;
    instances_.push_back({a, b, c, d, color});
}

bool Renderer::CreateTarget(Target& target, int width, int height)
{
    target.width = width;
    target.height = height;
    glGenTextures(1, &target.texture);
    glBindTexture(GL_TEXTURE_2D, target.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &target.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    bool ok = target.texture && target.framebuffer &&
              glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return ok;
}

void Renderer::InitializePostProcessing()
{
    filterProgram_ = Program(L"Fullscreen.vs", L"Filter.fs");
    compositeProgram_ = Program(L"Fullscreen.vs", L"PostProcess.fs");
    glGenVertexArrays(1, &fullscreenVao_);
    // Fixed logical resolution keeps blur radius and VRAM use stable on resize.
    bool ok = filterProgram_ && compositeProgram_ && fullscreenVao_;
    if (ok)
    {
        ok = CreateTarget(scene_, 1280, 800);
    }
    for (int i = 0; i < 2 && ok; ++i)
    {
        ok = CreateTarget(bloom_[i], 640, 400) && CreateTarget(blurred_[i], 640, 400);
    }
    if (!ok)
    {
        std::cerr << "HDR post-processing unavailable. Falling back to direct rendering.\n";
        ReleasePostProcessing();
        return;
    }
    filter_.source = glGetUniformLocation(filterProgram_, "u_Source");
    filter_.mode = glGetUniformLocation(filterProgram_, "u_Mode");
    filter_.direction = glGetUniformLocation(filterProgram_, "u_Direction");
    filter_.threshold = glGetUniformLocation(filterProgram_, "u_Threshold");
    composite_.scene = glGetUniformLocation(compositeProgram_, "u_Scene");
    composite_.bloom = glGetUniformLocation(compositeProgram_, "u_Bloom");
    composite_.blurred = glGetUniformLocation(compositeProgram_, "u_Blurred");
    composite_.exposure = glGetUniformLocation(compositeProgram_, "u_Exposure");
    composite_.bloomStrength = glGetUniformLocation(compositeProgram_, "u_BloomStrength");
    composite_.vignetteStrength = glGetUniformLocation(compositeProgram_, "u_VignetteStrength");
    composite_.edgeBlurStrength = glGetUniformLocation(compositeProgram_, "u_EdgeBlurStrength");
    postReady_ = true;
}

void Renderer::ReleasePostProcessing()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    for (Target* target : {&scene_, &bloom_[0], &bloom_[1], &blurred_[0], &blurred_[1]})
    {
        if (target->framebuffer)
        {
            glDeleteFramebuffers(1, &target->framebuffer);
        }
        if (target->texture)
        {
            glDeleteTextures(1, &target->texture);
        }
        *target = Target{};
    }
    if (filterProgram_)
    {
        glDeleteProgram(filterProgram_);
    }
    if (compositeProgram_)
    {
        glDeleteProgram(compositeProgram_);
    }
    if (fullscreenVao_)
    {
        glDeleteVertexArrays(1, &fullscreenVao_);
    }
    filterProgram_ = compositeProgram_ = fullscreenVao_ = 0;
    postReady_ = false;
}

void Renderer::Filter(GLuint source, Target& destination, int mode, float dx, float dy)
{
    // Source is always distinct from the attached destination texture.
    glBindFramebuffer(GL_FRAMEBUFFER, destination.framebuffer);
    glViewport(0, 0, destination.width, destination.height);
    glUseProgram(filterProgram_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source);
    glUniform1i(filter_.source, 0);
    glUniform1i(filter_.mode, mode);
    glUniform2f(filter_.direction, dx, dy);
    glUniform1f(filter_.threshold, std::max(.01f, effects_.bloomThreshold));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    ++frameDrawCalls_;
}

void Renderer::Blur(GLuint source, Target (&targets)[2], bool extractHighlights)
{
    Filter(source, targets[0], extractHighlights ? 0 : 2, 0, 0);
    // More, closer-spaced bloom passes soften small emitters without a drawn halo.
    // Keep the independent peripheral scene-blur settings unchanged.
    float radius = 1.5f;
    int iterations = extractHighlights ? 4 : 2;
    for (int pass = 0; pass < iterations; ++pass)
    {
        Filter(targets[0].texture, targets[1], 1, radius, 0);
        Filter(targets[1].texture, targets[0], 1, 0, radius);
    }
}

void Renderer::Composite()
{
    glDisable(GL_BLEND);
    glBindVertexArray(fullscreenVao_);
    if (effects_.bloom)
    {
        Blur(scene_.texture, bloom_, true);
    }
    if (effects_.edgeBlur)
    {
        Blur(scene_.texture, blurred_, false);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    WindowViewport();
    glUseProgram(compositeProgram_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene_.texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, effects_.bloom ? bloom_[0].texture : scene_.texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, effects_.edgeBlur ? blurred_[0].texture : scene_.texture);
    glUniform1i(composite_.scene, 0);
    glUniform1i(composite_.bloom, 1);
    glUniform1i(composite_.blurred, 2);
    glUniform1f(composite_.exposure, std::clamp(effects_.exposure, .25f, 3.f));
    glUniform1f(composite_.bloomStrength,
                effects_.bloom ? std::clamp(effects_.bloomStrength, 0.f, 2.f) : 0.f);
    glUniform1f(composite_.vignetteStrength,
                effects_.vignette ? std::clamp(effects_.vignetteStrength, 0.f, 1.f) : 0.f);
    glUniform1f(composite_.edgeBlurStrength,
                effects_.edgeBlur ? std::clamp(effects_.edgeBlurStrength, 0.f, 1.f) : 0.f);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    ++frameDrawCalls_;
    for (int unit = 2; unit >= 0; --unit)
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glBindVertexArray(0);
    glUseProgram(0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::Triangle(Point a, Point b, Point c, Color col)
{
    QueueMesh(MeshKind::Triangle, a, b, c, a, col);
}

void Renderer::Quad(Point a, Point b, Point c, Point d, Color col)
{
    QueueMesh(MeshKind::Quad, a, b, c, d, col);
}

void Renderer::Rect(float x, float y, float w, float h, Color col)
{
    Quad({x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}, col);
}

void Renderer::Ellipse(float x, float y, float rx, float ry, Color col)
{
    QueueMesh(MeshKind::Circle,
              {x - rx, y - ry},
              {x + rx, y - ry},
              {x + rx, y + ry},
              {x - rx, y + ry},
              col);
}

void Renderer::Line(Point a, Point b, float width, Color col)
{
    float dx = b.x - a.x, dy = b.y - a.y, length = std::sqrt(dx * dx + dy * dy);
    if (length < .001f)
    {
        return;
    }
    float x = -dy / length * width * .5f, y = dx / length * width * .5f;
    Quad({a.x + x, a.y + y}, {b.x + x, b.y + y}, {b.x - x, b.y - y}, {a.x - x, a.y - y}, col);
}

void Renderer::Text(float x, float y, const std::string& text, Color col, float scale)
{
    if (!font_ || !font_->valid || text.empty())
    {
        return;
    }
    int size = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!size)
    {
        std::cerr << "Invalid UTF-8 in interface text.\n";
        return;
    }
    std::wstring unicode(size, L'\0');
    MultiByteToWideChar(CP_UTF8,
                        MB_ERR_INVALID_CHARS,
                        text.data(),
                        static_cast<int>(text.size()),
                        &unicode[0],
                        size);
    float start = x, factor = scale * .5f;
    for (wchar_t character : unicode)
    {
        if (character == L'\n')
        {
            x = start;
            y += 32 * factor;
            continue;
        }
        if (character == L'\r')
        {
            continue;
        }
        const auto& glyph = font_->Get(character);
        for (const auto& run : glyph.runs)
        {
            Color ink = col;
            ink.a *= run.alpha;
            Rect(x + run.x * factor, y + run.y * factor, run.width * factor, factor, ink);
        }
        x += glyph.advance * factor;
    }
}

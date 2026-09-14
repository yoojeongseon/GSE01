#pragma once
#include <string>
#include <vector>
#include <memory>
#include <array>
#include "Dependencies/glew.h"

struct Point
{
    float x, y;
};

struct Color
{
    float r, g, b, a;

    Color(float red, float green, float blue, float alpha = 1.f)
        : r(red), g(green), b(blue), a(alpha)
    {
    }
};

// Batched, painter-ordered geometry in a 1280 x 800 logical canvas.
class Renderer
{
public:
    struct PostProcessSettings
    {
        bool enabled = true, bloom = true, vignette = true, edgeBlur = true;
        float exposure = 1.05f;
        float bloomStrength = .45f, bloomThreshold = 1.f;
        float vignetteStrength = .38f, edgeBlurStrength = .85f;
    };

    Renderer(int width, int height);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    bool IsInitialized() const;
    void Resize(int width, int height);
    void Begin();
    // Resolve the HDR scene before submitting screen-space UI geometry.
    void BeginInterface();
    void End();

    PostProcessSettings& Effects()
    {
        return effects_;
    }

    const PostProcessSettings& Effects() const
    {
        return effects_;
    }

    bool PostProcessingAvailable() const
    {
        return postReady_;
    }

    void Triangle(Point a, Point b, Point c, Color color);
    void Quad(Point a, Point b, Point c, Point d, Color color);
    void Rect(float x, float y, float w, float h, Color color);
    void Ellipse(float x, float y, float rx, float ry, Color color);
    void Line(Point a, Point b, float width, Color color);
    void Text(float x, float y, const std::string& text, Color color, float scale = 2.f);

private:
    struct FontData;
    std::unique_ptr<FontData> font_;

    enum class MeshKind
    {
        Triangle,
        Quad,
        Circle
    };

    struct MeshRange
    {
        GLint first = 0;
        GLsizei count = 0;
    };

    struct Instance
    {
        Point a, b, c, d;
        Color color;
    };

    struct DrawRun
    {
        MeshKind mesh;
        size_t first = 0;
        GLsizei count = 0;
    };

    GLuint program_ = 0, vao_ = 0, meshBuffer_ = 0, instanceBuffer_ = 0;
    int width_ = 1280, height_ = 800;
    // Only three canonical meshes exist; screen coordinates are not cache keys.
    std::array<MeshRange, 3> meshes_{};
    std::vector<Instance> instances_;
    std::vector<DrawRun> drawRuns_;
    size_t instanceCapacity_ = 0;
    void InitializeMeshes();
    void QueueMesh(MeshKind mesh, Point a, Point b, Point c, Point d, Color color);

    struct Target
    {
        GLuint framebuffer = 0, texture = 0;
        int width = 0, height = 0;
    };

    Target scene_, bloom_[2], blurred_[2];
    GLuint filterProgram_ = 0, compositeProgram_ = 0, fullscreenVao_ = 0;
    GLint worldUniform_ = -1;

    struct FilterUniforms
    {
        GLint source = -1, mode = -1, direction = -1, threshold = -1;
    } filter_;

    struct CompositeUniforms
    {
        GLint scene = -1, bloom = -1, blurred = -1, exposure = -1;
        GLint bloomStrength = -1, vignetteStrength = -1, edgeBlurStrength = -1;
    } composite_;

    PostProcessSettings effects_;
    bool postReady_ = false, inInterface_ = false, hdrFrame_ = false;
    void Flush();
    void WindowViewport();
    void InitializePostProcessing();
    void ReleasePostProcessing();
    bool CreateTarget(Target& target, int width, int height);
    void Filter(GLuint source, Target& destination, int mode, float dx, float dy);
    void Blur(GLuint source, Target (&targets)[2], bool extractHighlights);
    void Composite();
};

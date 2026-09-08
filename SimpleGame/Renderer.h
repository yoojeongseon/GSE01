#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Dependencies/glew.h"

struct Point { float x, y; };
struct Color {
    float r, g, b, a;
    Color(float red, float green, float blue, float alpha = 1.f)
        : r(red), g(green), b(blue), a(alpha) {}
};

// Batched, painter-ordered geometry in a 1280 x 800 logical canvas.
class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    bool IsInitialized() const;
    void Resize(int width, int height);
    void Begin();
    void End();
    void Triangle(Point a, Point b, Point c, Color color);
    void Quad(Point a, Point b, Point c, Point d, Color color);
    void Rect(float x, float y, float w, float h, Color color);
    void Ellipse(float x, float y, float rx, float ry, Color color);
    void Line(Point a, Point b, float width, Color color);
    void Text(float x, float y, const std::string& text, Color color, float scale = 2.f);

private:
    struct FontData;
    std::unique_ptr<FontData> font_;
    struct Vertex { float x, y, r, g, b, a; };
    GLuint program_ = 0, vao_ = 0, buffer_ = 0;
    int width_ = 1280, height_ = 800;
    std::vector<Vertex> vertices_;
};

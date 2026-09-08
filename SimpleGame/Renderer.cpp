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

namespace {
std::string ReadShader(const wchar_t* name) {
    wchar_t executable[32768] = {};
    DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    if (length == 0 || length >= 32768) return {};
    std::wstring path(executable, length);
    path = path.substr(0, path.find_last_of(L"\\/") + 1) + L"Shaders\\" + name;
    // The MSVC runtime supports wide file paths, including Korean directories.
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file) {
        std::wcerr << L"Missing shader: " << path << L"\nBuild the project to copy Shaders next to the executable.\n";
        return {};
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
GLuint Shader(GLenum type, const std::string& source) {
    if (source.empty()) return 0;
    GLuint shader = glCreateShader(type);
    if (!shader) return 0;
    const char* data = source.c_str();
    glShaderSource(shader, 1, &data, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compilation failed: " << log << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
}

// Rasterize installed Windows Unicode glyphs once, then reuse coverage runs in
// the existing ordered geometry batch. No legacy OpenGL text APIs are needed.
struct Renderer::FontData {
    struct Run { float x, y, width, alpha; };
    struct Glyph { float advance = 12; std::vector<Run> runs; };
    HDC dc = nullptr;
    HFONT handle = nullptr;
    HGDIOBJ previous = nullptr;
    int ascent = 24;
    bool valid = false;
    std::map<wchar_t, Glyph> cache;

    FontData() {
        dc = CreateCompatibleDC(nullptr);
        handle = CreateFontW(-24,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
        if (!dc || !handle) return;
        previous = SelectObject(dc,handle);
        if (!previous || previous == HGDI_ERROR) return;
        TEXTMETRICW metrics = {};
        if (!GetTextMetricsW(dc,&metrics)) return;
        ascent = metrics.tmAscent;
        valid = true;
    }
    ~FontData() {
        if (dc && previous && previous != HGDI_ERROR) SelectObject(dc,previous);
        if (handle) DeleteObject(handle);
        if (dc) DeleteDC(dc);
    }
    const Glyph& Get(wchar_t character) {
        auto existing = cache.find(character);
        if (existing != cache.end()) return existing->second;
        GLYPHMETRICS metrics = {};
        MAT2 transform = {};
        transform.eM11.value = 1; transform.eM22.value = 1;
        DWORD bytes = GetGlyphOutlineW(dc,character,GGO_GRAY8_BITMAP,&metrics,0,nullptr,&transform);
        Glyph glyph;
        if (bytes == GDI_ERROR) {
            // Keep missing characters visible rather than dropping them silently.
            glyph.advance = 24;
            glyph.runs = {{2,4,18,1},{2,23,18,1}};
            for(int row=5;row<23;++row) {
                glyph.runs.push_back({2,float(row),1,1});
                glyph.runs.push_back({19,float(row),1,1});
            }
        } else {
            glyph.advance = float(metrics.gmCellIncX);
            std::vector<unsigned char> bitmap(bytes);
            if (bytes && GetGlyphOutlineW(dc,character,GGO_GRAY8_BITMAP,&metrics,bytes,bitmap.data(),&transform)!=GDI_ERROR) {
                DWORD stride = (metrics.gmBlackBoxX+3)&~3UL;
                for(DWORD row=0;row<metrics.gmBlackBoxY;++row) {
                    for(DWORD column=0;column<metrics.gmBlackBoxX;) {
                        unsigned char coverage = bitmap[row*stride+column];
                        DWORD end=column+1;
                        while(end<metrics.gmBlackBoxX && bitmap[row*stride+end]==coverage) ++end;
                        if(coverage) glyph.runs.push_back({float(metrics.gmptGlyphOrigin.x)+column,
                            float(ascent-metrics.gmptGlyphOrigin.y)+row,float(end-column),coverage/64.f});
                        column=end;
                    }
                }
            }
        }
        return cache.emplace(character,std::move(glyph)).first->second;
    }
};

bool Renderer::IsInitialized() const {
    return program_ && vao_ && buffer_ && font_ && font_->valid;
}
Renderer::Renderer(int width, int height) {
    font_.reset(new FontData());
    if (!font_->valid) { std::cerr << "Could not initialize the Windows Unicode font.\n"; return; }
    Resize(width, height);
    GLuint vs = Shader(GL_VERTEX_SHADER, ReadShader(L"SolidRect.vs"));
    GLuint fs = Shader(GL_FRAGMENT_SHADER, ReadShader(L"SolidRect.fs"));
    if (vs && fs) {
        program_ = glCreateProgram();
        if (program_) {
            glAttachShader(program_, vs); glAttachShader(program_, fs);
            glLinkProgram(program_);
            GLint ok = GL_FALSE;
            glGetProgramiv(program_, GL_LINK_STATUS, &ok);
            if (!ok) {
                char log[4096] = {};
                glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
                std::cerr << "Shader link failed: " << log << '\n';
                glDeleteProgram(program_); program_ = 0;
            }
        }
    }
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    if (!program_) return;
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &buffer_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, r)));
    glBindVertexArray(0);
    vertices_.reserve(200000);
}
Renderer::~Renderer() {
    if (buffer_) glDeleteBuffers(1, &buffer_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (program_) glDeleteProgram(program_);
}
void Renderer::Resize(int w, int h) { width_ = std::max(w, 1); height_ = std::max(h, 1); }
void Renderer::Begin() {
    vertices_.clear();
    glDisable(GL_SCISSOR_TEST);
    glClearColor(.025f, .035f, .045f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    float fit = std::min(width_ / 1280.f, height_ / 800.f);
    int w = std::max(1, static_cast<int>(1280 * fit));
    int h = std::max(1, static_cast<int>(800 * fit));
    glViewport((width_-w)/2, (height_-h)/2, w, h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
void Renderer::End() {
    if (!IsInitialized() || vertices_.empty()) return;
    glUseProgram(program_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_);
    glBufferData(GL_ARRAY_BUFFER, vertices_.size()*sizeof(Vertex), vertices_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));
    glBindVertexArray(0);
    glUseProgram(0);
}
void Renderer::Triangle(Point a, Point b, Point c, Color col) {
    for (Point p : {a,b,c}) vertices_.push_back({p.x,p.y,col.r,col.g,col.b,col.a});
}
void Renderer::Quad(Point a, Point b, Point c, Point d, Color col) {
    Triangle(a,b,c,col); Triangle(a,c,d,col);
}
void Renderer::Rect(float x, float y, float w, float h, Color col) {
    Quad({x,y},{x+w,y},{x+w,y+h},{x,y+h},col);
}
void Renderer::Ellipse(float x, float y, float rx, float ry, Color col) {
    const int count = 16;
    for (int i=0; i<count; ++i) {
        float a = i*6.2831853f/count, b = (i+1)*6.2831853f/count;
        Triangle({x,y},{x+std::cos(a)*rx,y+std::sin(a)*ry},
                 {x+std::cos(b)*rx,y+std::sin(b)*ry},col);
    }
}
void Renderer::Line(Point a, Point b, float width, Color col) {
    float dx=b.x-a.x, dy=b.y-a.y, length=std::sqrt(dx*dx+dy*dy);
    if (length < .001f) return;
    float x=-dy/length*width*.5f, y=dx/length*width*.5f;
    Quad({a.x+x,a.y+y},{b.x+x,b.y+y},{b.x-x,b.y-y},{a.x-x,a.y-y},col);
}

void Renderer::Text(float x, float y, const std::string& text, Color col, float scale) {
    if (!font_ || !font_->valid || text.empty()) return;
    int size = MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
    if (!size) { std::cerr << "Invalid UTF-8 in interface text.\n"; return; }
    std::wstring unicode(size,L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),&unicode[0],size);
    float start=x, factor=scale*.5f;
    for (wchar_t character:unicode) {
        if (character==L'\n') { x=start; y+=32*factor; continue; }
        if (character==L'\r') continue;
        const auto& glyph=font_->Get(character);
        for (const auto& run:glyph.runs) {
            Color ink=col; ink.a*=run.alpha;
            Rect(x+run.x*factor,y+run.y*factor,run.width*factor,factor,ink);
        }
        x+=glyph.advance*factor;
    }
}


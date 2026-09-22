/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)
This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.
This program is distributed without any warranty.
*/
#include "stdafx.h"
#include "Renderer.h"
#include "Game.h"
#include "PerformanceLog.h"
#include "Dependencies/freeglut.h"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <memory>
#include <sstream>

namespace
{
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<Game> game;
    std::unique_ptr<PerformanceLog> performance;
    double pendingUpdateMs = 0;
    unsigned pendingUpdates = 0;
    bool closing = false;
    auto previous = std::chrono::steady_clock::now();

    void Display()
    {
        if (!closing && renderer && game && performance)
        {
            auto start = std::chrono::steady_clock::now();
            performance->BeginFrame();
            game->Draw(*renderer);
            performance->EndFrame();
            auto submitted = std::chrono::steady_clock::now();
            glutSwapBuffers();
            auto presented = std::chrono::steady_clock::now();
            performance->Record(
                *renderer,
                game->ActiveSceneName(),
                pendingUpdateMs,
                pendingUpdates,
                std::chrono::duration<double, std::milli>(submitted - start).count(),
                std::chrono::duration<double, std::milli>(presented - submitted).count());
            pendingUpdateMs = 0;
            pendingUpdates = 0;
        }
    }

    void Resize(int w, int h)
    {
        if (renderer)
        {
            renderer->Resize(w, h);
        }
    }

    void Close()
    {
        closing = true;
        if (game && !game->Save())
        {
            std::cerr << "Closing without a new save; previous save retained.\n";
        }
        performance.reset(); // Delete timing queries while the GL context still exists.
        game.reset();
        // GLUT invokes the close callback while the window context still exists.
        renderer.reset();
    }

    bool Down(int key)
    {
        return (GetAsyncKeyState(key) & 0x8000) != 0;
    }

    void Tick(int)
    {
        if (closing || !game)
        {
            return;
        }
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - previous).count();
        previous = now;
        dt = std::min(dt, .05f); // Avoid teleporting after a paused debugger or window drag.
        DWORD foregroundProcess = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
        bool focused = foregroundProcess == GetCurrentProcessId();
        const auto updateStart = std::chrono::steady_clock::now();
        game->Update(dt,
                     focused && (Down('W') || Down(VK_UP)),
                     focused && (Down('S') || Down(VK_DOWN)),
                     focused && (Down('A') || Down(VK_LEFT)),
                     focused && (Down('D') || Down(VK_RIGHT)),
                     focused && Down(VK_SHIFT));
        pendingUpdateMs += std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - updateStart)
                               .count();
        ++pendingUpdates;
        glutPostRedisplay();
        glutTimerFunc(16, Tick, 0);
    }

    void Key(unsigned char key, int, int)
    {
        if (!game)
        {
            return;
        }
        if (key == 27 && !game->IsDeathPrompt())
        {
            if (game->Save())
            {
                glutLeaveMainLoop();
            }
            return;
        }
        game->Action(key);
    }

    void Special(int key, int, int)
    {
        if (!game || game->IsDeathPrompt())
        {
            return;
        }
        if (key == GLUT_KEY_F3)
        {
            game->Action('g');
        }
        if (key == GLUT_KEY_F5)
        {
            game->Action('p');
        }
        if (!renderer)
        {
            return;
        }
        if (key == GLUT_KEY_F10)
        {
            renderer->SetBatchingEnabled(!renderer->BatchingEnabled());
        }
        auto& effects = renderer->Effects();
        if (key == GLUT_KEY_F6)
        {
            effects.enabled = !effects.enabled;
        }
        if (key == GLUT_KEY_F7)
        {
            effects.bloom = !effects.bloom;
        }
        if (key == GLUT_KEY_F8)
        {
            effects.vignette = !effects.vignette;
        }
        if (key == GLUT_KEY_F9)
        {
            effects.edgeBlur = !effects.edgeBlur;
        }
        if (key == GLUT_KEY_PAGE_UP)
        {
            effects.exposure = std::min(3.f, effects.exposure + .1f);
        }
        if (key == GLUT_KEY_PAGE_DOWN)
        {
            effects.exposure = std::max(.25f, effects.exposure - .1f);
        }
        if (key == GLUT_KEY_HOME)
        {
            effects = Renderer::PostProcessSettings{};
        }
    }
} // namespace

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitContextVersion(3, 3);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitWindowSize(1280, 800);
    glutInitWindowPosition(80, 40);
    int window = glutCreateWindow("GSE01 - The Ember Remains / Rendering Prototype");
    if (window <= 0)
    {
        return 1;
    }
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glewExperimental = GL_TRUE;
    GLenum result = glewInit();
    if (result != GLEW_OK || !GLEW_VERSION_3_3)
    {
        std::cerr << "OpenGL 3.3 is required. GLEW: " << glewGetErrorString(result) << '\n';
        glutDestroyWindow(window);
        return 1;
    }
    // GLEW may query a deprecated enum while discovering a core context.
    while (glGetError() != GL_NO_ERROR)
    {
    }
    renderer.reset(new Renderer(1280, 800));
    if (!renderer->IsInitialized())
    {
        std::cerr << "Renderer initialization failed. See shader log above.\n";
        renderer.reset();
        glutDestroyWindow(window);
        return 1;
    }
    game.reset(new Game());
    performance.reset(new PerformanceLog(*renderer));
    std::cout << "WASD/arrows: move | Shift: run | E: interact | K, Enter: succession demo\n"
              << "F3: chunk overlay | F5: save | Esc: save and exit\n";
    std::cout << "F6: post FX | F7: bloom | F8: vignette | F9: edge blur\n"
              << "F10: compare batching | PageUp/PageDown: exposure | Home: reset effects\n";
    glutIgnoreKeyRepeat(1);
    glutDisplayFunc(Display);
    glutReshapeFunc(Resize);
    glutKeyboardFunc(Key);
    glutSpecialFunc(Special);
    glutCloseFunc(Close);
    previous = std::chrono::steady_clock::now();
    glutTimerFunc(16, Tick, 0);
    glutMainLoop();
    // The close callback owns GPU cleanup. No OpenGL calls after context destruction.
    return 0;
}

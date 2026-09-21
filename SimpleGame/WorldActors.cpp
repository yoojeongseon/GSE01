#include "stdafx.h"
#include "Game.h"
#include "WorldGeneration.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

namespace
{
    using WorldGeneration::ChunkSize;
    using WorldGeneration::Hash;
    using WorldGeneration::Road;

    // Generation version 1. Keep this function stable for existing prototype saves.

    TileKey Tile(WorldPoint p)
    {
        return {static_cast<std::int64_t>(std::floor(p.x)),
                static_cast<std::int64_t>(std::floor(p.y))};
    }

    double Distance(WorldPoint a, WorldPoint b)
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    Color Tint(Color c, float f)
    {
        return {c.r * f, c.g * f, c.b * f, c.a};
    }

    const Color Ink(.055f, .073f, .087f), Gold(.85f, .65f, .35f), Paper(.82f, .84f, .77f);

    void Diamond(Renderer& r, Point p, float w, float h, Color c)
    {
        r.Quad({p.x, p.y - h}, {p.x + w, p.y}, {p.x, p.y + h}, {p.x - w, p.y}, c);
    }

    void Box(Renderer& r, Point p, float w, float d, float h, Color c)
    {
        r.Quad(
            {p.x - w, p.y - d}, {p.x, p.y}, {p.x, p.y - h}, {p.x - w, p.y - d - h}, Tint(c, .72f));
        r.Quad(
            {p.x, p.y}, {p.x + w, p.y - d}, {p.x + w, p.y - d - h}, {p.x, p.y - h}, Tint(c, .9f));
        Diamond(r, {p.x, p.y - d - h}, w, d, c);
    }

} // namespace

void Game::DrawObject(Renderer& r, const Object& o)
{
    Point p = Project(o.p);
    if (p.x < -180 || p.x > 1460 || p.y < -50 || p.y > 1080)
    {
        return;
    }
    float x = p.x, y = p.y;
    float fade = 1.f;
    if (o.kind == Kind::Tree || o.kind == Kind::House)
    {
        Point hero = Project(Player());
        if (std::abs(hero.x - x) < 65 && hero.y < y && hero.y > y - 150)
        {
            fade = .42f;
        }
    }
    r.Ellipse(x + 7,
              y + 3,
              o.kind == Kind::House ? 68.f : 20.f,
              o.kind == Kind::House ? 25.f : 8.f,
              {.025f, .04f, .05f, .3f});
    switch (o.kind)
    {
    case Kind::Tree:
    {
        float height = 72.f + float(o.variation % 35);
        r.Rect(x - 4, y - 33, 8, 34, {.18f, .16f, .13f, fade});
        for (int i = 0; i < 3; ++i)
        {
            float top = y - height - i * 17, bottom = y - 15 - i * 25, half = 36.f - i * 6;
            r.Triangle({x, top},
                       {x - half, bottom},
                       {x + half, bottom},
                       {.07f + i * .013f, .15f + i * .016f, .145f + i * .013f, fade});
            r.Triangle({x, top},
                       {x, bottom},
                       {x + half, bottom},
                       {.12f + i * .014f, .22f + i * .018f, .195f + i * .015f, fade});
        }
        break;
    }
    case Kind::Rock:
        Box(r, {x, y}, 16, 8, 12, {.38f, .42f, .41f});
        break;
    case Kind::House:
    {
        Box(r, {x, y}, 59, 28, 65, {.39f, .37f, .30f, fade});
        r.Quad({x - 69, y - 90},
               {x, y - 128},
               {x, y - 67},
               {x - 69, y - 35},
               {.20f, .23f, .24f, fade});
        r.Quad({x, y - 128},
               {x + 69, y - 90},
               {x + 69, y - 35},
               {x, y - 67},
               {.29f, .31f, .30f, fade});
        for (int i = 1; i < 5; ++i)
        {
            float t = i / 5.f;
            r.Line({x, y - 128 + 61 * t}, {x + 69, y - 90 + 55 * t}, 2, {.16f, .19f, .20f, fade});
        }
        r.Rect(x - 30, y - 39, 16, 29, {.105f, .115f, .115f, fade});
        r.Rect(x + 20, y - 50, 15, 20, {3.2f, 1.65f, .42f, fade});
        r.Line({x + 27, y - 50}, {x + 27, y - 30}, 2, Ink);
        r.Line({x + 20, y - 40}, {x + 35, y - 40}, 2, Ink);
        Box(r, {x + 34, y - 104}, 9, 5, 31, {.32f, .34f, .32f, fade});
        for (int i = 0; i < 4; ++i)
        {
            float t = std::fmod(time_ * .3f + i * .25f, 1.f);
            r.Ellipse(x + 34 + t * 15,
                      y - 145 - t * 55,
                      7 + t * 12,
                      5 + t * 8,
                      {.52f, .56f, .54f, (1 - t) * .12f});
        }
        break;
    }
    case Kind::Ruin:
        Box(r, {x - 17, y}, 16, 9, 65, {.34f, .40f, .40f});
        Box(r, {x + 27, y - 5}, 13, 8, 48, {.36f, .41f, .40f});
        Box(r, {x - 4, y - 60}, 29, 8, 15, {.43f, .47f, .44f});
        r.Line({x - 21, y - 48}, {x - 13, y - 36}, 2, Ink);
        r.Line({x - 13, y - 36}, {x - 19, y - 24}, 2, Ink);
        Box(r, {x + 18, y + 8}, 10, 6, 7, {.29f, .34f, .32f});
        break;
    case Kind::Shrine:
    {
        bool found = discoveries_.count(Tile(o.p)) != 0;
        Box(r, {x, y}, 18, 10, 8, {.33f, .40f, .39f});
        Box(r, {x, y - 8}, 8, 5, 35, {.44f, .52f, .49f});
        Diamond(r, {x, y - 36}, 4, 7, found ? Gold : Color(1.2f, 2.f, 1.9f));
        if (found)
        {
            for (int i = 0; i < 5; ++i)
            {
                r.Rect(x - 18 + i * 8, y + 3 + (i % 2) * 4, 2, 5, {.35f, .43f, .28f});
                r.Ellipse(x - 17 + i * 8, y + 3 + (i % 2) * 4, 3, 2, {.84f, .72f, .48f});
            }
        }
        break;
    }
    case Kind::Fire:
    {
        for (int i = 0; i < 8; ++i)
        {
            float a = i * 6.2831853f / 8;
            r.Ellipse(x + std::cos(a) * 19, y + std::sin(a) * 9, 6, 4, {.37f, .38f, .33f});
        }
        r.Line({x - 13, y - 2}, {x + 11, y + 4}, 5, {.22f, .14f, .09f});
        r.Line({x + 12, y - 2}, {x - 9, y + 4}, 5, {.28f, .17f, .10f});
        float flicker = std::sin(time_ * 8) * 3;
        r.Triangle({x - 12, y}, {x - 4, y - 31 - flicker}, {x + 10, y}, {4.f, 1.15f, .2f});
        r.Triangle({x - 5, y}, {x + 4, y - 22 + flicker}, {x + 9, y}, {5.f, 2.2f, .48f});
        r.Triangle({x - 4, y}, {x, y - 14}, {x + 5, y}, {7.f, 4.2f, 1.7f});
        for (int i = 0; i < 6; ++i)
        {
            float t = std::fmod(time_ * .4f + i * .17f, 1.f);
            r.Rect(
                x + std::sin(i * 4.f + t * 6) * 9, y - 12 - t * 53, 2, 2, {3.5f, 1.5f, .3f, 1 - t});
        }
        break;
    }
    case Kind::Villager:
    case Kind::Heir:
    case Kind::Player:
    {
        bool hero = o.kind == Kind::Player, keeper = o.kind == Kind::Villager;
        Color cloak = hero     ? Color(.40f, .55f, .55f)
                      : keeper ? Color(.56f, .37f, .23f)
                               : Color(.44f, .43f, .60f);
        if (o.kind == Kind::Heir && o.variation == 1)
        {
            cloak = {.54f, .52f, .32f};
        }
        if (o.kind == Kind::Heir && o.variation == 2)
        {
            cloak = {.32f, .51f, .47f};
        }
        float step = hero ? std::sin(walk_) * 2 : std::sin(time_ * 1.7f) * .5f;
        r.Rect(x - 7, y - 9, 5, 10 + step, Ink);
        r.Rect(x + 2, y - 9, 5, 10 - step, Ink);
        r.Triangle({x, y - 41}, {x - 14, y - 6}, {x + 14, y - 6}, Tint(cloak, .65f));
        r.Quad({x - 8, y - 33}, {x + 8, y - 33}, {x + 10, y - 8}, {x - 9, y - 8}, cloak);
        r.Ellipse(x, y - 37, 9, 10, Tint(cloak, .8f));
        r.Rect(x - 4, y - 38, 8, 8, {.71f, .61f, .47f});
        r.Rect(x - 4, y - 39, 8, 3, Ink);
        r.Line({x + 13, y - 25}, {x + 17, y + 1}, 2, {.48f, .37f, .23f});
        if (hero)
        {
            r.Rect(x + 11, y - 21, 6, 8, {2.8f, 1.5f, .4f});
            Diamond(r, {x, y - 59}, 4, 3, Gold);
        }
        break;
    }
    }
}

void WorldActor::Draw(Renderer& r, const SceneContext& context, ActorPosition world) const
{
    WorldActor object = *this;
    object.p = world;
    context.game->DrawObject(r, object);
}

void WorldActor::Update(float dt, SceneContext& context)
{
    auto& game = *context.game;
    if (kind != WorldKind::Player || game.deathPrompt_)
    {
        return;
    }
    bool up = context.up, down = context.down, left = context.left, right = context.right,
         run = context.run;

    double sx = double(right) - double(left), sy = double(down) - double(up);
    double length = std::hypot(sx, sy);
    if (length > 0)
    {
        sx /= length;
        sy /= length;
        // Inverse isometric basis: WASD follows screen directions.
        double dx = (sx + sy) * .70710678118, dy = (sy - sx) * .70710678118;
        double step = dt * (run ? 4.8 : 2.8);
        WorldPoint old = p;
        WorldPoint next = {p.x + dx * step, p.y};
        if (!game.Blocked(next))
        {
            p = next;
        }
        next = {p.x, p.y + dy * step};
        if (!game.Blocked(next))
        {
            p = next;
        }
        double travelled = Distance(old, p);
        game.distance_ += travelled;
        game.walk_ += static_cast<float>(travelled) * 5;
    }
}

void WorldTileActor::Draw(Renderer& r, const SceneContext& context, ActorPosition world) const
{
    auto tx = static_cast<std::int64_t>(std::floor(world.x));
    auto ty = static_cast<std::int64_t>(std::floor(world.y));
    Point p = context.game->Project(world);
    if (p.x < -50 || p.x > 1330 || p.y < -30 || p.y > 830)
    {
        return;
    }
    auto hash = Hash(tx, ty);
    float variation = float(hash % 12) * .003f;
    bool clearing = std::hypot(double(tx), double(ty)) < 5;
    Color ground = clearing ? Color(.23f + variation, .245f + variation, .21f + variation)
                            : Color(.13f + variation, .20f + variation, .18f + variation);
    if (Road(tx, ty))
    {
        ground = {.28f + variation, .275f + variation, .24f + variation};
    }
    Diamond(r, p, 42.2f, 21.2f, ground);
    if (Road(tx, ty) || clearing)
    {
        for (int stone = 0; stone < 3; ++stone)
        {
            float ox = float((hash >> (stone * 8)) % 35) - 17;
            float oy = float((hash >> (stone * 8 + 4)) % 13) - 6;
            r.Ellipse(p.x + ox, p.y + oy, 3, 1.3f, Tint(ground, .8f));
        }
    }
    else if (hash % 3 == 0)
    {
        r.Line({p.x - 4, p.y + 2}, {p.x - 6, p.y - 4}, 1, {.25f, .32f, .24f});
        r.Line({p.x, p.y + 2}, {p.x + 3, p.y - 3}, 1, {.20f, .29f, .23f});
    }
}

void ChunkBoundaryActor::Draw(Renderer& r, const SceneContext& context, ActorPosition world) const
{
    if (!context.game->debug_)
    {
        return;
    }
    double cx = world.x, cy = world.y;

    Point a = context.game->Project({double(cx), double(cy)}),
          b = context.game->Project({double(cx + 8), double(cy)});
    Point c = context.game->Project({double(cx + 8), double(cy + 8)}),
          d = context.game->Project({double(cx), double(cy + 8)});
    r.Line(a, b, 1, {.45f, .7f, .65f, .5f});
    r.Line(b, c, 1, {.45f, .7f, .65f, .5f});
    r.Line(c, d, 1, {.45f, .7f, .65f, .5f});
    r.Line(d, a, 1, {.45f, .7f, .65f, .5f});
}

void MistActor::Draw(Renderer& r, const SceneContext& context, ActorPosition world) const
{
    float time_ = context.game->time_;
    for (int i = 0; i < 5; ++i)
    {
        float x = std::fmod(time_ * 5 + i * 307.f, 1700.f) - 200;
        r.Ellipse(x, 540 + i * 31.f, 210, 17, {.52f, .62f, .60f, .018f});
    }
}

#include "stdafx.h"
#include "LevelOne.h"
#include <algorithm>
#include <cmath>

namespace
{

    void Diamond(Renderer& r, Point p, float w, float h, Color c)
    {
        r.Quad({p.x, p.y - h}, {p.x + w, p.y}, {p.x, p.y + h}, {p.x - w, p.y}, c);
    }

    const Color Paper(.85f, .86f, .78f), Gold(.85f, .65f, .35f), Ink(.045f, .06f, .07f);

    void Character(Renderer& r, Point p, float size, Color color, Color eyes)
    {
        r.Ellipse(p.x, p.y + 3, 15 * size, 6 * size, {0, 0, 0, .3f});
        r.Triangle({p.x, p.y - 38 * size}, {p.x - 13 * size, p.y}, {p.x + 13 * size, p.y}, color);
        r.Ellipse(p.x, p.y - 33 * size, 7 * size, 8 * size, color);
        r.Rect(p.x - 4 * size, p.y - 35 * size, 8 * size, 3 * size, eyes);
    }

} // namespace

void TileActor::Draw(Renderer& r, const SceneContext& context, ActorPosition world) const
{
    const auto& level = *context.level;
    Point p = level.Screen(world);
    if (p.x < -80 || p.x > 1360 || p.y < -80 || p.y > 950)
    {
        return;
    }
    if (!wall)
    {
        int x = int(this->p.x), y = int(this->p.y);
        float shade = float((x * 17 + y * 31) % 7) * .007f;
        Diamond(r, p, 32.2f, 16.2f, {.15f + shade, .21f + shade, .18f + shade});
        return;
    }
    r.Quad({p.x - 31, p.y},
           {p.x, p.y + 15},
           {p.x, p.y - 20},
           {p.x - 31, p.y - 35},
           {.20f, .26f, .25f});
    r.Quad({p.x, p.y + 15},
           {p.x + 31, p.y},
           {p.x + 31, p.y - 35},
           {p.x, p.y - 20},
           {.27f, .32f, .29f});
    Diamond(r, {p.x, p.y - 35}, 31, 15, {.34f, .39f, .34f});
}

void EntranceActor::Draw(Renderer& r, const SceneContext& context, ActorPosition world) const
{
    const auto& level = *context.level;
    Diamond(r, level.Screen(world), 68, 34, {.40f, .34f, .20f});
}

void RangeActor::Draw(Renderer& r, const SceneContext& context, ActorPosition world) const
{
    const auto& level = *context.level;
    for (int i = 0; i < 64; ++i)
    {
        float a = i * 6.2831853f / 64, b = (i + 1) * 6.2831853f / 64;
        ActorPosition pa{world.x + std::cos(a) * level.Range(),
                         world.y + std::sin(a) * level.Range()};
        ActorPosition pb{world.x + std::cos(b) * level.Range(),
                         world.y + std::sin(b) * level.Range()};
        r.Line(level.Screen(pa), level.Screen(pb), 1, {.55f, .69f, .60f, .12f});
    }
}

void WarningActor::Draw(Renderer& r, const SceneContext& context, ActorPosition world) const
{
    const auto& level = *context.level;
    const auto* ePtr = dynamic_cast<const EnemyActor*>(level.scene_.Find(owner_));
    if (!ePtr || ePtr->warning <= 0)
    {
        return;
    }
    const auto& e = *ePtr;

    Point p = level.Screen(e.impact);
    r.Ellipse(p.x, p.y, 126, 63, {2.f, .12f, .06f, .22f});
    r.Ellipse(p.x,
              p.y,
              126 * (1 - e.warning / 1.2f),
              63 * (1 - e.warning / 1.2f),
              {2.2f, .22f, .05f, .2f});
}

void PlayerActor::Draw(Renderer& r, const SceneContext& context, ActorPosition world) const
{
    const auto& level = *context.level;
    Color color(.39f, .59f, .60f);
    if (level.immunity_ > 0 && int(level.time_ * 12) % 2)
    {
        color = {.9f, .82f, .6f};
    }
    Character(r, level.Screen(world), 1.f, color, Gold);
}

void EnemyActor::Draw(Renderer& r, const SceneContext& context, ActorPosition world) const
{
    Point p = context.level->Screen(world);
    float size = type == 2 ? 1.9f : 1.f;
    Color color = type == 2   ? Color(.57f, .24f, .15f)
                  : type == 0 ? Color(.49f, .37f, .34f)
                              : Color(.36f, .42f, .48f);
    Character(r, p, size, color, {2.5f, .5f, .2f});
    float max = type == 2 ? 480.f : type == 0 ? 28.f : 42.f;
    r.Rect(p.x - 16 * size, p.y - 50 * size, 32 * size, 3, Ink);
    r.Rect(p.x - 16 * size,
           p.y - 50 * size,
           32 * size * std::clamp(hp / max, 0.f, 1.f),
           3,
           {.75f, .25f, .17f});
}

void LootActor::Draw(Renderer& r, const SceneContext& context, ActorPosition world) const
{
    const auto& level = *context.level;
    const auto& d = *this;

    Point p = level.Screen(world);
    Color c = d.type == 0   ? Color(.4f, 1.9f, 2.2f)
              : d.type == 1 ? Color(2.4f, 1.5f, .3f)
                            : Color(1.3f, .35f, .5f);
    Diamond(r, {p.x, p.y - 5 - std::sin(level.time_ * 3) * 2}, 5, 8, c);
}

void ProjectileActor::Draw(Renderer& r, const SceneContext& context, ActorPosition world) const
{
    const auto& level = *context.level;
    const auto& s = *this;

    Point p = level.Screen(world),
          tail = level.Screen({world.x - s.velocity.x * .025f, world.y - s.velocity.y * .025f});
    r.Line({tail.x, tail.y - 14}, {p.x, p.y - 14}, 3, {3.f, 2.f, .6f});
}

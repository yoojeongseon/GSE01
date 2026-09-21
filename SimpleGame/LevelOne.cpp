#include "stdafx.h"
#include "LevelOne.h"
#include <algorithm>
#include <cmath>

namespace
{
    float Length(ActorPosition a, ActorPosition b)
    {
        return static_cast<float>(std::hypot(a.x - b.x, a.y - b.y));
    }

} // namespace

LevelOne::LevelOne() : random_(std::random_device{}())
{
    terrain_ = scene_.Create<Actor>(0);
    characters_ = scene_.Create<Actor>(0);
    effects_ = scene_.Create<Actor>(0);
    playerId_ = scene_.Create<PlayerActor>(characters_);
    scene_.Create<WeaponActor>(playerId_);
    scene_.Create<RangeActor>(playerId_);
    Generate();
    RebuildTerrain();
    Paths();
}

void LevelOne::AddEnemy(const Enemy& enemy)
{
    ActorId id = scene_.Create<Enemy>(characters_, enemy);
    scene_.Create<WarningActor>(id, id);
}

void LevelOne::RebuildTerrain()
{
    scene_.Destroy(terrain_);
    scene_.FlushDestroyed();
    terrain_ = scene_.Create<Actor>(0);
    for (int y = 0; y < Size; ++y)
    {
        for (int x = 0; x < Size; ++x)
        {
            scene_.Create<TileActor>(terrain_, x, y, false);
            if (walls_[y * Size + x])
            {
                scene_.Create<TileActor>(terrain_, x, y, true);
            }
        }
    }
    scene_.Create<EntranceActor>(terrain_);
}

void LevelOne::Update(float dt, bool up, bool down, bool left, bool right, bool run)
{
    if (Dead())
    {
        return;
    }
    time_ += dt;
    noticeTime_ = std::max(0.f, noticeTime_ - dt);
    immunity_ = std::max(0.f, immunity_ - dt);
    SceneContext context{this, nullptr, up, down, left, right, run};
    scene_.UpdatePhase(ActorPhase::Movement, dt, context);
    camera_.x += (Player().x - camera_.x) * (1 - std::exp(-9 * dt));
    camera_.y += (Player().y - camera_.y) * (1 - std::exp(-9 * dt));
    pathTime_ -= dt;
    if (pathTime_ <= 0)
    {
        Paths();
        pathTime_ = .2f;
    }
    spawn_ = std::max(-1.f, spawn_ - dt);
    if (!cleared_ && !bossSpawned_ && kills_ >= 18)
    {
        Spawn(true);
    }
    if (!cleared_ && spawn_ <= 0 && Enemies().size() < 9 && kills_ < 24)
    {
        Spawn(false);
        spawn_ = std::max(1.4f, 3.f - kills_ * .05f);
    }

    scene_.UpdatePhase(ActorPhase::Enemy, dt, context);
    if (Dead())
    {
        return;
    }
    scene_.UpdatePhase(ActorPhase::Weapon, dt, context);
    scene_.UpdatePhase(ActorPhase::Projectile, dt, context);
    scene_.RemoveIf<Shot>(
        [](const Shot& shot)
        {
            return shot.remaining <= .001f;
        });
    ResolveDeaths();
    scene_.UpdatePhase(ActorPhase::Loot, dt, context);
    scene_.RemoveIf<Drop>(
        [](const Drop& drop)
        {
            return drop.type < 0;
        });
    scene_.FlushDestroyed();
}

void LevelOne::Draw(Renderer& r)
{
    r.Begin();
    r.Rect(0, 0, 1280, 800, {.07f, .11f, .12f});
    scene_.Draw(r, SceneContext{this});
    r.BeginInterface();
    DrawInterface(r);
    r.End();
}

bool LevelOne::CanLeave() const
{
    return Length(Player(), {4.5f, 4.5f}) < 3;
}

void LevelOne::ReturnToEntrance()
{
    Player() = {4.5f, 4.5f};
    camera_ = Player();
    hp_ = MaxHP();
    immunity_ = 2;
    scene_.RemoveIf<Shot>(
        [](const Shot&)
        {
            return true;
        });
    scene_.FlushDestroyed();
    Paths();
}

Point LevelOne::Screen(Vec p) const
{
    return {static_cast<float>(640 + (p.x - camera_.x - p.y + camera_.y) * 32),
            static_cast<float>(390 + (p.x - camera_.x + p.y - camera_.y) * 16)};
}

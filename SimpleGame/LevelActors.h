#pragma once
#include "Actor.h"

class PlayerActor : public CloneableActor<PlayerActor>
{
public:
    PlayerActor()
    {
        p = {4.5, 4.5};
        updatePhase = ActorPhase::Movement;
    }

    void Update(float dt, SceneContext& context) override;
    void Draw(Renderer& r, const SceneContext& context, ActorPosition world) const override;
};

class WeaponActor : public CloneableActor<WeaponActor>
{
public:
    WeaponActor()
    {
        updatePhase = ActorPhase::Weapon;
    }

    void Update(float dt, SceneContext& context) override;
};

class EnemyActor : public CloneableActor<EnemyActor>
{
public:
    EnemyActor()
    {
        updatePhase = ActorPhase::Enemy;
    }

    float hp = 0;
    int type = 0;
    float attack = 2, warning = 0;
    ActorPosition impact;
    void Update(float dt, SceneContext& context) override;
    void Draw(Renderer& r, const SceneContext& context, ActorPosition world) const override;
};

class ProjectileActor : public CloneableActor<ProjectileActor>
{
public:
    ProjectileActor(ActorPosition position = {},
                    ActorPosition direction = {},
                    float range = 0,
                    float power = 0)
        : velocity(direction), remaining(range), damage(power)
    {
        p = position;
        layer = RenderLayer::Projectile;
        updatePhase = ActorPhase::Projectile;
    }

    ActorPosition velocity;
    float remaining = 0, damage = 0;
    void Update(float dt, SceneContext& context) override;
    void Draw(Renderer& r, const SceneContext& context, ActorPosition world) const override;
};

class LootActor : public CloneableActor<LootActor>
{
public:
    LootActor(ActorPosition position = {}, int kind = 0, bool magnet = false)
        : type(kind), attracted(magnet)
    {
        p = position;
        layer = RenderLayer::Loot;
        updatePhase = ActorPhase::Loot;
    }

    int type = 0;
    bool attracted = false;
    void Update(float dt, SceneContext& context) override;
    void Draw(Renderer& r, const SceneContext& context, ActorPosition world) const override;
};

class TileActor : public CloneableActor<TileActor>
{
public:
    TileActor(int x, int y, bool solid) : wall(solid)
    {
        p = {x + .5, y + .5};
        layer = solid ? RenderLayer::Objects : RenderLayer::Ground;
    }

    bool wall;
    void Draw(Renderer& r, const SceneContext& context, ActorPosition world) const override;
};

class EntranceActor : public CloneableActor<EntranceActor>
{
public:
    EntranceActor()
    {
        p = {4.5, 4.5};
        layer = RenderLayer::Decal;
    }

    void Draw(Renderer& r, const SceneContext& context, ActorPosition world) const override;
};

class RangeActor : public CloneableActor<RangeActor>
{
public:
    RangeActor()
    {
        layer = RenderLayer::Decal;
    }

    void Draw(Renderer& r, const SceneContext& context, ActorPosition world) const override;
};

class WarningActor : public CloneableActor<WarningActor>
{
public:
    explicit WarningActor(ActorId owner = 0) : owner_(owner)
    {
        layer = RenderLayer::Decal;
    }

    void Draw(Renderer& r, const SceneContext& context, ActorPosition world) const override;

private:
    ActorId owner_;
};

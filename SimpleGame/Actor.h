#pragma once
#include "Renderer.h"
#include <cstdint>
#include <memory>

class LevelOne;
class Game;
using ActorId = std::uint64_t;

struct ActorPosition
{
    double x = 0, y = 0;
};

struct SceneContext
{
    LevelOne* level = nullptr;
    Game* game = nullptr;
    bool up = false, down = false, left = false, right = false, run = false;
};

enum class RenderLayer
{
    Ground,
    Decal,
    Objects,
    Loot,
    Projectile,
    Atmosphere
};

enum class ActorPhase
{
    None = -1,
    Movement,
    Enemy,
    Weapon,
    Projectile,
    Loot
};

// A node owns local translation. SceneGraph owns identity, hierarchy and lifetime.
// Mesh geometry, animation and screen-space UI remain Renderer responsibilities.
class Actor
{
public:
    virtual ~Actor() = default;
    ActorPosition p;
    bool enabled = true;
    bool visible = true;
    RenderLayer layer = RenderLayer::Objects;
    ActorPhase updatePhase = ActorPhase::None;

    virtual std::unique_ptr<Actor> Clone() const
    {
        return std::make_unique<Actor>(*this);
    }

    virtual void Update(float dt, SceneContext& context)
    {
    }

    virtual void Draw(Renderer& renderer, const SceneContext& context, ActorPosition world) const
    {
    }
};

// Supplies value-copy cloning without slicing derived actor state.
template <class Derived> class CloneableActor : public Actor
{
public:
    std::unique_ptr<Actor> Clone() const override
    {
        return std::make_unique<Derived>(static_cast<const Derived&>(*this));
    }
};

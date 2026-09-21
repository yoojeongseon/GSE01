#pragma once
#include "Actor.h"
enum class WorldKind
{
    Tree,
    Rock,
    House,
    Ruin,
    Shrine,
    Fire,
    Villager,
    Heir,
    Player
};

class WorldActor : public CloneableActor<WorldActor>
{
public:
    WorldActor(ActorPosition position = {},
               WorldKind value = WorldKind::Tree,
               std::uint64_t seed = 0)
        : kind(value), variation(seed)
    {
        p = position;
        if (kind == WorldKind::Player)
        {
            updatePhase = ActorPhase::Movement;
        }
    }

    WorldKind kind;
    std::uint64_t variation;
    void Update(float dt, SceneContext& context) override;
    void Draw(Renderer& r, const SceneContext& context, ActorPosition world) const override;
};

class WorldTileActor : public CloneableActor<WorldTileActor>
{
public:
    explicit WorldTileActor(ActorPosition position)
    {
        p = position;
        layer = RenderLayer::Ground;
    }

    void Draw(Renderer& r, const SceneContext& context, ActorPosition world) const override;
};

class ChunkBoundaryActor : public CloneableActor<ChunkBoundaryActor>
{
public:
    ChunkBoundaryActor()
    {
        layer = RenderLayer::Decal;
    }

    void Draw(Renderer& r, const SceneContext& context, ActorPosition world) const override;
};

class MistActor : public CloneableActor<MistActor>
{
public:
    MistActor()
    {
        layer = RenderLayer::Atmosphere;
    }

    void Draw(Renderer& r, const SceneContext& context, ActorPosition world) const override;
};

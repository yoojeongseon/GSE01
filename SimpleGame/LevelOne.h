#pragma once
#include "SceneGraph.h"
#include "LevelActors.h"
#include <array>
#include <istream>
#include <ostream>
#include <random>
#include <vector>

// A connected farming tutorial region, not a limit on the surrounding open world.
class LevelOne
{
public:
    using Vec = ActorPosition;

    LevelOne();
    void Update(float dt, bool up, bool down, bool left, bool right, bool run);
    void Draw(Renderer& renderer);
    void Write(std::ostream& out) const;
    bool Read(std::istream& in);

    bool Dead() const
    {
        return hp_ <= 0;
    }

    bool CanLeave() const;
    void ReturnToEntrance();

    double Travelled() const
    {
        return travelled_;
    }

    void Notify(const std::string& text)
    {
        notice_ = text;
        noticeTime_ = 8;
    }

private:
    friend class PlayerActor;
    friend class WeaponActor;
    friend class EnemyActor;
    friend class ProjectileActor;
    friend class LootActor;
    friend class TileActor;
    friend class EntranceActor;
    friend class RangeActor;
    friend class WarningActor;
    static constexpr int Size = 36;

    SceneGraph scene_;
    ActorId terrain_ = 0, characters_ = 0, effects_ = 0, playerId_ = 0;
    std::array<int, Size * Size> walls_{};
    std::array<int, Size * Size> paths_{};
    using Enemy = EnemyActor;
    using Shot = ProjectileActor;
    using Drop = LootActor;

    Vec& Player()
    {
        return scene_.Get<PlayerActor>(playerId_).p;
    }

    const Vec& Player() const
    {
        return scene_.Get<PlayerActor>(playerId_).p;
    }

    auto Enemies()
    {
        return scene_.Query<Enemy>();
    }

    auto Enemies() const
    {
        return scene_.Query<Enemy>();
    }

    auto Shots()
    {
        return scene_.Query<Shot>();
    }

    auto Shots() const
    {
        return scene_.Query<Shot>();
    }

    auto Drops()
    {
        return scene_.Query<Drop>();
    }

    auto Drops() const
    {
        return scene_.Query<Drop>();
    }

    void AddEnemy(const Enemy& enemy);
    void RebuildTerrain();
    void ResolveDeaths();
    void DrawInterface(Renderer& r);
    std::mt19937 random_;
    Vec camera_{4.5f, 4.5f};
    float hp_ = 100, fire_ = 0, spawn_ = 2, immunity_ = 0, pathTime_ = 0, time_ = 0;
    int level_ = 1, xp_ = 0, weapon_ = 0, kills_ = 0, picked_ = 0, healed_ = 0;
    bool bossSpawned_ = false, cleared_ = false;
    double travelled_ = 0;
    std::string notice_ = "이동하며 적을 사거리 안에 두세요. 조준과 발사는 자동입니다.";
    float noticeTime_ = 10;
    void Generate();
    bool Connected() const;
    bool Walkable(Vec p) const;
    bool Sight(Vec a, Vec b) const;
    void Move(Vec& p, Vec delta);
    void Paths();
    void Spawn(bool boss);
    void Experience(int value);
    void Hit(float damage);
    Point Screen(Vec p) const;
    float MaxHP() const;
    float Damage() const;
    float Cooldown() const;
    float Range() const;
    int RequiredXP() const;
};

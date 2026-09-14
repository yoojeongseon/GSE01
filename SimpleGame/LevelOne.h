#pragma once
#include "Renderer.h"
#include <array>
#include <istream>
#include <ostream>
#include <random>
#include <vector>

// A connected farming tutorial region, not a limit on the surrounding open world.
class LevelOne
{
public:
    struct Vec
    {
        float x = 0, y = 0;
    };

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
    static constexpr int Size = 36;

    struct Enemy
    {
        Vec p;
        float hp = 0;
        int type = 0;
        float attack = 2, warning = 0;
        Vec impact;
    };

    struct Shot
    {
        Vec p, velocity;
        float remaining = 0, damage = 0;
    };

    struct Drop
    {
        Vec p;
        int type = 0;
        bool attracted = false;
    };

    std::array<int, Size * Size> walls_{};
    std::array<int, Size * Size> paths_{};
    std::vector<Enemy> enemies_;
    std::vector<Shot> shots_;
    std::vector<Drop> drops_;
    std::mt19937 random_;
    Vec player_{4.5f, 4.5f}, camera_{4.5f, 4.5f};
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

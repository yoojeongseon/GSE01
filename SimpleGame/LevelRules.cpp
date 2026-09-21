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

float LevelOne::MaxHP() const
{
    return 100.f + 15.f * (level_ - 1);
}

float LevelOne::Damage() const
{
    return 12.f + 3.f * (level_ - 1) + 2.f * weapon_;
}

float LevelOne::Cooldown() const
{
    return std::max(.18f, .8f * std::pow(.93f, float(level_ - 1)));
}

float LevelOne::Range() const
{
    return 6.f + std::min(2.f, .1f * (level_ - 1));
}

int LevelOne::RequiredXP() const
{
    return 25 + 20 * (level_ - 1);
}

void LevelOne::Spawn(bool boss)
{
    std::vector<Vec> candidates;
    for (int y = 2; y < Size - 2; ++y)
    {
        for (int x = 2; x < Size - 2; ++x)
        {
            Vec p{x + .5f, y + .5f};
            float d = Length(p, Player());
            if (d < 9 || d > 15 || walls_[y * Size + x] || paths_[y * Size + x] < 0)
            {
                continue;
            }
            bool free = true;
            for (const auto& e : Enemies())
            {
                if (Length(e.p, p) < 1.5f)
                {
                    free = false;
                }
            }
            if (free)
            {
                candidates.push_back(p);
            }
        }
    }
    if (candidates.empty())
    {
        return;
    }
    Enemy enemy;
    enemy.p = candidates[random_() % candidates.size()];
    enemy.type = boss ? 2 : int(random_() % 2);
    enemy.hp = boss ? 480.f : (enemy.type == 0 ? 28.f : 42.f);
    enemy.impact = enemy.p;
    AddEnemy(enemy);
    if (boss)
    {
        bossSpawned_ = true;
        notice_ = "보스: 재의 파수꾼이 깨어났습니다. 붉은 원 밖으로 피하세요!";
        noticeTime_ = 8;
    }
}

void LevelOne::Experience(int value)
{
    if (level_ >= 30)
    {
        return;
    }
    xp_ += value;
    while (level_ < 30 && xp_ >= RequiredXP())
    {
        xp_ -= RequiredXP();
        ++level_;
        hp_ = std::min(MaxHP(), hp_ + 30.f);
        notice_ = "레벨 상승! 공격력·최대 체력 증가, 발사 간격 감소. 체력을 조금 회복했습니다.";
        noticeTime_ = 5;
    }
    if (level_ == 30)
    {
        xp_ = 0;
    }
}

void LevelOne::Hit(float damage)
{
    if (immunity_ > 0 || Dead())
    {
        return;
    }
    hp_ = std::max(0.f, hp_ - damage);
    immunity_ = .85f;
}

void LevelOne::ResolveDeaths()
{
    for (const auto& enemy : Enemies())
    {
        if (enemy.hp > 0)
        {
            continue;
        }
        if (enemy.type == 2)
        {
            cleared_ = true;
            Experience(100);
            notice_ = "레벨 1 완료! 재의 파수꾼을 쓰러뜨렸습니다. 입구로 돌아가 L을 누르세요.";
            noticeTime_ = 15;
            scene_.Create<Drop>(effects_, enemy.p, 1, false);
            scene_.Create<Drop>(effects_, enemy.p, 2, false);
        }
        else
        {
            ++kills_;
            Experience(8);
            if (kills_ % 3 == 0 || kills_ == 1)
            {
                scene_.Create<Drop>(effects_, enemy.p, 1, false);
            }
            if (kills_ % 5 == 0 || kills_ == 2)
            {
                scene_.Create<Drop>(effects_, enemy.p, 2, false);
            }
        }
        scene_.Create<Drop>(effects_, enemy.p, 0, false);
    }
    scene_.RemoveIf<Enemy>(
        [](const Enemy& enemy)
        {
            return enemy.hp <= 0;
        });
}

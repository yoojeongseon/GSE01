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

void PlayerActor::Update(float dt, SceneContext& context)
{
    auto& level = *context.level;
    bool up = context.up, down = context.down, left = context.left, right = context.right,
         run = context.run;
    float x = float(right) - float(left), y = float(down) - float(up), length = std::hypot(x, y);
    if (length > 0)
    {
        float step = dt * (run ? 4.6f : 3.2f) / length * .70710678f;
        ActorPosition before = level.Player();
        level.Move(level.Player(), {(x + y) * step, (y - x) * step});
        level.travelled_ += Length(before, level.Player());
    }
}

void EnemyActor::Update(float dt, SceneContext& context)
{
    auto& level = *context.level;
    auto& enemy = *this;

    if (enemy.hp <= 0)
    {
        return;
    }
    bool boss = enemy.type == 2;
    if (boss && enemy.warning > 0)
    {
        enemy.warning = std::max(0.f, enemy.warning - dt);
        if (enemy.warning <= 0)
        {
            if (Length(level.Player(), enemy.impact) < 2.8f)
            {
                level.Hit(28);
            }
            enemy.attack = 3.5f;
        }
    }
    else
    {
        ActorPosition goal = level.Player();
        if (!level.Sight(enemy.p, level.Player()))
        {
            int cx = int(enemy.p.x), cy = int(enemy.p.y),
                best = level.paths_[cy * LevelOne::Size + cx];
            goal = {cx + .5f, cy + .5f};
            for (auto d : {ActorPosition{1, 0},
                           ActorPosition{-1, 0},
                           ActorPosition{0, 1},
                           ActorPosition{0, -1}})
            {
                int nx = cx + int(d.x), ny = cy + int(d.y);
                if (nx < 1 || ny < 1 || nx >= LevelOne::Size - 1 || ny >= LevelOne::Size - 1)
                {
                    continue;
                }
                int cost = level.paths_[ny * LevelOne::Size + nx];
                if (cost >= 0 && (best < 0 || cost < best))
                {
                    best = cost;
                    goal = {nx + .5f, ny + .5f};
                }
            }
        }
        float distance = Length(enemy.p, goal);
        if (distance > .1f)
        {
            float speed = boss ? .95f : enemy.type == 0 ? 1.55f : 1.05f;
            float step = std::min(distance, speed * dt) / distance;
            level.Move(enemy.p, {(goal.x - enemy.p.x) * step, (goal.y - enemy.p.y) * step});
        }
        if (boss)
        {
            enemy.attack = std::max(-1.f, enemy.attack - dt);
            if (enemy.attack <= 0 && Length(enemy.p, level.Player()) < 5)
            {
                enemy.impact = level.Player();
                enemy.warning = 1.2f;
            }
        }
    }
    if (Length(enemy.p, level.Player()) < (boss ? .85f : .6f))
    {
        level.Hit(boss ? 16.f : 8.f);
    }
}

void WeaponActor::Update(float dt, SceneContext& context)
{
    auto& level = *context.level;

    level.fire_ = std::max(0.f, level.fire_ - dt);
    if (level.fire_ <= 0)
    {
        const EnemyActor* target = nullptr;
        float nearest = level.Range();
        for (const auto& enemy : level.Enemies())
        {
            float d = Length(level.Player(), enemy.p);
            if (enemy.hp > 0 && d < nearest && level.Sight(level.Player(), enemy.p))
            {
                nearest = d;
                target = &enemy;
            }
        }
        if (target)
        {
            float d = std::max(.01f, nearest);
            level.scene_.Create<ProjectileActor>(
                level.effects_,
                level.Player(),
                ActorPosition{(target->p.x - level.Player().x) / d * 13,
                              (target->p.y - level.Player().y) / d * 13},
                level.Range(),
                level.Damage());
            level.fire_ = level.Cooldown();
        }
    }
}

void ProjectileActor::Update(float dt, SceneContext& context)
{
    auto& level = *context.level;
    auto& shot = *this;
    const auto enemies = level.Enemies();

    float travel = std::min(13 * dt, shot.remaining);
    int steps = std::max(1, int(travel / .1f) + 1);
    for (int i = 0; i < steps && shot.remaining > 0; ++i)
    {
        float amount = travel / steps;
        ActorPosition next{shot.p.x + shot.velocity.x / 13 * amount,
                           shot.p.y + shot.velocity.y / 13 * amount};
        if (!level.Sight(shot.p, next))
        {
            shot.remaining = 0;
            break;
        }
        shot.p = next;
        shot.remaining -= amount;
        for (auto& enemy : enemies)
        {
            if (enemy.hp > 0 && Length(shot.p, enemy.p) < (enemy.type == 2 ? .8f : .45f))
            {
                enemy.hp -= shot.damage;
                shot.remaining = 0;
                break;
            }
        }
    }
}

void LootActor::Update(float dt, SceneContext& context)
{
    auto& level = *context.level;
    auto& drop = *this;

    float d = Length(drop.p, level.Player());
    if (d < 3.5f + std::min(2.5f, level.level_ * .08f))
    {
        drop.attracted = true;
    }
    // Magical drops fly over obstacles, so loot cannot get stuck behind a wall.
    if (drop.attracted && d > .2f)
    {
        float step = std::min(d, dt * (6.f + d * 2)) / d;
        drop.p.x += (level.Player().x - drop.p.x) * step;
        drop.p.y += (level.Player().y - drop.p.y) * step;
    }
    if (Length(drop.p, level.Player()) < .35f)
    {
        ++level.picked_;
        if (drop.type == 0)
        {
            level.Experience(12);
        }
        if (drop.type == 1)
        {
            level.weapon_ = std::min(25, level.weapon_ + 1);
            level.notice_ = "무기 강화 +1! 발사체 피해가 증가했습니다.";
            level.noticeTime_ = 3;
        }
        if (drop.type == 2)
        {
            level.hp_ = std::min(level.MaxHP(), level.hp_ + 35);
            ++level.healed_;
        }
        drop.type = -1;
    }
}

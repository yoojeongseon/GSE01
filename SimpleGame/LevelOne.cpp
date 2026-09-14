#include "stdafx.h"
#include "LevelOne.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <queue>
#include <sstream>

namespace
{
    float Length(LevelOne::Vec a, LevelOne::Vec b)
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    void Diamond(Renderer& r, Point p, float w, float h, Color c)
    {
        r.Quad({p.x, p.y - h}, {p.x + w, p.y}, {p.x, p.y + h}, {p.x - w, p.y}, c);
    }

    const Color Paper(.85f, .86f, .78f), Gold(.85f, .65f, .35f), Ink(.045f, .06f, .07f);
} // namespace

LevelOne::LevelOne() : random_(std::random_device{}())
{
    Generate();
    Paths();
}

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

bool LevelOne::Connected() const
{
    std::array<bool, Size * Size> seen{};
    std::queue<int> work;
    work.push(4 * Size + 4);
    seen[4 * Size + 4] = true;
    while (!work.empty())
    {
        int cell = work.front();
        work.pop();
        int x = cell % Size, y = cell / Size;
        for (auto d : {Vec{1, 0}, Vec{-1, 0}, Vec{0, 1}, Vec{0, -1}})
        {
            int nx = x + int(d.x), ny = y + int(d.y);
            if (nx < 1 || ny < 1 || nx >= Size - 1 || ny >= Size - 1)
            {
                continue;
            }
            int next = ny * Size + nx;
            if (!walls_[next] && !seen[next])
            {
                seen[next] = true;
                work.push(next);
            }
        }
    }
    for (int y = 1; y < Size - 1; ++y)
    {
        for (int x = 1; x < Size - 1; ++x)
        {
            if (!walls_[y * Size + x] && !seen[y * Size + x])
            {
                return false;
            }
        }
    }
    return true;
}

void LevelOne::Generate()
{
    for (int y = 0; y < Size; ++y)
    {
        for (int x = 0; x < Size; ++x)
        {
            walls_[y * Size + x] = (x == 0 || y == 0 || x == Size - 1 || y == Size - 1) ? 1 : 0;
        }
    }
    for (int attempt = 0; attempt < 230; ++attempt)
    {
        int x = 1 + int(random_() % (Size - 2)), y = 1 + int(random_() % (Size - 2));
        // Reserve entrance, broad central roads and the boss approach.
        if ((x < 9 && y < 9) || x % 8 <= 1 || y % 8 <= 1 || (x > 26 && y > 26))
        {
            continue;
        }
        int index = y * Size + x;
        if (walls_[index])
        {
            continue;
        }
        walls_[index] = 1;
        if (!Connected())
        {
            walls_[index] = 0;
        }
    }
}

bool LevelOne::Walkable(Vec p) const
{
    if (!std::isfinite(p.x) || !std::isfinite(p.y))
    {
        return false;
    }
    constexpr float radius = .23f;
    for (float x : {p.x - radius, p.x + radius})
    {
        for (float y : {p.y - radius, p.y + radius})
        {
            if (x < 1 || y < 1 || x >= Size - 1 || y >= Size - 1 || walls_[int(y) * Size + int(x)])
            {
                return false;
            }
        }
    }
    return true;
}

bool LevelOne::Sight(Vec a, Vec b) const
{
    if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(b.x) || !std::isfinite(b.y))
    {
        return false;
    }
    int steps = std::max(1, int(Length(a, b) / .12f) + 1);
    for (int i = 0; i <= steps; ++i)
    {
        float t = float(i) / steps;
        float x = a.x + (b.x - a.x) * t, y = a.y + (b.y - a.y) * t;
        if (x < 1 || y < 1 || x >= Size - 1 || y >= Size - 1 || walls_[int(y) * Size + int(x)])
        {
            return false;
        }
    }
    return true;
}

void LevelOne::Move(Vec& p, Vec delta)
{
    Vec next{p.x + delta.x, p.y};
    if (Walkable(next))
    {
        p = next;
    }
    next = {p.x, p.y + delta.y};
    if (Walkable(next))
    {
        p = next;
    }
}

void LevelOne::Paths()
{
    paths_.fill(-1);
    int start = int(player_.y) * Size + int(player_.x);
    if (start < 0 || start >= Size * Size)
    {
        return;
    }
    std::queue<int> work;
    work.push(start);
    paths_[start] = 0;
    while (!work.empty())
    {
        int cell = work.front();
        work.pop();
        int x = cell % Size, y = cell / Size;
        for (auto d : {Vec{1, 0}, Vec{-1, 0}, Vec{0, 1}, Vec{0, -1}})
        {
            int nx = x + int(d.x), ny = y + int(d.y);
            if (nx < 1 || ny < 1 || nx >= Size - 1 || ny >= Size - 1)
            {
                continue;
            }
            int next = ny * Size + nx;
            if (!walls_[next] && paths_[next] < 0)
            {
                paths_[next] = paths_[cell] + 1;
                work.push(next);
            }
        }
    }
}

void LevelOne::Spawn(bool boss)
{
    std::vector<Vec> candidates;
    for (int y = 2; y < Size - 2; ++y)
    {
        for (int x = 2; x < Size - 2; ++x)
        {
            Vec p{x + .5f, y + .5f};
            float d = Length(p, player_);
            if (d < 9 || d > 15 || walls_[y * Size + x] || paths_[y * Size + x] < 0)
            {
                continue;
            }
            bool free = true;
            for (const auto& e : enemies_)
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
    enemies_.push_back(enemy);
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

void LevelOne::Update(float dt, bool up, bool down, bool left, bool right, bool run)
{
    if (Dead())
    {
        return;
    }
    time_ += dt;
    noticeTime_ = std::max(0.f, noticeTime_ - dt);
    immunity_ = std::max(0.f, immunity_ - dt);
    float x = float(right) - float(left), y = float(down) - float(up), length = std::hypot(x, y);
    if (length > 0)
    {
        float step = dt * (run ? 4.6f : 3.2f) / length * .70710678f;
        Vec before = player_;
        Move(player_, {(x + y) * step, (y - x) * step});
        travelled_ += Length(before, player_);
    }
    camera_.x += (player_.x - camera_.x) * (1 - std::exp(-9 * dt));
    camera_.y += (player_.y - camera_.y) * (1 - std::exp(-9 * dt));
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
    if (!cleared_ && spawn_ <= 0 && enemies_.size() < 9 && kills_ < 24)
    {
        Spawn(false);
        spawn_ = std::max(1.4f, 3.f - kills_ * .05f);
    }

    for (auto& enemy : enemies_)
    {
        if (enemy.hp <= 0)
        {
            continue;
        }
        bool boss = enemy.type == 2;
        if (boss && enemy.warning > 0)
        {
            enemy.warning = std::max(0.f, enemy.warning - dt);
            if (enemy.warning <= 0)
            {
                if (Length(player_, enemy.impact) < 2.8f)
                {
                    Hit(28);
                }
                enemy.attack = 3.5f;
            }
        }
        else
        {
            Vec goal = player_;
            if (!Sight(enemy.p, player_))
            {
                int cx = int(enemy.p.x), cy = int(enemy.p.y), best = paths_[cy * Size + cx];
                goal = {cx + .5f, cy + .5f};
                for (auto d : {Vec{1, 0}, Vec{-1, 0}, Vec{0, 1}, Vec{0, -1}})
                {
                    int nx = cx + int(d.x), ny = cy + int(d.y);
                    if (nx < 1 || ny < 1 || nx >= Size - 1 || ny >= Size - 1)
                    {
                        continue;
                    }
                    int cost = paths_[ny * Size + nx];
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
                Move(enemy.p, {(goal.x - enemy.p.x) * step, (goal.y - enemy.p.y) * step});
            }
            if (boss)
            {
                enemy.attack = std::max(-1.f, enemy.attack - dt);
                if (enemy.attack <= 0 && Length(enemy.p, player_) < 5)
                {
                    enemy.impact = player_;
                    enemy.warning = 1.2f;
                }
            }
        }
        if (Length(enemy.p, player_) < (boss ? .85f : .6f))
        {
            Hit(boss ? 16.f : 8.f);
        }
    }
    if (Dead())
    {
        return;
    }

    fire_ = std::max(0.f, fire_ - dt);
    if (fire_ <= 0)
    {
        const Enemy* target = nullptr;
        float nearest = Range();
        for (const auto& enemy : enemies_)
        {
            float d = Length(player_, enemy.p);
            if (enemy.hp > 0 && d < nearest && Sight(player_, enemy.p))
            {
                nearest = d;
                target = &enemy;
            }
        }
        if (target)
        {
            float d = std::max(.01f, nearest);
            shots_.push_back(
                {player_,
                 {(target->p.x - player_.x) / d * 13, (target->p.y - player_.y) / d * 13},
                 Range(),
                 Damage()});
            fire_ = Cooldown();
        }
    }
    for (auto& shot : shots_)
    {
        float travel = std::min(13 * dt, shot.remaining);
        int steps = std::max(1, int(travel / .1f) + 1);
        for (int i = 0; i < steps && shot.remaining > 0; ++i)
        {
            float amount = travel / steps;
            Vec next{shot.p.x + shot.velocity.x / 13 * amount,
                     shot.p.y + shot.velocity.y / 13 * amount};
            if (!Sight(shot.p, next))
            {
                shot.remaining = 0;
                break;
            }
            shot.p = next;
            shot.remaining -= amount;
            for (auto& enemy : enemies_)
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
    shots_.erase(std::remove_if(shots_.begin(),
                                shots_.end(),
                                [](const Shot& s)
                                {
                                    return s.remaining <= .001f;
                                }),
                 shots_.end());
    for (const auto& enemy : enemies_)
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
            drops_.push_back({enemy.p, 1, false});
            drops_.push_back({enemy.p, 2, false});
        }
        else
        {
            ++kills_;
            Experience(8);
            if (kills_ % 3 == 0 || kills_ == 1)
            {
                drops_.push_back({enemy.p, 1, false});
            }
            if (kills_ % 5 == 0 || kills_ == 2)
            {
                drops_.push_back({enemy.p, 2, false});
            }
        }
        drops_.push_back({enemy.p, 0, false});
    }
    enemies_.erase(std::remove_if(enemies_.begin(),
                                  enemies_.end(),
                                  [](const Enemy& e)
                                  {
                                      return e.hp <= 0;
                                  }),
                   enemies_.end());
    for (auto& drop : drops_)
    {
        float d = Length(drop.p, player_);
        if (d < 3.5f + std::min(2.5f, level_ * .08f))
        {
            drop.attracted = true;
        }
        // Magical drops fly over obstacles, so loot cannot get stuck behind a wall.
        if (drop.attracted && d > .2f)
        {
            float step = std::min(d, dt * (6.f + d * 2)) / d;
            drop.p.x += (player_.x - drop.p.x) * step;
            drop.p.y += (player_.y - drop.p.y) * step;
        }
        if (Length(drop.p, player_) < .35f)
        {
            ++picked_;
            if (drop.type == 0)
            {
                Experience(12);
            }
            if (drop.type == 1)
            {
                weapon_ = std::min(25, weapon_ + 1);
                notice_ = "무기 강화 +1! 발사체 피해가 증가했습니다.";
                noticeTime_ = 3;
            }
            if (drop.type == 2)
            {
                hp_ = std::min(MaxHP(), hp_ + 35);
                ++healed_;
            }
            drop.type = -1;
        }
    }
    drops_.erase(std::remove_if(drops_.begin(),
                                drops_.end(),
                                [](const Drop& d)
                                {
                                    return d.type < 0;
                                }),
                 drops_.end());
}

bool LevelOne::CanLeave() const
{
    return Length(player_, {4.5f, 4.5f}) < 3;
}

void LevelOne::ReturnToEntrance()
{
    player_ = {4.5f, 4.5f};
    camera_ = player_;
    hp_ = MaxHP();
    immunity_ = 2;
    shots_.clear();
    Paths();
}

Point LevelOne::Screen(Vec p) const
{
    return {640 + (p.x - camera_.x - p.y + camera_.y) * 32,
            390 + (p.x - camera_.x + p.y - camera_.y) * 16};
}

void LevelOne::Draw(Renderer& r)
{
    r.Begin();
    r.Rect(0, 0, 1280, 800, {.07f, .11f, .12f});
    for (int y = 0; y < Size; ++y)
    {
        for (int x = 0; x < Size; ++x)
        {
            Point p = Screen({x + .5f, y + .5f});
            if (p.x < -80 || p.x > 1360 || p.y < -80 || p.y > 920)
            {
                continue;
            }
            float shade = float((x * 17 + y * 31) % 7) * .007f;
            Diamond(r, p, 32.2f, 16.2f, {.15f + shade, .21f + shade, .18f + shade});
        }
    }
    Point entrance = Screen({4.5f, 4.5f});
    Diamond(r, entrance, 68, 34, {.40f, .34f, .20f});
    // Flattened circle depicts the actual world-space attack range.
    Point hero = Screen(player_);
    for (int i = 0; i < 64; ++i)
    {
        float a = i * 6.2831853f / 64, b = (i + 1) * 6.2831853f / 64;
        Vec pa{player_.x + std::cos(a) * Range(), player_.y + std::sin(a) * Range()};
        Vec pb{player_.x + std::cos(b) * Range(), player_.y + std::sin(b) * Range()};
        r.Line(Screen(pa), Screen(pb), 1, {.55f, .69f, .60f, .12f});
    }
    for (const auto& e : enemies_)
    {
        if (e.warning > 0)
        {
            Point p = Screen(e.impact);
            r.Ellipse(p.x, p.y, 126, 63, {2.f, .12f, .06f, .22f});
            r.Ellipse(p.x,
                      p.y,
                      126 * (1 - e.warning / 1.2f),
                      63 * (1 - e.warning / 1.2f),
                      {2.2f, .22f, .05f, .2f});
        }
    }

    struct DrawItem
    {
        float depth;
        int type, index;
    };

    std::vector<DrawItem> order;
    for (int y = 0; y < Size; ++y)
    {
        for (int x = 0; x < Size; ++x)
        {
            if (walls_[y * Size + x])
            {
                order.push_back({float(x + y + 1), 0, y * Size + x});
            }
        }
    }
    for (size_t i = 0; i < enemies_.size(); ++i)
    {
        order.push_back({enemies_[i].p.x + enemies_[i].p.y, 1, int(i)});
    }
    order.push_back({player_.x + player_.y, 2, 0});
    std::stable_sort(order.begin(),
                     order.end(),
                     [](const DrawItem& a, const DrawItem& b)
                     {
                         return a.depth < b.depth;
                     });
    for (const auto& item : order)
    {
        if (item.type == 0)
        {
            int x = item.index % Size, y = item.index / Size;
            Point p = Screen({x + .5f, y + .5f});
            if (p.x < -80 || p.x > 1360 || p.y < -80 || p.y > 950)
            {
                continue;
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
        else
        {
            bool player = item.type == 2;
            const Enemy* e = player ? nullptr : &enemies_[item.index];
            Point p = player ? hero : Screen(e->p);
            float size = (!player && e->type == 2) ? 1.9f : 1.f;
            Color color = player         ? Color(.39f, .59f, .60f)
                          : e->type == 2 ? Color(.57f, .24f, .15f)
                          : e->type == 0 ? Color(.49f, .37f, .34f)
                                         : Color(.36f, .42f, .48f);
            if (player && immunity_ > 0 && int(time_ * 12) % 2)
            {
                color = {.9f, .82f, .6f};
            }
            r.Ellipse(p.x, p.y + 3, 15 * size, 6 * size, {0, 0, 0, .3f});
            r.Triangle(
                {p.x, p.y - 38 * size}, {p.x - 13 * size, p.y}, {p.x + 13 * size, p.y}, color);
            r.Ellipse(p.x, p.y - 33 * size, 7 * size, 8 * size, color);
            r.Rect(p.x - 4 * size,
                   p.y - 35 * size,
                   8 * size,
                   3 * size,
                   player ? Gold : Color(2.5f, .5f, .2f));
            if (!player)
            {
                float max = e->type == 2 ? 480.f : e->type == 0 ? 28.f : 42.f;
                r.Rect(p.x - 16 * size, p.y - 50 * size, 32 * size, 3, Ink);
                r.Rect(p.x - 16 * size,
                       p.y - 50 * size,
                       32 * size * std::clamp(e->hp / max, 0.f, 1.f),
                       3,
                       {.75f, .25f, .17f});
            }
        }
    }
    for (const auto& d : drops_)
    {
        Point p = Screen(d.p);
        Color c = d.type == 0   ? Color(.4f, 1.9f, 2.2f)
                  : d.type == 1 ? Color(2.4f, 1.5f, .3f)
                                : Color(1.3f, .35f, .5f);
        Diamond(r, {p.x, p.y - 5 - std::sin(time_ * 3) * 2}, 5, 8, c);
    }
    for (const auto& s : shots_)
    {
        Point p = Screen(s.p),
              tail = Screen({s.p.x - s.velocity.x * .025f, s.p.y - s.velocity.y * .025f});
        r.Line({tail.x, tail.y - 14}, {p.x, p.y - 14}, 3, {3.f, 2.f, .6f});
    }
    r.BeginInterface();
    r.Rect(0, 0, 1280, 107, {.025f, .04f, .05f, .95f});
    r.Text(28, 14, "레벨 1 · 잿빛 수확지", Paper, 2.2f);
    r.Text(28, 47, "자동 조준 · 자동 발사 / 사거리 안에서 적을 상대하세요", Gold, 1.4f);
    r.Rect(28, 82, 270, 10, {.2f, .13f, .13f});
    r.Rect(28, 82, 270 * hp_ / MaxHP(), 10, {.7f, .24f, .18f});
    r.Text(315,
           74,
           "체력 " + std::to_string(int(hp_)) + " / " + std::to_string(int(MaxHP())),
           Paper,
           1.3f);
    std::ostringstream stats;
    stats << "레벨 " << level_ << " / 강화 +" << weapon_ << " / 피해 " << int(Damage())
          << " / 간격 " << std::fixed << std::setprecision(2) << Cooldown() << "초";
    r.Text(590, 16, stats.str(), Paper, 1.35f);
    r.Text(590,
           46,
           "경험치 " + std::to_string(xp_) + " / " + std::to_string(RequiredXP()) + " / 처치 " +
               std::to_string(kills_) + " / 습득 " + std::to_string(picked_),
           Gold,
           1.3f);
    r.Rect(590, 83, 580, 7, Ink);
    r.Rect(590, 83, 580 * float(xp_) / RequiredXP(), 7, {.25f, .64f, .66f});
    std::string goal = cleared_       ? "완료: 입구로 돌아가 L로 귀환"
                       : kills_ == 0  ? "1. 적을 처치해 경험치와 드랍 획득"
                       : weapon_ == 0 ? "2. 노란 강화석을 자동 습득"
                       : level_ < 3   ? "3. 영혼석을 모아 레벨 3 달성"
                       : kills_ < 18  ? "4. 일반 적 18마리 처치"
                                      : "5. 재의 파수꾼 처치";
    r.Rect(22, 121, 510, 36, {.025f, .04f, .05f, .85f});
    r.Text(34, 126, goal, Paper, 1.4f);
    for (const auto& e : enemies_)
    {
        if (e.type == 2)
        {
            r.Rect(640, 122, 600, 38, {.025f, .04f, .05f, .9f});
            r.Text(651, 123, "재의 파수꾼", Gold, 1.2f);
            r.Rect(650, 149, 580, 5, {.2f, .1f, .1f});
            r.Rect(650, 149, 580 * std::max(0.f, e.hp) / 480, 5, {.8f, .2f, .12f});
        }
    }
    if (CanLeave())
    {
        r.Text(entrance.x - 35, entrance.y - 30, "L  마을 귀환", Paper, 1.5f);
    }
    if (noticeTime_ > 0)
    {
        r.Rect(75, 666, 1130, 39, {.025f, .04f, .05f, .9f});
        r.Text(89, 673, notice_, Paper, 1.4f);
    }
    r.Rect(0, 733, 1280, 67, {.025f, .04f, .05f, .95f});
    r.Text(28,
           742,
           "WASD 이동 / Shift 달리기 / L 입구 귀환 / F5 저장 / F6~F9 후처리 / Esc 저장 후 종료",
           Paper,
           1.25f);
    r.Text(28,
           773,
           "청록: 영혼석(경험치) / 노랑: 무기 강화 / 붉은색: 체력 회복 / 가까이 가면 자동 습득",
           Gold,
           1.2f);
    if (Dead())
    {
        r.Rect(0, 0, 1280, 800, {0, 0, 0, .7f});
        r.Text(280, 335, "쓰러졌습니다. 하지만 삶은 이어집니다.", Gold, 2.2f);
        r.Text(280,
               390,
               "Enter: 전승 후 마을 귀환 / 실패 시 Enter 재시도 / Esc 저장 후 종료",
               Paper,
               1.4f);
        if (noticeTime_ > 0)
        {
            r.Text(140, 460, notice_, Paper, 1.25f);
        }
    }
    r.End();
}

void LevelOne::Write(std::ostream& out) const
{
    out << "LEVEL_ONE 1\n"
        << player_.x << ' ' << player_.y << ' ' << hp_ << ' ' << level_ << ' ' << xp_ << ' '
        << weapon_ << ' ' << kills_ << ' ' << picked_ << ' ' << healed_ << ' ' << bossSpawned_
        << ' ' << cleared_ << ' ' << travelled_ << '\n';
    out << fire_ << ' ' << spawn_ << ' ' << immunity_ << '\n';
    for (int v : walls_)
    {
        out << v << ' ';
    }
    out << '\n' << random_ << '\n';
    out << enemies_.size() << '\n';
    for (const auto& e : enemies_)
    {
        out << e.p.x << ' ' << e.p.y << ' ' << e.hp << ' ' << e.type << ' ' << e.attack << ' '
            << e.warning << ' ' << e.impact.x << ' ' << e.impact.y << '\n';
    }
    out << drops_.size() << '\n';
    for (const auto& d : drops_)
    {
        out << d.p.x << ' ' << d.p.y << ' ' << d.type << ' ' << d.attracted << '\n';
    }
    out << shots_.size() << '\n';
    for (const auto& s : shots_)
    {
        out << s.p.x << ' ' << s.p.y << ' ' << s.velocity.x << ' ' << s.velocity.y << ' '
            << s.remaining << ' ' << s.damage << '\n';
    }
}

bool LevelOne::Read(std::istream& in)
{
    std::string tag;
    int version = 0;
    if (!(in >> tag >> version) || tag != "LEVEL_ONE" || version != 1)
    {
        return false;
    }
    if (!(in >> player_.x >> player_.y >> hp_ >> level_ >> xp_ >> weapon_ >> kills_ >> picked_ >>
          healed_ >> bossSpawned_ >> cleared_ >> travelled_))
    {
        return false;
    }
    auto finite = [](float v)
    {
        return std::isfinite(v);
    };
    if (level_ < 1 || level_ > 30 || xp_ < 0 || xp_ >= RequiredXP() || weapon_ < 0 ||
        weapon_ > 25 || kills_ < 0 || kills_ > 40 || picked_ < 0 || healed_ < 0 || !finite(hp_) ||
        hp_ < 0 || hp_ > MaxHP() || !std::isfinite(travelled_) || travelled_ < 0)
    {
        return false;
    }
    if (!(in >> fire_ >> spawn_ >> immunity_) || !finite(fire_) || !finite(spawn_) ||
        !finite(immunity_) || std::abs(fire_) > 5 || std::abs(spawn_) > 100000 ||
        std::abs(immunity_) > 5)
    {
        return false;
    }
    for (int& v : walls_)
    {
        if (!(in >> v) || (v != 0 && v != 1))
        {
            return false;
        }
    }
    for (int y = 0; y < Size; ++y)
    {
        for (int x = 0; x < Size; ++x)
        {
            if ((x == 0 || y == 0 || x == Size - 1 || y == Size - 1) && walls_[y * Size + x] != 1)
            {
                return false;
            }
        }
    }
    if (!Walkable(player_) || !Walkable({4.5f, 4.5f}) || !Connected() || !(in >> random_))
    {
        return false;
    }
    size_t count = 0;
    int bosses = 0;
    if (!(in >> count) || count > 10)
    {
        return false;
    }
    enemies_.clear();
    for (size_t i = 0; i < count; ++i)
    {
        Enemy e;
        if (!(in >> e.p.x >> e.p.y >> e.hp >> e.type >> e.attack >> e.warning >> e.impact.x >>
              e.impact.y) ||
            !Walkable(e.p) || !Walkable(e.impact) || !finite(e.hp) || e.hp <= 0 || e.hp > 480 ||
            e.type < 0 || e.type > 2 || !finite(e.attack) || std::abs(e.attack) > 100000 ||
            !finite(e.warning) || e.warning < 0 || e.warning > 1.21f)
        {
            return false;
        }
        if (e.type == 2)
        {
            ++bosses;
        }
        enemies_.push_back(e);
    }
    if (bosses > 1 || (cleared_ && (!bossSpawned_ || bosses)) ||
        (bossSpawned_ && !cleared_ && bosses != 1) || (!bossSpawned_ && bosses))
    {
        return false;
    }
    if (!(in >> count) || count > 200)
    {
        return false;
    }
    drops_.clear();
    for (size_t i = 0; i < count; ++i)
    {
        Drop d;
        if (!(in >> d.p.x >> d.p.y >> d.type >> d.attracted) || !finite(d.p.x) || !finite(d.p.y) ||
            d.p.x < 1 || d.p.y < 1 || d.p.x >= Size - 1 || d.p.y >= Size - 1 || d.type < 0 ||
            d.type > 2)
        {
            return false;
        }
        drops_.push_back(d);
    }
    if (!(in >> count) || count > 100)
    {
        return false;
    }
    shots_.clear();
    for (size_t i = 0; i < count; ++i)
    {
        Shot s;
        if (!(in >> s.p.x >> s.p.y >> s.velocity.x >> s.velocity.y >> s.remaining >> s.damage) ||
            !Sight(s.p, s.p) || !finite(s.velocity.x) || !finite(s.velocity.y) ||
            std::abs(s.velocity.x) > 13.1f || std::abs(s.velocity.y) > 13.1f ||
            !finite(s.remaining) || s.remaining <= 0 || s.remaining > 8.1f || !finite(s.damage) ||
            s.damage <= 0 || s.damage > 200)
        {
            return false;
        }
        shots_.push_back(s);
    }
    camera_ = player_;
    Paths();
    return true;
}

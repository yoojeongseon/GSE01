#include "stdafx.h"
#include "LevelOne.h"
#include <algorithm>
#include <cmath>
#include <queue>

namespace
{
    float Length(ActorPosition a, ActorPosition b)
    {
        return static_cast<float>(std::hypot(a.x - b.x, a.y - b.y));
    }

} // namespace

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
    int start = int(Player().y) * Size + int(Player().x);
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

#include "stdafx.h"
#include "LevelOne.h"
#include <algorithm>
#include <cmath>

void LevelOne::Write(std::ostream& out) const
{
    out << "LEVEL_ONE 1\n"
        << Player().x << ' ' << Player().y << ' ' << hp_ << ' ' << level_ << ' ' << xp_ << ' '
        << weapon_ << ' ' << kills_ << ' ' << picked_ << ' ' << healed_ << ' ' << bossSpawned_
        << ' ' << cleared_ << ' ' << travelled_ << '\n';
    out << fire_ << ' ' << spawn_ << ' ' << immunity_ << '\n';
    for (int v : walls_)
    {
        out << v << ' ';
    }
    out << '\n' << random_ << '\n';
    out << Enemies().size() << '\n';
    for (const auto& e : Enemies())
    {
        out << e.p.x << ' ' << e.p.y << ' ' << e.hp << ' ' << e.type << ' ' << e.attack << ' '
            << e.warning << ' ' << e.impact.x << ' ' << e.impact.y << '\n';
    }
    out << Drops().size() << '\n';
    for (const auto& d : Drops())
    {
        out << d.p.x << ' ' << d.p.y << ' ' << d.type << ' ' << d.attracted << '\n';
    }
    out << Shots().size() << '\n';
    for (const auto& s : Shots())
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
    if (!(in >> Player().x >> Player().y >> hp_ >> level_ >> xp_ >> weapon_ >> kills_ >> picked_ >>
          healed_ >> bossSpawned_ >> cleared_ >> travelled_))
    {
        return false;
    }
    auto finite = [](double v)
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
    if (!Walkable(Player()) || !Walkable({4.5f, 4.5f}) || !Connected() || !(in >> random_))
    {
        return false;
    }
    size_t count = 0;
    int bosses = 0;
    if (!(in >> count) || count > 10)
    {
        return false;
    }
    scene_.RemoveIf<Enemy>(
        [](const Enemy&)
        {
            return true;
        });
    scene_.FlushDestroyed();
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
        AddEnemy(e);
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
    scene_.RemoveIf<Drop>(
        [](const Drop&)
        {
            return true;
        });
    scene_.FlushDestroyed();
    for (size_t i = 0; i < count; ++i)
    {
        Drop d;
        if (!(in >> d.p.x >> d.p.y >> d.type >> d.attracted) || !finite(d.p.x) || !finite(d.p.y) ||
            d.p.x < 1 || d.p.y < 1 || d.p.x >= Size - 1 || d.p.y >= Size - 1 || d.type < 0 ||
            d.type > 2)
        {
            return false;
        }
        scene_.Create<Drop>(effects_, d);
    }
    if (!(in >> count) || count > 100)
    {
        return false;
    }
    scene_.RemoveIf<Shot>(
        [](const Shot&)
        {
            return true;
        });
    scene_.FlushDestroyed();
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
        scene_.Create<Shot>(effects_, s);
    }
    RebuildTerrain();
    camera_ = Player();
    Paths();
    return true;
}

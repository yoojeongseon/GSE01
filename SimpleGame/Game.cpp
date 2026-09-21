#include "stdafx.h"
#include "Game.h"
#include "WorldGeneration.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

namespace
{
    using WorldGeneration::ChunkSize;
    using WorldGeneration::Hash;
    using WorldGeneration::Road;

    // Generation version 1. Keep this function stable for existing prototype saves.

    TileKey Tile(WorldPoint p)
    {
        return {static_cast<std::int64_t>(std::floor(p.x)),
                static_cast<std::int64_t>(std::floor(p.y))};
    }

    TileKey ChunkAt(WorldPoint p)
    {
        return Tile({p.x / ChunkSize, p.y / ChunkSize});
    }

    double Distance(WorldPoint a, WorldPoint b)
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    const Color Ink(.055f, .073f, .087f), Gold(.85f, .65f, .35f), Paper(.82f, .84f, .77f);

    void Diamond(Renderer& r, Point p, float w, float h, Color c)
    {
        r.Quad({p.x, p.y - h}, {p.x + w, p.y}, {p.x, p.y + h}, {p.x - w, p.y}, c);
    }

    bool ValidPoint(WorldPoint p)
    {
        // Reject corrupted saves before converting coordinates to integer tile keys.
        return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) < 1.e12 &&
               std::abs(p.y) < 1.e12;
    }
} // namespace

Game::Game()
{
    const std::vector<Object> village = {{{-3, -1.5}, Kind::House, 0},
                                         {{1.5, -3}, Kind::House, 1},
                                         {{4, -.5}, Kind::House, 2},
                                         {{-4, 3}, Kind::Ruin, 0},
                                         {{5, 4}, Kind::Ruin, 1},
                                         {{0, 0}, Kind::Fire, 0},
                                         {{-.8, 1.2}, Kind::Villager, 0},
                                         {{-2, 4}, Kind::Shrine, 0}};
    villageRoot_ = scene_.Create<Actor>(0);
    heirRoot_ = scene_.Create<Actor>(0);
    scene_.Create<MistActor>(0);
    playerId_ = scene_.Create<WorldActor>(0, WorldPoint{1.5, 1.5}, Kind::Player, 0);
    for (const auto& object : village)
    {
        scene_.Create<WorldActor>(villageRoot_, object);
    }
    wchar_t executable[32768] = {};
    DWORD count = GetModuleFileNameW(nullptr, executable, 32768);
    if (count && count < 32768)
    {
        std::wstring path(executable, count);
        path = path.substr(0, path.find_last_of(L"\\/") + 1) + L"Saves";
        if (!CreateDirectoryW(path.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            saveBlocked_ = true;
            Message("저장 폴더에 쓸 수 없습니다. 진행 상황을 저장할 수 없습니다.");
        }
        savePath_ = path + L"\\prototype_v1.txt";
    }
    else
    {
        saveBlocked_ = true;
        Message("저장 경로를 찾을 수 없습니다. 진행 상황을 저장할 수 없습니다.");
    }
    if (!saveBlocked_ && !Load())
    {
        saveBlocked_ = true;
        Message(
            "저장 파일을 읽지 못했습니다. 기존 파일은 보존했습니다.\n프로토타입 안내서의 저장 복구 항목을 확인해 주세요.");
    }
    camera_ = Player();
    Stream();
}

void Game::Stream()
{
    TileKey center = ChunkAt(Player());
    // Fixed 7 x 7 working set, independent of the number of visited regions.
    for (auto it = chunks_.begin(); it != chunks_.end();)
    {
        if (std::abs(it->first.first - center.first) > 3 ||
            std::abs(it->first.second - center.second) > 3)
        {
            scene_.Destroy(it->second.root);
            it = chunks_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    for (int dy = -3; dy <= 3; ++dy)
    {
        for (int dx = -3; dx <= 3; ++dx)
        {
            TileKey key = {center.first + dx, center.second + dy};
            if (chunks_.count(key))
            {
                continue;
            }
            Chunk chunk;
            chunk.root = scene_.Create<Actor>(0);
            scene_.Get<Actor>(chunk.root).p = {double(key.first * ChunkSize),
                                               double(key.second * ChunkSize)};
            scene_.Create<ChunkBoundaryActor>(chunk.root);
            for (int y = 0; y < ChunkSize; ++y)
            {
                for (int x = 0; x < ChunkSize; ++x)
                {
                    std::int64_t tx = key.first * ChunkSize + x, ty = key.second * ChunkSize + y;
                    scene_.Create<WorldTileActor>(chunk.root, WorldPoint{x + .5, y + .5});
                    if ((std::abs(tx) < 7 && std::abs(ty) < 7) || Road(tx, ty))
                    {
                        continue;
                    }
                    auto hash = Hash(tx, ty);
                    WorldPoint p = {x + .5, y + .5};
                    if (hash % 157 == 0)
                    {
                        scene_.Create<WorldActor>(chunk.root, p, Kind::Shrine, hash);
                    }
                    else if (hash % 17 < 3)
                    {
                        scene_.Create<WorldActor>(chunk.root, p, Kind::Tree, hash);
                    }
                    else if (hash % 31 == 0)
                    {
                        scene_.Create<WorldActor>(chunk.root, p, Kind::Rock, hash);
                    }
                }
            }
            chunks_.emplace(key, std::move(chunk));
        }
    }

    // Heir records persist in saves; only nearby records have live scene nodes.
    for (auto it = heirActors_.begin(); it != heirActors_.end();)
    {
        const Actor* actor = scene_.Find(it->second);
        if (!actor || Distance(actor->p, Player()) >= 32)
        {
            scene_.Destroy(it->second);
            it = heirActors_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    for (const auto& heir : heirs_)
    {
        if (Distance(heir.p, Player()) < 32 && !heirActors_.count(heir.id))
        {
            std::uint64_t variation = heir.kindness > 0 ? 1 : heir.distance > 20 ? 2 : 0;
            heirActors_[heir.id] =
                scene_.Create<WorldActor>(heirRoot_, heir.p, Kind::Heir, variation);
        }
    }
    scene_.FlushDestroyed();
}

bool Game::Blocked(WorldPoint p) const
{
    auto collides = [&](const Object& o)
    {
        double radius = 0;
        switch (o.kind)
        {
        case Kind::Tree:
            radius = .36;
            break;
        case Kind::Rock:
            radius = .36;
            break;
        case Kind::House:
            radius = 1.12;
            break;
        case Kind::Ruin:
            radius = .6;
            break;
        case Kind::Shrine:
            radius = .38;
            break;
        case Kind::Fire:
            radius = .34;
            break;
        default:
            return false;
        }
        return Distance(p, o.p) < radius + .18;
    };
    bool blocked = false;
    scene_.Visit<WorldActor>(
        [&](const WorldActor& actor, WorldPoint world)
        {
            Object object = actor;
            object.p = world;
            blocked = blocked || collides(object);
        });
    return blocked;
}

void Game::Update(float dt, bool up, bool down, bool left, bool right, bool run)
{
    time_ += dt;
    autosave_ += dt;
    messageTime_ = std::max(0.f, messageTime_ - dt);
    if (levelActive_)
    {
        double before = levelOne_.Travelled();
        levelOne_.Update(dt, up, down, left, right, run);
        distance_ += levelOne_.Travelled() - before;
        if (autosave_ >= 10)
        {
            autosave_ = 0;
            Save();
        }
        return;
    }
    SceneContext context{nullptr, this, up, down, left, right, run};
    scene_.UpdatePhase(ActorPhase::Movement, dt, context);

    // Limit camera lag so the loaded region always covers the visible canvas.
    float follow = 1.f - std::exp(-9.f * dt);
    camera_.x += (Player().x - camera_.x) * follow;
    camera_.y += (Player().y - camera_.y) * follow;
    Stream();
    if (autosave_ >= 10)
    {
        autosave_ = 0;
        Save();
    }
}

Point Game::Project(WorldPoint p) const
{
    double x = p.x - camera_.x, y = p.y - camera_.y;
    return {640.f + static_cast<float>((x - y) * 42), 390.f + static_cast<float>((x + y) * 21)};
}

void Game::Draw(Renderer& r)
{
    if (levelActive_)
    {
        levelOne_.Draw(r);
        return;
    }
    Stream();
    r.Begin();
    r.Rect(0, 0, 1280, 800, {.10f, .15f, .16f});
    scene_.Draw(r, SceneContext{nullptr, this});
    // Ground mist belongs to the HDR scene. Edge effects are now screen-space passes.

    r.BeginInterface();
    // World-anchored prompts are UI as well: keep Hangul sharp at the screen edges.
    scene_.Visit<WorldActor>(
        [&](const WorldActor& actor, WorldPoint world)
        {
            Object o = actor;
            o.p = world;

            Point p = Project(o.p);
            if (o.kind == Kind::Shrine && Distance(Player(), o.p) < 1.7)
            {
                r.Text(p.x - 33,
                       p.y - 72,
                       discoveries_.count(Tile(o.p)) ? "기억된 장소" : "E  살펴보기",
                       Paper,
                       1.5f);
            }
            if ((o.kind == Kind::Villager || o.kind == Kind::Heir) && Distance(Player(), o.p) < 1.8)
            {
                r.Text(p.x - 27,
                       p.y - 61,
                       o.kind == Kind::Villager ? "E  불지기" : "E  이어진 삶",
                       Paper,
                       1.5f);
            }
        });
    Interface(r);
    r.End();
}

void Game::Interface(Renderer& r)
{
    r.Rect(0, 0, 1280, 94, {.035f, .055f, .065f, .94f});
    r.Rect(30, 92, 1220, 1, {.67f, .56f, .34f, .5f});
    r.Text(32, 20, "남겨진 불씨", Paper, 2.4f);
    r.Text(34, 57, "GSE01 / 하나의 삶이 다른 삶을 남긴다", Gold, 1.5f);
    std::string region = Distance(Player(), {0, 0}) < 8 ? "불씨 쉼터" : "적막의 숲";
    r.Text(855, 24, region, Paper, 2);
    r.Text(855,
           52,
           "삶 " + std::to_string(nextLife_) + " / 이어진 삶 " + std::to_string(heirs_.size()),
           Gold,
           1.5f);

    r.Rect(30, 118, 235, 84, {.035f, .055f, .065f, .85f});
    r.Rect(30, 118, 3, 84, Gold);
    r.Text(45, 132, "작은 친절", Gold, 1.5f);
    r.Text(45, 155, gift_ ? "품에 간직한 나무 새" : "불지기를 만나 보세요", Paper, 1.5f);
    r.Text(45, 177, "기억한 장소 " + std::to_string(discoveries_.size()), {.53f, .68f, .64f}, 1.5f);
    r.Rect(290, 118, 385, 48, {.035f, .055f, .065f, .85f});
    r.Text(303, 127, "L  레벨 1: 잿빛 수확지 입장", Gold, 1.5f);

    // Local compass map; icons represent actual nearby world objects.
    r.Rect(1110, 117, 140, 140, {.035f, .055f, .065f, .88f});
    r.Text(1125, 128, "주변", Gold, 1.5f);
    r.Line({1125, 188}, {1235, 188}, 1, {.25f, .32f, .31f});
    r.Line({1180, 152}, {1180, 238}, 1, {.25f, .32f, .31f});
    auto marker = [&](WorldPoint p, Color color, float size)
    {
        float x = 1180 + static_cast<float>((p.x - Player().x) * 4);
        float y = 192 + static_cast<float>((p.y - Player().y) * 4);
        if (x > 1118 && x < 1242 && y > 151 && y < 245)
        {
            Diamond(r, {x, y}, size, size, color);
        }
    };
    marker({0, 0}, Gold, 4);
    for (const auto& h : heirs_)
    {
        if (Distance(h.p, Player()) < 22)
        {
            marker(h.p, {.63f, .56f, .79f}, 3);
        }
    }
    marker(Player(), Paper, 3);
    r.Text(1116, 270, "불씨 / 나 / 전승", {.59f, .66f, .62f}, 1.2f);

    if (messageTime_ > 0)
    {
        r.Rect(155, 612, 970, 86, {.025f, .04f, .05f, .95f});
        r.Rect(155, 612, 3, 86, Gold);
        r.Text(177, 628, message_, Paper, 1.5f);
    }
    r.Rect(0, 731, 1280, 69, {.035f, .055f, .065f, .96f});
    r.Rect(30, 731, 1220, 1, {.67f, .56f, .34f, .5f});
    r.Text(32,
           748,
           "WASD / 방향키  이동     Shift  달리기     E  대화·조사     K  삶의 전승",
           Paper,
           1.5f);
    r.Text(32,
           776,
           "F3  정보     F5  저장     F6  후처리 비교     Esc  저장 후 종료",
           {.49f, .59f, .57f},
           1.3f);
    if (saveBlocked_)
    {
        r.Text(851, 776, "저장 불가", {.94f, .49f, .34f}, 1.3f);
    }
    else
    {
        r.Text(851, 776, "10초마다 자동 저장", Gold, 1.3f);
    }
    if (debug_)
    {
        TileKey c = ChunkAt(Player());
        r.Rect(30, 218, 450, 50, {0, 0, 0, .7f});
        r.Text(42,
               230,
               "청크 " + std::to_string(c.first) + " : " + std::to_string(c.second),
               Paper,
               1.5f);
        r.Text(42,
               250,
               "불러온 청크 " + std::to_string(chunks_.size()) + " / 시드 20260908",
               Gold,
               1.5f);
        const auto& effects = r.Effects();
        r.Rect(30, 278, 480, 106, {0, 0, 0, .8f});
        std::ostringstream exposure;
        exposure << std::fixed << std::setprecision(2) << effects.exposure;
        r.Text(42,
               286,
               !r.PostProcessingAvailable() ? "후처리 사용 불가: 콘솔 로그 확인"
               : effects.enabled            ? "F6  HDR 후처리 켜짐"
                                            : "F6  후처리 꺼짐",
               Paper,
               1.4f);
        r.Text(42,
               310,
               std::string("F7 블룸 ") + (effects.bloom ? "켜짐" : "꺼짐") + " / F8 비넷 " +
                   (effects.vignette ? "켜짐" : "꺼짐"),
               Paper,
               1.3f);
        r.Text(42,
               334,
               std::string("F9 가장자리 블러 ") + (effects.edgeBlur ? "켜짐" : "꺼짐") +
                   " / 노출 " + exposure.str(),
               Paper,
               1.3f);
        r.Text(42, 358, "PageUp / PageDown 노출 조절 / Home 초기화", Gold, 1.3f);
    }
    if (deathPrompt_)
    {
        r.Rect(0, 0, 1280, 800, {.01f, .02f, .03f, .78f});
        r.Rect(260, 269, 760, 238, {.07f, .09f, .10f});
        r.Rect(260, 269, 760, 2, Gold);
        r.Text(303, 291, "하나의 죽음, 하나의 새 생명.", Gold, 2.5f);
        r.Text(303, 348, "당신이 베푼 마음과 걸어온 길이 새로운 존재에게 이어집니다.", Paper, 1.8f);
        r.Text(303, 380, "이번 시연에서는 모닥불 곁에서 다음 삶을 시작합니다.", Paper, 1.8f);
        r.Text(303, 449, "Enter  삶을 잇기          Esc  더 살아가기", Gold, 2);
    }
}

void Game::Message(const std::string& text)
{
    if (levelActive_)
    {
        levelOne_.Notify(text);
    }
    message_ = text;
    messageTime_ = 10;
}

void Game::Interact()
{
    const Heir* nearest = nullptr;
    double nearestDistance = 1.7;
    for (const auto& h : heirs_)
    {
        double d = Distance(Player(), h.p);
        if (d < nearestDistance)
        {
            nearest = &h;
            nearestDistance = d;
        }
    }
    if (nearest)
    {
        std::string id = "이어진 삶 " + std::to_string(nearest->id) + " - ";
        Message(
            id +
            (nearest->kindness > 0
                 ? "다정함은 남아서\n불가에 자리를 하나 비워 뒀어요. 누군가 추워할 것 같아서요."
             : nearest->distance > 20
                 ? "길이 부르는 곳\n가 본 적 없는 길이 자꾸 꿈에 나와요. 언젠가 함께 걸어 줄래요?"
                 : "고요한 마음\n이상하죠. 처음 온 곳인데... 오래 머물렀던 집 같아요."));
        return;
    }
    if (Distance(Player(), {-.8, 1.2}) < 1.9)
    {
        bool firstKindness = kindness_ == 0;
        kindness_ = 1;
        if (!gift_)
        {
            gift_ = true;
            Message(
                "불지기 — 온기를 찾아왔구나.\n이 나무 새를 가져가렴. 누군가는 집까지 데려가 줘야지.");
            Save();
        }
        else
        {
            Message(
                "불지기 — 그 작은 새, 아직 가지고 있구나.\n잘됐네. 불 옆자리는 너 앉으라고 남겨 뒀어.");
            if (firstKindness)
            {
                Save();
            }
        }
        return;
    }
    auto examine = [&](const Object& o)
    {
        if (o.kind != Kind::Shrine || Distance(Player(), o.p) > 1.7)
        {
            return false;
        }
        if (discoveries_.insert(Tile(o.p)).second)
        {
            Message(
                "이름 없는 기념비 곁에 작은 꽃이 뿌리를 내립니다.\n이곳은 당신이 다녀간 일을 기억할 것입니다.");
            Save();
        }
        else
        {
            Message("꽃은 아직 이 자리에 있습니다.\n우리가 떠나도, 어떤 것들은 남습니다.");
        }
        return true;
    };
    bool examined = false;
    scene_.Visit<WorldActor>(
        [&](const WorldActor& actor, WorldPoint world)
        {
            Object object = actor;
            object.p = world;
            if (!examined)
            {
                examined = examine(object);
            }
        });
    if (examined)
    {
        return;
    }
    Message(
        "조금 더 가까이 다가가 보세요.\n불지기나 이어진 삶, 희미하게 빛나는 기념비와 이야기를 나눌 수 있습니다.");
}

void Game::Action(unsigned char key)
{
    if (levelActive_ && levelOne_.Dead() && key == 13)
    {
        deathPrompt_ = true;
    }
    if (deathPrompt_)
    {
        if (key == 27)
        {
            deathPrompt_ = false;
        }
        if (key == 13)
        {
            if (saveBlocked_)
            {
                deathPrompt_ = false;
                Message(
                    "삶을 전승하려면 먼저 저장할 수 있어야 합니다.\n기존 기록은 그대로 보존되어 있습니다.");
                return;
            }
            WorldPoint oldPlayer = Player();
            int oldKindness = kindness_;
            double oldDistance = distance_;
            bool oldLevelActive = levelActive_;
            LevelOne oldLevel = levelOne_;
            if (levelActive_)
            {
                levelActive_ = false;
                levelOne_.ReturnToEntrance();
            }
            heirs_.push_back({nextLife_, Player(), kindness_, distance_});
            ++nextLife_;
            Player() = {1.5, 1.5};
            kindness_ = 0;
            distance_ = 0;
            // Commit the entire transition in one atomic replacement; roll back on failure.
            if (!Save())
            {
                --nextLife_;
                heirs_.pop_back();
                Player() = oldPlayer;
                kindness_ = oldKindness;
                distance_ = oldDistance;
                levelActive_ = oldLevelActive;
                levelOne_ = std::move(oldLevel);
                if (levelActive_)
                {
                    levelOne_.Notify(
                        "전승 저장에 실패했습니다. 기록은 보존했습니다. Enter로 다시 시도하세요.");
                }
            }
            else
            {
                camera_ = Player();
                Stream();
                Message(
                    "당신의 발걸음이 멎은 곳에서 새로운 삶이 눈을 뜹니다.\n그곳으로 돌아가 보세요. 당신의 무언가가 남아 있습니다.");
            }
            deathPrompt_ = false;
        }
        return;
    }
    if (key == 'l' || key == 'L')
    {
        if (levelActive_)
        {
            if (!levelOne_.Dead() && levelOne_.CanLeave())
            {
                levelActive_ = false;
                Save();
                Message("마을로 돌아왔습니다. 레벨 1의 성장과 전투 기록은 유지됩니다.");
            }
            else
            {
                levelOne_.Notify(
                    "귀환하려면 황금색 입구로 돌아가세요. 쓰러졌다면 Enter로 전승하세요.");
            }
        }
        else if (Distance(Player(), {0, 0}) < 8)
        {
            levelActive_ = true;
            Save();
            levelOne_.Notify(
                "레벨 1: 적에게 접근하면 자동 발사합니다. 가까운 드랍은 자동으로 끌려옵니다.");
        }
        else
        {
            Message("레벨 1에 들어가려면 마을의 모닥불 근처로 돌아오세요.");
        }
        return;
    }
    if (levelActive_)
    {
        if (key == 'p')
        {
            if (Save())
            {
                levelOne_.Notify("전투 진행과 랜덤 맵을 저장했습니다.");
            }
        }
        return;
    }
    if (key == 'e' || key == 'E')
    {
        Interact();
    }
    if (key == 'k' || key == 'K')
    {
        deathPrompt_ = true;
    }
    if (key == 'g')
    {
        debug_ = !debug_; // Internal dispatch for F3.
    }
    if (key == 'p')
    {
        if (Save())
        {
            Message("당신의 발걸음과 세계의 기억을 저장했습니다.");
        }
    }
}

bool Game::Save()
{
    if (saveBlocked_ || savePath_.empty())
    {
        return false;
    }
    std::wstring temp = savePath_ + L".tmp";
    std::ofstream file(temp.c_str(), std::ios::trunc);
    file << "GSE01_PROTO 2\n"
         << std::setprecision(17) << Player().x << ' ' << Player().y << ' ' << nextLife_ << ' '
         << kindness_ << ' ' << distance_ << ' ' << gift_ << '\n'
         << heirs_.size() << '\n';
    for (const auto& h : heirs_)
    {
        file << h.id << ' ' << h.p.x << ' ' << h.p.y << ' ' << h.kindness << ' ' << h.distance
             << '\n';
    }
    file << discoveries_.size() << '\n';
    for (const auto& p : discoveries_)
    {
        file << p.first << ' ' << p.second << '\n';
    }
    file << levelActive_ << '\n';
    levelOne_.Write(file);
    file.flush();
    bool ok = file.good();
    file.close();
    ok = ok && !file.fail();
    if (!ok || !MoveFileExW(temp.c_str(),
                            savePath_.c_str(),
                            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        Message(
            "저장하지 못했습니다. 저장 공간과 폴더 권한을 확인해 주세요.\n이전 저장 파일은 그대로 보존했습니다.");
        std::cerr << "Prototype save failed.\n";
        return false;
    }
    return true;
}

bool Game::Load()
{
    DWORD attributes = GetFileAttributesW(savePath_.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }
    std::ifstream file(savePath_.c_str());
    std::string magic;
    int version = 0, kindness = 0, gift = 0;
    WorldPoint position;
    std::uint64_t life = 0;
    double travelled = 0;
    std::size_t count = 0;
    if (!(file >> magic >> version) || magic != "GSE01_PROTO" || (version != 1 && version != 2))
    {
        return false;
    }
    if (!(file >> position.x >> position.y >> life >> kindness >> travelled >> gift))
    {
        return false;
    }
    if (!ValidPoint(position) || life < 1 || kindness < 0 || kindness > 1 || gift < 0 || gift > 1 ||
        !std::isfinite(travelled) || travelled < 0)
    {
        return false;
    }
    if (!(file >> count) || count > 100000 || life != count + 1)
    {
        return false;
    }
    std::vector<Heir> heirs;
    for (std::size_t i = 0; i < count; ++i)
    {
        Heir h;
        if (!(file >> h.id >> h.p.x >> h.p.y >> h.kindness >> h.distance) || h.id != i + 1 ||
            !ValidPoint(h.p) || h.kindness < 0 || h.kindness > 1 || !std::isfinite(h.distance) ||
            h.distance < 0)
        {
            return false;
        }
        heirs.push_back(h);
    }
    if (!(file >> count) || count > 1000000)
    {
        return false;
    }
    std::set<TileKey> discoveries;
    for (std::size_t i = 0; i < count; ++i)
    {
        TileKey key;
        if (!(file >> key.first >> key.second) ||
            !ValidPoint({double(key.first), double(key.second)}) || !discoveries.insert(key).second)
        {
            return false;
        }
    }
    LevelOne loadedLevel = levelOne_;
    bool loadedActive = false;
    if (version == 2 && (!(file >> loadedActive) || !loadedLevel.Read(file)))
    {
        return false;
    }
    file >> std::ws;
    if (!file.eof())
    {
        return false;
    }
    Player() = position;
    nextLife_ = life;
    kindness_ = kindness;
    distance_ = travelled;
    gift_ = gift != 0;
    heirs_ = std::move(heirs);
    discoveries_ = std::move(discoveries);
    levelOne_ = std::move(loadedLevel);
    levelActive_ = loadedActive;
    Message("돌아왔군요. 불씨는 아직 꺼지지 않았습니다.\n이어진 삶과 발견의 기록을 불러왔습니다.");
    return true;
}

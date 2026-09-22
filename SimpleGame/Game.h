#pragma once
#include "Renderer.h"
#include "LevelOne.h"
#include "WorldActors.h"
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

using WorldPoint = ActorPosition;

using TileKey = std::pair<std::int64_t, std::int64_t>;

class Game
{
public:
    Game();
    void Update(float dt, bool up, bool down, bool left, bool right, bool run);
    void Draw(Renderer& r);
    void Action(unsigned char key);
    bool Save();

    const char* ActiveSceneName() const
    {
        return levelActive_ ? "level_one" : "world";
    }

    bool IsDeathPrompt() const
    {
        return deathPrompt_;
    }

private:
    friend class MistActor;
    friend class WorldActor;
    friend class WorldTileActor;
    friend class ChunkBoundaryActor;
    using Kind = WorldKind;
    using Object = WorldActor;
    SceneGraph scene_;
    ActorId playerId_ = 0, villageRoot_ = 0, heirRoot_ = 0;
    std::map<std::uint64_t, ActorId> heirActors_;

    WorldPoint& Player()
    {
        return scene_.Get<WorldActor>(playerId_).p;
    }

    const WorldPoint& Player() const
    {
        return scene_.Get<WorldActor>(playerId_).p;
    }

    struct Heir
    {
        std::uint64_t id;
        WorldPoint p;
        int kindness;
        double distance;
    };

    struct Chunk
    {
        ActorId root = 0;
    };

    WorldPoint camera_{1.5, 1.5};
    std::map<TileKey, Chunk> chunks_;
    std::set<TileKey> discoveries_;
    std::vector<Heir> heirs_;
    std::wstring savePath_;
    std::uint64_t nextLife_ = 1;
    int kindness_ = 0;
    double distance_ = 0;
    float time_ = 0, autosave_ = 0, messageTime_ = 9, walk_ = 0;
    bool gift_ = false, deathPrompt_ = false, debug_ = false, saveBlocked_ = false;
    LevelOne levelOne_;
    bool levelActive_ = false;
    std::string message_ =
        "아직 작은 불씨가 남아 있습니다.\n모닥불 곁의 불지기에게 다가가 E를 눌러 보세요.";
    void Stream();
    bool Blocked(WorldPoint p) const;
    Point Project(WorldPoint p) const;
    void DrawObject(Renderer& r, const Object& object);
    void Interface(Renderer& r);
    void Interact();
    bool Load();
    void Message(const std::string& text);
};

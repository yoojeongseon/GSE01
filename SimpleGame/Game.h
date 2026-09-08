#pragma once
#include "Renderer.h"
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

struct WorldPoint { double x = 0, y = 0; };
using TileKey = std::pair<std::int64_t, std::int64_t>;

class Game {
public:
    Game();
    void Update(float dt, bool up, bool down, bool left, bool right, bool run);
    void Draw(Renderer& r);
    void Action(unsigned char key);
    bool Save();
    bool IsDeathPrompt() const { return deathPrompt_; }
private:
    enum class Kind { Tree, Rock, House, Ruin, Shrine, Fire, Villager, Heir, Player };
    struct Object { WorldPoint p; Kind kind; std::uint64_t variation; };
    struct Heir { std::uint64_t id; WorldPoint p; int kindness; double distance; };
    struct Chunk { std::vector<Object> objects; };

    WorldPoint player_{1.5, 1.5}, camera_{1.5, 1.5};
    std::map<TileKey, Chunk> chunks_;
    std::set<TileKey> discoveries_;
    std::vector<Heir> heirs_;
    std::vector<Object> village_;
    std::wstring savePath_;
    std::uint64_t nextLife_ = 1;
    int kindness_ = 0;
    double distance_ = 0;
    float time_ = 0, autosave_ = 0, messageTime_ = 9, walk_ = 0;
    bool gift_ = false, deathPrompt_ = false, debug_ = false, saveBlocked_ = false;
    std::string message_ = "아직 작은 불씨가 남아 있습니다.\n모닥불 곁의 불지기에게 다가가 E를 눌러 보세요.";
    void Stream();
    bool Blocked(WorldPoint p) const;
    Point Project(WorldPoint p) const;
    void Ground(Renderer& r);
    void DrawObject(Renderer& r, const Object& object);
    void Interface(Renderer& r);
    void Interact();
    bool Load();
    void Message(const std::string& text);
};


#pragma once
#include "Actor.h"
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

// Query snapshots contain references; do not retain them across FlushDestroyed().
template <class T> class ActorRange
{
public:
    std::vector<T*> items;

    struct Iterator
    {
        typename std::vector<T*>::const_iterator it;

        T& operator*() const
        {
            return **it;
        }

        Iterator& operator++()
        {
            ++it;
            return *this;
        }

        bool operator!=(const Iterator& other) const
        {
            return it != other.it;
        }
    };

    Iterator begin() const
    {
        return {items.begin()};
    }

    Iterator end() const
    {
        return {items.end()};
    }

    std::size_t size() const
    {
        return items.size();
    }
};

class SceneGraph
{
public:
    SceneGraph() = default;
    SceneGraph(const SceneGraph& other);
    SceneGraph& operator=(const SceneGraph& other);
    SceneGraph(SceneGraph&&) noexcept = default;
    SceneGraph& operator=(SceneGraph&&) noexcept = default;

    template <class T, class... Args> ActorId Create(ActorId parent, Args&&... args)
    {
        if (parent && !Find(parent))
        {
            throw std::invalid_argument("Actor parent does not exist");
        }
        ActorId id = nextId_++;
        nodes_.emplace(id, Node{parent, std::make_unique<T>(std::forward<Args>(args)...), false});
        return id;
    }

    Actor* Find(ActorId id);
    const Actor* Find(ActorId id) const;

    ActorId Parent(ActorId id) const
    {
        return nodes_.at(id).parent;
    }

    std::vector<ActorId> Children(ActorId parent) const
    {
        std::vector<ActorId> result;
        for (const auto& entry : nodes_)
        {
            if (entry.second.parent == parent && !IsDestroyed(entry.first))
            {
                result.push_back(entry.first);
            }
        }
        return result;
    }

    template <class T> T& Get(ActorId id)
    {
        return dynamic_cast<T&>(*nodes_.at(id).actor);
    }

    template <class T> const T& Get(ActorId id) const
    {
        return dynamic_cast<const T&>(*nodes_.at(id).actor);
    }

    template <class T> ActorRange<T> Query()
    {
        ActorRange<T> result;
        for (auto& entry : nodes_)
        {
            if (!IsDestroyed(entry.first))
            {
                if (auto* actor = dynamic_cast<T*>(entry.second.actor.get()))
                {
                    result.items.push_back(actor);
                }
            }
        }
        return result;
    }

    template <class T> ActorRange<const T> Query() const
    {
        ActorRange<const T> result;
        for (const auto& entry : nodes_)
        {
            if (!IsDestroyed(entry.first))
            {
                if (const auto* actor = dynamic_cast<const T*>(entry.second.actor.get()))
                {
                    result.items.push_back(actor);
                }
            }
        }
        return result;
    }

    template <class T, class Predicate> void RemoveIf(Predicate predicate)
    {
        for (auto& entry : nodes_)
        {
            auto* actor = dynamic_cast<T*>(entry.second.actor.get());
            if (actor && predicate(*actor))
            {
                Destroy(entry.first);
            }
        }
    }

    template <class T, class Visitor> void Visit(Visitor visitor) const
    {
        for (const auto& entry : nodes_)
        {
            if (IsEnabled(entry.first, false))
            {
                if (const auto* actor = dynamic_cast<const T*>(entry.second.actor.get()))
                {
                    visitor(*actor, WorldPosition(entry.first));
                }
            }
        }
    }

    // Reparent preserves world position by default and rejects cycles.
    bool SetParent(ActorId id, ActorId parent, bool keepWorld = true);
    ActorPosition WorldPosition(ActorId id) const;
    void Destroy(ActorId id);
    void FlushDestroyed();
    void UpdatePhase(ActorPhase phase, float dt, SceneContext& context);
    void Draw(Renderer& renderer, const SceneContext& context) const;

private:
    struct Node
    {
        ActorId parent;
        std::unique_ptr<Actor> actor;
        bool destroyed;
    };

    std::map<ActorId, Node> nodes_;
    ActorId nextId_ = 1;
    unsigned updateDepth_ = 0;
    bool IsDestroyed(ActorId id) const;
    bool IsEnabled(ActorId id, bool drawing) const;
};

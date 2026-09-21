#include "stdafx.h"
#include "SceneGraph.h"
#include <algorithm>

SceneGraph::SceneGraph(const SceneGraph& other) : nextId_(other.nextId_)
{
    for (const auto& entry : other.nodes_)
    {
        const Node& node = entry.second;
        nodes_.emplace(entry.first, Node{node.parent, node.actor->Clone(), node.destroyed});
    }
}

SceneGraph& SceneGraph::operator=(const SceneGraph& other)
{
    if (this != &other)
    {
        SceneGraph copy(other);
        *this = std::move(copy);
    }
    return *this;
}

bool SceneGraph::IsDestroyed(ActorId id) const
{
    while (id)
    {
        auto it = nodes_.find(id);
        if (it == nodes_.end() || it->second.destroyed)
        {
            return true;
        }
        id = it->second.parent;
    }
    return false;
}

Actor* SceneGraph::Find(ActorId id)
{
    return const_cast<Actor*>(static_cast<const SceneGraph&>(*this).Find(id));
}

const Actor* SceneGraph::Find(ActorId id) const
{
    auto it = nodes_.find(id);
    return it == nodes_.end() || IsDestroyed(id) ? nullptr : it->second.actor.get();
}

ActorPosition SceneGraph::WorldPosition(ActorId id) const
{
    ActorPosition result;
    while (id)
    {
        const Node& node = nodes_.at(id);
        result.x += node.actor->p.x;
        result.y += node.actor->p.y;
        id = node.parent;
    }
    return result;
}

bool SceneGraph::SetParent(ActorId id, ActorId parent, bool keepWorld)
{
    if (!Find(id) || (parent && !Find(parent)))
    {
        return false;
    }
    for (ActorId ancestor = parent; ancestor; ancestor = nodes_.at(ancestor).parent)
    {
        if (ancestor == id)
        {
            return false;
        }
    }
    ActorPosition before = WorldPosition(id), origin = WorldPosition(parent);
    nodes_.at(id).parent = parent;
    if (keepWorld)
    {
        nodes_.at(id).actor->p = {before.x - origin.x, before.y - origin.y};
    }
    return true;
}

void SceneGraph::Destroy(ActorId id)
{
    auto it = nodes_.find(id);
    if (it != nodes_.end())
    {
        it->second.destroyed = true;
    }
}

void SceneGraph::FlushDestroyed()
{
    if (updateDepth_ != 0)
    {
        return;
    }
    std::vector<ActorId> removed;
    for (const auto& entry : nodes_)
    {
        if (IsDestroyed(entry.first))
        {
            removed.push_back(entry.first);
        }
    }
    for (ActorId id : removed)
    {
        nodes_.erase(id);
    }
}

bool SceneGraph::IsEnabled(ActorId id, bool drawing) const
{
    if (IsDestroyed(id))
    {
        return false;
    }
    while (id)
    {
        const Node& node = nodes_.at(id);
        if (!node.actor->enabled || (drawing && !node.actor->visible))
        {
            return false;
        }
        id = node.parent;
    }
    return true;
}

void SceneGraph::UpdatePhase(ActorPhase phase, float dt, SceneContext& context)
{
    std::vector<ActorId> snapshot;
    for (const auto& entry : nodes_)
    {
        if (entry.second.actor->updatePhase == phase)
        {
            snapshot.push_back(entry.first);
        }
    }
    // Spawned actors join the next phase; deletion is deferred until traversal completes.
    ++updateDepth_;
    try
    {
        for (ActorId id : snapshot)
        {
            if (IsEnabled(id, false))
            {
                nodes_.at(id).actor->Update(dt, context);
            }
        }
    }
    catch (...)
    {
        --updateDepth_;
        throw;
    }
    --updateDepth_;
}

void SceneGraph::Draw(Renderer& renderer, const SceneContext& context) const
{
    struct Item
    {
        const Actor* actor;
        ActorPosition world;
    };

    std::vector<Item> order;
    order.reserve(nodes_.size());
    for (const auto& entry : nodes_)
    {
        if (IsEnabled(entry.first, true))
        {
            order.push_back({entry.second.actor.get(), WorldPosition(entry.first)});
        }
    }
    std::stable_sort(order.begin(),
                     order.end(),
                     [](const Item& a, const Item& b)
                     {
                         if (a.actor->layer != b.actor->layer)
                         {
                             return a.actor->layer < b.actor->layer;
                         }
                         return a.world.x + a.world.y < b.world.x + b.world.y;
                     });
    for (const auto& item : order)
    {
        item.actor->Draw(renderer, context, item.world);
    }
}

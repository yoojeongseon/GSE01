#include "../SimpleGame/SceneGraph.h"
#include <cassert>

class CountingActor : public CloneableActor<CountingActor>
{
public:
    CountingActor()
    {
        updatePhase = ActorPhase::Movement;
    }

    int updates = 0;

    void Update(float, SceneContext&) override
    {
        ++updates;
    }
};

int main()
{
    SceneGraph scene;
    SceneContext context;
    ActorId parent = scene.Create<Actor>(0);
    scene.Get<Actor>(parent).p = {100, -40};
    ActorId child = scene.Create<CountingActor>(parent);
    scene.Get<CountingActor>(child).p = {2, 3};
    ActorId grandchild = scene.Create<CountingActor>(child);
    scene.Get<CountingActor>(grandchild).p = {5, 6};
    assert(scene.WorldPosition(grandchild).x == 107);
    assert(scene.WorldPosition(grandchild).y == -31);
    assert(scene.Children(parent).size() == 1);
    assert(!scene.SetParent(parent, grandchild));
    assert(!scene.SetParent(child, child));
    assert(!scene.SetParent(child, 99999));

    scene.Get<Actor>(parent).enabled = false;
    scene.UpdatePhase(ActorPhase::Movement, .016f, context);
    assert(scene.Get<CountingActor>(grandchild).updates == 0);
    scene.Get<Actor>(parent).enabled = true;
    scene.Get<Actor>(parent).visible = false;
    scene.UpdatePhase(ActorPhase::Movement, .016f, context);
    assert(scene.Get<CountingActor>(grandchild).updates == 1);

    SceneGraph copy = scene;
    copy.Get<CountingActor>(child).updates = 17;
    copy.Get<Actor>(parent).p.x = 300;
    assert(scene.Get<CountingActor>(child).updates == 1);
    assert(scene.WorldPosition(grandchild).x == 107);
    assert(copy.WorldPosition(grandchild).x == 307);
    assert(copy.SetParent(child, 0));
    assert(copy.Parent(child) == 0);
    assert(copy.WorldPosition(grandchild).x == 307);
    assert(copy.SetParent(child, parent, false));
    assert(copy.WorldPosition(grandchild).x == 607);

    scene.Destroy(parent);
    assert(scene.Find(child) == nullptr);
    assert(scene.Find(grandchild) == nullptr);
    assert(scene.Query<CountingActor>().size() == 0);
    scene.FlushDestroyed();
    assert(scene.Children(0).empty());
    assert(copy.Query<CountingActor>().size() == 2);

    SceneGraph assigned;
    assigned = copy;
    copy.Destroy(child);
    copy.FlushDestroyed();
    assert(assigned.Find(child) != nullptr);
    assert(assigned.Get<CountingActor>(child).updates == 17);
}

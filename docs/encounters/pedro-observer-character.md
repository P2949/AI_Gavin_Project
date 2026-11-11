# Pedro B2b-5a: physical observer character foundation

Baseline: merged main `6fbe30e` (PR #43).

This adds an opt-in `APedroObserverCharacter` with an explicitly positioned
`ObserverEye` component. The actor returns its CURRENT eye component position
and rotation from `GetActorEyesViewPoint`. The existing direct-observation
Director already queries that function synchronously when it receives a
confirmed gameplay event. No event type, acquisition policy, evidence rule,
completion gate, classification, progression API, or map is changed.

`APedroObserverCharacter` is a Character for possible later controller-driven
movement. It has NO AIController, pathfinding task, AI Perception, hearing,
attention, memory, or automatic tracking in this change. Its mesh must be
supplied by a future encounter-owned Blueprint. The default observer does
not deliberately turn toward the player or automatically move.

The two new native tests verify eye component existence/attachment and
GetActorEyesViewPoint delegation. They do NOT prove sight changes during
runtime movement or line-trace behaviour in PIE. A later isolated *new*
ObserverActorTest map must test physical observer translation, turning,
occluders, actual player inspections, and the final hub outcome without
editing the existing DirectPlayTest assets or production levels.

Expected change scope: two new C++ production-source files, one
new C++ test file, this documentation, and a narrowly scoped rename in
the existing PedroDirectObservationTests.cpp file. Its anonymous-namespace
Forward variable was renamed to TestForwardDirection to prevent a
collision with Unreal's Forward<T>() in unity-generated translation units.
The existing test behaviour and assertions are unchanged.

For review: avoid treating a bare Actor/Character's GetActorEyesViewPoint as a
complete awareness system. The Director's direct path currently checks one
Visibility ray from the eye to the world-event source actor origin at the
moment a confirmed event is emitted. The existing B2b-4b five-scenario logs
remain historical baseline evidence; no new PIE result is claimed here.

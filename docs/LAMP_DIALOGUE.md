# Reviewed lamp-attack command scheduler

The source is the actual `Data/Dialogue/Podunk/cutscenes/lamp_attack.yaml` at
commit `7d9246600fffe518408f5830d4848635019005a3`, interpreted by that commit's
`DialogueBox.gd` and `yaml_parser.gd`. This is not an `.ecs` conversion, the M0
fixture VM, or a generic GDScript translator. It covers the 13 textless phrases
and stops at the original battle-start request. It does not implement battle.

## Frontend and gates

`tools/lamp_dialogue.py --source ... --godot ...` runs the unchanged upstream
YAML parser in an isolated Godot 3.6.2 project. The resulting receipt includes
engine identity and whole-file source hashes. An offline build can use the
checked parser receipt in `reports/m5-lamp-dialogue-reference/dialogue.json`.
Both routes validate the exact reviewed document digest, command shapes,
actor/object/method names, numeric domains and linear phrase targets before
emitting external JSON. The reviewed IR compiler then encodes typed indexed commands into `romfs/data/opening.encroom`; no production C++ content header is emitted.

The review pins the dialogue, parser, handler, Timer scene, actor/NPC scripts,
house scene, MusicChanger's embedded script, room shaker script/scene and audio
manager. Changes fail closed. These dependency fingerprints are conservative
review gates; they do not approve every behavior in those files. Unknown text,
expressions, command fields, movement modes, method bodies or battle targets are
not silently ignored. The one explicit inert-field exception is the exact
original `movecam.time: 1`: the handler reads `length`, not `time`, and uses its
one-second default. The camera-shake handler looks up an Array key `['size']`;
the original `small` matches its fallback. Neither quirk is "fixed" by the port.

## Runtime phase contract

`DialoguePlayer` borrows a bounded immutable instruction stream from a checked RoomView. The world accepts
typed actions through `DialogueSink::apply`; rejecting an action stops the task.
There are no background threads, implicit blocking movement calls or allocations
in the scheduler. Each resumed dispatch is bounded to 32 instructions.

1. `start` emits cutscene start and requests Ninten's deferred actor binding
2. The matching `actor_ready(actor, generation)` signal resumes creation of the
   next actor, then both actors become persistent
3. After Lamp is ready, an explicit `idle_frame` yield precedes the first wait
4. `idle_begin` resumes idle-frame yields before idle callbacks
5. `idle_process(delta)` runs at the native WaitTimer phase. The world supplies
   the native `real_t` delta promoted to double, at 60Hz `double(1.0f / 60.0f)`
6. Object method requests are deferred until the world's MessageQueue phase;
   SceneTreeTimer and tween advancement belong to the world/actor modules

Initial actor-ready signals can drain in the same deferred-message flush. The
first phrase's timer starts at the next idle-frame signal and is decremented in
that same idle frame. A one-shot timer times out only when strictly negative.
Restarting it in the timeout callback discards overshoot and does not consume a
second delta in that frame. Unsupported nonpositive, nonfinite or >250ms idle
steps, duplicate phases, wrong actor signals and stale generations are rejected.
The >250ms restriction avoids unreviewed coroutine/timer races. Cancellation
prevents future callbacks from resuming the task.

The generated action order is the handler's fixed order, not YAML key order.
In phrase 12 it is wait start, deferred room-shaker request, overworld-battle
music flag, lamp move, lamp jump, battle queue. Move and jump are concurrent and
do not gate the final wait. `QueueBattle` registers the lamp enemy and the
`poltergeist` win-flag name; it neither enters battle nor writes that story flag.

After the final wait: stop the talker's interaction, clear the talker, invoke
Ninten restoration, release the drafted Lamp's persistence, emit cutscene-ended,
emit dialogue-done, request battle. Original Actor.update_npcs restores position
and camera synchronously but then yields sprite-frame/idle signals before final
visibility/persistence cleanup. `RestoreActor` denotes that invocation, not its
completion. The battle request does not wait for it.

Actor.stop_interaction delegates to the hidden replaced NPC: talking, pause-for-
interaction and mute become false. The non-staring lamp NPC also schedules a
one-second return-to-initial-direction callback. It does not immediately reset
the actor's Open animation or emit finished_action. This callback lies after the
current battle-request boundary.

## Native reference and measured sequence

`tools/reference_lamp_dialogue.py` copies the reviewed production command blocks,
timer callback and relevant lifecycle tail byte-for-byte into a recording
harness. It uses the real engine Timer, deferred node addition, actor-ready
signals, idle-frame yields and original YAML parser. Actor/camera/audio/object/
battle method bodies are recording boundaries; one sound-resource load is
explicitly replaced by a path-recording boundary. Hidden dialogue-box UI and
text rendering are outside this probe. Thus it verifies dispatcher semantics,
not a full original-game run or mixer/rendering equivalence.

Measured 60Hz reference frames, relative to initial creation/deferred flush:

| Event | Idle frame |
|---|---:|
| Actor bindings and ready signals | 0 |
| Phrase 0 timer and actions | 1 |
| Phrase 1, including camera selection | 60 |
| Camera movement after idle yield | 61 |
| Phrase 2 / deferred music invocation | 150 |
| Phrase 3 | 210 |
| Phrase 4 | 240 |
| Phrase 5 | 252 |
| Phrase 6 | 282 |
| Phrase 7 | 288 |
| Phrase 8 | 291 |
| Phrase 9 | 297 |
| Phrase 10 | 303 |
| Phrase 11 | 333 |
| Phrase 12 / deferred room shaker / battle queue | 363 |
| Cleanup, signals and actual battle request | 384 |

The clean native probe records 59 events. Four are internal actor-ready/object-
called observations; all 55 sink actions, including payloads and exact frame
numbers, are compared by the C++ test. Generated expectations come from the
recorded native trace, not from the C++ scheduler. The reviewed trace has a digest
and cannot be silently regenerated after timing changes.

## Verification and reproduction

```sh
python3 tools/lamp_dialogue.py --source upstream/MOTHER-Encore --godot /path/to/godot3
python3 tools/reference_lamp_dialogue.py --godot /path/to/godot3
python3 -m unittest discover -s tests -p test_lamp_dialogue.py -v
make test
```

The focused C++ test passes 848 checks, including the 55 native actions, every
phase barrier, generation mismatch, cancellation, callback rejection, strict
zero-time edge, overshoot discard, and bad delta/phase failures. Thirteen Python
tests cover receipt/external-data determinism, provenance, every supported command's
negative shapes/values, source-body changes, unknown controls, command order,
trace tampering, and block extraction ambiguity. Host/cross-build/integration
verification is recorded separately by the world integration work.

Audio actions remain typed requests. MusicChanger.play_music has song/player
reuse and crossfade state which this command reference does not validate.
RoomShaker.delayed_start defaults to five seconds, so no room shake executes
before the final 0.35-second wait hands off to battle. Neither these intents nor
the battle request should be described as completed audio or battle gameplay.

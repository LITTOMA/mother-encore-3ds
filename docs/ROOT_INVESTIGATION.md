# Root investigation

The field recipe now includes both original `Dead Bush.interact()` messages.
`bat=false` selects `Reusable/hardtobreak`; `bat=true` selects
`Reusable/easytobreak`. The original one-phrase YAML files have no actor, name,
voice, item grant or flag writes. Both admitted locales are taken from the
original reusable CSV. Content and selectors remain independent binary data.

Source review is recorded in `content/field-prop-dialogues.json`. It includes
the Root script/scene, map, Player, party object, UI manager, DialogueBox and
AbstractDialogueBox. Unknown fields and changed source bytes fail admission.
The private frozen Room/meta inputs are exact outputs from8956037; they are
not manually edited packets or a substitute for current formal generation.

## Runtime and data

Room remains capability14/rules7. Two seven-command actorless programmes are
appended after all eight existing programmes; earlier commands/tables/strings
are preserved. The metadata becomes capability5 with10 programmes,70 localized
rows and a52-byte checked prop policy. Old metadata1..4 remains readable.
Props3, catalog13, Session resource11 and the game save format are unchanged.

The policy owns the flag, false/true programmes, source ray origin/length/mask,
interaction filter, exact Props packet byte count/CRC and `stop_running`.
It joins the actual prop hard/easy source paths before scene commit. A scene
declaring these interactions cannot finish without prepared prop/contact
owners. Reusable programmes reject replacement actors and unsupported side
effects; malformed loads retain the previous owned packet.

The source Player ray uses offset0,6 and length16. The generator verifies that
the current field's admitted walking and ray collision sets agree; it rejects
a source filter difference rather than assuming future fields agree. The
consumer checks animation-enabled living prop areas, nearer NPC interactions
and existing map collision. Collider activation and visibility are distinct:
the original method refuses UI while invisible, and Break disables its area
on the animation processing pass. A nearer Iggy interaction can legitimately
win over the Root. Ray and motion precision are not asserted byte-identical
to a separate Godot physics run.

Source `uiManager.open_dialogue_box` calls `Player.pause(true,true)`.
The data-owned stop flag is passed through the shared programme entry, consumed
once by BeginCutscene, and cancels tap-run without an extra pause/camera handoff.
The default for existing programme callers is preserved. A still-replaced
Player or active Bat cannot start an interaction. Closing the manual text
restores ordinary movement without creating an actor or granting anything.

## Verification boundary

Original resource tools/tex3ds generated Room35704 bytes and metadata9912 bytes.
The unchanged full map, House Room, Root Props, object texture,60-entry audio
bank, mutable/key policies and default save were checked. Session changes are
dependency fingerprints only. Default-save SHA256 remains
`a9cfbb498a20cb2bbb4a0a56dc5a87f9f13e37bac7ebbef14c875b67c93e7e9f`.
The complete47-task native-content run passed.

Current GCC9.4 ASan/UBSan with leak checking passes the actorless programme
1897 checks, metadata60 checks, scene1440 checks and nine formal-resource
regressions. The scene uses explicit artificial placement and sound/grant
doubles; it covers both source branches, manual holds, nearer Iggy, map
occlusion, pause/attack, pending original Player release, failed admission
recovery and tap-run cancellation.25 Python source/composition/legacy cases
pass, including exact old metadata4 from recipe6. Earlier fixture failures
and their source-geometry/timing explanations are retained separately.

8956037's two ordinary simulator routes cover teaching, Bat/Root contact and
complete getshovel. They do not cover these new A messages. Current full CI,
genuine current3DSX/CIA and ordinary A-message verification are pending.
Audibility, a natural-opening playthrough, an original Godot runtime oracle,
physical hardware and the complete original game remain unverified.

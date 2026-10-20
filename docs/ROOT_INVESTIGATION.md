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

61ea4ea's genuine3DSX/CIA and377 packaged resources passed. Its3DSX SHA256 is
`fc6199d192be2ca55dcf64dcfa0bd13b427b41b6591ba61b340eae24ca5f596f`.
Two ordinary-input cold boots now complete LOAD, House/field Goods, the full
walk, different Iggy choice branches, the A/easy message3301, manual holds of
5/18 published frames, closing UI, B/Root and all five getshovel texts. Hint
inspection preserves flags, inventory and position. Both final gameplay
projections match, with the original1408,4720 left-facing return and resumed
movement. Memory writes were disabled and owned profiles were removed.
The initial slot is the previously recorded artificial post-Mayor checkpoint;
the false/hard hint branch is covered by native tests only. Screenshots, driver,
observations and limits are retained under reports/iggy-tutorial.

61ea's fullClang ordinary checks are254/259; its five fixture/source-budget
failures are recorded separately and do not negate or replace the actual
console/ordinary-input results. The component repairs and the new complete CI
are tracked independently; no complete current CI pass is claimed here.
Audibility, a natural-opening playthrough, an original Godot runtime oracle,
physical hardware and the complete original game remain unverified.

The later6e245cd full run39378374503 completed with Clang259 ordinary and259
ASan/UBSan checks and genuine3DSX/CIA passing. GCC passed274/275 ordinary
checks; its sole failing CTest was the aggregate Python suite, with a music
library proof mismatch and the dependent binding setup error. It completed
in974.70seconds; GCC sanitizers did not execute after the ordinary failure.
The phone source partition no longer timed out.

The existing music proof was generated against the59-entry root bank at
00cb71d (also8956037's parent). Comparing both historical recipe and parsed
bank to the current60-entry versions preserves every previous record and all
bank tuning. The only addition is the independently played Root sound,
`res://Audio/Sound effects/Barrel Coffin Explode.mp3`, ID3176391583. All13
music records, original/import identities, loop policy, default gain, fade,
master bus and source implementation mappings remain unchanged. Current
`MusicRegionData::load/matches` and `MusicLibraryBindings::admit/read_track`
were reviewed; they still check those same identities and PCM/loop bindings.

The original `music_library.source` and `music_library_bindings.source`
producers regenerated their complete review records. Only the former's root
recipe/bank fingerprints and the latter's review dependency fingerprint
changed. Both candidate CLIs ran successfully; all28 generated files match
the existing frozen global-library bundle byte for byte. All7 existing
source, coverage, encoder and negative decoder tests pass (9.140seconds).
No fixture expectation, code, PCM, formal game resource or save changed, and
the candidate global music library remains unstaged. Raw prior CI logs,
historical comparisons, producer invocations and current checks are retained
in reports/iggy-tutorial/music-source-dependency-fix. A new full run must
independently accept the current commit; this is not a hardware or complete
game verification claim.

That follow-up acceptance is now complete:2a9a340's full run39385216938 passes
GCC275 ordinary/259 ASan+UBSan, Clang259 ordinary/259 ASan+UBSan, genuine3DSX/CIA
and377 packaged resources (artifact12247222836). Its runtime/include/platform/
content/romfs diff from the61ea ordinary-input game is empty. With separate user
authorization, PR69 was marked ready and merged with history preserved as
main431545c58d14b8586bbaffd84d48a29b9762bba7. Current acceptance logs are retained
under reports/iggy-tutorial/root2a9-full-acceptance. This does not extend the
recorded emulator scope to natural opening, hard hint, audibility or hardware.

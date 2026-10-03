# Doll actions and bounded source victory

Pinned source: `7d9246600fffe518408f5830d4848635019005a3` (Act2v0.4.1.0).

The Doll entry resource has battle ID 2. The previous round resource belonged to
Lamp, battle ID 1, so using it at the Doll menu failed the checked participant
binding. `data/doll-entry.encround` now supplies the independent Doll binding,
original localized action text, 5:1 tackle/float AI, textures, boss presentation,
and rewards. `opening.encround` retains its v2 format and bytes. The Doll pack
uses v3 with checked Encounter, Growth, and BossShakes sections; execution and
all rule/content values remain separate.

`BattleSessionStats` carries experience, level, bank, cash, earned cash, HP/PP,
maximums, five combat stats and learned skill IDs. Optional session pointers to
`BattleActionPresentation::begin`, `BattleRound::begin`, and
`BattleOutcome::initialize` prevent a later encounter from restoring initial
HP or discarding Lamp earnings. `BattleOutcome::state()` returns the new session.
The bounded progression currently accepts the reviewed initial level and EXP for
each encounter; it does not claim arbitrary enemies or higher-level progression.

Items now has its own `BattleRoundPhase::Items` boundary. Opening it queues no
battle action and draws no random number. The independent item menu owns item
selection; `return_from_items()` restores the command menu. Bash targets/cancel,
Defend and subsequent Doll turns use the existing source scheduler.

Doll's lethal hit retains its overworld actor, stops scrolling HP, pauses action
progression and starts `bossDefeat`. The source method starts `DefeatFlash` at
4.5 seconds. Its separate 4.5-second completion requests victory; the enemy
sprite's 6-second completion is not the victory gate. Three original Shaker
bursts consume the shared source random stream. Source animation cursors use
float32 accumulation for these boss tracks. The source radial flash is represented
as an ellipse in source pixel coordinates, with source color/radius keys. Its
3DS rendering has not received a new emulator or hardware visual comparison.

After the victory banner and manually acknowledged EXP text, EXP 3+8 becomes
11 and level 2. Source deterministic stats are max HP 65, max PP 27, offense 12,
defense 12 with the existing equipped cap, speed 6, IQ 6 and guts 8. HP increases
by 3 and PP by 1, rather than resetting to their maximums. The player acknowledges
the level line, four positive stat increases and Telepathy learning. Telepathy
is a field-only learned skill and remains in the session. Its field menu is not
implemented by this change. Only after these acknowledgements do bank and earned
cash increase by 10, preserving Lamp's prior 5; wallet cash is unchanged.

Transition completion exposes `BattleOutcomePhase::PostWinRequested` and
`post_win_script() == "Podunk/cutscenes/doll_defeated"`. It leaves world input
paused for that source script and does not set `doll_defeated`, change
`poltergeist`, remove Doll, grant the melody, or fabricate an overworld completion.
The platform now resumes this handoff by matching the external program source path, executes the eight original phrases, then restores world input. Flags are written by those script commands, not by the battle outcome. See DOLL_POSTWIN.md. Audio track sources are
reviewed by the extractor; boss/level-up sound mapping is not added to the audio
backend in this change.

## Verification

- Existing Lamp loader: 17,809 checks; integrated victory: 3,636 checks
- Round compiler tests: 28 tests including malformed v3 sections, policies,
  growth/shake records, callback contracts and companion-file staging
- Standalone Doll integration: 12,206 checks, including valid-CRC negative loader
  cases, carried state, actions, boss pause, acknowledged growth and post-win gate
- Official Godot 3.6.2 extracted PartyMember/Character methods match growth and
  Telepathy learning; native global RNG remains unchanged
- Eight nonlethal Doll first rounds match original extracted BattleSystem action
  methods for damage, targets, weighted AI, HP, boundaries, raw RNG draw count and
  the next RNG value. Original battle sprite hit-selection randomness is retained;
  graphics/tween timing are explicit adapters in this mechanics oracle
- Native AnimationPlayer method tracks plus the original Shaker match C++ at
  fixed 60 Hz: flash start 270, enemy hidden 360, defeat-enemies callback 390,
  victory request 539; next RNG value 4045365244 for seed 725. Audio and rendering
  are excluded from this callback oracle

Receipts and retained native output are under `reports/doll-round/`. These are
host and headless engine results, not a new emulator or hardware acceptance.
The parent integration owns consolidated sanitizer, ARM and packaging checks.

Regenerate source content explicitly after dependencies change:

```
python3 tools/extract_battle_round.py
python3 tools/native_round.py compile
python3 tools/doll_round.py extract
```

Ordinary builds compile the reviewed IR with `tools/doll_round.py compile` and
must not silently rerun extraction. `native_round.stage_files` accepts a pack
path so the room's externally referenced `.encbattle` files can select their
same-basename `.encround` companions without a compiled game-ID mapping.

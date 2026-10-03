# Bounded original-audio backend

This implements playback submission and PCM streaming for the current original
opening slice. It is not the complete Godot audio graph and does not establish
speaker output on hardware or an emulator by itself.

## Source audit

Pinned Mother: Encore commit: `7d9246600fffe518408f5830d4848635019005a3`.

- `Scripts/global/audioManager.gd` SHA256
  `425bffd0f66c2da908232f8d5b644139388be58b99aff49ebbf8c127fa5b616c`
  - `music_fadeout_obj` interpolates volume in dB with quartic ease-in to the
    external `SILENT_SOUND_THRESHOLD`, then removes silent/unplaying music
  - `play_sfx` reuses a player by name; DialogueBox always names this cue
    `dialogBoxSound`, so the three impacts restart one effect voice
  - `play_music` assigns the stream and starts at time zero for this slice;
    imported built-in loops keep the stream running
- `Nodes/Overworld/MusicChanger.tscn` SHA256
  `f6a50dd17dd71d8070d19799b469e11b050474d9154ec8da6197375951d51506`
  - The idle first music player starts at the area's volume immediately
  - Full overlapping area crossfades, diegetic synchronization, reusable
    same-song area attachment, fade-in/fade-to, and filter buses are not covered
- `Scripts/UI/DialogueBox.gd`, music and soundeffect dispatch: empty music fades
  player zero over two seconds; soundeffect loads the named original asset
- `Scripts/UI/Battle/BattleSystem.gd` SHA256
  `d0af7731e0311855de1f0363f8712525269a9f50828033c88989796a657436ca`
  - Normal non-boss/no-advantage entry creates a separate music player for the
    encounter jingle
  - With `overworldBattleMusic=true`, existing overworld music is not paused
  - The battle data, not C++, selects the encounter asset identity

The reviewed assets are Poltergeist.ogg, bash.mp3 and Encounter Enemy.mp3. Each
source and its `.import` file is hash-gated in `content/native-audio.json`.
Poltergeist loops from 6.382 seconds, truncated to source frame 281446 at 44100Hz,
after playing its initial 2534475-frame track once. Both MP3 assets are one-shots.
The compiler preserves source sample rate and stereo channel count, converts to
signed little-endian PCM16, and does not normalize, trim or resample deliberately.
FFmpeg's codec delay/gapless behavior and decoded sample rounding have **not**
been shown bit-identical to Godot's decoders. Loop/timing intent is reviewed;
audio waveform identity with the original engine remains a separate validation.

The no-settings slice uses the source bus-layout master level (-5.93075dB) and
zero-dB Music/SFX buses. Upstream persisted user volume settings are not imported.

## Independent data and runtime

`tools/audio_asset.py` compiles the external recipe and reviewed imports into:

- `romfs/data/opening.encaudio`: 539-byte ENCAUD01 metadata (three records)
- Three independent PCM files under `romfs/audio/`, totaling 11212208 bytes
- `romfs/data/opening-audio-manifest.json`: source/import/PCM fingerprints,
  actual FFmpeg command/version/binary hash, and staging file list

No audio content, paths, stable IDs, gains or loop positions are C++ constants.
The existing room Resource table stays unchanged. The CTR request adapter checks
its source path, original SHA256 and stable identity against the audio bank.
The bank independently checks schema, length, CRC32, unique identities/paths,
reserved bytes, all offsets, supported encoding dimensions, loop bounds, finite
gains and safe paths. Loads fail transactionally. PCM streams are length/CRC
verified at initialization using an 8192-byte scratch buffer.

The platform has three bounded voices: music, the named dialogue effect, and a
jingle. Three 2048-frame double-channel buffers per voice consume **73728 bytes**
of linear memory. Files remain open; there is no full-track preload. Main-frame
updates refill free buffers; NDSP consumes them asynchronously. At 44100Hz,
three buffers provide about 139ms of queued data per voice. Sustained stalls can
underrun, and Old/New 3DS performance and sleep behavior still require testing.
Unsupported simultaneous use of one source in multiple voices fails explicitly.

The platform uses independent queues, so the encounter cue preserves Poltergeist.
The effect queue is cleared/restarted for repeated dialogue sounds. Music fading
uses quartic dB interpolation and clears the voice when the threshold is reached.
No source script, GDScript interpreter, decoder library or M0 sound is linked.

## DSP requirement and truthful diagnostics

The official libctru 2.7.0 implementation of `ndspInit` uses a DSP component
already supplied by the launcher, or the existing SD `/3ds/dspfirm.cdc` path.
This project does not contain, fetch or fabricate DSP firmware. Initialization
failure remains nonfatal to game rendering but is reported explicitly with the
actual Result. `available()` is false, no voice is submitted, and no successful
playback counter is incremented. Queue submission is still not proof of audible
speaker output; an actual listening/capture check is required.

CSND was considered and rejected as an emulator shortcut. libctru marks it
deprecated. The official Azahar implementation's CSND Initialize/ExecuteCommands
are stubbed and Start/configure-enable playback are TODOs, so a successful call
would not prove sound. No silent CSND fallback is used.

Primary platform references:

- https://github.com/devkitPro/libctru/blob/v2.7.0/libctru/source/ndsp/ndsp.c
- https://github.com/devkitPro/libctru/blob/v2.7.0/libctru/include/3ds/ndsp/channel.h
- https://github.com/devkitPro/3ds-examples/blob/master/audio/opus-decoding/source/main.c
- https://github.com/azahar-emu/azahar/blob/master/src/core/hle/service/csnd/csnd_snd.cpp

## Focused verification

Focused host/parser/stream tests use a host-only NDSP double. It exercises failures, lifecycle,
queue routing, retrigger, jingle layering and fade semantics. It cannot establish
real NDSP behavior, audibility, scheduling fidelity, sleep/resume or hardware
performance. The actual adapter was separately compiled against the installed
ARM/libctru SDK. Whole-project linking and emulator validation are separate from these focused checks.

Audio assets remain subject to Team Encore's game-related-use permission in the
upstream LICENSE. Their inclusion here is solely for this Mother: Encore port.
They are not relicensed under this project's MIT license. FFmpeg is only an
external conversion tool; none of its libraries or binary enters the 3DS build.

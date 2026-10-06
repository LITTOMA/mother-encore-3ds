#pragma once
#include "encore/audio_data.hpp"
#include "encore/field_node_recipe.hpp"
#include "encore/source_random.hpp"
namespace encore::upstream {
struct FieldDialogueAudioNode {
  uint32_t id = 0, parent = 0, ready = 0, pause = 0, stream = 0, mix_target = 0;
  int32_t priority = 0;
  float volume = 0, pitch = 0;
  bool autoplay = false, paused = false;
  std::string path, bus;
};
struct FieldDialogueAudioAsset {
  uint32_t id = 0;
  std::string source;
  std::array<uint8_t, 32> sha{};
};
class FieldDialogueAudioData {
public:
  bool load(const uint8_t *, size_t, const FieldIdentity &, std::string &);
  bool load_file(const char *, const FieldIdentity &, std::string &);
  bool valid() const { return valid_; }
  bool scene_admitted() const { return false; }
  FieldIdentity identity() const { return identity_; }
  const std::array<uint8_t, 32> &recipe_sha() const { return recipe_; }
  const std::vector<FieldDialogueAudioNode> &nodes() const { return nodes_; }
  const FieldDialogueAudioNode *node(uint32_t) const;
  const FieldDialogueAudioAsset *asset(uint32_t) const;
  const FieldDialogueAudioAsset *asset(std::string_view) const;
  bool verify_bank(const AudioBank &, std::string &) const;
  const std::array<double, 2> &pitch_range() const { return pitch_; }
  float silence_db() const { return silence_; }
  uint32_t fade_stop_frames() const { return fade_stop_; }
  uint32_t fade_replace_frames() const { return fade_replace_; }
  const std::string &text_prefix() const { return prefix_; }
  const std::string &extension() const { return extension_; }
  const std::string &finished_signal() const { return finished_; }

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> recipe_{};
  std::array<double, 2> pitch_{};
  float silence_ = 0;
  uint32_t fade_stop_ = 0, fade_replace_ = 0;
  std::string prefix_, extension_, finished_;
  std::vector<FieldDialogueAudioNode> nodes_;
  std::vector<FieldDialogueAudioAsset> assets_;
};
struct FieldDialogueAudioState {
  FieldObjectId object = 0;
  uint32_t source = 0, stream = 0;
  bool entered = false, ready = false, active = false, set_stop = false,
       stop_priority = false, paused = false, paused_fade = false,
       internal = false;
  double pending_seek = -1;
  float volume = 0, pitch = 1, mix_volume = 0;
  std::string bus;
};
struct FieldDialogueAudioObservation {
  bool actual_voice = false, stream_checked = false, mix_registered = false,
       playback_playing = false;
  double position = 0;
};
struct FieldDialogueAudioBackend {
  const AudioBank *bank = nullptr;
  std::function<bool(FieldObjectId, const AudioAsset *, std::string &)> create,
      assign, prepare_assign;
  std::function<bool(FieldObjectId, bool, std::string &)> register_mix;
  // A real driver buffer must be writable before a source mix consumes
  // PCM/flags.
  std::function<bool(FieldObjectId, bool &, std::string &)> mix_due;
  std::function<bool(FieldObjectId, FieldDialogueAudioObservation &,
                     std::string &)>
      observe;
  std::function<bool(FieldObjectId, uint32_t, float, bool, std::string &)>
      fade_stop;
  std::function<bool(FieldObjectId, double, float, std::string &)> start;
  std::function<bool(FieldObjectId, bool, std::string &)> pause;
  std::function<bool(FieldObjectId, float, float, float, std::string_view,
                     std::string &)>
      mix;
  std::function<bool(FieldObjectId, std::string &)> stop, release,
      preflight_play;
};
struct FieldDialogueAudioHost {
  FieldDialogueAudioBackend backend;
  // Actual AudioServer callback list, not an elapsed gameplay clock. Source
  // EnterTree appends, ExitTree removes; one mix invokes this callback once.
  std::function<bool(FieldObjectId, std::function<bool()>, std::string &)>
      add_audio_callback;
  std::function<bool(FieldObjectId, std::string &)> remove_audio_callback;
  std::function<bool(FieldObjectId, bool, std::string &)> internal_process;
  std::function<bool(FieldObjectId, std::string_view, std::string &)> emit;
};
class FieldDialogueAudioRuntime {
public:
  bool initialize(const FieldDialogueAudioData &, const FieldNodeRecipeData &,
                  FieldNodeTreeRuntime &, SourceRandom &,
                  FieldDialogueAudioHost, std::string &);
  bool attach(FieldObjectId, std::string &);
  bool enter_native(FieldObjectId, std::string &);
  bool ready_native(FieldObjectId, std::string &);
  bool exit_native(FieldObjectId, std::string &);
  bool release(FieldObjectId, std::string &);
  bool set_stream(FieldObjectId, uint32_t, std::string &);
  bool phrase_sound(FieldObjectId, std::string_view, std::string &);
  bool play(FieldObjectId, double, std::string &);
  bool seek(FieldObjectId, double, std::string &);
  bool stop(FieldObjectId, std::string &);
  bool set_volume(FieldObjectId, float, std::string &);
  bool set_pitch(FieldObjectId, float, std::string &);
  bool set_paused(FieldObjectId, bool, std::string &);
  bool set_bus(FieldObjectId, std::string_view, std::string &);
  bool tree_pause(FieldObjectId, bool paused, bool actual_can_process,
                  std::string &);
  bool character_sound(FieldObjectId, bool source_last_is_delay, std::string &);
  bool audio_mix(FieldObjectId, std::string &);
  bool internal_process(FieldObjectId, std::string &);
  bool is_playing(FieldObjectId, bool &, std::string &);
  bool playback_position(FieldObjectId, double &, std::string &);
  const FieldDialogueAudioState *state(FieldObjectId) const;

private:
  const FieldDialogueAudioData *data_ = nullptr;
  const FieldNodeRecipeData *recipe_ = nullptr;
  FieldNodeTreeRuntime *tree_ = nullptr;
  SourceRandom *random_ = nullptr;
  FieldDialogueAudioHost host_;
  std::map<FieldObjectId, FieldDialogueAudioState> states_;
  std::map<FieldObjectId, std::vector<FieldObjectId>> factories_;
  FieldDialogueAudioState *get(FieldObjectId, std::string &);
  bool observation(FieldDialogueAudioState &, FieldDialogueAudioObservation &,
                   std::string &);
  bool internal(FieldDialogueAudioState &, bool, std::string &);
};
} // namespace encore::upstream

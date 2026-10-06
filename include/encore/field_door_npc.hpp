#pragma once
#include "encore/battle_data.hpp"
#include <functional>
#include <map>
namespace encore::upstream {
struct FieldDoorConnection {
  uint32_t role = 0;
  std::string signal, method;
};
struct FieldDoorProgramme {
  std::string dialog, path;
  std::array<uint8_t, 32> sha{};
};
struct FieldDoorNpcAudio {
  std::string path, bus;
  float volume_db = 0, pitch = 0;
};
struct FieldDoorBinding {
  uint32_t id = 0, ready_ordinal = 0, shape_id = 0, audio_id = 0,
           audio_ready = 0, layer = 0, mask = 0, flags = 0;
  std::string node, dialog, appear, disappear;
  Vec2 position{}, centre{}, half{};
  FieldDoorNpcAudio audio;
  std::vector<std::vector<std::string>> groups;
  std::vector<FieldDoorProgramme> programmes;
  const FieldDoorProgramme *programme(std::string_view) const;
};
struct FieldDoorPolicy {
  std::array<bool, 3> pause{};
  std::array<bool, 2> turn{};
  std::string completion;
  float timer_seconds = 0;
  bool process_pause = false, ignore_time_scale = false,
       strict_negative = false, global_fifo = false;
  std::vector<FieldDoorConnection> connections;
};
class FieldDoorNpcData {
public:
  bool load(const uint8_t *, size_t, std::string &);
  bool load_file(const char *, std::string &);
  bool valid() const { return valid_; }
  const std::array<uint8_t, 20> &source_pin() const { return pin_; }
  const std::string &scene() const { return scene_; }
  const std::string &script() const { return script_; }
  const FieldDoorPolicy &policy() const { return policy_; }
  const std::vector<FieldDoorBinding> &bindings() const { return bindings_; }
  const FieldDoorBinding *binding(uint32_t) const;
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  std::array<uint8_t, 20> pin_{};
  std::string scene_, script_;
  FieldDoorPolicy policy_;
  std::vector<FieldDoorBinding> bindings_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
struct FieldDoorUi {
  bool cutscene = false, battle = false, pause = false;
};
struct FieldDoorSelection {
  std::string dialog, seen_key;
};
struct FieldDoorNpcHost {
  // Admit actual full source body/geometry + child AudioStreamPlayer Ready.
  std::function<bool(const FieldDoorBinding &, const FieldDoorPolicy &,
                     std::string &)>
      admit_ready;
  std::function<bool(uint32_t, bool &, std::string &)> body_is_current_player;
  std::function<bool(FieldDoorUi &, std::string &)> query_ui;
  std::function<bool(std::string_view, bool &, std::string &)> read_flag,
      read_seen;
  std::function<bool(std::string_view, std::string &)> mark_seen;
  // Actual Node.get_path runtime identity, never a fabricated /root name. A
  // detached live Node yields the source empty get_path diagnostic result.
  std::function<bool(uint32_t, std::string &, std::string &)> node_path;
  // All candidate Room programmes/effects, player pause/turn/blackbars/audio
  // and global timer capacity/order admitted before the first source side
  // effect.
  std::function<bool(const FieldDoorBinding &, const FieldDoorPolicy &,
                     std::string &)>
      admit_start;
  std::function<bool(uint32_t, const std::array<bool, 2> &, std::string &)>
      turn_player;
  std::function<bool(const std::array<bool, 3> &, std::string &)> pause_player;
  std::function<bool(bool, std::string &)> set_cutscene, black_bars;
  // Real bound child voice, source WAV/bus/volume/pitch, actual NDSP endpoint.
  std::function<bool(const FieldDoorBinding &, std::string &)> play_knock;
  // Source-global SceneTreeTimer insertion token. Scaled idle float time_left,
  // strict <0, pause_process default true, source insertion FIFO, end-of-list
  // snapshot excludes timers created during this pass. No private timer clock.
  std::function<bool(uint32_t, const FieldDoorPolicy &, uint64_t &,
                     std::string &)>
      create_timer;
  // Validate selected checked Room programme at timeout before marking seen.
  std::function<bool(const FieldDoorBinding &, const FieldDoorSelection &,
                     std::string &)>
      admit_dialogue;
  // Asynchronous existing Room source open_dialogue_box_and_unpause, actual
  // player completion funcref. Does not wait for Ready/done before returning.
  std::function<bool(const FieldDoorBinding &, const FieldDoorSelection &,
                     std::string_view, std::string &)>
      open_room_and_unpause;
};
struct FieldDoorState {
  uint32_t id = 0;
  bool ready = false, alive = true, attached = true, processing = false;
  std::vector<std::vector<std::string>> groups;
};
struct FieldDoorWait {
  uint64_t token = 0;
  uint32_t owner = 0;
};
class FieldDoorNpcRuntime {
public:
  bool initialize(const FieldDoorNpcData &, FieldDoorNpcHost, std::string &);
  bool ready(uint32_t, std::string &);
  bool body_enter(uint32_t, uint32_t, std::string &);
  bool idle_process(uint32_t, bool tree_can_process, std::string &);
  bool select_dialogue(uint32_t, FieldDoorSelection &, std::string &);
  // Actual scene-global source timer signal order calls this once per token.
  bool timer_timeout(uint64_t, std::string &);
  // Source detach is distinct from freeing. Detached live coroutine survives
  // SceneTreeTimer; re-add does not call Ready again. Free invalidates resume.
  bool exit_tree(uint32_t, std::string &);
  bool enter_tree(uint32_t, std::string &);
  bool free_instance(uint32_t, std::string &);
  const FieldDoorState *state(uint32_t) const;
  const std::vector<FieldDoorWait> &waiters() const { return waits_; }
  const FieldDoorNpcData *content() const { return data_; }

private:
  FieldDoorState *active(uint32_t, std::string &);
  bool flags(const FieldDoorBinding &, bool &, std::string &);
  bool start(uint32_t, std::string &);
  const FieldDoorNpcData *data_ = nullptr;
  FieldDoorNpcHost host_;
  std::vector<FieldDoorState> states_;
  std::vector<FieldDoorWait> waits_;
  size_t ready_index_ = 0;
  uint64_t last_token_ = 0;
};
} // namespace encore::upstream

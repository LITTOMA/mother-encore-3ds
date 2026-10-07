#pragma once
#include "encore/movement.hpp"
#include <array>
#include <functional>
#include <map>
#include <string>
#include <vector>
namespace encore::upstream {
enum class FieldPromptClipRole : uint32_t {
  Float = 1,
  Hide,
  Press,
  Reset,
  Show
};
enum class FieldPromptProperty : uint32_t {
  ArrowPosition = 1,
  BoxPosition,
  LabelPosition,
  LabelColor,
  RootColor,
  ArrowColor,
  Visible,
  Glow,
  FlashColor,
  FlashModifier,
  HideMethod
};
struct FieldPromptKey {
  float time = 0, ease = 1;
  std::array<float, 4> value{};
};
struct FieldPromptTrack {
  FieldPromptProperty property{};
  uint32_t update = 0;
  std::vector<FieldPromptKey> keys;
};
struct FieldPromptClip {
  FieldPromptClipRole role{};
  std::string name;
  float length = 0;
  bool loop = false;
  std::vector<FieldPromptTrack> tracks;
};
struct FieldPromptDescriptor {
  uint32_t id = 0, parent_id = 0, ready_ordinal = 0, category = 0;
  bool enabled = false;
  std::string node, key;
  Vec2 offset{};
};
struct FieldPromptArt {
  std::string path, label;
  std::array<uint8_t, 32> sha256{}, layout_sha256{};
  std::array<uint8_t, 20> engine_hash{};
  uint32_t bytes = 0, crc32 = 0, width = 0, height = 0;
  std::array<uint32_t, 4> label_crop{}, arrow_crop{};
  Vec2 label_origin{}, arrow_origin{};
};
class FieldPromptData {
public:
  bool load(const uint8_t *, size_t, std::string &);
  bool load_file(const char *, std::string &);
  bool valid() const { return valid_; }
  const FieldPromptArt &art() const { return art_; }
  const std::array<uint8_t, 20> &source_pin() const { return pin_; }
  const std::array<uint8_t, 32> &script_hash() const { return script_; }
  const std::array<uint8_t, 32> &scene_hash() const { return scene_; }
  uint32_t scene_id() const { return scene_id_; }
  const std::vector<FieldPromptDescriptor> &records() const { return records_; }
  const FieldPromptDescriptor *record(uint32_t) const;
  const FieldPromptClip *clip(FieldPromptClipRole) const;
  const std::array<std::string, 4> &choices() const { return choices_; }
  const std::array<std::array<float, 4>, 10> &initial() const {
    return initial_;
  }

private:
  FieldPromptArt art_;
  bool valid_ = false;
  uint32_t scene_id_ = 0;
  std::array<uint8_t, 20> pin_{};
  std::array<uint8_t, 32> script_{}, scene_{};
  std::array<std::string, 4> choices_{};
  std::array<std::array<float, 4>, 10> initial_{};
  std::vector<FieldPromptDescriptor> records_;
  std::vector<FieldPromptClip> clips_;
};
struct FieldPromptInstance {
  uint32_t id = 0;
  bool ready = false, enabled = false, nearby = false, force_show = false,
       force_hide = false, hidden = true, pressing = false, process = false,
       playing = false, started = false;
  FieldPromptClipRole clip{};
  float elapsed = 0;
  Vec2 position{}, scale{};
  std::string label;
  std::array<std::array<float, 4>, 10> properties{};
  bool visible() const { return properties[6][0] != 0; }
};
struct FieldPromptObservation {
  uint32_t settings_choice = 0;
  bool paused = true;
  Vec2 parent_scale{};
};
struct FieldPromptHost {
  std::function<bool(uint32_t parent, FieldPromptObservation &, std::string &)>
      observe;
  std::function<bool(std::string_view action, std::string &, std::string &)>
      key_name;
  // Bind actual authoritative entered/exited, pause/unpause and input/locale
  // signals. Signal owner must disconnect borrowed callbacks on scene exit.
  std::function<bool(uint32_t, std::function<bool(uint32_t, bool)>,
                     std::function<bool()>, std::function<bool()>,
                     std::string &)>
      connect;
  std::function<bool(uint32_t, const FieldPromptInstance &, std::string &)>
      publish;
  // Apply source local visibility immediately, including actual CanvasItem
  // visibility_changed/hide signals. publish must not duplicate these signals.
  std::function<bool(uint32_t, bool, std::string &)> visibility;
  std::function<bool(uint32_t, std::string &)> hide_signal;
  // Actual native AP endpoint: 1=play, 2=finished, 3=stop. No second clock.
  std::function<bool(uint32_t, FieldPromptClipRole, uint32_t, std::string &)> native_animation;
};
class FieldPromptRuntime {
public:
 const FieldPromptData*data()const{return data_;}
  bool initialize(const FieldPromptData &, FieldPromptHost, std::string &);
  bool create(uint32_t);
  bool ready(uint32_t);
  bool nearby(uint32_t, uint32_t object, bool);
  bool pause_changed(uint32_t);
  bool inputs_or_locale_changed(uint32_t);
  bool set_enabled(uint32_t, bool, bool quick = false);
  bool force(uint32_t, int mode, bool quick = false);
  bool press(uint32_t);
  bool assign_enabled(uint32_t, bool);
  bool canvas_hide(uint32_t);
  bool idle_frame(uint32_t, float);
  bool destroy(uint32_t);
  const FieldPromptInstance *instance(uint32_t) const;
  const std::string &error() const { return error_; }

private:
  const FieldPromptData *data_ = nullptr;
  FieldPromptHost host_;
  std::map<uint32_t, FieldPromptInstance> instances_;
  std::string error_;
  uint32_t last_ready_ = 0;
  bool had_ready_ = false;
  bool fail(const char *);
  FieldPromptInstance *get(uint32_t);
  bool observe(uint32_t, FieldPromptObservation &);
  bool reset_scale(FieldPromptInstance &, const FieldPromptObservation &);
  bool key_name(FieldPromptInstance &);
  bool refresh(FieldPromptInstance &, bool);
  bool play(FieldPromptInstance &, FieldPromptClipRole);
  bool publish(FieldPromptInstance &);
  bool apply(FieldPromptInstance &, FieldPromptProperty,
             const std::array<float, 4> &);
  bool animate(FieldPromptInstance &, const FieldPromptClip &, float, float,
               bool);
};
} // namespace encore::upstream

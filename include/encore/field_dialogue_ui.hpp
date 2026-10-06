#pragma once
#include "encore/dialogue_choices.hpp"
#include "encore/field_dialogue_lifecycle.hpp"
#include "encore/house_presentation.hpp"
#include "encore/localization.hpp"
#include <set>
namespace encore::upstream {
class FieldDialogueVisualData;
class FieldDialogueAudioData;
class FieldNativeTimerData;
struct FieldDialogueUiOwnershipRoster {
  const FieldDialogueVisualData *visual = nullptr;
  const FieldDialogueAudioData *audio = nullptr;
  const FieldNativeTimerData *timers = nullptr;
};
enum class FieldDialogueUiKind : uint32_t {
  Pending,
  CanvasLayer,
  Control,
  NinePatchRect,
  GridContainer,
  HBoxContainer,
  Label,
  RichTextLabel,
  VScrollBar,
  AnimationPlayer
};
enum class FieldDialogueUiRole : uint32_t {
  None,
  Canvas,
  Box,
  NameBox,
  Name,
  Clip,
  HBox,
  Text,
  Bullet,
  Options,
  Option1,
  Option2,
  Option3,
  Option4,
  Option5,
  Option6,
  TextScroll,
  BulletScroll,
  NameClip,
  BoxAnimation,
  NameAnimation
};
struct FieldDialogueUiNode {
  uint32_t id = 0, parent = 0, ready = 0;
  FieldDialogueUiRole role{};
  FieldDialogueUiKind kind{};
  std::string path, native_class, script;
};
struct FieldDialogueUiControl {
  uint32_t id = 0, flags = 0, align = 0, valign = 0, columns = 0;
  int32_t visible_characters = -1;
  float hseparation = 0, vseparation = 0, line_separation = 0, font_height = 0,
        font_ascent = 0, font_descent = 0;
  Vec2 minimum{};
  BattleValue patch{}, style{};
  std::array<float, 5> range{};
  std::string font, texture, material, text, bbcode;
};
struct FieldDialogueUiAnimation {
  uint32_t owner = 0, target = 0;
  FieldDialogueClip clip;
};
struct FieldDialogueUiResource {
  uint32_t id = 0, width = 0, height = 0;
  std::string path, source;
  std::array<uint8_t, 32> sha{};
};
class FieldDialogueUiData {
public:
  bool load(const uint8_t *, size_t, const FieldIdentity &, std::string &);
  bool load_file(const char *, const FieldIdentity &, std::string &);
  bool valid() const { return valid_; }
  bool scene_admitted() const { return false; }
  FieldIdentity identity() const { return identity_; }
  const std::array<uint8_t, 32> &recipe_ir_sha() const { return recipe_; }
  const std::vector<FieldDialogueUiNode> &nodes() const { return nodes_; }
  const std::vector<FieldDialogueUiControl> &controls() const {
    return controls_;
  }
  const std::vector<FieldDialogueUiAnimation> &animations() const {
    return animations_;
  }
  const std::vector<FieldDialogueUiResource> &resources() const {
    return resources_;
  }
  const FieldDialogueUiNode *node(uint32_t) const;
  const FieldDialogueUiNode *role(FieldDialogueUiRole) const;
  const FieldDialogueUiControl *control(uint32_t) const;
  BattleValue display() const { return display_; }
  BattleValue name_tween() const { return name_tween_; }
  const std::string &started_signal() const { return started_; }
  const std::string &finished_signal() const { return finished_; }
  const std::string &rect_signal() const { return rect_; }
  const std::string &range_signal() const { return range_; }
  const std::string &value_signal() const { return value_; }
  const std::string &delay_marker() const { return delay_; }
  const std::string &wait_marker() const { return wait_; }
  bool verify_house(HouseView, std::string &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> recipe_{}, house_{};
  BattleValue display_{}, name_tween_{};
  std::vector<FieldDialogueUiNode> nodes_;
  std::vector<FieldDialogueUiControl> controls_;
  std::vector<FieldDialogueUiAnimation> animations_;
  std::vector<FieldDialogueUiResource> resources_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
  std::string started_, finished_, rect_, range_, value_, delay_, wait_;
};
struct FieldDialogueUiNativeControl {
  FieldObjectId object = 0;
  uint32_t source = 0;
  BattleValue rect{};
  std::array<float, 4> margins{}, anchors{};
  Vec2 minimum{};
  bool entered = false, ready = false, visible = false;
  int32_t visible_characters = -1;
  std::string text, bbcode;
  std::array<double, 5> range{};
  std::vector<HouseSourceTextLine> parsed_lines;
};
struct FieldDialogueUiHost {
  // Resolve the real source DynamicFont/Fallback atlas, not invented metrics.
  std::function<bool(std::string_view, std::string_view, Vec2 &, std::string &)>
      font_minimum;
  // Attach this actual CanvasLayer to the actual Viewport/World2D and source
  // layer.
  std::function<bool(FieldObjectId, const FieldRecipeCanvasLayer &,
                     std::string &)>
      canvas_enter;
  std::function<bool(FieldObjectId, std::string_view, std::string_view,
                     std::string &)>
      emit;
  std::function<bool(FieldObjectId, std::string_view, std::string &)> rename;
  // The actual original ShaderMaterial and texture palette service must be
  // bound.
  std::function<bool(FieldObjectId, std::string_view, std::string_view,
                     std::string &)>
      material_admit;
  std::function<bool(FieldObjectId, std::string &)> canvas_exit;
  std::function<bool(FieldObjectId, const FieldRecipeControl &, std::string &)>
      control_enter;
  std::function<bool(FieldObjectId, std::string &)> control_exit;
  std::function<bool(FieldObjectId, std::string_view, double, std::string &)>
      range_value;
};
// Owns the 20 audited native bodies only. Script Ready is never approved by
// class-name membership; remaining 27 original nodes require separate owners.
class FieldDialogueUiRuntime {
public:
  FieldDialogueUiRuntime() = default;
  FieldDialogueUiRuntime(const FieldDialogueUiRuntime &) = delete;
  FieldDialogueUiRuntime &operator=(const FieldDialogueUiRuntime &) = delete;
  const FieldDialogueUiData *data() const { return data_; }
  bool initialize(const FieldDialogueUiData &, const FieldNodeRecipeData &,
                  HouseView, FieldNodeTreeRuntime &, FieldDialogueUiHost,
                  std::string &);
  bool attach(FieldObjectId, std::string &);
  // Full recipe composition supplies actual independently checked owners for
  // every foreign native body. This grants no foreign lifecycle notification.
  bool attach(FieldObjectId, const FieldDialogueUiOwnershipRoster &,
              std::string &);
  bool admits_native(FieldObjectId, std::string &) const;
  bool enter_native(FieldObjectId, std::string &);
  bool ready_native(FieldObjectId, std::string &);
  bool exit_native(FieldObjectId, std::string &);
  bool play(FieldObjectId, std::string_view, std::string &);
  bool animation_process(FieldObjectId, double, std::string &);
  bool native_step(const FieldDialogueStep &, FieldObjectId, std::string &);
  bool sort_children(FieldObjectId, std::string &);
  // Borrow the existing source text printer, including tags, locale and shared
  // RNG.
  bool sync_text(FieldObjectId, const HousePresentation &, std::string &);
  bool sync_choices(FieldObjectId, const DialogueChoices &,
                    const LocaleSelection *, std::string &);
  bool pose(FieldObjectId, WorldDialoguePose &, std::string &) const;
  std::vector<FieldDialogueUiNativeControl> option_labels(FieldObjectId) const;
  const FieldDialogueUiNativeControl *control(FieldObjectId) const;
  bool release(FieldObjectId, std::string &);

private:
  struct Animation {
    const FieldDialogueUiAnimation *data = nullptr;
    float time = 0;
    bool playing = false;
    uint64_t revision = 0;
  };
  struct Instance {
    FieldObjectId root = 0;
    std::map<uint32_t, FieldObjectId> ids;
    std::map<uint32_t, FieldDialogueUiNativeControl> controls;
    std::map<uint32_t, Animation> animations;
    std::set<uint32_t> pending_sort;
    bool canvas_entered = false, text_bound = false, name_connection = false;
    WorldDialoguePose text_pose{};
  };
  const FieldDialogueUiData *data_ = nullptr;
  const FieldNodeRecipeData *recipe_ = nullptr;
  HouseView house_{};
  FieldNodeTreeRuntime *tree_ = nullptr;
  FieldDialogueUiHost host_{};
  std::map<FieldObjectId, Instance> instances_;
  std::map<FieldObjectId, FieldObjectId> owners_;
  bool attach_owned(FieldObjectId, const FieldDialogueUiOwnershipRoster *,
                    std::string &);
  Instance *instance(FieldObjectId);
  const Instance *instance(FieldObjectId) const;
  bool layout(Instance &, std::string &);
  bool queue_sort(Instance &, uint32_t, std::string &);
  bool position(Instance &, uint32_t, Vec2, std::string &);
  bool font_size(Instance &, uint32_t, std::string &);
  bool update_scroll(Instance &, uint32_t, uint32_t, std::string &);
  bool scalar_sample(const FieldDialogueClip &, float, Vec2 &,
                     std::string &) const;
};
} // namespace encore::upstream

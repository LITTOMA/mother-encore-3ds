#pragma once
#include "encore/field_camera_arrows.hpp"
#include "encore/field_game_camera.hpp"
#include "encore/field_node_recipe.hpp"
#include <set>
namespace encore::upstream {
struct FieldDialogueVisualNode {
  uint32_t id = 0, parent = 0, ready = 0, pause = 0;
  int32_t priority = 0;
  std::string path, native_class, script;
};
struct FieldDialogueCursor {
  uint32_t id = 0, sprite = 0, player = 0, timer = 0, flags = 0, frame = 0,
           sprite_flags = 0;
  bool visible = false, playing = false, centered = false;
  std::string menu;
  Vec2 offset{}, size{}, position{}, scale{}, drawing_offset{},
      sprite_position{}, sprite_offset{}, sprite_scale{};
  float rotation = 0, fps = 0, sprite_rotation = 0;
  std::vector<FieldArrowClip> clips;
};
class FieldDialogueVisualData {
public:
  bool load(const uint8_t *, size_t, const FieldIdentity &, std::string &);
  bool load_file(const char *, const FieldIdentity &, std::string &);
  bool valid() const { return valid_; }
  bool scene_admitted() const { return false; }
  FieldIdentity identity() const { return identity_; }
  const std::array<uint8_t, 32> &recipe_sha() const { return recipe_; }
  const std::vector<FieldDialogueVisualNode> &nodes() const { return nodes_; }
  const std::vector<FieldDialogueCursor> &cursors() const { return cursors_; }
  const FieldDialogueVisualNode *node(uint32_t) const;
  const FieldDialogueCursor *cursor(uint32_t) const;
  const FieldCameraArrowsData &arrows() const { return arrows_; }
  const FieldGameCameraData &camera() const { return camera_; }
  float tween_length() const { return tween_; }
  float timer_length() const { return timer_; }
  const std::string &transition() const { return transition_; }
  const std::string &ease() const { return ease_; }
  const std::vector<std::string> &signals() const { return signals_; }
  const std::vector<std::string> &actions() const { return actions_; }
  const std::vector<std::string> &sounds() const { return sounds_; }

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> recipe_{};
  float tween_ = 0, timer_ = 0;
  std::vector<std::string> signals_, actions_, sounds_;
  std::string transition_, ease_;
  std::vector<FieldDialogueVisualNode> nodes_;
  std::vector<FieldDialogueCursor> cursors_;
  FieldCameraArrowsData arrows_;
  FieldGameCameraData camera_;
};
struct FieldDialogueCursorItem {
  FieldObjectId object = 0;
  Vec2 position{}, size{};
  bool visible = false, label = false;
  std::string text;
};
struct FieldDialogueCursorMenu {
  FieldObjectId object = 0;
  uint32_t columns = 0;
  Vec2 position{};
  std::vector<FieldDialogueCursorItem> items;
};
struct FieldDialogueCursorState {
  FieldObjectId object = 0, menu = 0;
  uint32_t source = 0, frame = 0;
  int32_t index = 0;
  bool ready = false, on = false, playing = false, visible = false,
       animation_playing = false;
  float timeout = 0, animation_time = 0;
  uint32_t clip = 0;
  uint64_t revision = 0, tween = 0;
  Vec2 position{}, offset{};
};
struct FieldDialogueVisualHost {
  // Real CanvasItem/AnimationPlayer/Camera2D/Area/Shape server bodies. Enter
  // and Ready must inspect actual object/viewport/material/shape ownership.
  std::function<bool(FieldObjectId, const FieldDialogueVisualNode &,
                     FieldTreePhase, std::string &)>
      native;
  std::function<bool(FieldObjectId, FieldDialogueCursorMenu &, std::string &)>
      menu;
  std::function<bool(FieldObjectId, Vec2 &, std::string &)> controls;
  std::function<bool(FieldObjectId, bool &can_process, bool &update_pending,
                     std::string &)>
      observe;
  std::function<bool(FieldObjectId, FieldObjectId, std::string_view,
                     std::function<bool()>, std::string &)>
      connect;
  std::function<bool(FieldObjectId, std::string_view,
                     const FieldDeferredValue &, std::string &)>
      emit;
  std::function<bool(FieldObjectId, const FieldDialogueCursorState &,
                     std::string &)>
      publish;
  std::function<bool(FieldObjectId, float &, std::string &)> timer_left;
  std::function<bool(FieldObjectId, std::string &)> timer_start;
  std::function<bool(FieldObjectId, std::string_view, std::string &)> sound;
  std::function<bool(FieldObjectId, uint64_t &, Vec2, float, std::string_view,
                     std::string_view, std::string &)>
      tween_position;
  // pause/custom_step/kill preserves source _set_on(false) final position.
  std::function<bool(uint64_t, float, bool, std::string &)> finish_tween;
  std::function<bool(FieldObjectId, std::function<bool()>, std::string &)>
      await_idle;
  FieldObjectId global = 0;
};
// One owner per real factory instance. No source stable ID is a runtime ID.
class FieldDialogueVisualRuntime {
public:
  bool initialize(const FieldDialogueVisualData &, const FieldNodeRecipeData &,
                  FieldNodeTreeRuntime &, FieldDialogueVisualHost,
                  SourceRandom &, FieldGameCameraHost, FieldCameraArrowsHost,
                  std::string &);
  bool attach(FieldObjectId, std::string &);
  FieldObjectId object(uint32_t) const;
  bool enter_native(FieldObjectId, std::string &);
  bool ready_native(FieldObjectId, std::string &);
  bool exit_native(FieldObjectId, std::string &);
  bool ready_script(FieldObjectId, std::string &);
  bool physics_cursor(FieldObjectId, std::string &);
  bool input_cursor(FieldObjectId, std::string_view, bool, std::string &);
  // Source direct cursor_index assignment does not move the CanvasItem.
  bool cursor_index_property(FieldObjectId, int32_t, std::string &);
  bool cursor_index(FieldObjectId, int32_t, bool, std::string &);
  bool cursor_on(FieldObjectId, bool, std::string &);
  bool refresh_cursor(FieldObjectId, bool, std::string &);
  bool visibility_cursor(FieldObjectId, std::string &);
  bool play_cursor(FieldObjectId, std::string_view, std::string &);
  bool idle_cursor(FieldObjectId, float, std::string &);
  bool cursor_draw(FieldObjectId, FieldArrowDraw &, std::string &);
  const FieldDialogueCursorState *cursor_state(FieldObjectId) const;
  FieldGameCameraRuntime &camera() { return camera_; }
  FieldCameraArrowsRuntime &arrows() { return arrows_; }
  const std::map<uint32_t, FieldObjectId> &objects() const { return objects_; }

private:
  const FieldDialogueVisualData *data_ = nullptr;
  const FieldNodeRecipeData *recipe_ = nullptr;
  FieldNodeTreeRuntime *tree_ = nullptr;
  FieldDialogueVisualHost host_;
  FieldObjectId root_ = 0;
  std::map<uint32_t, FieldObjectId> objects_;
  std::map<FieldObjectId, FieldDialogueCursorState> cursors_;
  std::set<FieldObjectId> entered_, ready_;
  FieldGameCameraRuntime camera_;
  FieldCameraArrowsRuntime arrows_;
  const FieldDialogueVisualNode *node(FieldObjectId) const;
  FieldDialogueCursorState *cursor(FieldObjectId, std::string &);
  bool menu(FieldDialogueCursorState &, FieldDialogueCursorMenu &,
            std::string &);
  bool frame(FieldDialogueCursorState &, uint32_t, std::string &);
  bool publish(FieldDialogueCursorState &, std::string &);
  bool animate(FieldDialogueCursorState &, float, std::string &);
  int32_t valid_index(const FieldDialogueCursor &,
                      const FieldDialogueCursorMenu &, int32_t, int32_t) const;
};
} // namespace encore::upstream

#pragma once
#include "encore/field_node_recipe.hpp"
#include "encore/field_node_tree.hpp"
#include "encore/field_npc.hpp"
#include "encore/field_programme.hpp"
#include <array>
#include <functional>
#include <map>
namespace encore::upstream {
enum class FieldDialogueStage : uint32_t {
  OpenCreated,
  Ready,
  Begin,
  EndPrefix,
  AfterStop,
  EndSignal,
  ManagerDone,
  AfterDone,
  CloseFinished
};
enum class FieldDialogueOp : uint32_t {
  StoreDialogue,
  PauseMenuInactive,
  StackPush,
  DeferredAdd,
  UiCutscene,
  KeyClose,
  BlackBars,
  GlobalCutscene,
  InfoHide,
  SetProgramme,
  SetTalker,
  ConnectName,
  InputRelease,
  ArrowHide,
  VisibleCharacters,
  TextClear,
  BulletClear,
  PhoneLocation,
  VoiceVolume,
  TextHide,
  NameClose,
  CashClose,
  TelepathyRestore,
  KeyUpdate,
  EmitCutsceneEnded,
  ResetPhrase,
  CloseBox,
  ClearDialogue,
  UnpauseIfOwned,
  EmitDone,
  ReturnCamera,
  ReturnOffset,
  RemoveUi,
  StopTalker
};
struct FieldDialogueStep {
  FieldDialogueStage stage{};
  FieldDialogueOp op{};
  uint32_t role = 0;
  double value = 0;
  std::string text;
};
struct FieldDialogueKey {
  double time = 0, transition = 0;
  Vec2 value{};
};
struct FieldDialogueClip {
  std::string name, path;
  uint32_t resource = 0, interpolation = 0;
  bool loop = false, loop_wrap = false;
  double length = 0, step = 0;
  std::vector<FieldDialogueKey> keys;
};
struct FieldDialogueNativeRef {
  uint32_t id = 0, ready = 0;
  std::string native_class;
};
struct FieldDialogueProgramme {
  std::string path, source;
  std::array<uint8_t, 32> sha{};
};
class FieldDialogueLifecycleData {
public:
  bool load(const uint8_t *, size_t, std::string &);
  bool load_file(const char *, std::string &);
  bool valid() const { return valid_; }
  const std::array<uint8_t, 20> &commit() const { return pin_; }
  const std::string &scene() const { return scene_; }
  const std::array<uint8_t, 32> &scene_sha() const { return scene_sha_; }
  std::string_view node(uint32_t role) const;
  const std::vector<FieldDialogueStep> &steps() const { return steps_; }
  const std::vector<FieldDialogueProgramme> &programmes() const {
    return programmes_;
  }
  const FieldDialogueClip &close_clip() const { return close_; }
  const FieldDialogueClip &name_close_clip() const { return name_close_; }
  const std::array<uint8_t, 32> &factory_ir_sha() const {
    return factory_ir_sha_;
  }
  uint32_t factory_scene_id() const { return factory_scene_id_; }
  uint32_t factory_node_count() const { return factory_node_count_; }
  const FieldDialogueNativeRef *reference(uint32_t role) const {
    return role && role <= references_.size() ? &references_[role - 1]
                                              : nullptr;
  }
  double closed_y() const { return closed_y_; }
  int64_t initial_response() const { return response_; }
  const std::array<bool, 2> &pause_args() const { return pause_; }
  const std::string &done_signal() const { return done_; }
  const std::string &ready_signal() const { return ready_; }
  const std::string &animation_signal() const { return animation_; }
  const std::string &name_signal() const { return name_signal_; }
  const std::string &paused_method() const { return paused_method_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;
  bool supports(const FieldProgrammeData &, uint32_t, std::string &) const;

private:
  bool valid_ = false;
  std::array<uint8_t, 20> pin_{};
  std::string scene_, done_, ready_, animation_, name_signal_, paused_method_;
  std::array<uint8_t, 32> scene_sha_{};
  double closed_y_ = 0;
  int64_t response_ = 0;
  std::array<bool, 2> pause_{};
  std::array<uint8_t, 32> factory_ir_sha_{};
  uint32_t factory_scene_id_ = 0, factory_node_count_ = 0;
  std::array<FieldDialogueNativeRef, 12> references_{};
  std::array<std::string, 12> nodes_{};
  std::vector<FieldDialogueStep> steps_;
  std::vector<FieldDialogueProgramme> programmes_;
  FieldDialogueClip close_, name_close_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
struct FieldDialogueObservation {
  FieldObjectId stable_canvas = 0, talker = 0;
  bool player_paused = false, dialogue_present = false, queued_battle = false,
       talker_valid = false, in_ui_stack = false, set_respawn = false,
       name_nonempty = false;
  uint32_t actor_count = 0;
  double dialogue_y = 0;
};
struct FieldDialogueLifecycleHost {
  // These validate the independently checked native NodeRecipe and all source
  // native/script Ready adapters before instantiate or any player/UI mutation.
  std::function<bool(const FieldDialogueLifecycleData &, std::string &)>
      admit_factory;
  std::function<bool(FieldObjectId, std::string &)> admit_parent;
  std::function<bool(const FieldDialogueStep &, const FieldProgrammeContext &,
                     std::string &)>
      admit_step;
  std::function<bool(FieldObjectId, FieldDialogueObservation &, std::string &)>
      observe;
  std::function<bool(bool, bool, std::string &)> pause_player;
  std::function<bool(std::string &)> unpause_player;
  // Actual source singletons and native typed node consumers, never event logs.
  std::function<bool(const FieldDialogueStep &, FieldObjectId, std::string &)>
      manager;
  std::function<bool(const FieldDialogueStep &, FieldObjectId, FieldObjectId,
                     std::string &)>
      global;
  std::function<bool(const FieldDialogueStep &, FieldObjectId, std::string &)>
      native;
  std::function<bool(FieldObjectId, const FieldDialogueClip &, std::string &)>
      play_animation;
  std::function<bool(std::string &)> close_sound, restore_telepathy;
  std::function<bool(bool, double, std::string &)> return_camera;
  std::function<bool(FieldObjectId, uint32_t, std::string_view,
                     std::function<bool()>, std::string &)>
      connect_ready;
  std::function<bool(FieldObjectId, uint32_t, std::string_view,
                     std::function<bool(int64_t)>, std::string &)>
      connect_done;
  std::function<bool(FieldObjectId, uint32_t, std::string_view,
                     std::function<bool()>, std::string &)>
      connect_animation;
  std::function<bool(FieldObjectId, uint32_t, std::string &)> start_programme;
  std::function<bool(const FieldProgrammeData &, uint32_t, FieldObjectId,
                     const FieldProgrammeContext &, std::string &)>
      bind_programme;
  std::function<bool(FieldObjectId, std::string_view, int64_t, std::string &)>
      emit_done;
  std::function<bool(FieldObjectId, uint32_t, std::string &)> disconnect;
};
enum class FieldDialogueLifecyclePhase : uint32_t {
  Closed,
  WaitingReady,
  Ready,
  Running,
  Stopped,
  TalkerCleared,
  EndSignal,
  EmittingDone,
  Closing,
  Removed,
  Error
};
// Specific DialogueBox/UI manager coroutine bridge. There is no new dialogue VM
// or synthetic time. Real Ready/animation/Done and global SceneTree order drive
// it.
class FieldDialogueLifecycleRuntime {
public:
  bool initialize(const FieldDialogueLifecycleData &,
                  const FieldProgrammeData &, const FieldNodeRecipeData &,
                  FieldNodeTreeRuntime &, FieldNpcRuntime &,
                  FieldDialogueLifecycleHost, std::string &);
  bool admit(const DialogueAction &, const FieldProgrammeContext &,
             std::string &);
  bool open(const FieldProgrammeData &, uint32_t, const FieldProgrammeContext &,
            uint32_t, FieldObjectId &, std::string &);
  bool ready(FieldObjectId, uint32_t, std::string &);
  bool admit_ready(FieldObjectId, uint32_t, std::string &) const;
  bool apply(const DialogueAction &, const FieldProgrammeContext &,
             std::string &);
  bool done(FieldObjectId, uint32_t, int64_t, std::string &);
  bool animation_finished(FieldObjectId, uint32_t, std::string &);
  bool deleted(FieldObjectId, std::string &);
  FieldDialogueLifecyclePhase phase() const { return phase_; }
  FieldObjectId object() const { return object_; }
  const std::string &error() const { return error_; }

private:
  bool fail(std::string &, const char *);
  bool preflight(const FieldProgrammeContext &, std::string &);
  bool run(FieldDialogueStage, std::string &);
  bool step(const FieldDialogueStep &, std::string &);
  bool close(std::string &);
  bool remove(std::string &);
  bool remove_object(FieldObjectId, std::string &);
  bool owned(std::string &) const;
  const FieldDialogueLifecycleData *data_ = nullptr;
  const FieldProgrammeData *programmes_ = nullptr;
  const FieldNodeRecipeData *recipe_ = nullptr;
  FieldNodeTreeRuntime *tree_ = nullptr;
  FieldNpcRuntime *npcs_ = nullptr;
  FieldDialogueLifecycleHost host_{};
  FieldDialogueLifecyclePhase phase_ = FieldDialogueLifecyclePhase::Closed;
  FieldProgrammeContext context_{};
  FieldObjectId object_ = 0, parent_ = 0, animation_owner_ = 0;
  uint32_t generation_ = 0, programme_ = 0;
  std::array<FieldObjectId, 12> nodes_{};
  bool was_paused_ = false, done_seen_ = false, closing_wait_ = false;
  struct Retired {
    FieldObjectId animation = 0;
    uint32_t generation = 0;
    bool waiting = false;
  };
  std::map<FieldObjectId, Retired> retired_;
  std::string error_;
};
} // namespace encore::upstream

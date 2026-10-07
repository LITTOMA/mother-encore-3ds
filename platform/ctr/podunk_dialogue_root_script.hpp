#pragma once
#include "podunk_dialogue_host.hpp"
#include "podunk_programme_host.hpp"
#include "encore/house_ui_continuation.hpp"

namespace encore::ctr {
// Independent source receipt for the concrete script's constructor/input.
// Native recipe, lifecycle, UI and text printer remain their existing owners.
class PodunkDialogueRootData {
public:
  bool load(const uint8_t *, size_t, std::string &);
  bool load_file(const char *, std::string &);
  bool valid() const { return valid_; }
  upstream::FieldIdentity identity() const { return identity_; }
  const std::array<uint8_t, 32> &recipe_sha() const { return recipe_; }
  const std::array<uint8_t, 32> &script_sha() const { return script_; }
  const std::array<uint8_t, 32> &base_sha() const { return base_; }
  const std::string &base_source() const { return base_source_; }
  const std::array<uint8_t, 32> &actor_sha() const { return actor_sha_; }
  const std::array<std::string, 3> &actions() const { return actions_; }
  const std::array<bool, 6> &defaults() const { return defaults_; }
  const std::string &actor_source() const { return actor_; }
  const std::string &bullet_format() const { return bullet_format_; }
  const std::string &bullet_key() const { return bullet_key_; }
  const std::string &wait_method() const { return wait_method_; }
  const std::string &name_method() const { return name_method_; }
  const std::string &initial_phrase() const { return phrase_; }
  const std::string &open_animation() const { return open_animation_; }
  const std::string &wait_signal() const { return wait_signal_; }
  int64_t initial_response() const { return response_; }
  uint32_t options_source() const { return options_; }
  int32_t cursor_reset() const { return cursor_reset_; }

private:
  bool valid_ = false;
  upstream::FieldIdentity identity_{};
  std::array<uint8_t, 32> recipe_{}, script_{}, base_{}, actor_sha_{};
  uint32_t options_ = 0;
  int32_t cursor_reset_ = 0;
  std::array<std::string, 3> actions_{};
  std::array<bool, 6> defaults_{};
  std::string base_source_, actor_, bullet_format_, bullet_key_, wait_method_,
      name_method_, phrase_, open_animation_, wait_signal_;
  int64_t response_ = 0;
};
struct PodunkDialogueWaitConnection {
  upstream::FieldObjectId emitter = 0, receiver = 0;
  std::string signal, method;
  uint32_t flags = 0;
};
struct PodunkDialogueRootEndpoints {
  // Must return the registry's current actual owner; never capture old Tree.
  std::function<upstream::FieldNodeTreeRuntime *(upstream::FieldObjectId)> tree;
  std::function<bool(upstream::FieldObjectId, PodunkDialogueFrame &,
                     std::string &)>
      frame;
  std::function<bool(upstream::Vec2 &, std::string &)> player_position;
  std::function<bool(upstream::FieldObjectId, bool &, std::string &)>
      animation_playing;
  // Observe the actual source PackedScene connection, registered before Ready.
  std::function<bool(upstream::FieldObjectId, PodunkDialogueWaitConnection &,
                     std::string &)>
      wait_connection;
  // Actual ResourceLoader and translations; source receipt is checked first.
  std::function<bool(std::string_view, const std::array<uint8_t, 32> &,
                     uint64_t &, std::string &)>
      preload_actor;
  std::function<bool(std::string_view, std::string_view, std::string &,
                     std::string &)>
      translate_bullet;
  std::function<bool(uint64_t, std::string &)> release_actor;
  // Real source Viewport input event and same text printer ownership.
  std::function<bool(upstream::FieldObjectId, std::string &)> input_handled;
  std::function<bool(upstream::FieldObjectId, upstream::HousePresentation &,
                     bool, std::string &)>
      printer_ownership;
  // Actual source labels are updated before Arrow/show/on/placement and
  // Grid.show.
  std::function<bool(upstream::FieldObjectId, const upstream::DialogueChoices &,
                     const upstream::LocaleSelection *, std::string &)>
      prepare_options_labels;
  std::function<bool(upstream::FieldObjectId, std::string &)>
      hide_options_labels;
  std::function<bool(upstream::FieldObjectId, bool, std::string &)>
      talker_talking;
  // Native UI's source name-size/tween receiver. It does not advance text.
  std::function<bool(upstream::FieldObjectId, std::string &)> name_rect_changed;
};
struct PodunkDialogueRootSourceState {
  PodunkDialogueScriptState script;
  uint32_t generation = 0;
  upstream::FieldObjectId options = 0;
  bool running = false, owns_printer = false, closing = false,
       choices_shown = false, phrase_prepared = false;
};
class PodunkDialogueRootOwner final : public PodunkDialogueRootScript {
public:
  bool initialize(const PodunkDialogueRootData &,
                  const upstream::FieldDialogueLifecycleData &,
                  const upstream::FieldNodeRecipeData &,
                  upstream::HousePresentation &, PodunkDialogueProgrammePort &,
                  PodunkDialogueHost &, upstream::DialogueChoices &,
                  upstream::FieldNativeTimers &,
                  const upstream::LocaleSelection *,
                  PodunkDialogueRootEndpoints, std::string &);
  // Closed actual source owners only; no factory/Ready/callback replay.
  bool observes_closed_printer(const upstream::HousePresentation &,std::string &) const;
  bool admit_printer_rebind(const upstream::HousePresentation &,
                           const upstream::HousePresentation &,std::string &) const;
  bool rebind_printer(const upstream::HousePresentation &,
                     upstream::HousePresentation &,std::string &);
  bool admit_programme_rebind(const PodunkDialogueProgrammePort &old,
                             const PodunkDialogueProgrammeBinding &expected_old,
                             const PodunkDialogueProgrammePort &next,
                             const upstream::HousePresentation &next_printer,
                             std::string &) const;
  bool rebind_programme(const PodunkDialogueProgrammePort &old,
                       const PodunkDialogueProgrammeBinding &expected_old,
                       PodunkDialogueProgrammePort &next, std::string &);
  bool bind_observation_defaults(const upstream::HouseUiContinuationData &,std::string &);
  bool source_observation(upstream::FieldObjectId,upstream::FieldDialogueObservation &,std::string &) const;
  bool admit(const upstream::FieldDialogueLifecycleData &,
             const upstream::FieldNodeRecipeData &,
             upstream::HousePresentation &, std::string &) const override;
  bool construct(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
                 upstream::HousePresentation &, std::string &) override;
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase,
             const std::array<upstream::FieldObjectId, 12> &,
             std::string &) override;
  bool state(upstream::FieldObjectId, PodunkDialogueScriptState &,
             std::string &) const override;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &) override;
  bool release(upstream::FieldObjectId, std::string &) override;
  // Invoked by the actual lifecycle start_programme and before ShowDialogue.
  bool begin(upstream::FieldObjectId, uint32_t, std::string &);
  bool phrase_begin(upstream::FieldObjectId, std::string &);
  // Same actual programme text object, only after the one printer accepted it.
  bool presented_text(upstream::FieldObjectId,
                      const upstream::FieldProgrammeText &, std::string &);
  bool presented_text(upstream::FieldObjectId, upstream::RoomView,
                      uint32_t absolute_command, const upstream::HouseDialogue &,
                      std::string &);
  bool observe_instances(std::vector<PodunkDialogueRootSourceState> &,
                         std::string &) const;
  bool source_frame(upstream::FieldObjectId, PodunkDialogueFrame &,
                    std::string &) const;
  // Same printer callback, synchronously at the actual source finish point.
  bool text_completed(upstream::FieldObjectId, std::string &);
  bool wait_timeout(upstream::FieldObjectId, std::string &);
  bool native_step(upstream::FieldObjectId, const upstream::FieldDialogueStep &,
                   std::string &);

private:
  struct Instance {
    PodunkDialogueScriptState state{};
    uint64_t actor_resource = 0;
    upstream::FieldObjectId options = 0;
    uint32_t generation = 0;
    bool can_input = false, auto_advance = false, finished = false,
         stopped = false, box_shown = false, name_shown = false,
         running = false, owns_printer = false, choices_shown = false,
         phrase_prepared = false, closing = false;
    std::map<std::string,upstream::FieldObjectId> actors;
    bool queued_battle=false,set_respawn=false;
    std::string bullet, phrase;
    int64_t response = 0;
  };
  const upstream::HouseUiContinuationData *observation_data_=nullptr;
  const PodunkDialogueRootData *data_ = nullptr;
  const upstream::FieldDialogueLifecycleData *life_ = nullptr;
  const upstream::FieldNodeRecipeData *recipe_ = nullptr;
  upstream::HousePresentation *printer_ = nullptr;
  PodunkDialogueProgrammePort *programme_ = nullptr;
  PodunkDialogueHost *dialogue_ = nullptr;
  upstream::DialogueChoices *choices_ = nullptr;
  upstream::FieldNativeTimers *timers_ = nullptr;
  const upstream::LocaleSelection *locale_ = nullptr;
  PodunkDialogueRootEndpoints host_{};
  std::map<upstream::FieldObjectId, Instance> instances_;
  Instance *live(upstream::FieldObjectId, bool, std::string &);
  bool checked_refs(Instance &, const std::array<upstream::FieldObjectId, 12> &,
                    std::string &);
  bool finish(Instance &, std::string &);
  bool flush_audio(Instance &, std::string &);
  bool input(Instance &, const PodunkDialogueFrame &, std::string &);
};
} // namespace encore::ctr

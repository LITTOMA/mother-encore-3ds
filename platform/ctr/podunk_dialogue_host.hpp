#pragma once
#include "encore/field_dialogue_audio.hpp"
#include "encore/field_dialogue_ui.hpp"
#include "encore/field_dialogue_visual.hpp"
#include "encore/field_global_registry.hpp"
#include "encore/field_native_timer.hpp"
#include "field_dialogue_audio_player.hpp"
#include <memory>
#include <set>

namespace encore::ctr {
// Actual owner of the original DialogueBox/AbstractDialogueBox script instance.
// It borrows the same text printer and programme executor used by the existing
// world. Its source constructor/onready/input state is never inferred from a
// native CanvasLayer callback or the existence of a function object.
struct PodunkDialogueScriptState {
  upstream::FieldIdentity identity{};
  upstream::FieldObjectId object = 0;
  upstream::HousePresentation *printer = nullptr;
  std::array<upstream::FieldObjectId, 12> references{};
  bool constructed = false, entered = false, ready = false;
};
class PodunkDialogueRootScript {
public:
  virtual ~PodunkDialogueRootScript() = default;
  virtual bool admit(const upstream::FieldDialogueLifecycleData &,
                     const upstream::FieldNodeRecipeData &,
                     upstream::HousePresentation &, std::string &) const = 0;
  virtual bool construct(upstream::FieldObjectId,
                         const upstream::FieldNodeDescriptor &,
                         upstream::HousePresentation &, std::string &) = 0;
  virtual bool phase(upstream::FieldObjectId, upstream::FieldTreePhase,
                     const std::array<upstream::FieldObjectId, 12> &,
                     std::string &) = 0;
  virtual bool state(upstream::FieldObjectId, PodunkDialogueScriptState &,
                     std::string &) const = 0;
  virtual bool deferred(const upstream::FieldDeferredMessage &,
                        std::string &) = 0;
  virtual bool release(upstream::FieldObjectId, std::string &) = 0;
};
// This is borrowed from the real SceneTree traversal/input dispatch. The
// owner sets it immediately before dispatching that same notification; this
// class neither advances nor manufactures another clock or input event.
struct PodunkDialogueFrame {
  float delta = 0;
  bool tree_paused = false, pressed = false;
  upstream::FieldTreePhase phase{};
  std::string action;
};
struct PodunkDialogueFactoryServices {
  upstream::FieldDialogueUiHost ui;
  upstream::FieldDialogueVisualHost visual;
  upstream::FieldGameCameraHost camera;
  upstream::FieldCameraArrowsHost arrows;
  upstream::FieldDialogueAudioHost audio;
};
struct PodunkDialogueServices {
  // Camera/Arrows legacy cores address checked source IDs. Their adapters
  // must capture this actual factory's ObjectID map, especially while an old
  // dialogue closes concurrently with its replacement.
  std::function<bool(upstream::FieldObjectId,
                     const std::map<uint32_t, upstream::FieldObjectId> &,
                     PodunkDialogueFactoryServices &, std::string &)>
      factory_services;
  std::function<bool(const upstream::FieldNodeRecipeData &, std::string &)>
      admit_native_services;
  upstream::FieldDialogueLifecycleHost lifecycle;
  PodunkDialogueRootScript *root_script = nullptr;
  // Bind to the actual source AudioServer, shared signal bus and native
  // Viewport/Canvas/Physics object owner. These endpoints must inspect their
  // state; no callbacks below are supplied with permissive defaults.
  std::function<bool(upstream::FieldObjectId,
                     const upstream::FieldNodeDescriptor &,
                     upstream::FieldTreePhase, std::string &)>
      native_notification;
  std::function<bool(const upstream::FieldDeferredMessage &, std::string &)>
      native_deferred;
  std::function<bool(upstream::FieldObjectId, std::string_view,
                     const upstream::FieldDeferredValue &, std::string &)>
      emit;
  std::function<bool(upstream::FieldObjectId, PodunkDialogueFrame &,
                     std::string &)>
      frame;
  std::function<bool(upstream::FieldObjectId, const upstream::HouseAudioEvent &,
                     std::string &)>
      menu_sound;
  // Source InputSound is not the primary voice. This resolves its actual
  // independently checked node binding; a source path string in C++ would
  // duplicate game content. Returning another Audio node is rejected below.
  std::function<bool(upstream::FieldObjectId, upstream::FieldObjectId &,
                     const upstream::FieldDialogueAudioData &, std::string &)>
      input_sound;
};
// Specific owning composition of the original DialogueBox recipe. The four
// resource families cover its complete subtree mutually exclusively. Creation
// grants no Ready: each native body and each real script sees actual Tree
// notifications. RootScript is an explicit remaining source owner, not a fake
// UI singleton or a second programme VM.
class PodunkDialogueHost {
public:
  PodunkDialogueHost() = default;
  PodunkDialogueHost(const PodunkDialogueHost &) = delete;
  PodunkDialogueHost &operator=(const PodunkDialogueHost &) = delete;
  bool initialize(const upstream::FieldDialogueLifecycleData &,
                  const upstream::FieldProgrammeData &,
                  const upstream::FieldNodeRecipeData &,
                  const upstream::FieldDialogueUiData &,
                  const upstream::FieldDialogueVisualData &,
                  const upstream::FieldDialogueAudioData &,
                  const upstream::FieldNativeTimerData &,
                  upstream::FieldGlobalRegistry &,
                  upstream::FieldNativeTimers &, upstream::FieldNpcRuntime &,
                  upstream::HouseView, upstream::HousePresentation &,
                  upstream::SourceRandom &, FieldDialogueAudioPlayer &,
                  PodunkDialogueServices, std::string &);
  bool admit_factory(std::string &) const;
  // Called by the one real Tree host only for this checked recipe's nodes.
  bool bind(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
            upstream::FieldNodeBinding &, std::string &);
  bool dispatch(upstream::FieldObjectId, const upstream::FieldNodeBinding &,
                upstream::FieldTreePhase, std::string &);
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  bool open(const upstream::FieldProgrammeData &, uint32_t,
            const upstream::FieldProgrammeContext &, uint32_t,
            upstream::FieldObjectId &, std::string &);
  bool admit(const upstream::DialogueAction &,
             const upstream::FieldProgrammeContext &, std::string &);
  bool apply(const upstream::DialogueAction &,
             const upstream::FieldProgrammeContext &, std::string &);
  bool admit_ready(upstream::FieldObjectId, uint32_t, std::string &) const;
  // Invoke only after actual ShowDialogue has updated that same printer.
  bool presented_text(upstream::FieldObjectId,
                      const upstream::FieldProgrammeText &, std::string &);
  bool sync_choices(upstream::FieldObjectId, const upstream::DialogueChoices &,
                    const upstream::LocaleSelection *, std::string &);
  // The caller passes the events already drawn by HousePresentation. In
  // particular event.pitch is used directly; no second RNG draw occurs.
  bool audio_event(upstream::FieldObjectId, const upstream::HouseAudioEvent &,
                   std::string &);
  bool native_step(const upstream::FieldDialogueStep &, upstream::FieldObjectId,
                   std::string &);
  bool play_animation(upstream::FieldObjectId,
                      const upstream::FieldDialogueClip &, std::string &);
  upstream::FieldDialogueUiRuntime *ui(upstream::FieldObjectId);
  upstream::FieldDialogueVisualRuntime *visual(upstream::FieldObjectId);
  upstream::FieldDialogueAudioRuntime *audio(upstream::FieldObjectId);
  upstream::FieldDialogueLifecycleRuntime *lifecycle() {
    return initialized_ ? &lifecycle_ : nullptr;
  }

private:
  enum class Owner { Ui, Visual, Timer, Audio };
  struct Factory {
    upstream::FieldObjectId root = 0;
    std::shared_ptr<upstream::FieldNodeTreeRuntime> tree;
    upstream::FieldDialogueUiRuntime ui;
    upstream::FieldDialogueVisualRuntime visual;
    upstream::FieldDialogueAudioRuntime audio;
    std::map<uint32_t, upstream::FieldObjectId> objects;
    std::set<upstream::FieldObjectId> entered, native_ready, script_ready,
        timer_attached, deleting;
  };
  const upstream::FieldDialogueLifecycleData *life_ = nullptr;
  const upstream::FieldProgrammeData *programmes_ = nullptr;
  const upstream::FieldNodeRecipeData *recipe_ = nullptr;
  const upstream::FieldDialogueUiData *ui_data_ = nullptr;
  const upstream::FieldDialogueVisualData *visual_data_ = nullptr;
  const upstream::FieldDialogueAudioData *audio_data_ = nullptr;
  const upstream::FieldNativeTimerData *timer_data_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldNativeTimers *timers_ = nullptr;
  upstream::FieldNpcRuntime *npcs_ = nullptr;
  upstream::HouseView house_{};
  upstream::HousePresentation *printer_ = nullptr;
  upstream::SourceRandom *random_ = nullptr;
  FieldDialogueAudioPlayer *audio_player_ = nullptr;
  PodunkDialogueServices services_;
  std::shared_ptr<upstream::FieldNodeTreeRuntime> lifecycle_tree_;
  upstream::FieldDialogueLifecycleRuntime lifecycle_;
  std::map<upstream::FieldObjectId, std::unique_ptr<Factory>> factories_;
  std::map<upstream::FieldObjectId, upstream::FieldObjectId> owners_;
  bool initialized_ = false;
  bool owner(const upstream::FieldNodeDescriptor &, Owner &,
             std::string &) const;
  bool attach(upstream::FieldObjectId, std::string &);
  Factory *factory(upstream::FieldObjectId);
  const Factory *factory(upstream::FieldObjectId) const;
  bool script_state(const Factory &, bool, std::string &) const;
  bool full_ready(const Factory &, std::string &) const;
  bool references(const Factory &, std::array<upstream::FieldObjectId, 12> &,
                  std::string &) const;
  bool process(Factory &, upstream::FieldObjectId, Owner,
               upstream::FieldTreePhase, std::string &);
  bool erase(Factory &, upstream::FieldObjectId, std::string &);
};
} // namespace encore::ctr

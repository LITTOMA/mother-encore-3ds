#pragma once
#include "encore/field_battle_bg_resources.hpp"
#include "encore/field_native_timer.hpp"
#include "encore/field_ui_preloads.hpp"
#include "podunk_dialogue_host.hpp"
namespace encore::ctr {
// Actual globaldata object; the flags-only component and old House save
// snapshot do not implement this contract or approve its constructor/Ready.
class PodunkUiGlobalDataOwner {
public:
  virtual ~PodunkUiGlobalDataOwner() = default;
  virtual upstream::FieldGlobalExternalBinding binding() const = 0;
  virtual bool state(upstream::FieldGlobalExternalState &,
                     std::string &) const = 0;
  virtual bool menu_flavor(std::string &, std::string &) const = 0;
};
class PodunkUiSignalOwner {
public:
  virtual ~PodunkUiSignalOwner() = default;
  virtual bool attach_ui(const upstream::FieldGlobalExternalBinding &,
                         std::string &) = 0;
  virtual bool menu_flavor_updated(upstream::FieldObjectId, std::string &) = 0;
};
// The process' existing native SceneTree/Viewport and other complete autoload
// owners. No fallback Node/identity Canvas/constant Ready object is provided.
class PodunkUiExternalFactory {
public:
  virtual ~PodunkUiExternalFactory() = default;
  virtual bool construct(upstream::FieldObjectId,
                         const upstream::FieldGlobalExternalSpec &,
                         std::unique_ptr<upstream::FieldGlobalExternalObject> &,
                         std::string &) = 0;
};
struct PodunkUiFrame {
  upstream::FieldTreePhase phase{};
  float delta = 0;
  bool tree_paused = false;
};
struct PodunkUiNativeState {
  upstream::FieldObjectId object = 0;
  upstream::FieldNodeBinding binding;
  bool constructed = false, script_constructed = false, native_ready = false,
       script_ready = false;
};
// Actual constructor/native/script owner for the seven source UI trees.
// Dialogue and native Timer are routed to their already implemented owners.
class PodunkUiNativeOwner {
public:
  virtual ~PodunkUiNativeOwner() = default;
  virtual bool admit_constructor(const upstream::FieldNodeRecipeData &,
                                 std::string &) const = 0;
  virtual bool bind(upstream::FieldNodeTreeRuntime &, upstream::FieldObjectId,
                    const upstream::FieldNodeDescriptor &,
                    upstream::FieldNodeBinding &, std::string &) = 0;
  // Construct the actual native Node part using this same Timer receipt;
  // Timer-specific state/process belongs exclusively to FieldNativeTimers.
  virtual bool bind_timer_node(upstream::FieldNodeTreeRuntime &,
                               upstream::FieldObjectId,
                               const upstream::FieldNodeDescriptor &,
                               const upstream::FieldNodeBinding &,
                               std::string &) = 0;
  virtual bool state(upstream::FieldObjectId, PodunkUiNativeState &,
                     std::string &) const = 0;
  virtual bool phase(upstream::FieldObjectId,
                     const upstream::FieldNodeBinding &,
                     upstream::FieldTreePhase, std::string &) = 0;
  virtual bool deferred(const upstream::FieldDeferredMessage &,
                        std::string &) = 0;
  virtual bool release(upstream::FieldObjectId,
                       const upstream::FieldNodeBinding &, std::string &) = 0;
  virtual bool frame(upstream::FieldObjectId, PodunkUiFrame &,
                     std::string &) const = 0;
};
struct PodunkUiHostData {
  std::shared_ptr<const upstream::FieldUiManagerData> ui;
  std::shared_ptr<const upstream::FieldUiPreloadsData> preloads;
  std::shared_ptr<const upstream::FieldNodeRecipeData> dialogue;
  std::shared_ptr<const upstream::FieldBattleBgData> backgrounds;
  const upstream::FieldNativeTimerData *timer_data = nullptr;
};
struct PodunkUiHostServices {
  PodunkUiGlobalDataOwner *globaldata = nullptr;
  PodunkUiSignalOwner *signals = nullptr;
  PodunkUiExternalFactory *external = nullptr;
  PodunkUiNativeOwner *native_ui = nullptr;
  PodunkDialogueHost *dialogue = nullptr;
  upstream::FieldNativeTimers *timers = nullptr;
};
class PodunkUiHost {
public:
  PodunkUiHost() = default;
  PodunkUiHost(const PodunkUiHost &) = delete;
  PodunkUiHost &operator=(const PodunkUiHost &) = delete;
  // Must outlive the Registry-owned UiManager/Shader callbacks and resources.
  bool initialize(const upstream::FieldGlobalRegistryData &,
                  upstream::FieldGlobalRegistry &, upstream::SourceRandom &,
                  PodunkUiHostData, PodunkUiHostServices, std::string &);
  upstream::FieldGlobalRegistryHost registry_host();
  bool connect_tree(std::shared_ptr<upstream::FieldNodeTreeRuntime>,
                    upstream::FieldGlobalRegistry::NodeDispatch, std::string &);
  bool construct(upstream::FieldObjectId,
                 const upstream::FieldGlobalExternalSpec &,
                 std::unique_ptr<upstream::FieldGlobalExternalObject> &,
                 std::string &);
  // Called by the actual Viewport add_child/exit path, never by a guessed ID.
  bool entered(upstream::FieldObjectId parent, std::string &);
  bool advance_ready(std::string &);
  // Actual global.LOAD source call before UiManager Ready. This changes the
  // same Registry-owned ShaderMaterial that later UI nodes share.
  bool set_menu_flavors(std::string_view, std::string &);
  bool menu_shader(const upstream::FieldUiMenuShader *&, std::string &) const;
  bool exited(std::string &);
  bool state(upstream::FieldGlobalExternalState &, std::string &) const;
  bool bind(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
            upstream::FieldNodeBinding &, std::string &);
  bool dispatch(upstream::FieldObjectId, const upstream::FieldNodeBinding &,
                upstream::FieldTreePhase, std::string &);
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  bool release(upstream::FieldObjectId, const upstream::FieldNodeBinding &,
               std::string &);
  upstream::FieldUiManagerRuntime *ui() const;
  upstream::FieldBattleBgResources &backgrounds() { return backgrounds_; }
  const upstream::FieldUiPreloadRecipes &preloads() const { return preloads_; }
  std::vector<std::string> pending_owners() const;

private:
  enum class Owner { Timer, Dialogue, Native };
  struct Owned {
    Owner kind;
    upstream::FieldNodeBinding binding;
    bool deleting = false, timer_released = false;
  };
  const upstream::FieldGlobalRegistryData *registry_data_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::SourceRandom *random_ = nullptr;
  PodunkUiHostData data_;
  PodunkUiHostServices services_;
  upstream::FieldUiPreloadRecipes preloads_;
  upstream::FieldBattleBgResources backgrounds_;
  std::shared_ptr<upstream::FieldNodeTreeRuntime> tree_;
  upstream::FieldGlobalRegistry::NodeDispatch node_dispatch_;
  upstream::FieldUiManagerRuntime *ui_ = nullptr;
  upstream::FieldObjectId ui_object_ = 0;
  uint64_t tick_origin_ = 0;
  bool clock_started_ = false, backgrounds_bound_ = false;
  std::map<upstream::FieldObjectId, Owned> owned_;
  bool clock(uint64_t &, uint64_t &, std::string &);
  bool construct_instances(std::string &);
  bool flavor(std::string &, std::string &) const;
  bool verify_binding(upstream::FieldObjectId,
                      const upstream::FieldNodeDescriptor &,
                      const upstream::FieldNodeBinding &, std::string &) const;
  const upstream::FieldNodeRecipeData *
  recipe(const upstream::FieldIdentity &) const;
};
} // namespace encore::ctr

#pragma once
#include "encore/house_reentry.hpp"
#include "podunk_house_door_fade.hpp"
#include "podunk_player_scene_services.hpp"
#include "podunk_scene_audio.hpp"
#include "music_region_service.hpp"

namespace encore::ctr {
struct PodunkDoorScene {
  std::shared_ptr<upstream::FieldNodeTreeRuntime> tree;
  upstream::FieldObjectId root = 0, player_parent = 0;
  upstream::FieldIdentity identity{};
};
// A source-backed destination keeper owns its actual native/House consumers.
// Each method executes its named operation; returned objects are inspected by
// the adapter. Preparing a detached candidate never supplies Enter or Ready.
class PodunkDoorTargetOwner {
public:
  virtual ~PodunkDoorTargetOwner() = default;
  virtual const upstream::HouseReentryData *data() const = 0;
  virtual const upstream::FieldGlobalRegistry *registry() const = 0;
  virtual bool prepare(const upstream::FieldDoorDescriptor &,
                       upstream::FieldDoorCandidate &, std::string &) = 0;
  virtual bool release(const upstream::FieldDoorCandidate &, bool,
                       std::string &) = 0;
  virtual bool instantiate(const upstream::FieldDoorCandidate &, PodunkDoorScene &,
                           std::string &) = 0;
  virtual bool leave_old(upstream::FieldObjectId, upstream::FieldSceneHost &,
                         std::string &) = 0;
  virtual bool free_old(std::shared_ptr<upstream::FieldNodeTreeRuntime>,
                        upstream::FieldObjectId, std::string &) = 0;
  virtual bool attach_root(const PodunkDoorScene &, std::string &) = 0;
  // Transfer the SAME subtree, update Registry ownership, rebind every native
  // owner before Enter, then add_child. No new Player or cold Ready replay.
  virtual bool attach_player(const PodunkDoorScene &, PodunkPlayerHost &,
                            std::string &) = 0;
  virtual bool reparent_persistent(const PodunkDoorScene &,
                                  upstream::FieldObjectId, std::string &) = 0;
  virtual PodunkPlayerSceneServices *player_services() = 0;
  virtual bool update_key_indicator(std::string &) = 0;
  virtual bool make_player_camera_current(upstream::FieldObjectId,
                                          std::string &) = 0;
};
struct PodunkOutdoorDoorInput {
  const upstream::FieldDoorData *doors = nullptr;
  const upstream::FieldSceneData *scene = nullptr;
  const upstream::FieldSceneAudioData *audio_data = nullptr;
  upstream::FieldGlobalRegistry *registry = nullptr;
  PodunkNativeRoot *native_root = nullptr;
  upstream::FieldObjectSignals *signals = nullptr;
  upstream::FieldGlobalConstructorRuntime *global = nullptr;
  upstream::FieldGlobalDataRuntime *characters = nullptr;
  upstream::FieldGlobalFlagsRuntime *flags = nullptr;
  upstream::FieldSceneHost *scene_lifecycle = nullptr;
  upstream::FieldMusicChangerRuntime *music = nullptr;
  MusicRegionService *music_service = nullptr;
  HouseUiContinuation *ui = nullptr;
  PodunkPlayerHost *player = nullptr;
  PodunkPlayerSceneServices *player_services = nullptr;
  PodunkSceneAudio *audio = nullptr;
  PodunkHouseDoorFade *fade = nullptr;
  PodunkDoorTargetOwner *target = nullptr;
  // State's real allocation map can resolve children during source Ready,
  // before Tree.source_object has finished building its static index.
  std::function<bool(uint32_t, upstream::FieldObjectId &, std::string &)> source;
  std::function<bool(uint32_t, const std::vector<uint32_t> &, std::string &)>
      shapes_ready;
  // Source create_party_followers yields the same SceneTree idle signal even
  // for a singleton. This registers its actual delayed party_changed waiter.
  std::function<bool(std::string &)> schedule_party_changed;
};
// FieldDoorRuntime remains the only coroutine/execution cursor. This adapter
// adds actual source callbacks to the existing Consumers owner before Ready.
class PodunkOutdoorDoor {
public:
  bool prepare(PodunkOutdoorDoorInput, std::string &);
  bool apply(upstream::FieldDoorHost &, std::string &);
  bool bind_runtime(upstream::FieldDoorRuntime &, std::string &);
  bool handles(const upstream::FieldDeferredMessage &) const;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  bool declaration(upstream::FieldObjectId, std::string_view, uint32_t &,
                   std::string &) const;
  const upstream::FieldDoorData *source_data()const{return in_.doors;}
  upstream::FieldDoorRuntime *source_runtime()const{return runtime_;}
  const upstream::FieldGlobalRegistry *registry()const{return in_.registry;}
  // Checks the same onready references, body callback slot and live source
  // coroutine during ReparentPersistent. It creates no new runtime/waiter.
  bool rebind_persistent_door(upstream::FieldNodeTreeRuntime &old_tree,
      upstream::FieldNodeTreeRuntime &next,
      const std::vector<upstream::FieldObjectId>&,std::string &);
  const PodunkDoorScene &destination() const { return destination_; }
private:
  bool live(std::string &) const;
  bool source(uint32_t, upstream::FieldObjectId &, std::string &) const;
  bool player(upstream::FieldObjectId, std::string &) const;
  bool actual_root(upstream::FieldObjectId &, std::string &) const;
  bool observe(upstream::FieldDoorContext &, std::string &) const;
  bool resolve(const upstream::FieldDoorDescriptor &,
               const upstream::FieldDoorAudio &, std::string &);
  bool connect(uint32_t, std::function<bool(uint64_t, std::string &)>, std::string &);
  bool scene_step(upstream::FieldDoorSceneStep,
                  const upstream::FieldDoorCandidate &, uint64_t,
                  upstream::Vec2, upstream::Vec2, std::string &);
  bool position(upstream::FieldObjectId, upstream::Vec2, std::string &);
  bool update_party(upstream::FieldObjectId, std::string &);
  bool persistent(uint32_t, bool, std::string &);
  bool schedule(uint32_t, std::string &);
  bool verify_destination(bool entered, std::string &) const;
  PodunkPlayerSceneServices *services() const;
  PodunkOutdoorDoorInput in_;
  upstream::FieldDoorRuntime *runtime_ = nullptr;
  upstream::FieldDoorCandidate candidate_{};
  PodunkDoorScene destination_{};
  std::shared_ptr<upstream::FieldNodeTreeRuntime> old_tree_;
  upstream::FieldObjectId old_root_ = 0, transitioning_door_ = 0;
  std::vector<upstream::FieldObjectId> detached_persistent_;
  // Original onready references survive old-scene deletion and reparenting.
  std::map<uint32_t, upstream::FieldObjectId> source_objects_;
  std::map<upstream::FieldObjectId,
           std::function<bool(uint64_t, std::string &)>> body_slots_;
  uint32_t next_step_ = 0;
  struct DeferredCall {
    upstream::FieldObjectId receiver = 0;
    upstream::FieldDoorCandidate candidate;
    upstream::Vec2 position{}, direction{};
    // The fourth original argument is a real typed empty Array, not nil.
    std::vector<upstream::FieldDeferredValue> parameters;
  } deferred_call_;
  bool deferred_pending_ = false, failed_ = false;
};
} // namespace encore::ctr

#pragma once
#include "encore/player_motion.hpp"
#include "podunk_player_physics_world.hpp"
#include "podunk_scene_audio.hpp"
#include "podunk_scene_consumers.hpp"
namespace encore::ctr {
struct PodunkReadyInteractionInput {
  PodunkSceneConsumerInput scene;
  upstream::FieldSceneConsumers consumers;
  PodunkPlayerPhysicsWorld *physics = nullptr;
  PodunkSceneAudio *audio = nullptr;
  const upstream::PlayerMotionData *motion = nullptr;
  std::function<bool(uint32_t, upstream::FieldObjectId &, std::string &)>
      source;
  std::function<bool(bool &, bool &, std::string &)> frame;
};
// Concrete source Ready operations. All state/clock belongs to the existing
// typed consumers; this bridge owns only actual signal and deferred slots.
class PodunkReadyInteractionBridges {
public:
  bool prepare(PodunkReadyInteractionInput, std::string &);
  bool apply(PodunkSceneMechanismOwners &, std::string &);
  bool handles(const upstream::FieldDeferredMessage &) const;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  bool release(upstream::FieldObjectId, std::string &);

private:
  using Args = std::vector<upstream::FieldDeferredValue>;
  using Callback = std::function<bool(const Args &, std::string &)>;
  struct Deferred {
    bool value = false;
    std::function<bool()> commit;
  };
  struct Connection {
    upstream::FieldObjectId emitter = 0, target = 0;
    std::string signal, method;
  };
  bool source(uint32_t, upstream::FieldObjectId &, std::string &,
              bool entered = false) const;
  bool position(uint32_t, upstream::Vec2, std::string &);
  bool observe_sparkle(uint32_t, upstream::FieldSparklesObservation &,
                       std::string &);
  bool observe_door(uint32_t, upstream::FieldOpenableObservation &,
                    std::string &);
  bool connect_area(uint32_t, std::function<bool(uint64_t)>,
                    std::function<bool(uint64_t)>, std::string &);
  bool audio_playing(uint32_t, bool, std::string &);
  PodunkReadyInteractionInput input_{};
  std::map<std::pair<upstream::FieldObjectId, std::string>, Callback> methods_;
  std::map<upstream::FieldObjectId, std::deque<Deferred>> disabled_;
  std::vector<Connection> connections_;
  bool prepared_ = false;
};
} // namespace encore::ctr

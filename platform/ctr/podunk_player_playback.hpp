#pragma once
#include "encore/player_ready.hpp"
namespace encore::ctr {
// The actual local-to-scene Playback Resource owns this player's state
// machine. Ready, motion and internal animation all borrow this one object.
class PodunkPlayerPlayback final : public upstream::FieldGlobalSourceResource {
public:
  static bool construct(
      std::shared_ptr<const upstream::PlayerInitializationData>,
      std::shared_ptr<const upstream::PlayerReadyData>,
      upstream::FieldGlobalRegistry &, upstream::FieldObjectId &,
      PodunkPlayerPlayback *&, std::string &);
  bool bind_tracks(upstream::PlayerGraphHost, std::string &);
  upstream::PlayerAnimationGraph &graph() { return graph_; }
  const upstream::PlayerAnimationGraph &graph() const { return graph_; }
  bool tracks_bound() const { return graph_.data() == ready_.get(); }
  upstream::FieldGlobalExternalBinding binding() const override { return binding_; }
  const char *resource_class() const override {
    return "AnimationNodeStateMachinePlayback";
  }
  bool state(upstream::FieldGlobalExternalState &, std::string &) const override;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &) override;
  bool persist_append(upstream::FieldObjectId, std::string &) override;
  bool assign_stable_canvas(upstream::FieldObjectId, std::string &) override;
private:
  std::shared_ptr<const upstream::PlayerInitializationData> player_;
  std::shared_ptr<const upstream::PlayerReadyData> ready_;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldGlobalExternalBinding binding_{};
  upstream::PlayerAnimationGraph graph_;
};
} // namespace encore::ctr

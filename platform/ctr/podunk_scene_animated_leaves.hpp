#pragma once
#include "encore/field_camera_arrows.hpp"
#include "encore/field_dropped.hpp"
#include "encore/field_global_registry.hpp"
#include "encore/field_object_signals.hpp"
#include "encore/field_sparkles.hpp"
#include <memory>
class FieldCameraArrowsRenderer;
class FieldSparklesRenderer;
namespace encore::ctr {
// Native leaf adapter borrowing the source consumers' actual single bodies and
// clocks. It never creates a second arrow, sparkle or animation state.
class PodunkSceneAnimatedLeaves final
    : public upstream::FieldPresentSparklesLeafOwner {
public:
  PodunkSceneAnimatedLeaves();
  ~PodunkSceneAnimatedLeaves();
  bool prepare(const upstream::FieldNodeTreeData &,
               upstream::FieldNodeTreeRuntime &,
               upstream::FieldGlobalRegistry &, upstream::FieldObjectSignals &,
               const upstream::FieldCameraArrowsData &,
               upstream::FieldCameraArrowsRuntime &,
               const upstream::FieldSparklesData &,
               upstream::FieldSparklesRuntime &,
               upstream::FieldPresentRuntime &, upstream::FieldDroppedRuntime &,
               const char *, std::string &);
  bool construct(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
                 const upstream::FieldIdentity &, std::string &);
  bool bind(upstream::FieldObjectId, upstream::FieldNodeBinding &,
            std::string &) const;
  bool finish_factory(std::string &);
  bool owns(upstream::FieldObjectId) const;
  bool drawable(upstream::FieldObjectId) const;
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase, float,
             bool tree_paused, bool actual_update_pending, std::string &);
  bool publish_arrow(uint32_t, const upstream::FieldArrowSpriteState &,
                     std::string &);
  bool animation_signal(uint32_t, bool started, uint32_t clip, std::string &);
  bool sprite_signal(uint32_t, upstream::FieldArrowSignal, std::string &);
  bool sparkle_signal(uint32_t, upstream::FieldSparklesSignal, std::string &);
  bool begin_draw(uint64_t, std::string &);
  bool draw(upstream::FieldObjectId, upstream::Vec2 camera, std::string &);
  bool admit(const upstream::FieldPresentData &,
             const upstream::FieldPresentBinding &,
             std::string &) const override;
  bool signal(uint32_t, upstream::FieldPresentSparklesEvent,
              std::string &) override;
  upstream::FieldNodeTreeRuntime *tree() const { return tree_; }
  const upstream::FieldGlobalRegistry *registry() const { return registry_; }
  const upstream::FieldNodeTreeData *source() const { return source_; }

private:
  enum class Kind { Arrow, Player, Sparkles };
  struct Instance {
    Kind kind{};
    upstream::FieldNodeBinding binding{};
    bool entered = false, ready = false;
  };
  bool actual(upstream::FieldObjectId, std::string &) const;
  bool resolve(uint32_t, upstream::FieldObjectId &, std::string &) const;
  bool schedule(upstream::FieldObjectId, bool, std::string &);
  const upstream::FieldNodeTreeData *source_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldObjectSignals *signals_ = nullptr;
  const upstream::FieldCameraArrowsData *arrows_data_ = nullptr;
  upstream::FieldCameraArrowsRuntime *arrows_ = nullptr;
  const upstream::FieldSparklesData *sparkles_data_ = nullptr;
  upstream::FieldSparklesRuntime *sparkles_ = nullptr;
  upstream::FieldPresentRuntime *present_ = nullptr;
  upstream::FieldDroppedRuntime *dropped_ = nullptr;
  std::unique_ptr<FieldCameraArrowsRenderer> arrows_gpu_;
  std::unique_ptr<FieldSparklesRenderer> sparkles_gpu_;
  std::map<upstream::FieldObjectId, Instance> instances_;
  std::map<uint32_t, upstream::FieldObjectId> objects_;
  uint64_t draw_epoch_ = 0;
  bool finished_ = false, draw_started_ = false;
};
} // namespace encore::ctr

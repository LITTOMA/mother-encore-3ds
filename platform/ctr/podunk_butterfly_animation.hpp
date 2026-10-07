#pragma once
#include "encore/field_butterfly.hpp"
#include "podunk_scene_npc_world.hpp"
namespace encore::ctr {
// Two native AnimationPlayers per checked Butterfly. The existing source
// runtime owns all clip playback/time; this owner controls actual Tree dispatch.
class PodunkButterflyAnimation final : public PodunkSceneNativeMechanism {
public:
  bool prepare(const upstream::FieldButterflyData &,
               const upstream::FieldNodeTreeData &,
               upstream::FieldNodeTreeRuntime &, upstream::FieldGlobalRegistry &,
               upstream::FieldButterflyRuntime &, PodunkSceneNpcWorld &,
               std::string &);
  // Wrap the real Sprite/Area publisher and actual source admission port.
  bool apply(upstream::FieldButterflyHost &, std::string &);
  bool finish_factory(std::string &) const;
  bool set_active(upstream::FieldObjectId, bool, std::string &);
  bool active(upstream::FieldObjectId, bool &, std::string &) const;
  bool playback(upstream::FieldObjectId, bool &, float &, std::string &) const;
  bool owns(const upstream::FieldNodeDescriptor &) const override;
  bool owns(upstream::FieldObjectId) const override;
  bool construct(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
                 const upstream::FieldIdentity &, std::string &) override;
  bool bind(upstream::FieldObjectId, upstream::FieldNodeBinding &,
            std::string &) override;
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase, float, bool,
             bool, std::string &) override;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &) override;
  bool release(upstream::FieldObjectId, std::string &) override;
private:
  struct Player {
    const upstream::FieldButterflyBinding *source = nullptr;
    uint32_t leaf = 0, role = 0;
    upstream::FieldObjectId object = 0;
    bool active = true, entered = false, ready = false;
  };
  const Player *actual(upstream::FieldObjectId, std::string &) const;
  bool sync(Player &, std::string &);
  bool source_ready(const upstream::FieldButterflyBinding &, std::string &) const;
  bool publish(const upstream::FieldButterflyBinding &,
               const upstream::FieldButterflyState &, std::string &);
  const upstream::FieldButterflyData *data_ = nullptr;
  const upstream::FieldNodeTreeData *nodes_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldButterflyRuntime *runtime_ = nullptr;
  PodunkSceneNpcWorld *world_ = nullptr;
  std::map<uint32_t, Player> players_;
  std::map<upstream::FieldObjectId, uint32_t> objects_;
  bool applied_ = false;
};
} // namespace encore::ctr

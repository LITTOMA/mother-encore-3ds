#pragma once
#include "encore/field_global_registry.hpp"
#include "encore/field_npc.hpp"
#include "encore/field_sprite_bridge.hpp"
#include "podunk_scene_loop.hpp"
#include <map>

namespace encore::ctr {
// Actual CharacterSprite AnimationTree.new() nodes. The already checked NPC
// graph is the only frame evaluator; Sprite/Canvas owns the actual image.
class PodunkNpcAnimationHost final : public PodunkSceneNativeMechanism {
public:
  bool prepare(const upstream::FieldNodeTreeData &,
               const upstream::FieldSpriteData &,
               const upstream::FieldNpcData &, upstream::FieldNodeTreeRuntime &,
               upstream::FieldGlobalRegistry &, upstream::FieldSpriteRuntime &,
               upstream::FieldNpcRuntime &, std::string &);
  // Replace only graph endpoints and wrap the actual Sprite publisher.
  // Other source Sprite signals/reflection/native-property owners stay intact.
  bool apply(upstream::FieldSpriteHost &, std::string &);
  // For a compositor that supplies its actual publish endpoint after graph
  // ports are assembled: invoke in that endpoint, before native pose publish.
  bool observe_sprite(uint32_t, const upstream::FieldSpriteInstance &,
                      std::string &);
  bool owns(const upstream::FieldNodeDescriptor &) const override;
  bool owns(upstream::FieldObjectId) const override;
  bool construct(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
                 const upstream::FieldIdentity &, std::string &) override;
  bool bind(upstream::FieldObjectId, upstream::FieldNodeBinding &,
            std::string &) override;
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase,
             float actual_delta, bool tree_paused, bool actual_update_pending,
             std::string &) override;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &) override;
  bool release(upstream::FieldObjectId, std::string &) override;
  upstream::FieldObjectId animation_tree(uint32_t sprite_source) const;

private:
  struct Graph {
    uint32_t sprite = 0, npc = 0;
    upstream::FieldNodeDescriptor descriptor;
    upstream::FieldIdentity identity{};
    upstream::FieldObjectId object = 0, parent = 0;
    uint32_t player_source = 0;
    const upstream::FieldSpriteAnimation *animation = nullptr;
    std::vector<upstream::FieldSpriteTag> tags;
    std::vector<upstream::FieldSpriteConnection> connections;
    std::vector<std::string> states;
    std::string travel;
    upstream::Vec2 blend{};
    float scale = 1;
    bool entered = false, ready = false, active = false;
  };
  struct ClipPlayer {
    uint32_t sprite = 0;
    upstream::FieldObjectId object = 0;
    bool entered = false, ready = false;
  };
  bool create(uint32_t, const upstream::FieldSpriteDescriptor &, std::string &);
  bool rebuild(uint32_t, const upstream::FieldSpriteAnimation &,
               const std::vector<upstream::FieldSpriteTag> &,
               const std::vector<upstream::FieldSpriteConnection> &,
               const std::vector<std::string> &, std::string &);
  bool travel(uint32_t, const std::string &, std::string &);
  bool blend(uint32_t, upstream::Vec2,
             const std::vector<upstream::FieldSpriteTag> &, std::string &);
  bool scale(uint32_t, float, const std::vector<std::string> &, std::string &);
  Graph *graph(uint32_t);
  const upstream::FieldNpcInstance *npc(uint32_t) const;
  bool actual(const Graph &, std::string &) const;
  const upstream::FieldNodeTreeData *nodes_ = nullptr;
  const upstream::FieldSpriteData *sprites_ = nullptr;
  const upstream::FieldNpcData *npcs_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldSpriteRuntime *sprite_runtime_ = nullptr;
  upstream::FieldNpcRuntime *npc_runtime_ = nullptr;
  std::map<uint32_t, Graph> graphs_;
  std::map<upstream::FieldObjectId, uint32_t> objects_;
  std::map<uint32_t, ClipPlayer> players_;
  std::map<upstream::FieldObjectId, uint32_t> player_objects_;
  bool applied_ = false;
};
} // namespace encore::ctr

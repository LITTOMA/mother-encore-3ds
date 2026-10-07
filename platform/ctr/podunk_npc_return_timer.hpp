#pragma once
#include "encore/field_scene_tree_timer.hpp"
#include "podunk_scene_npc_world.hpp"

namespace encore::ctr {
// The two actual NPC create_timer call sites share the global SceneTree timer
// list and real ObjectDB signal dispatch. No duplicate NPC body or idle clock.
class PodunkNpcReturnTimers {
public:
  bool prepare(const upstream::FieldNativeRootData &,
               const upstream::FieldNpcWorldData &,
               const upstream::FieldNpcData &, upstream::FieldGlobalRegistry &,
               upstream::FieldObjectSignals &, upstream::FieldNodeTreeRuntime &,
               upstream::FieldNpcRuntime &, PodunkSceneNpcWorld &,
               std::string &);
  bool create(upstream::FieldObjectId actual_npc, double seconds,
              uint64_t actual_waiter, std::string &);
  bool idle(uint64_t actual_epoch, float actual_delta, bool paused,
            std::string &);
  bool handles_callback(const upstream::FieldDeferredMessage &) const;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  bool signal_declaration(upstream::FieldObjectId, std::string_view, uint32_t &,
                          std::string &) const;
  bool owns(upstream::FieldObjectId) const;
  bool shutdown(std::string &);
  upstream::FieldSceneTreeTimers &scene_tree_timers() { return timers_; }

private:
  struct Waiting {
    upstream::FieldObjectId target = 0;
    uint32_t source = 0;
    uint64_t waiter = 0;
    std::weak_ptr<upstream::FieldSceneTreeTimer> timer;
    bool returned = false;
  };
  const upstream::FieldNpcWorldData *data_ = nullptr;
  const upstream::FieldNpcData *npcs_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldObjectSignals *signals_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldNpcRuntime *runtime_ = nullptr;
  PodunkSceneNpcWorld *world_ = nullptr;
  upstream::FieldSceneTreeTimers timers_;
  std::map<upstream::FieldObjectId, Waiting> waiting_;
  std::string method_;
};
} // namespace encore::ctr

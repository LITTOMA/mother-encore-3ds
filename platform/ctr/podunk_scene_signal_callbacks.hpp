#pragma once
#include "encore/field_scene_signal_callbacks.hpp"
#include "podunk_scene_consumers.hpp"
namespace encore::ctr {
// Actual ObjectDB methods and borrowed source signal callbacks. It owns no
// scene clock, lifecycle cursor, gameplay data or replacement GDScript VM.
class PodunkSceneSignalCallbacks {
public:
  ~PodunkSceneSignalCallbacks();
  bool prepare(const upstream::FieldSceneSignalCallbacksData &,
               PodunkSceneConsumerInput, std::string &);
  bool apply(PodunkSceneMechanismOwners &, std::string &);
  // Called after actual native publication, before the same node's source
  // constructor. The Tree's final source index does not yet exist then.
  bool observe_allocated(upstream::FieldObjectId,
                         const upstream::FieldNodeDescriptor &,
                         const upstream::FieldIdentity &, std::string &);
  bool declaration(upstream::FieldObjectId, std::string_view, uint32_t &,
                   std::string &) const;
  bool emission(upstream::FieldObjectId, std::string_view, size_t,
                std::string &) const;
  bool dispatch(const upstream::FieldDeferredMessage &, std::string &);
  bool handles(const upstream::FieldDeferredMessage &) const;
  bool emit_flags(std::string &);
  bool disconnect(std::string &);

private:
  using Args = std::vector<upstream::FieldDeferredValue>;
  using Callback = std::function<bool(const Args &, std::string &)>;
  struct Connection {
    upstream::FieldObjectId sender = 0, target = 0;
    std::string signal, method;
  };
  class Wait;
  bool source(uint32_t, upstream::FieldObjectId &, std::string &) const;
  upstream::FieldObjectId area_object() const;
  bool connect(uint32_t, upstream::SceneCallbackRole, upstream::FieldObjectId,
               upstream::SceneSignalSymbol, Callback,
               std::vector<upstream::FieldDeferredValue>, std::string &);
  bool flags(uint32_t, upstream::FieldSceneSignalSlot, std::string &);
  bool area(uint32_t, upstream::FieldSceneAreaSlot, std::string &);
  bool wait(uint32_t, uint64_t, std::function<bool()>, std::string &);
  bool cancel(uint32_t, uint64_t, std::string &);
  const upstream::FieldSceneSignalCallbacksData *data_ = nullptr;
  PodunkSceneConsumerInput input_{};
  std::map<uint32_t, upstream::FieldObjectId> allocated_;
  std::map<std::pair<upstream::FieldObjectId, std::string>, Callback> methods_;
  std::vector<Connection> connections_;
  std::map<std::pair<uint32_t, uint64_t>, std::shared_ptr<Wait>> waits_;
  std::map<upstream::FieldObjectId,
           std::pair<uint32_t, std::function<bool(uint32_t)>>>
      arrow_finished_;
};
} // namespace encore::ctr

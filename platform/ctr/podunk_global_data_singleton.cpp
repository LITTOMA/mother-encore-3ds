#include "podunk_global_data_singleton.hpp"
namespace encore::ctr {
using namespace upstream;
bool PodunkGlobalDataSingleton::fail(std::string &e, const char *s) const {
  e = s; return false;
}
bool PodunkGlobalDataSingleton::poison(std::string &e) {
  failed_ = true;
  if (e.empty()) e = "globalData actual source execution failed";
  return false;
}
bool PodunkGlobalDataSingleton::initialize(
    FieldObjectId id, const FieldGlobalExternalSpec &s,
    PodunkGlobalDataSingletonData d, PodunkGlobalDataSingletonServices h,
    std::string &e) {
  if (initialized_ || failed_ || !id || !d.members || !d.constructor || !d.flags ||
      !d.caches || !d.directories || !d.files || !d.items || !h.registry || !h.root ||
      !h.random || !h.uid_ledger || !h.clock || !h.flags_updated || !h.warning ||
      !h.locale || !h.signal || h.registry->kernel() != h.root->kernel_object() ||
      h.registry->root() != h.root->viewport_object())
    return fail(e, "globalData singleton complete actual owners/resources absent");
  binding_ = {id, s, 0x454e0053, 1};
  data_ = d; services_ = std::move(h);
  if (!host_.construct_members(*d.members, s, id, *services_.registry, e) ||
      !host_.initialize_source_constructor(*d.constructor, e) ||
      !host_.construct_cache_prefix(*d.flags, *d.caches, *d.items,
                                     services_.flags_updated, services_.warning, e) ||
      !host_.begin_cache_directory_prefix(*d.directories, *d.files, e))
    return poison(e);
  while (!host_.cache_prefix_complete())
    if (!host_.step_cache_directory_prefix(e)) return poison(e);
  if (!host_.complete_source_constructor(e)) return poison(e);
  initialized_ = true;
  e.clear(); return true;
}
bool PodunkGlobalDataSingleton::actual(std::string &e) const {
  if (!initialized_ || failed_ || !services_.registry || !services_.root ||
      services_.registry->kernel() != services_.root->kernel_object() ||
      services_.registry->root() != services_.root->viewport_object() ||
      !host_.runtime().constructor_complete())
    return fail(e, "globalData actual singleton constructor unavailable");
  e.clear(); return true;
}
bool PodunkGlobalDataSingleton::state(FieldGlobalExternalState &s, std::string &e) const {
  return actual(e) && host_.source_state(s, e);
}
bool PodunkGlobalDataSingleton::menu_flavor(std::string &s, std::string &e) const {
  return actual(e) && host_.menu_flavor(s, e);
}
bool PodunkGlobalDataSingleton::stage_parent(FieldObjectId p, std::string &e) {
  if (!actual(e) || !host_.source_stage_parent(binding_, p, e)) return false;
  if (p) { parented_ = false; unparented_ = false; }
  else if (!unparented_) return poison(e);
  return true;
}
bool PodunkGlobalDataSingleton::native_notification(FieldTreePhase phase, std::string &e) {
  FieldGlobalExternalState s;
  if (!state(s, e)) return false;
  if (phase == FieldTreePhase::Parented) {
    if (!s.parent || parented_ || s.parent != services_.registry->root())
      return fail(e, "globalData native Parented source boundary rejected");
    parented_ = true;
  } else if (phase == FieldTreePhase::Unparented) {
    if (!s.parent || s.inside || !parented_ || unparented_)
      return fail(e, "globalData native Unparented source boundary rejected");
    unparented_ = true;
  } else if (phase == FieldTreePhase::PostEnterNative) {
    if (!s.inside || !s.parent || !parented_ || unparented_ ||
        services_.root->external_parent(binding_.object) != s.parent)
      return fail(e, "globalData native PostEnter source boundary rejected");
  } else if (phase == FieldTreePhase::ChildMoved) {
    if (!s.parent || !parented_ || unparented_ ||
        services_.root->external_parent(binding_.object) != s.parent)
      return fail(e, "globalData native ChildMoved source boundary rejected");
  } else return fail(e, "globalData native notification not implemented");
  e.clear(); return true;
}
bool PodunkGlobalDataSingleton::enter(FieldObjectId p, std::string &e) {
  if (!actual(e) || !parented_ || unparented_ ||
      !host_.source_enter(binding_, p, e)) return false;
  if (!services_.signal(binding_.object, "tree_entered", e) ||
      !services_.root->node_notification(binding_.object, FieldTreePhase::NodeAdded, e) ||
      !services_.root->node_notification(binding_.object, FieldTreePhase::ChildEntered, e))
    return poison(e);
  return true;
}
bool PodunkGlobalDataSingleton::ready(std::string &e) {
  FieldGlobalExternalState s;
  if (!state(s, e) || !s.inside || s.ready || !parented_ || unparented_ ||
      !native_notification(FieldTreePhase::PostEnterNative, e)) return false;
  const bool first = !host_.runtime().god_storage_complete();
  if (first) {
    if (!services_.root->node_notification(binding_.object, FieldTreePhase::ReadyNative, e))
      return poison(e);
    std::string locale;
    if (!services_.root->node_notification(binding_.object, FieldTreePhase::ReadyScript, e) ||
        !host_.source_begin_ready(binding_, s.parent, e) || !services_.locale(locale, e) ||
        !host_.construct_god_storage(locale, *services_.random, *services_.uid_ledger,
                                     services_.clock, e)) return poison(e);
  }
  if (!host_.source_finish_ready(binding_, s.parent, e) ||
      (first && !services_.signal(binding_.object, "ready", e))) return poison(e);
  return true;
}
bool PodunkGlobalDataSingleton::exit(std::string &e) {
  FieldGlobalExternalState s;
  if (!state(s, e) || !s.inside || !parented_ || unparented_) return false;
  if (!services_.signal(binding_.object, "tree_exiting", e) ||
      !host_.source_exit(binding_, s.parent, e) ||
      !services_.root->node_notification(binding_.object, FieldTreePhase::NodeRemoved, e) ||
      !services_.root->node_notification(binding_.object, FieldTreePhase::ChildExiting, e) ||
      !services_.signal(binding_.object, "tree_exited", e)) return poison(e);
  return true;
}
bool PodunkGlobalDataSingleton::deferred(const FieldDeferredMessage &, std::string &e) {
  return fail(e, "globalData deferred method/property/opcode not implemented");
}
bool PodunkGlobalDataSingleton::persist_append(FieldObjectId, std::string &e) {
  return fail(e, "globalData does not own global persistent Nodes");
}
bool PodunkGlobalDataSingleton::assign_stable_canvas(FieldObjectId, std::string &e) {
  return fail(e, "globalData does not own UI stable CanvasLayer");
}
bool PodunkGlobalDataFactory::initialize(const FieldGlobalRegistryData &r,
    PodunkGlobalDataSingletonData d, PodunkGlobalDataSingletonServices s,
    std::string &e) {
  if (registry_data_ || !r.valid() || !d.members || !d.constructor ||
      !s.registry || !s.root || !d.members->valid() || !d.constructor->valid()) {
    e = "globalData factory checked actual source resources absent"; return false;
  }
  registry_data_ = &r; data_ = d; services_ = std::move(s);
  e.clear(); return true;
}
PodunkGlobalDataSingleton *PodunkGlobalDataFactory::globaldata() const {
  return object_ && services_.registry && services_.registry->object_exists(id_)
       ? object_ : nullptr;
}
bool PodunkGlobalDataFactory::construct(FieldObjectId id,
    const FieldGlobalExternalSpec &s, std::unique_ptr<FieldGlobalExternalObject> &out,
    std::string &e) {
  if (!registry_data_ || !id || out || object_ || s.role != 3) {
    e = "globalData actual source factory slot/opcode rejected"; return false;
  }
  const auto &expected = registry_data_->identity();
  const FieldGlobalAutoload *source = nullptr;
  for (const auto &a : registry_data_->autoloads())
    if (a.script == data_.members->owner_source()) source = &a;
  if (!source || source->id != s.stable_id || source->name != s.name ||
      source->path != s.source || source->script != s.script ||
      source->native_class != s.native_class || source->source_sha != s.source_sha ||
      source->script_sha != s.script_sha || expected.scene_id != s.identity.scene_id ||
      expected.upstream_commit != s.identity.upstream_commit ||
      expected.source_sha256 != s.identity.source_sha256) {
    e = "globalData factory exact namespace autoload identity differs"; return false;
  }
  auto value = std::make_unique<PodunkGlobalDataSingleton>();
  if (!value->initialize(id, s, data_, services_, e)) return false;
  id_ = id; object_ = value.get(); out = std::move(value);
  e.clear(); return true;
}
bool PodunkGlobalDataFactory::register_native_root(std::string &e) {
  auto *object = globaldata();
  if (!object) { e = "globalData singleton not published by actual Registry"; return false; }
  return services_.root->register_external_child(*object, *object, e);
}
} // namespace encore::ctr

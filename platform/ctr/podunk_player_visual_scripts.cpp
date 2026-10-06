#include "podunk_player_visual_scripts.hpp"
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
bool PodunkPlayerVisualScripts::initialize(
    const PlayerVisualScriptsData &d, const PlayerInitializationData &p,
    FieldNodeTreeRuntime &t, FieldGlobalRegistry &r,
    FieldGlobalConstructorRuntime &global, PlayerInitializationBody &body,
    PlayerVisualNativeOwner &shadow, PlayerVisualNativeOwner &bat,
    PlayerVisualFetcherOwner &fetcher, std::string &e) {
  if (data_ ||
      !core_.initialize(d, p, t, r, global, body, shadow, bat, fetcher, e))
    return false;
  data_ = &d;
  tree_ = &t;
  return true;
}
bool PodunkPlayerVisualScripts::construct(FieldObjectId id,
                                          const FieldNodeDescriptor &d,
                                          std::string &e) {
  return core_.construct(id, d, e);
}
bool PodunkPlayerVisualScripts::source_shadow_property(FieldObjectId id,
                                                       std::string_view member,
                                                       std::string &e) {
  return core_.apply_shadow_export(id, member, e);
}
bool PodunkPlayerVisualScripts::script_phase(FieldObjectId id,
                                             FieldTreePhase phase,
                                             std::string &e) {
  if (!data_ || !tree_)
    return fail(e, "Player visual script host not initialized");
  const auto *d = tree_->descriptor(id);
  if (!d || (d->id != data_->shadow().id && d->id != data_->bat().id))
    return fail(e, "Player visual source phase object unknown");
  if (phase == FieldTreePhase::ReadyScript)
    return core_.ready(id, e);
  if (phase == FieldTreePhase::Idle && d->id == data_->bat().id)
    return core_.process(id, e);
  return fail(e, "Player visual source script phase not present/unsupported");
}
} // namespace encore::ctr

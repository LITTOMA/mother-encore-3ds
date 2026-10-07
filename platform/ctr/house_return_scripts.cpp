#include "house_return_scripts.hpp"
#include "encore/global_data_constructor.hpp"
#include <algorithm>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) { e = s; return false; }
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
}
bool HouseReturnScripts::prepare(Input i, std::string &e) {
  if (input_.reentry || !i.reentry || !i.nodes || !i.doors || !i.tree ||
      !i.registry || !i.signals || !i.global || !i.globaldata || !i.flags ||
      !i.callbacks || !i.reentry->valid() || !i.nodes->valid() ||
      !i.callbacks->valid() ||
      i.callbacks->identity().upstream_commit != i.reentry->identity().upstream_commit ||
      !i.registry->data() ||
      i.signals->registry() != i.registry ||
      i.globaldata->registry() != i.registry || !i.global->data() ||
      !i.globaldata->constructor_data() ||
      !i.reentry->matches(*i.doors, i.room, i.house, e))
    return fail(e, "House source adapter checked owners/resources missing");
  auto identity = i.nodes->identity();
  std::array<uint8_t,32> sha{}, other{};
  if (i.nodes->source_scene() != i.reentry->target_scene() ||
      identity.upstream_commit != i.reentry->identity().upstream_commit ||
      !i.reentry->source_hash(i.nodes->source_scene(), sha) ||
      sha != identity.source_sha256 || i.nodes->records().empty())
    return fail(e, "House full native source tree identity differs");
  const auto &root = i.nodes->records().front();
  const auto &ready = i.reentry->ready();
  if (root.id != identity.scene_id || root.parent || root.path != "." ||
      root.name != i.reentry->target_root_name() || root.script.empty() ||
      root.script_methods != 1 ||
      !i.reentry->source_hash(root.script, sha) || sha != root.script_sha ||
      !i.nodes->source_hash(ready.script, sha) ||
      !i.reentry->source_hash(ready.script, other) || sha != other ||
      !i.global->data()->source_hash(i.global->data()->owner_source(), sha) ||
      !i.reentry->source_hash(i.global->data()->owner_source(), other) ||
      sha != other ||
      !i.globaldata->constructor_source_hash(
          i.globaldata->constructor_data()->owner_source(), sha) ||
      !i.reentry->source_hash(
          i.globaldata->constructor_data()->owner_source(), other) || sha != other)
    return fail(e, "House inherited AreaRoom/source singleton closure differs");
  const auto *flags = i.callbacks->symbol(SceneSignalSymbol::Flags);
  const auto *switches = i.callbacks->symbol(SceneSignalSymbol::Switches);
  const auto *left = i.callbacks->symbol(SceneSignalSymbol::AreaLeft);
  if (!flags || flags->declaration_arguments || flags->emission_arguments ||
      !switches || switches->name != ready.switch_signal ||
      switches->declaration_arguments != 3 || switches->emission_arguments != 3 ||
      !left || left->name != i.reentry->area_left_signal() ||
      left->emission_arguments != i.reentry->area_left_arguments())
    return fail(e, "House source signal symbol/signature differs");
  bool switch_proof = false;
  for (const auto &c : i.callbacks->callbacks()) {
    if (c.role != SceneCallbackRole::Switches || c.method_source != ready.script ||
        c.method != ready.switch_method || c.arguments != 3) continue;
    if (!i.nodes->source_hash(c.method_source,sha) || sha != c.method_sha)
      return fail(e, "House inherited switches source method proof differs");
    switch_proof = true;
  }
  if (!switch_proof) return fail(e, "House inherited switches source method missing");
  const auto *party = i.global->data()->member(FieldGlobalMemberRole::PartyNpcs);
  if (!party || party->name != ready.party_npcs_member ||
      !i.registry->object_exists(i.global->owner()) ||
      !i.registry->object_exists(i.globaldata->globaldata_object()))
    return fail(e, "House source uses a foreign/missing singleton Array");
  std::map<uint32_t,HouseReentryLandmark> landmarks;
  std::string method;
  for (const auto &landmark : i.reentry->landmarks()) {
    const auto found = std::find_if(i.nodes->records().begin(),
        i.nodes->records().end(), [&](const auto &n) { return n.path == landmark.node; });
    if (found == i.nodes->records().end() || found->script.empty() ||
        found->script_methods != 1 || landmarks.count(found->id))
      return fail(e, "House FlagLandmark actual source instance missing/duplicate");
    bool proof = false;
    for (const auto &c : i.callbacks->callbacks()) {
      if (c.role != SceneCallbackRole::CheckFlags || c.script != found->script ||
          c.method_source != found->script || c.arguments ||
          c.leaf_sha != found->script_sha || c.method_sha != found->script_sha)
        continue;
      if (!i.reentry->source_hash(c.method_source, sha) || sha != c.method_sha ||
          !i.nodes->source_hash(c.method_source, other) || other != sha ||
          (!method.empty() && method != c.method))
        return fail(e, "House FlagLandmark source callback proof differs");
      method = c.method;
      proof = true;
    }
    if (!proof) return fail(e, "House FlagLandmark checked source method missing");
    landmarks.emplace(found->id, landmark);
  }
  if (landmarks.empty() || method.empty())
    return fail(e, "House FlagLandmark source closure empty");
  // Every script is retained in the routing roster. Matching source symbols
  // never approves a different node's source constructor/Ready.
  std::vector<RosterEntry> roster;
  for (const auto &n : i.nodes->records()) {
    if (n.script.empty()) continue;
    Role role = n.id == root.id ? Role::AreaRoom :
                landmarks.count(n.id) ? Role::FlagLandmark : Role::Unmapped;
    for (const auto &l : landmarks)
      if (n.script == i.nodes->record(l.first)->script && role != Role::FlagLandmark)
        return fail(e, "House FlagLandmark omitted from checked continuation data");
    roster.push_back({n.id,n.path,n.script,role});
  }
  // The same singleton runtime must own the actual flag dictionary. A passed
  // unrelated flag store is rejected before any source mutation/connection.
  bool owns_flags = false;
  for (const auto &d : i.globaldata->constructor_data()->declarations()) {
    if (d.adapter != 5) continue;
    FieldGlobalDataMemberState member;
    if (!i.globaldata->read_global_member(d.name, member, e)) return false;
    owns_flags |= member.flags == i.flags;
  }
  if (!owns_flags) return fail(e, "House actual globaldata flag owner differs");
  input_ = i;
  landmarks_ = std::move(landmarks);
  roster_ = std::move(roster);
  reentry_ir_ = i.reentry->ir_sha256();
  node_identity_ = identity;
  root_source_ = root.id;
  flags_signal_ = flags->name;
  check_method_ = method;
  e.clear();
  return true;
}
HouseReturnScripts::Role HouseReturnScripts::mapped_source(uint32_t source) const {
  for (const auto &n : roster_) if (n.source == source) return n.role;
  return Role::Unmapped;
}
bool HouseReturnScripts::available(std::string &e) const {
  if (!input_.reentry || poisoned_ || !input_.reentry->valid() ||
      input_.reentry->ir_sha256() != reentry_ir_ || !input_.nodes->valid() ||
      !same(input_.nodes->identity(), node_identity_) || input_.registry->poisoned() ||
      input_.signals->registry() != input_.registry ||
      input_.globaldata->registry() != input_.registry ||
      !input_.globaldata->constructor_data() || !input_.global->data() ||
      input_.tree->object_domain() != input_.registry->kernel())
    return fail(e, "House source adapter unavailable/reloaded/foreign SceneTree");
  return true;
}
bool HouseReturnScripts::actual(FieldObjectId id, std::string &e) const {
  if (!available(e)) return false;
  const auto *n = input_.tree->state(id);
  const auto *d = input_.tree->descriptor(id);
  const auto *expected = d ? input_.nodes->record(d->id) : nullptr;
  FieldIdentity identity;
  std::array<uint8_t,32> script_sha{};
  if (!n || !n->alive || !d || !expected || n->source != d->id ||
      !input_.reentry->source_hash(d->script,script_sha) || script_sha != d->script_sha ||
      !input_.registry->object_exists(id) ||
      input_.registry->tree_owner(id).get() != input_.tree ||
      !input_.tree->object_identity(id, identity) || !same(identity,node_identity_) ||
      d->script != expected->script || d->script_sha != expected->script_sha ||
      d->path != expected->path || d->native_class != expected->native_class ||
      d->class_index != expected->class_index ||
      d->script_methods != expected->script_methods ||
      mapped_source(d->id) == Role::Unmapped)
    return fail(e, "House actual mapped source ObjectDB owner differs");
  return true;
}
bool HouseReturnScripts::construct(FieldObjectId id, std::string &e) {
  if (!actual(id,e)) return false;
  const auto *n = input_.tree->state(id);
  const auto *d = input_.tree->descriptor(id);
  if (bodies_.count(id) || n->inside || n->ready_notified || n->parent || n->bound)
    return fail(e, "House source constructor cursor duplicate/late");
  Body body;
  body.source = d->id;
  body.role = mapped_source(d->id);
  if (body.role == Role::AreaRoom) {
    if (root_) return fail(e, "House actual AreaRoom constructed twice");
    // Source _init connects synchronously to this same object. The native
    // signal declaration/dispatch must already be published by its owner.
    if (!input_.signals->connect(id, input_.reentry->ready().switch_signal, id,
          input_.reentry->ready().switch_method, 0, {}, e)) {
      poisoned_ = true;
      return false;
    }
    root_ = id;
  } else {
    body.landmark = landmarks_.at(d->id);
  }
  bodies_.emplace(id,std::move(body));
  e.clear();
  return true;
}
bool HouseReturnScripts::read_flag(std::string_view key, bool &value,
                                   std::string &e) const {
  bool present = false;
  if (!input_.flags->read(false,key,present,value,e)) return false;
  if (!present) value = false; // audited check_flags Dictionary.get default
  return true;
}
bool HouseReturnScripts::shared_flags(std::string &e) const {
  for (const auto &d : input_.globaldata->constructor_data()->declarations()) {
    if (d.adapter != 5) continue;
    FieldGlobalDataMemberState m;
    if (!input_.globaldata->read_global_member(d.name,m,e)) return false;
    if (m.flags == input_.flags) return true;
  }
  return fail(e, "House actual source flags dictionary detached");
}
bool HouseReturnScripts::area_ready(std::string &e) {
  if (!shared_flags(e)) return false;
  const auto &ready = input_.reentry->ready();
  if (!ready.visit_flag.empty() &&
      !input_.flags->set_normal(ready.visit_flag,true,true,e)) return false;
  // Preserve source short-circuit order: outside Magicant the indexed flag
  // read is not evaluated, even if that key would be missing.
  const bool magicant = input_.reentry->target_region() == ready.magicant_region;
  bool flying = false;
  if (magicant) {
    bool present = false;
    if (!input_.flags->read(false,ready.flying_flag,present,flying,e)) return false;
    if (!present) return fail(e, "House indexed flyingman flag missing");
  }
  if (magicant && !flying) return true;
  FieldObjectId character = 0;
  for (const auto &d : input_.globaldata->constructor_data()->declarations()) {
    if (d.adapter != 1) continue;
    FieldGlobalDataMemberState m;
    if (!input_.globaldata->read_global_member(d.name,m,e)) return false;
    for (const auto &r : m.references) if (r.first == ready.flying_character) {
      if (character) return fail(e, "House source flyingman member ambiguous");
      character = r.second;
    }
  }
  FieldGlobalDataObject actual_character;
  if (!character || !input_.registry->object_exists(character) ||
      !input_.globaldata->read_constructed_object(character,actual_character,e) ||
      actual_character.object != character || actual_character.kind != 1)
    return fail(e, "House real globaldata flyingman Character absent");
  std::shared_ptr<const GlobalLoadObjectArray> party;
  if (!input_.global->array(FieldGlobalMemberRole::PartyNpcs,party,e) || !party)
    return false;
  const bool present = std::find(party->values.begin(),party->values.end(),character)
                       != party->values.end();
  if (magicant == present) return true;
  return magicant ? input_.global->append_array(FieldGlobalMemberRole::PartyNpcs,character,e)
                  : input_.global->erase_array_first(FieldGlobalMemberRole::PartyNpcs,character,e);
}
bool HouseReturnScripts::check_landmark(FieldObjectId id, Body &b, std::string &e) {
  if (!shared_flags(e)) return false;
  bool shown = true, value = false;
  if (!b.landmark.appear.empty()) {
    if (!read_flag(b.landmark.appear,value,e)) return false;
    shown = value;
  }
  if (shown && !b.landmark.disappear.empty()) {
    if (!read_flag(b.landmark.disappear,value,e)) return false;
    shown = !value;
  }
  // queue_free leaves visibility and native colliders alive until the actual
  // SceneTree deletion boundary; it must never become a hide/body-disable.
  return b.landmark.delete_if_hidden && !shown ? input_.tree->queue_free(id,e)
                                             : input_.tree->set_visible(id,shown,e);
}
bool HouseReturnScripts::script_phase(FieldObjectId id, FieldTreePhase phase,
                                      std::string &e) {
  if (!actual(id,e)) return false;
  auto found = bodies_.find(id);
  if (found == bodies_.end()) return fail(e, "House source body not constructed");
  auto &b = found->second;
  const auto *n = input_.tree->state(id);
  if (phase == FieldTreePhase::EnterScript) {
    if (!n->inside || b.entered) return fail(e, "House source Enter cursor differs");
    b.entered = true;
  } else if (phase == FieldTreePhase::ReadyScript) {
    if (!n->inside || !n->ready_notified || !b.entered || b.ready)
      return fail(e, "House source Ready cursor absent/replayed");
    bool ok = b.role == Role::AreaRoom ? area_ready(e) : check_landmark(id,b,e);
    if (ok && b.role == Role::FlagLandmark)
      ok = input_.signals->connect(input_.global->owner(),flags_signal_,id,
                                   check_method_,0,{},e);
    if (!ok) { poisoned_ = true; return false; }
    b.ready = true;
  } else if (phase == FieldTreePhase::ExitScript) {
    if (!n->inside || !b.entered) return fail(e, "House source Exit cursor differs");
    b.entered = false;
    // Neither reviewed source defines _exit_tree. Connections persist until
    // real ObjectDB deletion, including a detached still-live landmark.
  } else return fail(e, "House source adapter cannot dispatch native/unknown phase");
  e.clear();
  return true;
}
bool HouseReturnScripts::dispatch(const FieldDeferredMessage &m, std::string &e) {
  if (!actual(m.object,e)) return false;
  auto found = bodies_.find(m.object);
  if (found == bodies_.end() || m.kind != FieldDeferredKind::Call)
    return fail(e, "House source method cursor/body unsupported");
  auto &b = found->second;
  if (b.role == Role::FlagLandmark && b.ready && m.member == check_method_ &&
      m.args.empty() && input_.signals->emitting_to(input_.global->owner(),
                                 flags_signal_,m.object,check_method_)) {
    if (!check_landmark(m.object,b,e)) { poisoned_ = true; return false; }
    return true;
  }
  const auto &ready = input_.reentry->ready();
  if (b.role == Role::AreaRoom && m.member == ready.switch_method &&
      m.args.size() == 3 && std::holds_alternative<FieldObjectRef>(m.args[0]) &&
      std::holds_alternative<bool>(m.args[1]) && std::holds_alternative<bool>(m.args[2]) &&
      input_.signals->emitting_to(m.object,ready.switch_signal,m.object,m.member)) {
    const auto emitter = std::get<FieldObjectRef>(m.args[0]).id;
    if (emitter) {
      const auto *type = input_.callbacks->symbol(SceneSignalSymbol::SwitchSource);
      const auto owner = input_.registry->tree_owner(emitter);
      const auto *d = owner ? owner->descriptor(emitter) : nullptr;
      const auto proof = type ? input_.callbacks->sources().find(type->name)
                              : input_.callbacks->sources().end();
      if (!d || !owner->state(emitter) || !input_.registry->object_exists(emitter) ||
          !type || proof == input_.callbacks->sources().end() ||
          d->script != type->name || d->script_sha != proof->second)
        return fail(e, "House switches actual TwoStatesSwitch emitter differs");
    }
    b.switches = std::get<bool>(m.args[1]);
    e.clear();
    return true;
  }
  return fail(e, "House unknown/unmapped source method or foreign signal callback");
}
bool HouseReturnScripts::leave_for(FieldObjectId destination,
                                   const FieldSceneData &data, std::string &e) {
  if (!actual(root_,e) || !data.valid() ||
      data.identity().upstream_commit != node_identity_.upstream_commit)
    return fail(e, "House leave_for source/destination unadmitted");
  const auto body = bodies_.find(root_);
  const auto owner = input_.registry->tree_owner(destination);
  const auto *n = owner ? owner->state(destination) : nullptr;
  const auto *d = owner ? owner->descriptor(destination) : nullptr;
  FieldIdentity identity;
  if (body == bodies_.end() || !body->second.ready || !n || !n->alive || !d ||
      d->id != data.area().id || owner->root() != destination ||
      !owner->object_identity(destination,identity) || !same(identity,data.identity()))
    return fail(e, "House leave_for actual destination AreaRoom differs");
  const bool changed = data.string(data.area().region) != input_.reentry->target_region();
  if (!input_.signals->emit(root_,input_.reentry->area_left_signal(),{changed},e)) {
    poisoned_ = true;
    return false;
  }
  return true;
}
bool HouseReturnScripts::switches_state(FieldObjectId id, bool &out,
                                       std::string &e) const {
  if (!actual(id,e)) return false;
  const auto b = bodies_.find(id);
  if (b == bodies_.end() || b->second.role != Role::AreaRoom)
    return fail(e, "House switches actual source body absent");
  out = b->second.switches;
  e.clear();
  return true;
}
bool HouseReturnScripts::release_deleted(FieldObjectId id, std::string &e) {
  if (!input_.reentry || input_.registry->object_exists(id) ||
      input_.tree->state(id) || !bodies_.count(id))
    return fail(e, "House source release requires actual native/ObjectDB deletion");
  if (!input_.signals->release(id,e)) return false;
  bodies_.erase(id);
  if (id == root_) root_ = 0;
  e.clear();
  return true;
}
} // namespace encore::ctr

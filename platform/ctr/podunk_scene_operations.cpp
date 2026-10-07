#include "podunk_scene_operations.hpp"
#include "encore/global_yaml_caches.hpp"
#include "encore/global_data_constructor.hpp"
#include <algorithm>
#include <cmath>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) { e = s; return false; }
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
}
bool PodunkSceneOperations::prepare(PodunkSceneOperationInput in, std::string &e) {
  if (prepared_ || !in.sources || !in.sources->valid() || !in.continuation ||
      !in.continuation->initialized() || !in.tree || !in.player || !in.motion ||
      !in.motion->valid() || !in.lifecycle || !in.sources->signals().valid())
    return fail(e, "Scene operations require actual checked continuation owners");
  auto *registry = in.continuation->registry();
  auto *global = in.continuation->global();
  auto *characters = in.continuation->characters();
  auto *signals = in.continuation->signals();
  if (!registry || registry->poisoned() || !global || !global->core().data() ||
      !characters || characters->runtime().registry() != registry ||
      !signals || signals->registry() != registry || !characters->runtime().data() ||
      in.sources->tree().identity().upstream_commit !=
          characters->runtime().data()->identity().upstream_commit)
    return fail(e, "Scene operations mixed ObjectDB/source owners rejected");
  const auto source_member=in.sources->signals().party_member_declaration();
  const auto *constructor=characters->runtime().constructor_data();
  if (!constructor || !constructor->valid() ||
      constructor->ir_sha256()!=in.sources->signals().constructor_sha() ||
      !source_member || (in.bindings.flyingman_declaration &&
                         in.bindings.flyingman_declaration != source_member))
    return fail(e,"Scene party member differs from checked Area source binding");
  in.bindings.flyingman_declaration=source_member;
  input_ = std::move(in);
  prepared_ = true;
  FieldObjectId member = 0;
  if (!flyingman(member, e)) { failed_ = true; return false; }
  e.clear(); return true;
}
bool PodunkSceneOperations::live(std::string &e) const {
  if (!prepared_ || failed_ || !input_.continuation->initialized() ||
      input_.continuation->registry()->poisoned())
    return fail(e, "Scene operations owner unavailable/poisoned");
  return true;
}
bool PodunkSceneOperations::observe_allocated(FieldObjectId id,
  const FieldNodeDescriptor &d, const FieldIdentity &identity, std::string &e) {
  if (!live(e) || !same(identity,input_.sources->tree().identity()))
    return fail(e,"Scene allocation source identity differs");
  const auto *expected=input_.sources->tree().record(d.id);
  const auto *actual=input_.tree->descriptor(id);
  const auto *node=input_.tree->state(id);
  if (!expected || !actual || !node || !node->alive ||
      input_.continuation->registry()->tree_owner(id).get()!=input_.tree ||
      actual->id!=d.id || actual->script!=expected->script ||
      actual->script_sha!=expected->script_sha ||
      d.class_index>=input_.sources->tree().classes().size() ||
      d.native_class!=input_.sources->tree().classes()[d.class_index] ||
      actual->native_class!=d.native_class || expected->class_index!=d.class_index ||
      allocated_.count(d.id))
    return fail(e,"Scene actual native allocation/source duplicate rejected");
  allocated_.emplace(d.id,id); e.clear(); return true;
}
bool PodunkSceneOperations::source(uint32_t stable, FieldObjectId &out,
                                    std::string &e) const {
  if (!live(e)) return false;
  auto id = input_.tree->source_object(stable);
  auto allocated = allocated_.find(stable);
  if (!id && allocated != allocated_.end()) id = allocated->second;
  if (allocated != allocated_.end() && allocated->second != id)
    return fail(e, "Scene constructor allocation differs from source index");
  auto n = input_.tree->state(id);
  auto d = input_.tree->descriptor(id);
  FieldIdentity identity;
  if (!n || !n->alive || !d || d->id != stable ||
      input_.continuation->registry()->tree_owner(id).get() != input_.tree ||
      !input_.tree->object_identity(id, identity) ||
      !same(identity, input_.sources->tree().identity()))
    return fail(e, "Scene operation target is not actual source object");
  out = id; e.clear(); return true;
}
bool PodunkSceneOperations::area(FieldObjectId &out, std::string &e) const {
  if (!source(input_.sources->lifecycle().area().id, out, e)) return false;
  auto d = input_.tree->descriptor(out);
  for (uint32_t i = 0; i < input_.sources->lifecycle().ready_count(); ++i) {
    auto r = input_.sources->lifecycle().ready(i);
    if (r.id == d->id && r.role == FieldSceneRole::AreaRoom &&
        d->script == input_.sources->lifecycle().string(r.script) &&
        d->script_sha == r.sha) return true;
  }
  return fail(e, "AreaRoom actual script/source binding rejected");
}
bool PodunkSceneOperations::flyingman(FieldObjectId &out, std::string &e) const {
  if (!live(e)) return false;
  auto &owner = input_.continuation->characters()->runtime();
  const auto *data = owner.data();
  const FieldGlobalDataDeclaration *declaration = nullptr;
  for (const auto &d : data->declarations())
    if (d.id == input_.bindings.flyingman_declaration) declaration = &d;
  if (!declaration || declaration->kind != 1 || declaration->role != 1 ||
      declaration->name != input_.sources->signals().party_member_name())
    return fail(e, "AreaRoom party NPC source declaration absent");
  for (const auto &actual : owner.objects()) {
    if (actual.declaration != declaration->id) continue;
    FieldGlobalDataObject body;
    if (!owner.read_constructed_object(actual.object, body, e) ||
        body.declaration != declaration->id ||
        !owner.constructed_body_alive(body.object)) return false;
    out = body.object; e.clear(); return true;
  }
  return fail(e, "AreaRoom actual party NPC object not constructed");
}
bool PodunkSceneOperations::flyingman_present(bool &out, std::string &e) const {
  FieldObjectId member = 0;
  std::shared_ptr<const GlobalLoadObjectArray> party;
  if (!flyingman(member, e) || !input_.continuation->global()->core().array(
        FieldGlobalMemberRole::PartyNpcs, party, e) || !party) return false;
  out = std::find(party->values.begin(), party->values.end(), member) != party->values.end();
  e.clear(); return true;
}
bool PodunkSceneOperations::set_flyingman(bool present, std::string &e) {
  FieldObjectId member = 0;
  bool before = false;
  if (!flyingman(member, e) || !flyingman_present(before, e)) return false;
  if (before == present) { e.clear(); return true; }
  auto &global = input_.continuation->global()->core();
  if (present) return global.append_array(FieldGlobalMemberRole::PartyNpcs, member, e);
  // No signal/callout/ObjectDB allocation occurs inside these native Array
  // operations. Preserve the actual shared root and source first-match order.
  std::shared_ptr<const GlobalLoadObjectArray> borrowed;
  if (!global.array(FieldGlobalMemberRole::PartyNpcs, borrowed, e) || !borrowed)
    return false;
  auto values = borrowed->values;
  auto first = std::find(values.begin(), values.end(), member);
  if (first == values.end()) return fail(e, "Party NPC erase source state changed");
  values.erase(first);
  const auto &owner = input_.continuation->characters()->runtime();
  for (auto id : values) {
    FieldGlobalDataObject actual;
    if (!owner.read_constructed_object(id, actual, e) || actual.kind != 1 ||
        actual.role != 1 || !owner.constructed_body_alive(id))
      return fail(e, "Party NPC erase contains an unowned source Object");
  }
  if (!global.clear_array(FieldGlobalMemberRole::PartyNpcs, e)) return false;
  for (auto id : values)
    if (!global.append_array(FieldGlobalMemberRole::PartyNpcs, id, e)) {
      failed_ = true; return false;
    }
  std::shared_ptr<const GlobalLoadObjectArray> after;
  if (!global.array(FieldGlobalMemberRole::PartyNpcs, after, e) ||
      after.get() != borrowed.get() || after->values != values) {
    failed_ = true;
    return fail(e, "Party NPC erase did not preserve the actual Array root");
  }
  e.clear(); return true;
}
bool PodunkSceneOperations::debug_context(bool &debug, bool &area_current,
                                           bool &paused, std::string &e) const {
  if (!live(e)) return false;
  FieldObjectId area_id = 0;
  if (!area(area_id, e)) return false;
  debug = input_.actual_debug_build;
  area_current = input_.continuation->registry()->tree_current_scene() == area_id;
  if (!debug || !area_current) { paused = false; e.clear(); return true; }
  PlayerInitializationMember value;
  if (!input_.player->body().constructed() ||
      input_.player->registry() != input_.continuation->registry() ||
      !input_.player->body().member(input_.motion->field(PlayerMotionField::Paused), value, e) ||
      !value.value || value.value->kind != 1)
    return fail(e, "DebugStart actual Player pause state unavailable");
  paused = value.value->boolean; e.clear(); return true;
}
bool PodunkSceneOperations::teleport(Vec2 p, std::string &e) {
  if (!live(e) || !std::isfinite(p.x) || !std::isfinite(p.y)) return false;
  auto id = input_.player->body().object();
  auto n = input_.tree->state(id);
  if (!input_.player->body().constructed() || !n || !n->alive ||
      input_.player->tree() != input_.tree ||
      input_.continuation->registry()->tree_owner(id).get() != input_.tree)
    return fail(e, "DebugStart teleport does not own actual Player");
  auto local = n->local; local[2] = p;
  return input_.tree->set_local(id, local, e);
}
FieldSceneHostOps PodunkSceneOperations::ops() {
  FieldSceneHostOps o;
  o.read_flag = [this](bool obj, auto key, bool &p, bool &v, auto &e) {
    return live(e) && input_.continuation->characters()->flags().read(obj,key,p,v,e);
  };
  o.write_flag = [this](bool obj, auto key, bool v, auto &e) {
    return live(e) && input_.continuation->characters()->flags().write(obj,key,v,e);
  };
  o.emit_flags = [this](auto &e) {
    return live(e) && input_.continuation->characters()->flags().emit(e);
  };
  o.visibility = [this](uint32_t id, bool v, auto &e) {
    FieldObjectId actual=0;
    return source(id,actual,e) && input_.tree->set_visible(actual,v,e);
  };
  o.queue_free = [this](uint32_t id, auto &e) {
    FieldObjectId actual=0;
    return source(id,actual,e) && input_.tree->queue_free(actual,e);
  };
  o.map_possessed = [this](auto name, bool &v, auto &e) {
    if (!live(e)) return false;
    if (!input_.map_possessed) return fail(e,"Inventory source map ownership query not assembled");
    return input_.map_possessed(name,v,e);
  };
  o.flyingman_present = [this](bool &v, auto &e) { return flyingman_present(v,e); };
  o.set_flyingman_present = [this](bool v, auto &e) { return set_flyingman(v,e); };
  o.debug_context = [this](bool &d,bool &a,bool &p,auto &e) { return debug_context(d,a,p,e); };
  o.teleport_player = [this](Vec2 p,auto &e) { return teleport(p,e); };
  o.current_scene = [this](const FieldSceneData *&out, auto &e) {
    FieldObjectId id=0,current=0;
    if (!area(id,e) || !input_.continuation->global()->core().object(
          FieldGlobalMemberRole::CurrentScene,current,e)) return false;
    if (current!=id || input_.continuation->registry()->current_scene()!=id)
      return fail(e,"Scene operations global currentScene does not own destination");
    out=&input_.sources->lifecycle(); e.clear(); return true;
  };
  return o;
}
bool PodunkSceneOperations::publish_current_scene_before_enter(std::string &e) {
  if (published_ || !live(e)) return fail(e,"Destination source publication repeated/unavailable");
  FieldObjectId id=0;
  if (!area(id,e)) return false;
  auto n=input_.tree->state(id);
  if (!n || n->inside || n->parent || n->queued)
    return fail(e,"Destination must be detached before source currentScene assignment");
  if (!input_.continuation->global()->core().set_object(FieldGlobalMemberRole::CurrentScene,id,e) ||
      !input_.continuation->registry()->observe_global_current_scene(id,e)) {
    failed_=true; return false;
  }
  published_=true; e.clear(); return true;
}
bool PodunkSceneOperations::attach_existing_player(std::string &e) {
  if (!live(e) || !published_ || attached_ || !input_.lifecycle->scene_ready() ||
      !input_.player->assembled() ||
      input_.player->tree()!=input_.tree || input_.player->registry()!=input_.continuation->registry())
    return fail(e,"Existing continuation Player factory is not assembled");
  auto id=input_.player->body().object();
  auto n=input_.tree->state(id);
  const auto *init=input_.player->body().data();
  FieldObjectId root=0,parent=0;
  if (!n || !n->alive || n->inside || n->parent || n->queued || !init || !area(root,e) ||
      !input_.tree->state(root)->inside)
    return fail(e,"Player source attachment requires completed destination root Enter/Ready");
  for (const auto &path:init->policy().parent_candidates) {
    FieldObjectId found=0;
    if (!input_.tree->get_node(root,path,found,e)) return false;
    if (found) { parent=found; break; }
  }
  if (!parent) return fail(e,"Destination lacks source currentScene Player parent");
  std::shared_ptr<const GlobalLoadObjectArray> party;
  if (!input_.continuation->global()->core().array(FieldGlobalMemberRole::PartyObjects,party,e) ||
      !party || party->values.size()!=1 || party->values.front()!=id)
    return fail(e,"Continuation global partyObjects does not own the same Player");
  if (!input_.tree->add_child(parent,id,e)) { failed_=true; return false; }
  attached_=true; e.clear(); return true;
}
} // namespace encore::ctr

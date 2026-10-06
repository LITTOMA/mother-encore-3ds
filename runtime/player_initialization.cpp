#include "encore/player_initialization.hpp"
#include <algorithm>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool leader(const PlayerInitializationData &d, const FieldGlobalDataRuntime &c,
            FieldGlobalRegistry &r, FieldObjectId &out, std::string &e) {
  FieldGlobalDataMemberState v;
  if (!c.data() ||
      c.data()->identity().upstream_commit != d.identity().upstream_commit ||
      !c.constructor_complete() ||
      !c.read_global_member(d.policy().leader_member, v, e))
    return false;
  if (v.adapter != 1)
    return fail(e, "Player actual Character Dictionary owner rejected");
  auto it = std::find_if(
      v.references.begin(), v.references.end(),
      [&](const auto &x) { return x.first == d.policy().leader_key; });
  if (it == v.references.end() || !r.object_exists(it->second) ||
      !c.constructed_body_alive(it->second))
    return fail(e, "Player source leader actual Object body unavailable");
  FieldGlobalDataObject body;
  if (!c.read_constructed_object(it->second, body, e) ||
      body.object != it->second ||
      body.declaration != d.policy().leader_declaration)
    return fail(e, "Player source leader actual declaration mismatch");
  out = it->second;
  return true;
}
} // namespace
namespace player_initialization_detail {
bool admit_dynamic_ready_subtree(const PlayerInitializationData &data,
                                 const FieldNodeTreeRuntime &tree,
                                 FieldObjectId actual_player,
                                 std::string &error) {
  if (!data.valid() || !actual_player)
    return fail(error, "Player dynamic subtree source/root unavailable");
  const auto &expected = data.recipe().identity();
  for (const auto &record : data.recipe().records()) {
    FieldObjectId object = 0;
    if (!tree.get_node(actual_player, record.path, object, error))
      return false;
    const auto *state = tree.state(object);
    const auto *descriptor = tree.descriptor(object);
    FieldIdentity identity;
    if (!state || !descriptor || !state->alive ||
        !tree.object_identity(object, identity) ||
        identity.scene_id != expected.scene_id ||
        identity.source_sha256 != expected.source_sha256 ||
        identity.upstream_commit != expected.upstream_commit ||
        descriptor->id != record.id ||
        descriptor->native_class != record.native_class ||
        descriptor->script != record.script ||
        descriptor->script_sha != record.script_sha)
      return fail(error, "Player dynamic relative path/source owner rejected");
    if (!state->inside || !state->bound || !state->ready_notified)
      return fail(error,
                  "Player complete source subtree native/script Ready pending");
  }
  return true;
}
} // namespace player_initialization_detail
bool PlayerInitializationBody::construct(const PlayerInitializationData &d,
                                         FieldNodeTreeRuntime &t,
                                         FieldObjectId id,
                                         const FieldGlobalDataRuntime &c,
                                         FieldGlobalRegistry &r,
                                         ResourceLoader load, std::string &e) {
  if (data_ || poisoned_ || !d.valid() || t.object_domain() != r.kernel())
    return fail(e, "Player constructor owner/domain/reentry rejected");
  auto state = t.state(id);
  auto desc = t.descriptor(id);
  FieldIdentity identity;
  if (!state || !desc || !t.object_identity(id, identity) ||
      identity.scene_id != d.identity().scene_id ||
      identity.source_sha256 != d.identity().source_sha256 ||
      identity.upstream_commit != d.identity().upstream_commit ||
      state->inside || state->parent || !state->children.empty() ||
      !state->name.empty() || desc->id != d.identity().scene_id ||
      desc->script != d.player_source())
    return fail(e, "Player script constructor requires actual pre-child "
                   "attachment cursor");
  std::array<uint8_t, 32> h{};
  if (!d.source_hash(desc->script, h) || h != desc->script_sha)
    return fail(e, "Player constructor actual source script rejected");
  // This audited root has its three native properties before script and none
  // after it. Name/groups/owner are not observed by these source declarations.
  data_ = &d;
  tree_ = &t;
  object_ = id;
  for (const auto &f : d.fields()) {
    PlayerInitializationMember v;
    v.kind = f.kind;
    v.value = f.value;
    v.vector = f.vector;
    if (f.adapter == 1) {
      if (!leader(d, c, r, v.object, e)) {
        poisoned_ = true;
        return false;
      }
    } else if (f.adapter == 2) {
      if (!load || !load(f, v.object, e)) {
        poisoned_ = true;
        return false;
      }
      auto resource = r.source_resource(v.object);
      if (!resource || std::string(resource->resource_class()) != f.native) {
        poisoned_ = true;
        return fail(e,
                    "Player constructor requires actual source audio Resource");
      }
      auto b = resource->binding();
      if (!d.source_hash(f.resource, h) || b.object != v.object ||
          b.source.source != f.resource || b.source.source_sha != h ||
          b.source.identity.upstream_commit != d.identity().upstream_commit) {
        poisoned_ = true;
        return fail(e,
                    "Player constructor loaded resource source/owner rejected");
      }
    }
    members_.emplace(f.name, std::move(v));
  }
  complete_ = true;
  return true;
}
bool PlayerInitializationBody::member(std::string_view name,
                                      PlayerInitializationMember &out,
                                      std::string &e) const {
  if (!constructed() || !tree_ || !tree_->state(object_) ||
      !tree_->state(object_)->alive)
    return fail(e, "Player actual script body not live/constructed");
  auto it = members_.find(std::string(name));
  if (it == members_.end())
    return fail(e, "Player unknown source member");
  out = it->second;
  return true;
}
bool PlayerInitializationRuntime::initialize(const PlayerInitializationData &d,
                                             FieldGlobalConstructorRuntime &g,
                                             const FieldGlobalDataRuntime &c,
                                             FieldGlobalRegistry &r,
                                             PlayerInitializationHost h,
                                             std::string &e) {
  if (data_ || !d.valid() || !g.data() || !g.owner() ||
      !r.object_exists(g.owner()))
    return fail(e, "Player initialization actual global owner unavailable");
  const auto &p = d.policy();
  auto matches = [&](FieldGlobalMemberRole role, const std::string &name) {
    auto m = g.data()->member(role);
    return m && m->name == name;
  };
  std::array<uint8_t, 32> a{}, b{};
  if (!matches(FieldGlobalMemberRole::CurrentScene, p.current_scene) ||
      !matches(FieldGlobalMemberRole::Party, p.party) ||
      !matches(FieldGlobalMemberRole::PartyObjects, p.party_objects) ||
      !d.source_hash(d.owner_source(), a) ||
      !g.data()->source_hash(d.owner_source(), b) || a != b ||
      g.data()->identity().upstream_commit != d.identity().upstream_commit)
    return fail(e, "Player source global bindings rejected");
  data_ = &d;
  global_ = &g;
  characters_ = &c;
  registry_ = &r;
  host_ = std::move(h);
  return true;
}
bool PlayerInitializationRuntime::run(std::string &e) {
  if (!data_ || started_ || poisoned_)
    return fail(e, "Player initialization unbound/reentry/poison rejected");
  auto actual_global = registry_->external_object(global_->owner());
  FieldGlobalExternalState global_state;
  if (!actual_global || !actual_global->state(global_state, e) ||
      !global_state.inside)
    return fail(e, "Player source global method requires actual entered owner");
  auto root = registry_->external_object(registry_->root());
  FieldGlobalExternalState root_state;
  if (!root || !root->state(root_state, e) || !root_state.inside ||
      root_state.children.empty())
    return fail(e, "Player initialization actual SceneTree root unavailable");
  const auto scene = root_state.children.back();
  if (!registry_->object_exists(scene) ||
      !global_->set_object(FieldGlobalMemberRole::CurrentScene, scene, e))
    return false;
  std::shared_ptr<const GlobalLoadObjectArray> existing;
  if (!global_->array(FieldGlobalMemberRole::PartyObjects, existing, e) ||
      !existing)
    return false;
  if (!existing->values.empty()) {
    complete_ = true;
    started_ = true;
    player_ = existing->values.front();
    return true;
  }
  // A missing actual method fails before the source party append/allocations.
  // Presence is not Ready admission; the returned live kernel must still run
  // its complete source native/script lifecycle below.
  if (!host_.instantiate || !host_.connect_ready_pause || !host_.followers ||
      !host_.set_respawn)
    return fail(
        e, "Player native/source factory, pause or respawn endpoint pending");
  auto scene_tree = registry_->tree_owner(scene);
  if (!scene_tree)
    return fail(e, "Player currentScene actual Tree owner pending");
  FieldObjectId character = 0;
  if (!leader(*data_, *characters_, *registry_, character, e))
    return false;
  started_ = true;
  auto abort = [&]() {
    poisoned_ = true;
    return false;
  };
  if (!global_->append_array(FieldGlobalMemberRole::Party, character, e))
    return abort();
  std::shared_ptr<FieldNodeTreeRuntime> tree;
  if (!host_.instantiate(*data_, tree, player_, e))
    return abort();
  auto player_state = tree ? tree->state(player_) : nullptr;
  auto desc = tree ? tree->descriptor(player_) : nullptr;
  FieldIdentity identity;
  if (!tree || tree.get() != scene_tree.get() ||
      registry_->tree_owner(player_).get() != tree.get() ||
      tree->object_domain() != registry_->kernel() || !player_state ||
      player_state->inside || player_state->parent || !desc ||
      desc->id != data_->identity().scene_id ||
      !tree->object_identity(player_, identity) ||
      identity.upstream_commit != data_->identity().upstream_commit ||
      identity.source_sha256 != data_->identity().source_sha256) {
    fail(e, "Player factory returned wrong/duplicate source ObjectDB owner");
    return abort();
  }
  if (!tree->set_name(player_, data_->policy().player_name, e))
    return abort();
  auto transform = player_state->local;
  transform[2] = data_->policy().position;
  if (!tree->set_local(player_, transform, e) ||
      !global_->assign_array(FieldGlobalMemberRole::PartyObjects, {player_}, e))
    return abort();
  FieldObjectId parent = 0;
  auto scene_state = tree->state(scene);
  if (!scene_state) {
    fail(e, "Player currentScene live source state unavailable");
    return abort();
  }
  for (const auto &name : data_->policy().parent_candidates) {
    for (auto child : scene_state->children) {
      auto s = tree->state(child);
      if (s && s->alive && s->name == name) {
        parent = child;
        break;
      }
    }
    if (parent)
      break;
  }
  const bool fallback = !parent;
  if (fallback)
    parent = scene;
  if (!tree->add_child(parent, player_, e))
    return abort();
  // add_child synchronously enters and readies the source subtree. Nothing
  // here substitutes a class-name approval or pause-before-Ready shortcut.
  if (!player_initialization_detail::admit_dynamic_ready_subtree(*data_, *tree,
                                                                 player_, e))
    return abort();
  if (fallback &&
      (!tree->set_visible(player_, false, e) ||
       !host_.connect_ready_pause(player_, data_->policy().ready_signal,
                                  data_->policy().pause_method,
                                  data_->policy().connect_flags, e)))
    return abort();
  if (!host_.followers(data_->policy().followers_method,
                       data_->policy().followers_emit, e) ||
      !host_.set_respawn(data_->policy(), player_, scene, e))
    return abort();
  complete_ = true;
  return true;
}
} // namespace encore::upstream

#include "podunk_outdoor_door.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *message) { e = message; return false; }
std::string_view source_path(std::string_view path) {
  constexpr std::string_view prefix = "res://";
  return path.substr(0, prefix.size()) == prefix ? path.substr(prefix.size()) : path;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
bool finite(Vec2 p) { return std::isfinite(p.x) && std::isfinite(p.y); }
}
bool PodunkOutdoorDoor::prepare(PodunkOutdoorDoorInput in, std::string &e) {
  if (in_.registry || !in.doors || !in.doors->valid() || !in.scene ||
      !in.scene->valid() || !in.audio_data || !in.audio_data->valid() ||
      !same(in.doors->identity(), in.scene->identity()) ||
      !same(in.doors->identity(), in.audio_data->identity()) ||
      !in.registry || !in.native_root ||
      in.native_root->kernel_object() != in.registry->kernel() ||
      !in.signals || in.signals->registry() != in.registry ||
      !in.global || !in.global->data() || !in.characters ||
      in.characters->registry() != in.registry || !in.flags ||
      !in.scene_lifecycle || !in.music || !in.music_service || !in.ui ||
      in.registry->external_object(in.ui->binding().object) != in.ui ||
      !in.player || !in.player_services || !in.audio || !in.fade || !in.target ||
      in.target->registry() != in.registry || !in.target->data() ||
      !in.target->data()->valid() || !in.source || !in.shapes_ready ||
      !in.schedule_party_changed)
    return fail(e, "Outdoor Door requires the same checked actual source owners");
  const auto &d = *in.target->data();
  FieldDoorDescriptor bound;
  if (!in.doors->find(d.door_id(), bound) ||
      source_path(d.source_scene()) != source_path(in.doors->source_scene()) ||
      d.identity().upstream_commit != in.doors->identity().upstream_commit ||
      d.body_method() != in.doors->body_method() || d.body_signal().empty() ||
      d.deferred_method().empty() || d.scene_changed_signal().empty() ||
      d.party_changed_signal().empty() || d.party_changed_wait_signal().empty() ||
      !d.empty_target_params())
    return fail(e, "Outdoor Door source target/method policy differs");
  for (auto signal : {FieldDoorSignal::Entered, FieldDoorSignal::MovedPlayer,
                      FieldDoorSignal::Done})
    if (d.door_signal(signal).empty())
      return fail(e, "Outdoor Door source signal policy is incomplete");
  in_ = std::move(in);
  e.clear(); return true;
}
bool PodunkOutdoorDoor::live(std::string &e) const {
  if (!in_.registry || in_.registry->poisoned() || failed_)
    return fail(e, "Outdoor Door owner is unavailable or poisoned");
  e.clear(); return true;
}
bool PodunkOutdoorDoor::source(uint32_t stable, FieldObjectId &out,
                              std::string &e) const {
  if (!live(e) || !stable) return false;
  const auto retained = source_objects_.find(stable);
  if (retained != source_objects_.end()) out = retained->second;
  else if (!in_.source(stable, out, e)) return false;
  auto tree = in_.registry->tree_owner(out);
  const auto *d = tree ? tree->descriptor(out) : nullptr;
  FieldIdentity identity;
  if (!d || d->id != stable || !in_.registry->object_exists(out) ||
      !tree->object_identity(out, identity) ||
      !same(identity, in_.doors->identity()))
    return fail(e, "Outdoor Door source resolver returned a foreign actual node");
  e.clear(); return true;
}
bool PodunkOutdoorDoor::player(FieldObjectId actual, std::string &e) const {
  if (!live(e) || actual != in_.player->body().object() || !actual ||
      !in_.registry->object_exists(actual))
    return fail(e, "Outdoor Door operation targets a different actual Player");
  auto tree = in_.registry->tree_owner(actual);
  const auto *n = tree ? tree->state(actual) : nullptr;
  if (!n || !n->alive || n->queued || in_.player->registry() != in_.registry ||
      in_.player->tree() != tree.get() || in_.player->body().tree() != tree.get())
    return fail(e, "Outdoor Door actual Player/native ownership is not rebound");
  e.clear(); return true;
}
bool PodunkOutdoorDoor::actual_root(FieldObjectId &out, std::string &e) const {
  if (!live(e) || !in_.global->object(FieldGlobalMemberRole::CurrentScene, out, e))
    return false;
  auto tree = in_.registry->tree_owner(out);
  const auto *n = tree ? tree->state(out) : nullptr;
  if (!out || !n || !n->alive || n->queued ||
      in_.registry->current_scene() != out)
    return fail(e, "Outdoor Door global/current source scene owners differ");
  e.clear(); return true;
}
bool PodunkOutdoorDoor::observe(FieldDoorContext &out, std::string &e) const {
  FieldObjectId root = 0;
  out.player = in_.player ? in_.player->body().object() : 0;
  if (!player(out.player, e) || !actual_root(root, e) ||
      !in_.global->boolean(FieldGlobalMemberRole::EnteringDoor, out.entering, e) ||
      !in_.ui->source_is_in_cutscene(in_.ui->binding().object, out.in_cutscene, e))
    return false;
  auto tree = in_.registry->tree_owner(root);
  const auto *n = tree->state(root);
  if (!n->inside || !n->bound || !n->ready_notified || n->name.empty())
    return fail(e, "Outdoor Door current actual scene has not completed native lifecycle");
  out.current_scene_name = n->name;
  e.clear(); return true;
}
bool PodunkOutdoorDoor::resolve(const FieldDoorDescriptor &d,
                               const FieldDoorAudio &a, std::string &e) {
  FieldDoorDescriptor original;
  FieldObjectId root = 0, marker = 0, audio = 0;
  if (!in_.doors->find(d.id, original) || original.marker != d.marker ||
      original.audio != d.audio || original.shape != d.shape || a.id != d.audio ||
      !source(d.id, root, e) || !source(d.marker, marker, e) ||
      !source(d.audio, audio, e) || !in_.shapes_ready(d.id, {d.shape}, e))
    return false;
  auto tree = in_.registry->tree_owner(root);
  const auto *body = tree->state(root);
  const auto *m = tree->state(marker), *v = tree->state(audio);
  const auto *md = tree->descriptor(marker), *ad = tree->descriptor(audio);
  PodunkSceneAudioState playing;
  if (!body || !body->inside || !body->bound || !m || !v ||
      m->parent != root || v->parent != root || !m->inside || !v->inside ||
      !m->ready_notified || !v->ready_notified || !md || !ad ||
      md->native_class != "Position2D" || ad->native_class != "AudioStreamPlayer" ||
      !in_.audio->owns(audio) || !in_.audio->state(audio, playing, e) || !playing.ready)
    return fail(e, "Outdoor Door onready children lack actual native lifecycle");
  const auto *node = in_.audio_data->node(d.audio);
  const auto *stream = node ? in_.audio_data->stream(node->stream) : nullptr;
  if (!node || node->bus != in_.doors->string(a.bus) || !stream ||
      stream->source != in_.doors->string(a.source) || stream->source_sha != a.sha)
    return fail(e, "Outdoor Door initial actual audio source properties differ");
  for (const auto &entry : {std::pair<uint32_t, FieldObjectId>{d.id, root},
                           {d.marker, marker}, {d.audio, audio}}) {
    const auto existing = source_objects_.find(entry.first);
    if (existing != source_objects_.end() && existing->second != entry.second)
      return fail(e, "Outdoor Door onready changed an existing actual source reference");
  }
  source_objects_.emplace(d.id, root);
  source_objects_.emplace(d.marker, marker);
  source_objects_.emplace(d.audio, audio);
  e.clear(); return true;
}
bool PodunkOutdoorDoor::connect(uint32_t id,
    std::function<bool(uint64_t, std::string &)> slot, std::string &e) {
  FieldObjectId actual = 0;
  if (!slot || !source(id, actual, e) || body_slots_.count(actual))
    return fail(e, "Outdoor Door source body callback is missing or duplicated");
  const auto &d = *in_.target->data();
  if (!in_.signals->connect(actual, d.body_signal(), actual, d.body_method(),
                            0, {}, e)) return false;
  body_slots_.emplace(actual, std::move(slot));
  e.clear(); return true;
}
bool PodunkOutdoorDoor::bind_runtime(FieldDoorRuntime &runtime, std::string &e) {
  if (!live(e) || runtime_ || runtime.data() != in_.doors)
    return fail(e, "Outdoor Door executor does not own this exact source pack");
  runtime_ = &runtime;
  e.clear(); return true;
}
PodunkPlayerSceneServices *PodunkOutdoorDoor::services() const {
  return destination_.root ? in_.target->player_services() : in_.player_services;
}
bool PodunkOutdoorDoor::position(FieldObjectId id, Vec2 p, std::string &e) {
  if (!player(id, e) || !finite(p))
    return fail(e, "Outdoor Door warp actual Player/vector differs");
  auto tree = in_.registry->tree_owner(id);
  const auto *n = tree->state(id);
  FieldTransform transform{Vec2{1, 0}, Vec2{0, 1}, Vec2{0, 0}};
  if (n->parent && !tree->world_transform(n->parent, transform, e)) return false;
  const float det = transform[0].x * transform[1].y -
                    transform[0].y * transform[1].x;
  if (!std::isfinite(det) || det == 0)
    return fail(e, "Outdoor Door actual Player parent transform is singular");
  const Vec2 delta{p.x - transform[2].x, p.y - transform[2].y};
  auto local = n->local;
  local[2] = {(transform[1].y * delta.x - transform[1].x * delta.y) / det,
              (-transform[0].y * delta.x + transform[0].x * delta.y) / det};
  return tree->set_local(id, local, e);
}
bool PodunkOutdoorDoor::update_party(FieldObjectId id, std::string &e) {
  if (!player(id, e)) return false;
  std::shared_ptr<const FieldGlobalPartySpaceArray> crumbs;
  if (!in_.global->party_space(crumbs, e) || !crumbs) return false;
  if (crumbs->values.size() <= 1) { e.clear(); return true; }
  std::shared_ptr<const GlobalLoadObjectArray> objects;
  if (!in_.global->array(FieldGlobalMemberRole::PartyObjects, objects, e) ||
      !objects || objects->values != std::vector<FieldObjectId>{id})
    return fail(e, "Outdoor Door actual follower reinit/disappear owner is unavailable");
  auto tree = in_.registry->tree_owner(id);
  const auto local = tree->state(id)->local[2];
  for (size_t i = 0, count = crumbs->values.size(); i < count; ++i) {
    FieldGlobalPartySpaceValue removed;
    if (!in_.global->push_front_party_space(local, e) ||
        !in_.global->pop_back_party_space(removed, e)) return false;
  }
  e.clear(); return true;
}
bool PodunkOutdoorDoor::persistent(uint32_t id, bool add, std::string &e) {
  FieldObjectId actual = 0;
  std::shared_ptr<const GlobalLoadObjectArray> list;
  if (!source(id, actual, e) ||
      !in_.global->array(FieldGlobalMemberRole::Persistent, list, e) || !list)
    return false;
  const auto at = std::find(list->values.begin(), list->values.end(), actual);
  if (add) {
    // Original global.add_persistent appends without a duplicate guard.
    // Retain the same Array root and any aliases, including repeated entries.
    return in_.global->append_array(FieldGlobalMemberRole::Persistent, actual, e);
  }
  if (at == list->values.end()) { e.clear(); return true; }
  return in_.global->erase_array_first(FieldGlobalMemberRole::Persistent, actual, e);
}
bool PodunkOutdoorDoor::verify_destination(bool entered, std::string &e) const {
  if (!destination_.tree || !destination_.root || !destination_.player_parent ||
      destination_.tree->object_domain() != in_.registry->kernel() ||
      in_.registry->tree_owner(destination_.root) != destination_.tree ||
      in_.registry->tree_owner(destination_.player_parent) != destination_.tree)
    return fail(e, "Outdoor Door destination has no same ObjectDB native subtree");
  const auto *root = destination_.tree->state(destination_.root);
  const auto *parent = destination_.tree->state(destination_.player_parent);
  FieldIdentity identity;
  if (!root || !parent || !root->alive || !parent->alive || root->queued ||
      parent->queued || !destination_.tree->object_identity(destination_.root, identity) ||
      !same(identity, destination_.identity) || identity.upstream_commit != candidate_.pin ||
      identity.source_sha256 != candidate_.source_sha ||
      root->name != in_.target->data()->target_root_name())
    return fail(e, "Outdoor Door destination source native identity differs");
  FieldObjectId actual_parent = 0;
  if (!destination_.tree->get_node(destination_.root,
      in_.target->data()->player_parent(), actual_parent, e) ||
      actual_parent != destination_.player_parent)
    return fail(e, "Outdoor Door destination player parent is not the source node");
  if (entered ? (!root->inside || !root->bound || !root->ready_notified ||
                 !parent->inside || !parent->bound || !parent->ready_notified)
              : root->inside)
    return fail(e, "Outdoor Door destination actual Enter/Ready state differs");
  e.clear(); return true;
}
bool PodunkOutdoorDoor::scene_step(FieldDoorSceneStep step,
    const FieldDoorCandidate &candidate, uint64_t actual, Vec2 p, Vec2 dir,
    std::string &e) {
  const auto &policy = *in_.target->data();
  if (!live(e) || !candidate_.token || candidate.token != candidate_.token ||
      candidate.path != candidate_.path || candidate.pin != candidate_.pin ||
      candidate.source_sha != candidate_.source_sha ||
      next_step_ >= policy.steps().size() || policy.steps()[next_step_] != step ||
      !player(actual, e) || p.x != policy.position().x || p.y != policy.position().y ||
      dir.x != 0 || dir.y != 0)
    return fail(e, "Outdoor Door actual source transition cursor/arguments differ");
  bool result = false;
  switch (step) {
  case FieldDoorSceneStep::DetachPlayer: {
    if (!actual_root(old_root_, e)) break;
    old_tree_ = in_.registry->tree_owner(old_root_);
    const auto *n = old_tree_ ? old_tree_->state(actual) : nullptr;
    if (!n || !n->parent || !n->inside) {
      e = "Outdoor Door source Player is not attached to current scene"; break;
    }
    result = old_tree_->remove_child(n->parent, actual, e);
    break;
  }
  case FieldDoorSceneStep::DisablePlayerCollisions:
    result = services() && services()->collisions(false, e); break;
  case FieldDoorSceneStep::DetachPersistent: {
    std::shared_ptr<const GlobalLoadObjectArray> list;
    if (!in_.global->array(FieldGlobalMemberRole::Persistent, list, e) || !list) break;
    detached_persistent_ = list->values; result = true;
    for (auto id : detached_persistent_) {
      auto tree = in_.registry->tree_owner(id);
      const auto *n = tree ? tree->state(id) : nullptr;
      if (!n || !in_.registry->object_exists(id)) { result = fail(e, "Outdoor Door persistent source node is dead"); break; }
      if (n->parent && !tree->remove_child(n->parent, id, e)) { result = false; break; }
    }
    break;
  }
  case FieldDoorSceneStep::InstanceDestination:
    result = in_.target->instantiate(candidate, destination_, e) &&
             verify_destination(false, e); break;
  case FieldDoorSceneStep::LeaveOldArea:
    result = in_.target->leave_old(old_root_, *in_.scene_lifecycle, e); break;
  case FieldDoorSceneStep::FreeOldScene:
    result = in_.target->free_old(old_tree_, old_root_, e);
    if (result && (old_tree_->state(old_root_) || in_.registry->object_exists(old_root_)))
      result = fail(e, "Outdoor Door source free left the old native scene alive");
    break;
  case FieldDoorSceneStep::AssignCurrentScene:
    result = in_.global->set_object(FieldGlobalMemberRole::CurrentScene,
                                    destination_.root, e) &&
             in_.registry->observe_global_current_scene(destination_.root, e);
    break;
  case FieldDoorSceneStep::InitParameters:
    result = policy.empty_target_params();
    if (!result) e = "Outdoor Door nonempty init_params has no reviewed consumer";
    break;
  case FieldDoorSceneStep::AddRootAndReady:
    result = in_.target->attach_root(destination_, e) && verify_destination(true, e); break;
  case FieldDoorSceneStep::AddPlayer: {
    const auto *before = old_tree_->state(actual);
    if (!before || before->inside || before->parent) {
      e = "Outdoor Door source Player was not retained detached"; break;
    }
    result = in_.target->attach_player(destination_, *in_.player, e) && player(actual, e);
    const auto *after = result ? destination_.tree->state(actual) : nullptr;
    if (result && (!after || !after->inside || after->parent != destination_.player_parent))
      result = fail(e, "Outdoor Door destination did not attach the same Player");
    break;
  }
  case FieldDoorSceneStep::CreateFollowers: {
    std::shared_ptr<const GlobalLoadObjectArray> party, npcs, objects;
    if (!in_.global->array(FieldGlobalMemberRole::Party, party, e) ||
        !in_.global->array(FieldGlobalMemberRole::PartyNpcs, npcs, e) ||
        !in_.global->array(FieldGlobalMemberRole::PartyObjects, objects, e)) break;
    result = party && npcs && objects && party->values.size() == 1 &&
             npcs->values.empty() && objects->values == std::vector<FieldObjectId>{actual};
    if (!result) e = "Outdoor Door actual multi-party follower factory is not supported";
    // resize(1) retains the same already-singleton Array root. The original
    // asynchronous signal branch still executes, without delaying this caller.
    if (result) result = in_.schedule_party_changed(e);
    break;
  }
  case FieldDoorSceneStep::SetPartyPosition: {
    const auto *n = destination_.tree->state(actual);
    if (!n) break;
    auto local = n->local; local[2] = p;
    if (!destination_.tree->set_local(actual, local, e)) break;
    std::shared_ptr<const FieldGlobalPartySpaceArray> crumbs;
    if (!in_.global->party_space(crumbs, e) || !crumbs) break;
    result = true;
    if (crumbs->values.size() > 1)
      for (size_t i = 0, ncrumbs = crumbs->values.size(); i < ncrumbs; ++i) {
        FieldGlobalPartySpaceValue removed;
        if (!in_.global->push_front_party_space(p, e) ||
            !in_.global->pop_back_party_space(removed, e)) { result = false; break; }
      }
    break;
  }
  case FieldDoorSceneStep::ReparentPersistent:
    result = true;
    for (auto id : detached_persistent_) {
      if (!in_.target->reparent_persistent(destination_, id, e)) { result = false; break; }
      const auto tree = in_.registry->tree_owner(id);
      const auto *n = tree ? tree->state(id) : nullptr;
      if (tree != destination_.tree || !n || !n->inside || n->parent != destination_.player_parent) {
        result = fail(e, "Outdoor Door source persistent object was not rebound before Enter"); break;
      }
    }
    break;
  case FieldDoorSceneStep::SetTreeCurrent:
    result = in_.native_root->set_current_scene(destination_.root, e) &&
             in_.registry->observe_tree_current_scene(destination_.root, e); break;
  case FieldDoorSceneStep::UpdateKeyIndicator:
    result = in_.target->update_key_indicator(e); break;
  case FieldDoorSceneStep::EnablePlayerCollisions:
    result = services() && services()->collisions(true, e); break;
  }
  if (!result) { failed_ = true; if (e.empty()) e = "Outdoor Door actual source step failed"; return false; }
  ++next_step_; e.clear(); return true;
}
bool PodunkOutdoorDoor::schedule(uint32_t stable, std::string &e) {
  if (!live(e) || !runtime_ || !candidate_.token || deferred_pending_ ||
      stable != in_.target->data()->door_id() || !source(stable, transitioning_door_, e))
    return fail(e, "Outdoor Door deferred source invocation lacks an admitted candidate");
  FieldObjectId receiver = 0;
  if (!in_.global->object(FieldGlobalMemberRole::SceneTransition, receiver, e) ||
      !receiver || !in_.registry->object_exists(receiver) ||
      candidate_.token > uint64_t(std::numeric_limits<int64_t>::max()))
    return fail(e, "Outdoor Door actual SceneTransition receiver is unavailable");
  FieldDoorDescriptor door;
  if (!in_.doors->find(stable, door) || !in_.target->data()->empty_target_params())
    return fail(e, "Outdoor Door deferred source arguments are not admitted");
  DeferredCall call_state;
  call_state.receiver = receiver; call_state.candidate = candidate_;
  call_state.position = door.target;
  call_state.position.y -= in_.doors->ground_offset();
  if (call_state.position.x != in_.target->data()->position().x ||
      call_state.position.y != in_.target->data()->position().y)
    return fail(e, "Outdoor Door deferred position differs from the checked source call");
  // Typed capsule: the source path, position, zero direction and checked empty
  // Array stay owned by this exact candidate. It is not a generic callv ABI or
  // a nil substitution for the source Array argument.
  FieldDeferredMessage call;
  call.object = receiver; call.kind = FieldDeferredKind::Call;
  call.member = in_.target->data()->deferred_method();
  call.args = {int64_t(candidate_.token), FieldObjectRef{transitioning_door_}};
  if (!in_.registry->enqueue(std::move(call), e)) return false;
  deferred_call_ = std::move(call_state);
  deferred_pending_ = true; e.clear(); return true;
}
bool PodunkOutdoorDoor::handles(const FieldDeferredMessage &m) const {
  if (!in_.global) return false;
  FieldObjectId receiver = 0; std::string e;
  if (in_.global->object(FieldGlobalMemberRole::SceneTransition, receiver, e) &&
      m.object == receiver && m.member == in_.target->data()->deferred_method()) return true;
  return body_slots_.count(m.object) &&
         m.member == in_.target->data()->body_method();
}
bool PodunkOutdoorDoor::deferred(const FieldDeferredMessage &m, std::string &e) {
  if (!live(e) || !handles(m) || m.kind != FieldDeferredKind::Call)
    return fail(e, "Outdoor Door received a foreign native source call");
  const auto at = body_slots_.find(m.object);
  if (at != body_slots_.end()) {
    if (m.args.size() != 1 || !std::holds_alternative<FieldObjectRef>(m.args[0]) ||
        !in_.signals->emitting_to(m.object, in_.target->data()->body_signal(),
                                  m.object, m.member))
      return fail(e, "Outdoor Door body signal is not actual synchronous source dispatch");
    return at->second(std::get<FieldObjectRef>(m.args[0]).id, e);
  }
  if (!deferred_pending_ || !runtime_ || m.args.size() != 2 ||
      m.object != deferred_call_.receiver ||
      deferred_call_.candidate.token != candidate_.token ||
      deferred_call_.candidate.path != candidate_.path ||
      deferred_call_.candidate.pin != candidate_.pin ||
      deferred_call_.candidate.source_sha != candidate_.source_sha ||
      deferred_call_.position.x != in_.target->data()->position().x ||
      deferred_call_.position.y != in_.target->data()->position().y ||
      deferred_call_.direction.x != 0 || deferred_call_.direction.y != 0 ||
      !deferred_call_.parameters.empty() || !in_.target->data()->empty_target_params() ||
      !std::holds_alternative<int64_t>(m.args[0]) ||
      std::get<int64_t>(m.args[0]) <= 0 ||
      uint64_t(std::get<int64_t>(m.args[0])) != candidate_.token ||
      !std::holds_alternative<FieldObjectRef>(m.args[1]) ||
      std::get<FieldObjectRef>(m.args[1]).id != transitioning_door_ ||
      runtime_->phase() != FieldDoorPhase::Deferred)
    return fail(e, "Outdoor Door deferred source capsule/coroutine cursor differs");
  deferred_pending_ = false;
  return runtime_->deferred_commit(e);
}
bool PodunkOutdoorDoor::declaration(FieldObjectId id, std::string_view signal,
                                    uint32_t &argc, std::string &e) const {
  if (!live(e)) return false;
  auto tree = in_.registry->tree_owner(id);
  const auto *d = tree ? tree->descriptor(id) : nullptr;
  FieldDoorDescriptor door;
  if (!d || !in_.doors->find(d->id, door))
    return fail(e, "Outdoor Door signal emitter is not an actual source Door");
  for (auto role : {FieldDoorSignal::Entered, FieldDoorSignal::MovedPlayer,
                    FieldDoorSignal::Done})
    if (signal == in_.target->data()->door_signal(role)) {
      argc = 0; e.clear(); return true;
    }
  return fail(e, "Outdoor Door source signal declaration is unknown");
}
bool PodunkOutdoorDoor::apply(FieldDoorHost &host, std::string &e) {
  if (!live(e)) return false;
  host.observe = [this](auto &out, auto &error) { return observe(out, error); };
  host.resolve_onready = [this](const auto &d, const auto &a, auto &error) { return resolve(d, a, error); };
  host.connect_body = [this](auto id, auto slot, auto &error) { return connect(id, std::move(slot), error); };
  host.prepare_destination = [this](const auto &door, auto &out, auto &error) {
    if (candidate_.token || door.id != in_.target->data()->door_id())
      return fail(error, "Outdoor Door destination has no checked target consumer");
    if (!in_.target->prepare(door, out, error)) return false;
    std::array<uint8_t, 32> hash{};
    if (!out.token || out.path != in_.doors->string(door.target_path) ||
        source_path(out.path) != source_path(in_.target->data()->target_scene()) ||
        out.pin != in_.doors->identity().upstream_commit ||
        !in_.doors->source_hash(out.path, hash) || out.source_sha != hash) {
      std::string release;
      in_.target->release(out, false, release);
      return fail(error, "Outdoor Door source candidate identity is not the actual target");
    }
    candidate_ = out; next_step_ = 0; destination_ = {};
    error.clear(); return true;
  };
  host.release_candidate = [this](const auto &candidate, bool committed, auto &error) {
    if (candidate.token != candidate_.token ||
        (committed && next_step_ != in_.target->data()->steps().size()) ||
        !in_.target->release(candidate, committed, error)) return false;
    candidate_ = {}; deferred_pending_ = false; deferred_call_ = {};
    error.clear(); return true;
  };
  host.pause_player = [this](auto id, bool stop, bool idle, auto &error) {
    return player(id, error) && services() && services()->pause(stop, idle, true, error);
  };
  host.set_entering = [this](bool value, auto &error) {
    return in_.global->set_boolean(FieldGlobalMemberRole::EnteringDoor, value, error);
  };
  host.flag_exists = [this](auto key, bool &present, auto &error) {
    bool value = false; return in_.flags->read(false, key, present, value, error);
  };
  host.write_flag = [this](auto key, bool value, auto &error) {
    return in_.flags->write(false, key, value, error);
  };
  host.music_changers = [this](auto &out, auto &error) {
    out.clear();
    const auto *data = in_.music->content();
    if (!data) return fail(error, "Outdoor Door actual MusicChanger owner is unavailable");
    std::vector<uint64_t> regions;
    if (!in_.music_service->registered_regions(regions, error)) return false;
    for (auto region : regions) {
      const FieldMusicChangerBinding *binding = nullptr;
      for (const auto &b : data->bindings()) {
        if (b.region_id != region) continue;
        if (binding) return fail(error, "Outdoor Door actual musicChangers has ambiguous source ownership");
        binding = &b;
      }
      const auto *state = binding ? in_.music->state(binding->id) : nullptr;
      if (!state || !state->alive || !state->ready)
        return fail(error, "Outdoor Door actual musicChangers contains a foreign/dead source owner");
      FieldObjectId actual = 0;
      if (!source(binding->id, actual, error)) return false;
      out.push_back(actual);
    }
    error.clear(); return true;
  };
  host.stop_music = [this](auto id, float duration, auto &error) {
    auto tree = in_.registry->tree_owner(id);
    const auto *d = tree ? tree->descriptor(id) : nullptr;
    const auto *n = tree ? tree->state(id) : nullptr;
    return d && n && n->inside && in_.music->content()->binding(d->id) ?
      in_.music->stop_music(d->id, duration, error) :
      fail(error, "Outdoor Door music stop targets a foreign actual MusicChanger");
  };
  host.play_audio = [this](auto stable, auto path, const auto &hash, auto &error) {
    FieldObjectId actual = 0;
    if (!source(stable, actual, error) || !in_.audio->owns(actual)) return false;
    const FieldSceneAudioStream *found = nullptr;
    for (const auto &s : in_.audio_data->streams())
      if (s.source == path && s.source_sha == hash) { found = &s; break; }
    if (!found) return fail(error, "Outdoor Door source sound has no actual checked PCM stream");
    return in_.audio->set_stream(actual, found->id, error) && in_.audio->play(actual, 0, error);
  };
  host.fade = [this](bool entering, auto animation, const auto &color, float speed, auto &error) {
    if (!runtime_ || !runtime_->active_door()) return fail(error, "Outdoor Door Fade has no actual executor request");
    return in_.fade->start_source(*in_.doors, runtime_->active_door(),
                                  entering, animation, color, speed, error);
  };
  host.emit = [this](auto stable, FieldDoorSignal signal, auto &error) {
    FieldObjectId actual = 0;
    if (!source(stable, actual, error)) return false;
    return in_.signals->emit(actual, in_.target->data()->door_signal(signal), {}, error);
  };
  host.marker_world = [this](auto stable, auto &out, auto &error) {
    FieldObjectId actual = 0; FieldTransform transform;
    if (!source(stable, actual, error)) return false;
    auto tree = in_.registry->tree_owner(actual);
    if (tree->descriptor(actual)->native_class != "Position2D" ||
        !tree->world_transform(actual, transform, error)) return false;
    out = transform[2]; error.clear(); return true;
  };
  host.set_player_global_position = [this](auto id, auto p, auto &error) { return position(id, p, error); };
  host.persistent = [this](auto id, bool add, auto &error) { return persistent(id, add, error); };
  host.schedule_deferred = [this](auto id, auto &error) { return schedule(id, error); };
  host.clear_enemies = [this](auto &error) { return in_.ui->source_clear_on_screen_enemies(error); };
  host.scene_step = [this](auto step, const auto &candidate, auto id, auto p, auto dir, auto &error) {
    return scene_step(step, candidate, id, p, dir, error);
  };
  host.emit_scene_changed = [this](auto &error) {
    return in_.signals->emit(in_.global->owner(), in_.target->data()->scene_changed_signal(), {}, error);
  };
  host.camera_current_and_visible = [this](auto id, auto &error) {
    if (!player(id, error) || !in_.target->make_player_camera_current(id, error)) return false;
    return in_.registry->tree_owner(id)->set_visible(id, true, error);
  };
  host.fade_cut = [this](auto &error) { return in_.fade->cut(error); };
  host.direction_and_input = [this](auto id, auto direction, auto &error) {
    return player(id, error) && services() && services()->direction_and_input(direction, error);
  };
  host.update_party = [this](auto id, auto &error) {
    return update_party(id, error);
  };
  host.unpause_player = [this](auto id, auto &error) {
    return player(id, error) && services() && services()->unpause(true, error);
  };
  host.queue_free = [this](auto stable, auto &error) {
    FieldObjectId actual = 0;
    return source(stable, actual, error) && in_.registry->tree_owner(actual)->queue_free(actual, error);
  };
  host.set_respawn = [this](auto &error) {
    return services() && services()->respawn(error);
  };
  e.clear(); return true;
}
} // namespace encore::ctr

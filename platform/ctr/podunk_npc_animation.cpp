#include "podunk_npc_animation.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
bool tags_same(const std::vector<FieldSpriteTag> &a,
               const std::vector<FieldSpriteTag> &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i].animation_id != b[i].animation_id ||
        a[i].motion_index != b[i].motion_index)
      return false;
  return true;
}
bool transform_same(const FieldTransform &a, const FieldTransform &b) {
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i].x != b[i].x || a[i].y != b[i].y)
      return false;
  return true;
}
bool base_notification(FieldTreePhase p) {
  switch (p) {
  case FieldTreePhase::EnterScript:
  case FieldTreePhase::TreeEntered:
  case FieldTreePhase::NodeAdded:
  case FieldTreePhase::ChildEntered:
  case FieldTreePhase::PostEnterNative:
  case FieldTreePhase::ReadyScript:
  case FieldTreePhase::ReadySignal:
  case FieldTreePhase::ExitScript:
  case FieldTreePhase::TreeExiting:
  case FieldTreePhase::NodeRemoved:
  case FieldTreePhase::ChildExiting:
  case FieldTreePhase::TreeExited:
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
  case FieldTreePhase::ChildMoved:
  case FieldTreePhase::Deleting:
  case FieldTreePhase::PathChanged:
    return true;
  default:
    return false;
  }
}
} // namespace
bool PodunkNpcAnimationHost::prepare(
    const FieldNodeTreeData &n, const FieldSpriteData &s, const FieldNpcData &d,
    FieldNodeTreeRuntime &t, FieldGlobalRegistry &r, FieldSpriteRuntime &sr,
    FieldNpcRuntime &nr, std::string &e) {
  if (nodes_ || !n.valid() || !s.valid() || !d.valid() || !r.kernel() ||
      (t.object_domain() && t.object_domain() != r.kernel()) ||
      n.identity().scene_id != s.scene_id() || s.scene_id() != d.scene_id() ||
      n.identity().upstream_commit != s.source_pin() ||
      s.source_pin() != d.source_pin() || !field_sprite_npc_binding(s, d, e))
    return fail(e, "NPC AnimationTree source packs/domain rejected");
  std::map<uint32_t, Graph> prepared;
  std::map<uint32_t, ClipPlayer> players;
  std::set<uint32_t> ids;
  for (const auto &sprite : s.records()) {
    if (sprite.kind != FieldSpriteKind::Character)
      continue;
    const auto *node = n.record(sprite.id);
    std::array<uint8_t, 32> sha{};
    const auto parent =
        std::find_if(d.npcs().begin(), d.npcs().end(),
                     [&](const auto &v) { return v.id == sprite.parent_id; });
    if (!node || node->class_index >= n.classes().size() ||
        n.classes()[node->class_index] != "Sprite" || node->script.empty() ||
        !n.source_hash(node->script, sha) || sha != node->script_sha ||
        parent == d.npcs().end())
      return fail(e, "NPC AnimationTree caller script/parent binding rejected");
    Graph g;
    g.sprite = sprite.id;
    g.npc = parent->id;
    g.identity = n.identity();
    // The source onready reference binds the actual direct AnimationPlayer.
    // This is its checked native clip repository, not a second playback clock.
    for (const auto &child : n.records()) {
      if (child.parent != sprite.id ||
          child.class_index >= n.classes().size() ||
          n.classes()[child.class_index] != "AnimationPlayer")
        continue;
      if (g.player_source || !child.script.empty() || child.script_methods)
        return fail(
            e, "NPC CharacterSprite native AnimationPlayer ambiguous/scripted");
      g.player_source = child.id;
      players.emplace(child.id, ClipPlayer{sprite.id, 0, false, false});
    }
    if (!g.player_source)
      return fail(e,
                  "NPC CharacterSprite actual AnimationPlayer child missing");
    g.descriptor.path = node->path + "#AnimationTree.new";
    const auto &key = g.descriptor.path;
    g.descriptor.id = encore::crc32(
        reinterpret_cast<const uint8_t *>(key.data()), key.size());
    g.descriptor.index = -1;
    g.descriptor.native_class = "AnimationTree";
    g.descriptor.local = g.descriptor.world = {{{1, 0}, {0, 1}, {0, 0}}};
    g.descriptor.modulate = g.descriptor.self_modulate = {{1, 1, 1, 1}};
    if (!g.descriptor.id || n.record(g.descriptor.id) ||
        !ids.insert(g.descriptor.id).second)
      return fail(e, "NPC AnimationTree builtin identity collision");
    prepared.emplace(sprite.id, std::move(g));
  }
  if (prepared.empty())
    return fail(e, "NPC AnimationTree caller closure empty");
  nodes_ = &n;
  sprites_ = &s;
  npcs_ = &d;
  tree_ = &t;
  registry_ = &r;
  sprite_runtime_ = &sr;
  npc_runtime_ = &nr;
  graphs_ = std::move(prepared);
  players_ = std::move(players);
  e.clear();
  return true;
}
bool PodunkNpcAnimationHost::apply(FieldSpriteHost &h, std::string &e) {
  if (!nodes_ || applied_ || !h.publish)
    return fail(e, "NPC AnimationTree actual Sprite publisher missing/rebound");
  h.create_tree = [this](auto id, const auto &d, auto &error) {
    return create(id, d, error);
  };
  h.rebuild_tree = [this](auto id, const auto &a, const auto &tags,
                          const auto &connections, const auto &states,
                          auto &error) {
    return rebuild(id, a, tags, connections, states, error);
  };
  h.travel = [this](auto id, const auto &name, auto &error) {
    return travel(id, name, error);
  };
  h.blend = [this](auto id, auto value, const auto &tags, auto &error) {
    return blend(id, value, tags, error);
  };
  h.time_scale = [this](auto id, auto value, const auto &tags, auto &error) {
    return scale(id, value, tags, error);
  };
  auto publish = h.publish;
  h.publish = [this, publish](auto id, const auto &state, auto &error) {
    return observe_sprite(id, state, error) && publish(id, state, error);
  };
  applied_ = true;
  e.clear();
  return true;
}
PodunkNpcAnimationHost::Graph *PodunkNpcAnimationHost::graph(uint32_t id) {
  auto i = graphs_.find(id);
  return i == graphs_.end() ? nullptr : &i->second;
}
const FieldNpcInstance *PodunkNpcAnimationHost::npc(uint32_t id) const {
  for (const auto &v : npc_runtime_->npcs())
    if (v.id == id)
      return &v;
  return nullptr;
}
FieldObjectId PodunkNpcAnimationHost::animation_tree(uint32_t id) const {
  auto i = graphs_.find(id);
  return i == graphs_.end() ? 0 : i->second.object;
}
bool PodunkNpcAnimationHost::owns(const FieldNodeDescriptor &d) const {
  if (nodes_ && d.native_class == "AnimationPlayer" && d.script.empty()) {
    const auto *source = nodes_->record(d.id);
    return players_.count(d.id) && source && source->path == d.path &&
           source->class_index == d.class_index &&
           source->script_sha == d.script_sha;
  }
  if (!nodes_ || d.native_class != "AnimationTree" || !d.script.empty())
    return false;
  for (const auto &entry : graphs_)
    if (entry.second.descriptor.id == d.id &&
        entry.second.descriptor.path == d.path)
      return true;
  return false;
}
bool PodunkNpcAnimationHost::owns(FieldObjectId id) const {
  return objects_.count(id) != 0 || player_objects_.count(id) != 0;
}
bool PodunkNpcAnimationHost::construct(FieldObjectId id,
                                       const FieldNodeDescriptor &d,
                                       const FieldIdentity &identity,
                                       std::string &e) {
  if (!owns(d) || objects_.count(id) || !tree_ ||
      registry_->tree_owner(id).get() != tree_ || !registry_->object_exists(id))
    return fail(e, "NPC AnimationTree actual allocation/source rejected");
  if (d.native_class == "AnimationPlayer") {
    auto &p = players_.at(d.id);
    const auto *s = tree_->state(id);
    const auto *original = nodes_->record(d.id);
    if (p.object || !same(identity, nodes_->identity()) || !s || !original ||
        s->inside || s->parent || s->ready_notified || !s->name.empty() ||
        original->parent != p.sprite || d.parent != original->parent ||
        d.native_generated || d.script_methods)
      return fail(
          e, "NPC native clip player constructor source/lifecycle rejected");
    p.object = id;
    player_objects_.emplace(id, d.id);
    e.clear();
    return true;
  }
  Graph *g = nullptr;
  for (auto &entry : graphs_)
    if (entry.second.descriptor.id == d.id)
      g = &entry.second;
  const auto *state = tree_->state(id);
  const auto &expected = g->descriptor;
  if (g->object || !g->parent || !same(identity, g->identity) || !state ||
      state->inside || state->parent || state->ready_notified ||
      !state->name.empty() || d.parent || d.owner || d.canvas_parent ||
      d.class_index || d.ready || d.pause || d.flags || d.priority || d.z ||
      d.index != -1 || d.native_generated || !d.groups.empty() ||
      !d.name.empty() || d.script_methods ||
      d.script_sha != expected.script_sha ||
      !transform_same(d.local, expected.local) ||
      !transform_same(d.world, expected.world) ||
      d.modulate != expected.modulate ||
      d.self_modulate != expected.self_modulate)
    return fail(e, "NPC AnimationTree native constructor descriptor rejected");
  g->object = id;
  objects_.emplace(id, g->sprite);
  e.clear();
  return true;
}
bool PodunkNpcAnimationHost::actual(const Graph &g, std::string &e) const {
  const auto *s = tree_->state(g.object);
  const auto *d = tree_->descriptor(g.object);
  FieldIdentity identity;
  if (!g.object || !s || !d || d->id != g.descriptor.id ||
      d->native_class != "AnimationTree" || !d->script.empty() ||
      !tree_->object_identity(g.object, identity) ||
      !same(identity, g.identity) || !registry_->object_exists(g.object) ||
      registry_->tree_owner(g.object).get() != tree_ ||
      (s->parent && s->parent != g.parent))
    return fail(e, "NPC AnimationTree actual object owner disappeared");
  return true;
}
bool PodunkNpcAnimationHost::bind(FieldObjectId id, FieldNodeBinding &out,
                                  std::string &e) {
  auto player = player_objects_.find(id);
  if (player != player_objects_.end()) {
    const auto *d = tree_->descriptor(id);
    FieldIdentity identity;
    if (!d || !owns(*d) || registry_->tree_owner(id).get() != tree_ ||
        !registry_->object_exists(id) ||
        !tree_->object_identity(id, identity) ||
        !same(identity, nodes_->identity()))
      return fail(e, "NPC native clip player bind actual identity mismatch");
    out = {identity, d->id,         d->class_index, 0x454e003c,
           2,        d->script_sha, d->native_class};
    e.clear();
    return true;
  }
  auto i = objects_.find(id);
  if (i == objects_.end())
    return fail(e, "NPC AnimationTree bind foreign node");
  const auto &g = graphs_.at(i->second);
  if (!actual(g, e))
    return false;
  out = {g.identity, g.descriptor.id, 0, 0x454e003c, 2, {}, "AnimationTree"};
  e.clear();
  return true;
}
bool PodunkNpcAnimationHost::create(uint32_t id, const FieldSpriteDescriptor &d,
                                    std::string &e) {
  auto *g = graph(id);
  if (!g || sprites_->record(id) != &d || g->object ||
      sprite_runtime_->data() != sprites_ || npc_runtime_->data() != npcs_)
    return fail(e, "NPC AnimationTree source Ready duplicate/foreign runtime");
  g->parent = tree_->source_object(id);
  const auto *p = tree_->state(g->parent);
  if (!p || !p->inside || !p->ready_notified ||
      registry_->tree_owner(g->parent).get() != tree_)
    return fail(e, "NPC AnimationTree source caller not in actual Ready");
  const auto &player = players_.at(g->player_source);
  const auto *actual_player = tree_->state(player.object);
  if (!player.object || !player.entered || !player.ready || !actual_player ||
      actual_player->parent != g->parent || !actual_player->inside ||
      registry_->tree_owner(player.object).get() != tree_)
    return fail(
        e, "NPC AnimationTree source onready clip player not actually Ready");
  FieldObjectId object = 0;
  if (!tree_->instantiate_builtin_source(g->parent, *nodes_, *sprites_, object,
                                         e))
    return false;
  if (object != g->object || !actual(*g, e) ||
      !tree_->add_child(g->parent, object, e))
    return false;
  const auto *child = tree_->state(object);
  if (!child || child->parent != g->parent || !child->inside ||
      !child->ready_notified || !g->entered || !g->ready)
    return fail(e, "NPC AnimationTree actual add_child Enter/Ready incomplete");
  e.clear();
  return true;
}
bool PodunkNpcAnimationHost::rebuild(
    uint32_t id, const FieldSpriteAnimation &a,
    const std::vector<FieldSpriteTag> &tags,
    const std::vector<FieldSpriteConnection> &connections,
    const std::vector<std::string> &states, std::string &e) {
  auto *g = graph(id);
  if (!g || !actual(*g, e) || !g->ready || sprites_->animation(a.id) != &a ||
      states.empty())
    return fail(e, "NPC AnimationTree checked graph missing");
  for (const auto &tag : tags) {
    const auto *owner = sprites_->animation(tag.animation_id);
    if (!owner || tag.motion_index >= owner->motions.size())
      return fail(e,
                  "NPC AnimationTree retained blend tag outside checked graph");
  }
  for (const auto &c : connections)
    if (c.mode == 1 || c.mode > 2)
      return fail(
          e, "NPC AnimationTree unknown/sync transition requires consumer");
  for (const auto &name : states)
    if (name != sprites_->fallback() &&
        std::none_of(a.motions.begin(), a.motions.end(),
                     [&](const auto &m) { return m.name == name; }))
      return fail(e, "NPC AnimationTree state outside checked source graph");
  g->animation = &a;
  g->tags = tags;
  g->connections = connections;
  g->states = states;
  g->scale = 1;
  g->travel.clear();
  e.clear();
  return true;
}
bool PodunkNpcAnimationHost::travel(uint32_t id, const std::string &name,
                                    std::string &e) {
  auto *g = graph(id);
  if (!g || !actual(*g, e) || !g->animation ||
      std::find(g->states.begin(), g->states.end(), name) == g->states.end())
    return fail(e, "NPC AnimationTree travel unbuilt/unknown state");
  const auto *body = npc(g->npc);
  if (body && body->ready && body->current_motion != UINT32_MAX) {
    const auto &motions = npcs_->npcs().at(body->index).motions;
    const auto target = body->pending_motion == UINT32_MAX
                            ? body->current_motion
                            : body->pending_motion;
    if (target >= motions.size() || motions[target].name != name)
      return fail(e,
                  "NPC AnimationTree travel diverged from actual motion owner");
  }
  g->travel = name;
  e.clear();
  return true;
}
bool PodunkNpcAnimationHost::blend(uint32_t id, Vec2 value,
                                   const std::vector<FieldSpriteTag> &tags,
                                   std::string &e) {
  auto *g = graph(id);
  if (!g || !actual(*g, e) || !std::isfinite(value.x) ||
      !std::isfinite(value.y) || !tags_same(g->tags, tags))
    return fail(e, "NPC AnimationTree blend actual source tag/value rejected");
  g->blend = value;
  e.clear();
  return true;
}
bool PodunkNpcAnimationHost::scale(uint32_t id, float value,
                                   const std::vector<std::string> &tags,
                                   std::string &e) {
  auto *g = graph(id);
  const auto *sprite = sprite_runtime_->instance(id);
  if (!g || !actual(*g, e) || !sprite || tags != sprite->all_tags ||
      !std::isfinite(value) || value < 0)
    return fail(e, "NPC AnimationTree timescale/tag not admitted");
  g->scale = value;
  e.clear();
  return true;
}
bool PodunkNpcAnimationHost::observe_sprite(uint32_t id,
                                            const FieldSpriteInstance &s,
                                            std::string &e) {
  const auto *d = sprites_->record(id);
  if (!d)
    return fail(e, "NPC AnimationTree foreign Sprite publication");
  if (d->kind == FieldSpriteKind::Fetcher)
    return true;
  auto *g = graph(id);
  if (!g || (s.tree_created && (!g->ready || !actual(*g, e))))
    return fail(e,
                "NPC AnimationTree Sprite references unfinished actual child");
  if (!s.tree_created)
    return true;
  if (s.tree_active && !g->animation)
    return fail(e, "NPC AnimationTree active without compiled tree_root");
  if (s.tree_active != g->active) {
    if (!(s.tree_active
              ? tree_->add_group(g->object, "_process_internal", e)
              : tree_->remove_group(g->object, "_process_internal", e)))
      return false;
    g->active = s.tree_active;
  }
  e.clear();
  return true;
}
bool PodunkNpcAnimationHost::phase(FieldObjectId id, FieldTreePhase phase,
                                   float dt, bool paused, bool update_pending,
                                   std::string &e) {
  auto player = player_objects_.find(id);
  if (player != player_objects_.end()) {
    auto &p = players_.at(player->second);
    const auto *state = tree_->state(id);
    if (!state || !registry_->object_exists(id) ||
        registry_->tree_owner(id).get() != tree_)
      return fail(e, "NPC native clip player owner disappeared");
    switch (phase) {
    case FieldTreePhase::EnterNative:
      if (p.entered || !state->inside ||
          state->parent != tree_->source_object(p.sprite))
        return fail(e, "NPC native clip player Enter parent mismatch");
      p.entered = true;
      break;
    case FieldTreePhase::ReadyNative:
      if (!p.entered || p.ready || !state->ready_notified)
        return fail(e, "NPC native clip player Ready order mismatch");
      p.ready = true;
      break;
    case FieldTreePhase::ExitNative:
      if (!p.entered)
        return fail(e, "NPC native clip player Exit before Enter");
      p.entered = false;
      break;
    case FieldTreePhase::IdleInternal:
    case FieldTreePhase::PhysicsInternal:
    case FieldTreePhase::Idle:
    case FieldTreePhase::Physics:
    case FieldTreePhase::Input:
    case FieldTreePhase::UnhandledInput:
    case FieldTreePhase::UnhandledKeyInput:
      return fail(e, "NPC clip player independent playback not source enabled");
    default:
      if (!base_notification(phase))
        return fail(e, "NPC clip player unknown native notification");
      break;
    }
    e.clear();
    return true;
  }
  auto at = objects_.find(id);
  if (at == objects_.end())
    return fail(e, "NPC AnimationTree foreign native phase");
  auto &g = graphs_.at(at->second);
  if (!actual(g, e))
    return false;
  const auto *state = tree_->state(id);
  switch (phase) {
  case FieldTreePhase::EnterNative:
    if (g.entered || !state->inside || state->parent != g.parent)
      return fail(e, "NPC AnimationTree Enter source parent mismatch");
    g.entered = true;
    break;
  case FieldTreePhase::ReadyNative:
    if (!g.entered || g.ready || !state->ready_notified)
      return fail(e, "NPC AnimationTree native Ready order rejected");
    g.ready = true;
    break;
  case FieldTreePhase::ExitNative:
    if (!g.entered)
      return fail(e, "NPC AnimationTree native Exit before Enter");
    g.entered = false;
    break;
  case FieldTreePhase::IdleInternal: {
    if (!g.active || !update_pending || !tree_->can_process(id, paused)) {
      e.clear();
      return true;
    }
    if (!g.entered || !g.ready || !g.active || !g.animation ||
        !std::isfinite(dt) || dt < 0 || dt > 60 ||
        !tree_->can_process(id, paused))
      return fail(e,
                  "NPC AnimationTree actual internal idle admission rejected");
    const auto *sprite = sprites_->record(g.sprite);
    const auto *body = npc(g.npc);
    const auto scaled = dt * g.scale;
    if (!sprite || g.animation->id != sprite->setup_animation || !body ||
        !body->ready || body->destroyed || !std::isfinite(scaled) ||
        scaled > 60)
      return fail(e, "NPC AnimationTree source graph/body/scale not admitted");
    if (!npc_runtime_->idle_animations(g.npc, scaled)) {
      e = npc_runtime_->error();
      return false;
    }
    // The source presentation callback applies the frame to the real Sprite
    // native setter and publishes the same FieldSpriteRuntime projection.
    body = npc(g.npc);
    const auto *actual_sprite = sprite_runtime_->instance(g.sprite);
    if (!body || !actual_sprite || actual_sprite->frame != body->frame)
      return fail(e,
                  "NPC AnimationTree frame track missing actual Sprite setter");
    break;
  }
  case FieldTreePhase::PhysicsInternal:
  case FieldTreePhase::Idle:
  case FieldTreePhase::Physics:
  case FieldTreePhase::Input:
  case FieldTreePhase::UnhandledInput:
  case FieldTreePhase::UnhandledKeyInput:
    return fail(e, "NPC AnimationTree unsupported processing mode/callback");
  default:
    // This source-created class is Node, has no script and has no signal
    // listeners. Tree owns the generic parent/path/Enter/Ready notifications.
    if (!base_notification(phase))
      return fail(e, "NPC AnimationTree unknown native notification");
    break;
  }
  e.clear();
  return true;
}
bool PodunkNpcAnimationHost::deferred(const FieldDeferredMessage &m,
                                      std::string &e) {
  if (!owns(m.object))
    return fail(e, "NPC AnimationTree foreign deferred object");
  return fail(e, "NPC AnimationTree unmapped deferred property/method");
}
bool PodunkNpcAnimationHost::release(FieldObjectId id, std::string &e) {
  auto player = player_objects_.find(id);
  if (player != player_objects_.end()) {
    auto &p = players_.at(player->second);
    const auto *state = tree_->state(id);
    if (!state || state->inside || !state->children.empty() || p.entered)
      return fail(e, "NPC native clip player release before Exit");
    p.object = 0;
    p.ready = false;
    player_objects_.erase(player);
    e.clear();
    return true;
  }
  auto at = objects_.find(id);
  if (at == objects_.end())
    return fail(e, "NPC AnimationTree foreign release");
  auto &g = graphs_.at(at->second);
  const auto *state = tree_->state(id);
  if (!state || state->inside || !state->children.empty() || g.entered)
    return fail(e,
                "NPC AnimationTree release before actual exit/child cleanup");
  objects_.erase(at);
  g.object = 0;
  g.active = false;
  g.ready = false;
  g.animation = nullptr;
  e.clear();
  return true;
}
} // namespace encore::ctr

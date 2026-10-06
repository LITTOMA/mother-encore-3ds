#include "podunk_scene_scripts.hpp"
#include <algorithm>
#include <cmath>
namespace encore::ctr {
using namespace upstream;
namespace {
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
bool same(const FieldNodeBinding &a, const FieldNodeBinding &b) {
  return same(a.identity, b.identity) && a.stable_id == b.stable_id &&
         a.class_index == b.class_index && a.family == b.family &&
         a.capability == b.capability && a.script_sha == b.script_sha &&
         a.native_class == b.native_class;
}
} // namespace
bool PodunkSceneScripts::fail(std::string &e, const char *s) const {
  e = s;
  return false;
}
bool PodunkSceneScripts::supported(FieldSceneRole r) const {
  switch (r) {
  case FieldSceneRole::Grass:
    return consumers_.grass != nullptr;
  case FieldSceneRole::Npc:
    return consumers_.npc != nullptr;
  case FieldSceneRole::EnemySpawner:
    return consumers_.enemy != nullptr;
  case FieldSceneRole::Tint:
    return consumers_.tint != nullptr;
  case FieldSceneRole::CharacterSprite:
  case FieldSceneRole::SpriteFetcher:
    return consumers_.sprite != nullptr;
  case FieldSceneRole::Emotes:
    return consumers_.emote != nullptr;
  case FieldSceneRole::DandelionSpawner:
    return consumers_.dandelion != nullptr;
  case FieldSceneRole::Door:
    return consumers_.door != nullptr;
  case FieldSceneRole::ButtonPrompt:
    return consumers_.prompt != nullptr;
  case FieldSceneRole::DeadBush:
    return consumers_.bush != nullptr;
  case FieldSceneRole::OpenableDoor:
    return consumers_.openable != nullptr;
  case FieldSceneRole::Sparkles:
    return consumers_.sparkles != nullptr;
  case FieldSceneRole::InteractDialog:
    return consumers_.interact != nullptr;
  case FieldSceneRole::Payphone:
    return consumers_.payphone != nullptr;
  case FieldSceneRole::Present:
    return consumers_.present != nullptr;
  case FieldSceneRole::DroppedItem:
    return consumers_.dropped != nullptr;
  case FieldSceneRole::Butterfly:
    return consumers_.butterfly != nullptr;
  case FieldSceneRole::CutsceneArea:
    return consumers_.cutscene != nullptr;
  case FieldSceneRole::Birds:
    return consumers_.birds != nullptr;
  case FieldSceneRole::CameraArea:
    return consumers_.camera_area != nullptr;
  case FieldSceneRole::MusicChanger:
    return consumers_.music != nullptr;
  case FieldSceneRole::MapArrows:
    return consumers_.arrows != nullptr;
  case FieldSceneRole::GameCamera:
    return consumers_.camera != nullptr;
  case FieldSceneRole::Reparenter:
  case FieldSceneRole::EventActivator:
    return consumers_.actions != nullptr;
  case FieldSceneRole::SteppingSounds:
    return consumers_.stepping != nullptr;
  case FieldSceneRole::JumpArea:
  case FieldSceneRole::Stairs:
    return consumers_.transitions != nullptr;
  case FieldSceneRole::DoorNpc:
    return consumers_.door_npc != nullptr;
  case FieldSceneRole::MelodyBackground:
    return consumers_.melody != nullptr;
  case FieldSceneRole::VendingMachine:
    return consumers_.vending != nullptr;
  case FieldSceneRole::FlagLandmark:
  case FieldSceneRole::AreaRoom:
  case FieldSceneRole::DebugStart:
    return true;
  default:
    return false;
  }
}
bool PodunkSceneScripts::prepare(
    const FieldSceneSources &s, FieldSceneConsumers c, FieldSceneHostOps ops,
    FieldNodeTreeRuntime &t, FieldGlobalRegistry &r, FieldObjectSignals &bus,
    PodunkSceneGrassFactory *factory, std::string &e) {
  if (sources_ || !s.valid() || !r.data() || bus.registry() != &r ||
      !r.kernel() || (t.object_domain() && t.object_domain() != r.kernel()) ||
      (!t.object_domain() && t.object_count()) ||
      r.data()->identity().upstream_commit !=
          s.tree().identity().upstream_commit)
    return fail(
        e,
        "SceneScripts actual source/tree/Registry/signal ownership rejected");
  if (!s.bind_sources(c, e))
    return false;
  // A missing actual runtime is a recorded capability gap; never attach a
  // source Data pointer alone and let it masquerade as the runtime owner.
#define BORROW(member)                                                         \
  if (!c.member)                                                               \
  c.member##_data = nullptr
  BORROW(grass);
  BORROW(npc);
  BORROW(enemy);
  BORROW(tint);
  BORROW(sprite);
  BORROW(emote);
  BORROW(dandelion);
  BORROW(door);
  BORROW(prompt);
  BORROW(bush);
  BORROW(interact);
  BORROW(present);
  BORROW(dropped);
  BORROW(sparkles);
  BORROW(openable);
  BORROW(payphone);
  BORROW(butterfly);
  BORROW(cutscene);
  BORROW(birds);
  BORROW(camera_area);
  BORROW(music);
  BORROW(arrows);
  BORROW(actions);
  BORROW(stepping);
  BORROW(transitions);
  BORROW(camera);
  BORROW(door_npc);
  BORROW(melody);
  BORROW(vending);
#undef BORROW
  if (!c.geometry)
    c.geometry_data = nullptr;
  if ((c.grass && c.grass->data() != c.grass_data) ||
      (c.npc && c.npc->data() != c.npc_data) ||
      (c.tint && c.tint->data() != c.tint_data) ||
      (c.sprite && c.sprite->data() != c.sprite_data) ||
      (c.emote && c.emote->data() != c.emote_data))
    return fail(
        e, "SceneScripts borrowed runtime has another actual source owner");

#define SAME_DATA(member)                                                      \
  if (c.member && c.member->data() != c.member##_data)                         \
  return fail(e, "SceneScripts actual typed source owner differs: " #member)
  SAME_DATA(enemy);
  SAME_DATA(dandelion);
  SAME_DATA(door);
  SAME_DATA(prompt);
  SAME_DATA(bush);
  SAME_DATA(openable);
  SAME_DATA(payphone);
  SAME_DATA(interact);
  SAME_DATA(sparkles);
  SAME_DATA(birds);
  SAME_DATA(camera_area);
  SAME_DATA(arrows);
  SAME_DATA(camera);
  SAME_DATA(vending);
#undef SAME_DATA
#define SAME_CONTENT(member)                                                   \
  if (c.member && c.member->content() != c.member##_data)                      \
  return fail(e, "SceneScripts actual typed source owner differs: " #member)
  SAME_CONTENT(present);
  SAME_CONTENT(dropped);
  SAME_CONTENT(butterfly);
  SAME_CONTENT(cutscene);
  SAME_CONTENT(music);
  SAME_CONTENT(actions);
  SAME_CONTENT(stepping);
  SAME_CONTENT(transitions);
  SAME_CONTENT(door_npc);
  SAME_CONTENT(melody);
#undef SAME_CONTENT

  std::map<uint32_t, FieldSceneReady> roster;
  for (uint32_t i = 0; i < s.lifecycle().ready_count(); ++i) {
    const auto n = s.lifecycle().ready(i);
    const auto *d = s.tree().record(n.id);
    std::array<uint8_t, 32> sha{};
    if (!d || d->path != s.lifecycle().string(n.node) ||
        d->name != s.lifecycle().string(n.name) || d->ready != n.ordinal ||
        d->script != s.lifecycle().string(n.script) || d->script_sha != n.sha ||
        !s.lifecycle().source_hash(d->script, sha) || sha != d->script_sha ||
        !s.tree().source_hash(d->script, sha) || sha != d->script_sha ||
        !roster.emplace(n.id, n).second)
      return fail(e, "SceneScripts complete source roster/native/script/Ready "
                     "identity differs");
  }
  size_t actual_scripts = 0;
  for (const auto &d : s.tree().records())
    if (!d.script.empty()) {
      ++actual_scripts;
      if (!roster.count(d.id))
        return fail(
            e, "SceneScripts source script omitted from actual tree roster");
    }
  if (actual_scripts != roster.size())
    return fail(e,
                "SceneScripts script-null or inherited roster closure differs");
  if (!ops.visibility || !ops.queue_free)
    return fail(e, "SceneScripts actual canvas/delete observers missing");
  auto visible = ops.visibility;
  auto queue = ops.queue_free;
  ops.visibility = [this, visible](uint32_t stable, bool on, std::string &err) {
    FieldObjectId id = 0;
    if (!object_for_source(stable, id, err) || !tree_->set_visible(id, on, err))
      return false;
    return visible(stable, on, err);
  };
  ops.queue_free = [this, queue](uint32_t stable, std::string &err) {
    FieldObjectId id = 0;
    if (!object_for_source(stable, id, err) || !tree_->queue_free(id, err))
      return false;
    return queue(stable, err);
  };
  if (!lifecycle_.configure(s.lifecycle(), c, std::move(ops), e))
    return false;
  sources_ = &s;
  consumers_ = c;
  tree_ = &t;
  registry_ = &r;
  signals_ = &bus;
  grass_factory_ = factory;
  roster_ = std::move(roster);
  for (const auto &pair : roster_) {
    const auto &n = pair.second;
    if (!supported(n.role))
      gaps_.push_back(
          {n.id, n.ordinal, n.role, std::string(s.lifecycle().string(n.node)),
           std::string(s.lifecycle().string(n.script)),
           "Source constructor/native bridge not implemented in this owner"});
  }
  std::sort(gaps_.begin(), gaps_.end(),
            [](const auto &a, const auto &b) { return a.ordinal < b.ordinal; });
  e.clear();
  return true;
}
bool PodunkSceneScripts::owns(const FieldNodeDescriptor &d) const {
  if (!sources_ || d.script.empty())
    return false;
  auto p = roster_.find(d.id);
  const auto *actual = sources_->tree().record(d.id);
  return p != roster_.end() && actual && actual->path == d.path &&
         actual->native_class == d.native_class &&
         actual->class_index == d.class_index && actual->script == d.script &&
         actual->script_sha == d.script_sha && actual->ready == d.ready;
}
bool PodunkSceneScripts::owns(FieldObjectId id) const {
  return instances_.count(id) != 0;
}
bool PodunkSceneScripts::construct_body(const FieldSceneReady &n,
                                        std::string &e) {
  switch (n.role) {
  case FieldSceneRole::CharacterSprite:
  case FieldSceneRole::SpriteFetcher:
    if (consumers_.sprite->instance(n.id))
      return fail(e, "SceneScripts Sprite source body constructed twice");
    if (!consumers_.sprite->create(n.id)) {
      e = consumers_.sprite->error();
      return false;
    }
    return true;
  case FieldSceneRole::Emotes:
    if (consumers_.emote->instance(n.id))
      return fail(e, "SceneScripts Emotes source body constructed twice");
    if (!consumers_.emote->create(n.id)) {
      e = consumers_.emote->error();
      return false;
    }
    return true;
  case FieldSceneRole::Tint:
    if (consumers_.tint->instance(n.id))
      return fail(e, "SceneScripts Tint source body constructed twice");
    if (!consumers_.tint->create(n.id, n.id)) {
      e = consumers_.tint->error();
      return false;
    }
    return true;
  case FieldSceneRole::Npc: {
    const auto &v = consumers_.npc->npcs();
    auto body = std::find_if(v.begin(), v.end(), [&](const auto &x) {
      return x.id == n.id && !x.ready && !x.destroyed;
    });
    return body != v.end() ||
           fail(
               e,
               "SceneScripts actual initialized NPC body absent/already Ready");
  }
  case FieldSceneRole::Grass:
    for (uint32_t i = 0; i < consumers_.grass_data->grass_count(); ++i) {
      const auto g = consumers_.grass_data->grass(i);
      if (g.stable_id == n.id)
        return g.ready_ordinal == n.ordinal ||
               fail(e, "SceneScripts Grass body ordinal differs");
    }
    return fail(e, "SceneScripts Grass source body missing");
  case FieldSceneRole::MapArrows:
    return lifecycle_.construct_camera_arrows(n.id, e);
  case FieldSceneRole::DandelionSpawner:
    return consumers_.dandelion->source_body_unready(n.id) ||
           fail(e, "Dandelion actual initializer source body missing/already "
                   "Ready");
  case FieldSceneRole::EnemySpawner:
    return consumers_.enemy->source_body_unready(n.id) ||
           fail(e, "EnemySpawner actual initializer source body "
                   "missing/already Ready");
  case FieldSceneRole::Door: {
    FieldDoorDescriptor d;
    return (consumers_.door_data->find(n.id, d) &&
            !consumers_.door->source_ready(n.id)) ||
           fail(e, "Door actual initial source body missing/already Ready");
  }
#define CREATE_ROLE(role, member, source_constructor)                          \
  case FieldSceneRole::role:                                                   \
    if (consumers_.member->instance(n.id))                                     \
      return fail(e, "SceneScripts " #role " source body constructed twice");  \
    if (!consumers_.member->create(n.id, source_constructor)) {                \
      e = consumers_.member->error();                                          \
      return false;                                                            \
    }                                                                          \
    return true

    CREATE_ROLE(DeadBush, bush, true);
    CREATE_ROLE(OpenableDoor, openable, true);

#undef CREATE_ROLE
  case FieldSceneRole::ButtonPrompt:
    if (consumers_.prompt->instance(n.id) || !consumers_.prompt->create(n.id)) {
      e = consumers_.prompt->error();
      return false;
    }
    return true;
  case FieldSceneRole::Payphone:
    if (consumers_.payphone->instance(n.id) ||
        !consumers_.payphone->create(n.id)) {
      e = consumers_.payphone->error();
      return false;
    }
    return true;
  case FieldSceneRole::Sparkles:
    if (!consumers_.sparkles->create(n.id)) {
      e = consumers_.sparkles->error();
      return false;
    }
    return true;
  case FieldSceneRole::InteractDialog:
    if (!consumers_.interact->instantiate(n.id, true)) {
      e = consumers_.interact->error();
      return false;
    }
    return true;
#define CREATE_STATE(role, member)                                             \
  case FieldSceneRole::role:                                                   \
    if (consumers_.member->state(n.id))                                        \
      return fail(e, "SceneScripts " #role " source body constructed twice");  \
    if (!consumers_.member->create(n.id, true)) {                              \
      e = consumers_.member->error();                                          \
      return false;                                                            \
    }                                                                          \
    return true
    CREATE_STATE(Birds, birds);
    CREATE_STATE(CameraArea, camera_area);
    CREATE_STATE(GameCamera, camera);
#undef CREATE_STATE
#define BORROW_STATE(role, member)                                             \
  case FieldSceneRole::role: {                                                 \
    const auto *v = consumers_.member->state(n.id);                            \
    return (v && v->id == n.id && v->alive && !v->ready) ||                    \
           fail(e, "SceneScripts " #role                                       \
                   " actual initialized body absent/not live/already Ready");  \
  }
    BORROW_STATE(Butterfly, butterfly);
    BORROW_STATE(CutsceneArea, cutscene);
    BORROW_STATE(MusicChanger, music);
    BORROW_STATE(Reparenter, actions);
    BORROW_STATE(EventActivator, actions);
    BORROW_STATE(SteppingSounds, stepping);
    BORROW_STATE(DoorNpc, door_npc);
    BORROW_STATE(MelodyBackground, melody);
#undef BORROW_STATE
  case FieldSceneRole::Present: {
    const auto *v = consumers_.present->state(n.id);
    return (v && v->id == n.id && v->alive && !v->parent_ready &&
            !v->child_ready) ||
           fail(e, "Present actual parent/borrowed-child source constructor "
                   "missing/already Ready");
  }
  case FieldSceneRole::DroppedItem: {
    const auto *v = consumers_.dropped->state(n.id);
    return (v && v->id == n.id && v->alive && !v->parent_ready &&
            !v->child_ready) ||
           fail(e, "Dropped actual parent/borrowed-child source constructor "
                   "missing/already Ready");
  }
  case FieldSceneRole::JumpArea:
  case FieldSceneRole::Stairs: {
    const auto &v = consumers_.transitions->instances();
    auto at = std::find_if(v.begin(), v.end(), [&](const auto &x) {
      return x.id == n.id && !x.ready;
    });
    return at != v.end() || fail(e, "Transition actual source initializer body "
                                    "missing/already Ready");
  }
  case FieldSceneRole::VendingMachine:
    return consumers_.vending_data->descriptor().id == n.id ||
           fail(e, "Vending actual single source body missing");

  case FieldSceneRole::FlagLandmark:
    for (uint32_t i = 0; i < sources_->lifecycle().landmark_count(); ++i)
      if (sources_->lifecycle().landmark(i).id == n.id)
        return true;
    return fail(e, "SceneScripts Flaggable native declaration missing");
  case FieldSceneRole::AreaRoom:
    return sources_->lifecycle().area().id == n.id ||
           fail(e, "SceneScripts actual AreaRoom body differs");
  case FieldSceneRole::DebugStart:
    return sources_->lifecycle().debug(n.profile).id == n.id ||
           fail(e, "SceneScripts actual DebugStart body differs");
  default:
    return fail(e, "SceneScripts source constructor concrete owner pending");
  }
}
bool PodunkSceneScripts::construct_source(FieldObjectId id,
                                          const FieldNodeDescriptor &d,
                                          const FieldIdentity &i,
                                          std::string &e) {
  const auto *n = tree_ ? tree_->state(id) : nullptr;
  FieldIdentity actual_id;
  if (poisoned_ || !owns(d) || !n || !n->alive || n->inside || n->parent ||
      n->owner || !n->name.empty() || n->ready_notified || n->source != d.id ||
      instances_.count(id) || objects_.count(d.id) ||
      !same(i, sources_->tree().identity()) ||
      !tree_->object_identity(id, actual_id) || !same(i, actual_id) ||
      tree_->object_domain() != registry_->kernel() ||
      registry_->tree_owner(id).get() != tree_ || !registry_->object_exists(id))
    return fail(e, "SceneScripts source attachment actual "
                   "allocation/identity/order rejected");
  const auto ready = roster_.at(d.id);
  if (!supported(ready.role)) {
    e = "SceneScripts source constructor pending: " + d.path + " / " + d.script;
    return false;
  }
  if (!construct_body(ready, e)) {
    poisoned_ = true;
    return false;
  }
  Instance value;
  value.source = ready;
  value.binding = {i, d.id,         d.class_index, 0x454e001c,
                   6, d.script_sha, d.native_class};
  instances_.emplace(id, std::move(value));
  objects_.emplace(d.id, id);
  e.clear();
  return true;
}
bool PodunkSceneScripts::actual(FieldObjectId id, Instance *&out,
                                std::string &e) {
  auto at = instances_.find(id);
  const auto *n = tree_ ? tree_->state(id) : nullptr;
  const auto *d = tree_ ? tree_->descriptor(id) : nullptr;
  FieldIdentity identity;
  if (poisoned_ || at == instances_.end() || at->second.released || !n ||
      !n->alive || !d || !owns(*d) || n->source != at->second.source.id ||
      !tree_->object_identity(id, identity) ||
      !same(identity, at->second.binding.identity) ||
      tree_->object_domain() != registry_->kernel() ||
      registry_->tree_owner(id).get() != tree_ || !registry_->object_exists(id))
    return fail(e, "SceneScripts actual source ObjectDB owner unavailable");
  out = &at->second;
  return true;
}
bool PodunkSceneScripts::bind(FieldObjectId id, const FieldNodeDescriptor &d,
                              FieldNodeBinding &out, std::string &e) {
  Instance *v = nullptr;
  if (!actual(id, v, e) || !owns(d) || d.id != v->source.id || v->bound)
    return fail(e, "SceneScripts duplicate/foreign source binding rejected");
  if (v->source.role == FieldSceneRole::InteractDialog &&
      !consumers_.interact->complete_source_constructor(v->source.id)) {
    poisoned_ = true;
    e = consumers_.interact->error();
    return false;
  }
  v->bound = true;
  out = v->binding;
  e.clear();
  return true;
}
bool PodunkSceneScripts::grass_preload(std::string &e) const {
  // Source preload is observed only by instance() at screen entry. The checked
  // profile/texture recipe is the native template, without allocating a tree,
  // drawing random values, or requiring a dynamic instance backend at Ready.
  const auto *d = consumers_.grass_data;
  if (!d || !d->valid() || !consumers_.grass || consumers_.grass->data() != d ||
      !d->profile_count() || !d->texture_count() ||
      d->identity().upstream_commit !=
          sources_->tree().identity().upstream_commit)
    return fail(e, "Grass checked typed preload/actual runtime owner missing");
  e.clear();
  return true;
}
bool PodunkSceneScripts::grass_dynamic_factory(std::string &e) const {
  if (!grass_factory_ || grass_factory_->registry() != registry_ ||
      grass_factory_->source_data() != consumers_.grass_data ||
      grass_factory_->resource_class() != std::string_view("PackedScene") ||
      !grass_factory_->recipe().valid() ||
      grass_factory_->recipe().identity().upstream_commit !=
          sources_->tree().identity().upstream_commit)
    return fail(
        e, "Grass actual onready PackedScene recipe/factory owner pending");
  auto binding = grass_factory_->binding();
  std::array<uint8_t, 32> sha{};
  if (!binding.object ||
      registry_->source_resource(binding.object) != grass_factory_ ||
      !registry_->object_exists(binding.object) ||
      !sources_->tree().source_hash(grass_factory_->recipe().source_scene(),
                                    sha) ||
      sha != grass_factory_->recipe().identity().source_sha256 ||
      binding.source.identity.source_sha256 != sha ||
      binding.source.identity.upstream_commit !=
          sources_->tree().identity().upstream_commit)
    return fail(e, "Grass actual PackedScene ObjectDB/source closure differs");
  return true;
}
bool PodunkSceneScripts::grass_preview(FieldObjectId id,
                                       const FieldSceneReady &source,
                                       FieldObjectId &out,
                                       std::string &e) const {
  const auto *n = tree_->state(id);
  out = 0;
  if (!n)
    return fail(e, "Grass source preview owner dead");
  // The checked source has exactly one immediate, unscripted Sprite preview.
  // Resolve the actual node from the immutable tree; no game NodePath table.
  for (auto child : n->children) {
    const auto *d = tree_->descriptor(child);
    const auto *s = tree_->state(child);
    if (d && s && d->native_class == "Sprite" && d->script.empty()) {
      if (out || d->parent != source.id || !s->alive || !s->inside ||
          s->parent != id || s->queued)
        return fail(e, "Grass actual preview source child differs");
      out = child;
    }
  }
  return out != 0 || fail(e, "Grass actual source Sprite preview missing");
}
bool PodunkSceneScripts::phase(FieldObjectId id,
                               const FieldNodeBinding &binding,
                               FieldTreePhase p, float dt, std::string &e) {
  Instance *v = nullptr;
  if (!actual(id, v, e) || !v->bound || !same(binding, v->binding))
    return fail(e, "SceneScripts script phase typed binding rejected");
  const auto *d = tree_->descriptor(id);
  const auto *n = tree_->state(id);
  if (p == FieldTreePhase::ReadyScript) {
    if (v->ready || !n->inside || !n->ready_notified || n->ready_first ||
        lifecycle_.ready_cursor() >= sources_->lifecycle().ready_count())
      return fail(e,
                  "SceneScripts Ready phase duplicate/native order rejected");
    auto next = sources_->lifecycle().ready(lifecycle_.ready_cursor());
    if (next.id != v->source.id || next.ordinal != v->source.ordinal ||
        next.sha != v->source.sha)
      return fail(
          e, "SceneScripts actual postorder cursor differs; no RNG consumed");
    if (next.role == FieldSceneRole::Grass) {
      FieldObjectId preview = 0;
      if (!grass_preload(e) || !grass_preview(id, next, preview, e))
        return false;
      if (!tree_->queue_free(preview, e)) {
        poisoned_ = true;
        return false;
      }
    }
    if (!lifecycle_.ready_next(e)) {
      poisoned_ = true;
      return false;
    }
    FieldSceneScriptAdmission receipt;
    if (!lifecycle_.script_admission(next.id, receipt) ||
        receipt.id != next.id || receipt.source_sha != next.sha) {
      poisoned_ = true;
      return fail(e, "SceneScripts typed source Ready receipt missing");
    }
    v->ready = true;
    e.clear();
    return true;
  }
  if (p == FieldTreePhase::EnterScript || p == FieldTreePhase::ExitScript) {
    const auto method = p == FieldTreePhase::EnterScript ? 2u : 4u;
    if (p == FieldTreePhase::ExitScript &&
        v->source.role == FieldSceneRole::DroppedItem &&
        (d->script_methods & method))
      return consumers_.dropped->exit_tree(v->source.id, e);
    if (d->script_methods & method)
      return fail(e, "SceneScripts actual source enter/exit method pending");
    e.clear();
    return true;
  }
  if (p == FieldTreePhase::VisibilityChanged &&
      v->source.role == FieldSceneRole::Npc && v->ready) {
    if (!consumers_.npc->visibility_changed(v->source.id)) {
      e = consumers_.npc->error();
      return false;
    }
    return true;
  }
  if (p == FieldTreePhase::Idle || p == FieldTreePhase::Physics) {
    if (!v->ready || !n->inside || !std::isfinite(dt) || dt < 0 || dt > 1)
      return fail(e, "SceneScripts actual process phase/delta rejected");
    if (p == FieldTreePhase::Idle &&
        v->source.role == FieldSceneRole::SpriteFetcher) {
      if (!consumers_.sprite->process_fetcher(v->source.id)) {
        e = consumers_.sprite->error();
        return false;
      }
      return true;
    }
    if (p == FieldTreePhase::Physics && v->source.role == FieldSceneRole::Npc) {
      if (!consumers_.npc->physics_step(v->source.id, dt)) {
        e = consumers_.npc->error();
        return false;
      }
      return true;
    }
    if (p == FieldTreePhase::Idle &&
        v->source.role == FieldSceneRole::JumpArea) {
      if (!consumers_.transitions->process_source(v->source.id, dt)) {
        e = consumers_.transitions->error();
        return false;
      }
      return true;
    }
    if (p == FieldTreePhase::Physics &&
        v->source.role == FieldSceneRole::Stairs) {
      if (!consumers_.transitions->physics_source(v->source.id, dt)) {
        e = consumers_.transitions->error();
        return false;
      }
      return true;
    }
    // These are ordinary source callbacks. Internal animation/timer leaves
    // retain the concrete native owner's sole clock, not a second idle_leaf.
    if (p == FieldTreePhase::Idle) {
      switch (v->source.role) {
      case FieldSceneRole::Butterfly:
        return consumers_.butterfly->idle_process(v->source.id, dt, true, e);
      case FieldSceneRole::CutsceneArea:
        return consumers_.cutscene->idle_process(v->source.id, true, e);
      case FieldSceneRole::Birds:
        if (!consumers_.birds->idle_process(v->source.id, dt, true)) {
          e = consumers_.birds->error();
          return false;
        }
        return true;
      case FieldSceneRole::DoorNpc:
        return consumers_.door_npc->idle_process(v->source.id, true, e);
      case FieldSceneRole::GameCamera:
        if (!consumers_.camera->idle(v->source.id, dt)) {
          e = consumers_.camera->error();
          return false;
        }
        return true;
      default:
        break;
      }
    }
    if (p == FieldTreePhase::Physics &&
        v->source.role == FieldSceneRole::GameCamera) {
      if (!consumers_.camera->physics(v->source.id, dt)) {
        e = consumers_.camera->error();
        return false;
      }
      return true;
    }
    if (!(d->script_methods & (p == FieldTreePhase::Idle ? 64u : 128u))) {
      e.clear();
      return true;
    }
    return fail(e, "SceneScripts actual source process implementation pending");
  }
  if (p == FieldTreePhase::Input &&
      v->source.role == FieldSceneRole::GameCamera) {
    if (!v->ready || !n->inside)
      return fail(e, "SceneScripts Camera input before actual Ready");
    if (!consumers_.camera->input(v->source.id)) {
      e = consumers_.camera->error();
      return false;
    }
    return true;
  }
  if (p == FieldTreePhase::Input || p == FieldTreePhase::UnhandledInput ||
      p == FieldTreePhase::UnhandledKeyInput)
    return fail(e, "SceneScripts actual source input method pending");
  return fail(e, "SceneScripts received a native phase; concrete native owner "
                 "must execute it");
}
bool PodunkSceneScripts::deferred(const FieldDeferredMessage &m,
                                  std::string &e) {
  Instance *v = nullptr;
  if (!actual(m.object, v, e))
    return false;
  // Business method names/opcodes are not present in the source tree pack.
  // The concrete Programme/Signal owner must dispatch their typed methods;
  // this lifecycle owner must not treat an unknown deferred call as delivered.
  return fail(e, "SceneScripts source deferred method/setter policy pending");
}
bool PodunkSceneScripts::release(FieldObjectId id, const FieldNodeBinding &b,
                                 std::string &e) {
  Instance *v = nullptr;
  if (!actual(id, v, e) || !same(b, v->binding))
    return false;
  const auto *n = tree_->state(id);
  if (n->inside || n->parent || !n->children.empty())
    return fail(e, "SceneScripts source release before actual native "
                   "detach/delete rejected");
  bool ok = true;
  switch (v->source.role) {
  case FieldSceneRole::Npc:
    ok = consumers_.npc->destroy(v->source.id);
    if (!ok)
      e = consumers_.npc->error();
    break;
  case FieldSceneRole::CharacterSprite:
  case FieldSceneRole::SpriteFetcher:
    ok = consumers_.sprite->destroy(v->source.id);
    if (!ok)
      e = consumers_.sprite->error();
    break;
  case FieldSceneRole::Emotes:
    ok = consumers_.emote->destroy(v->source.id);
    if (!ok)
      e = consumers_.emote->error();
    break;
  case FieldSceneRole::Tint:
    ok = consumers_.tint->destroy(v->source.id);
    if (!ok)
      e = consumers_.tint->error();
    break;
  case FieldSceneRole::MapArrows:
    ok = consumers_.arrows->exit_tree(v->source.id);
    if (!ok)
      e = consumers_.arrows->error();
    break;
  default:
    break;
  }
  if (!ok) {
    poisoned_ = true;
    return false;
  }
  v->released = true;
  e.clear();
  return true;
}
bool PodunkSceneScripts::collect_deleted(std::string &e) {
  if (!sources_ || poisoned_)
    return fail(e, "SceneScripts deleted collection unavailable");
  for (auto &pair : instances_) {
    auto &v = pair.second;
    if (!v.released || v.deleted)
      continue;
    if (tree_->state(pair.first) || registry_->object_exists(pair.first))
      return fail(e, "SceneScripts actual ObjectDB deletion not committed");
    if (!signals_->release(pair.first, e) ||
        !lifecycle_.commit_deleted(v.source.id, e))
      return false;
    v.deleted = true;
    objects_.erase(v.source.id);
  }
  e.clear();
  return true;
}
bool PodunkSceneScripts::object_for_source(uint32_t source, FieldObjectId &out,
                                           std::string &e) const {
  auto p = objects_.find(source);
  if (p == objects_.end() || !tree_ || !registry_->object_exists(p->second) ||
      registry_->tree_owner(p->second).get() != tree_)
    return fail(e, "SceneScripts actual source ObjectDB node unavailable");
  out = p->second;
  return true;
}
bool PodunkSceneScripts::admission(FieldObjectId id,
                                   FieldSceneScriptAdmission &out) const {
  auto p = instances_.find(id);
  return p != instances_.end() && p->second.ready && !p->second.released &&
         registry_->object_exists(id) &&
         lifecycle_.script_admission(p->second.source.id, out);
}
bool PodunkSceneScripts::grass_screen(FieldObjectId id, bool entered,
                                      std::string &e) {
  Instance *v = nullptr;
  if (!actual(id, v, e) || !v->ready ||
      v->source.role != FieldSceneRole::Grass || !grass_preload(e) ||
      !grass_dynamic_factory(e))
    return false;
  return entered ? grass_factory_->screen_entered(id, v->source.id, e)
                 : grass_factory_->screen_exited(id, v->source.id, e);
}
bool PodunkSceneScripts::npc_screen(FieldObjectId id, bool entered,
                                    std::string &e) {
  Instance *v = nullptr;
  if (!actual(id, v, e) || !v->ready || v->source.role != FieldSceneRole::Npc)
    return false;
  const bool ok = entered ? consumers_.npc->screen_entered(v->source.id)
                          : consumers_.npc->screen_exited(v->source.id);
  if (!ok)
    e = consumers_.npc->error();
  return ok;
}
bool PodunkSceneScripts::npc_interact(FieldObjectId id, bool telepathy,
                                      std::string &e) {
  Instance *v = nullptr;
  if (!actual(id, v, e) || !v->ready || v->source.role != FieldSceneRole::Npc)
    return false;
  const bool ok = telepathy ? consumers_.npc->telepathy(v->source.id)
                            : consumers_.npc->interact(v->source.id);
  if (!ok)
    e = consumers_.npc->error();
  return ok;
}
bool PodunkSceneScripts::transition_accept(FieldObjectId id, bool pressed,
                                           std::string &e) {
  Instance *v = nullptr;
  if (!actual(id, v, e) || !v->ready ||
      v->source.role != FieldSceneRole::JumpArea || !tree_->state(id)->inside)
    return fail(e, "SceneScripts actual Jump input owner/lifecycle rejected");
  if (pressed && !consumers_.transitions->accept_source(v->source.id)) {
    e = consumers_.transitions->error();
    return false;
  }
  e.clear();
  return true;
}
bool PodunkSceneScripts::transition_native_idle(FieldObjectId id, double dt,
                                                bool processing,
                                                std::string &e) {
  Instance *v = nullptr;
  if (!actual(id, v, e) || !v->ready ||
      v->source.role != FieldSceneRole::JumpArea || !tree_->state(id)->inside)
    return fail(
        e, "SceneScripts actual Jump native clock owner/lifecycle rejected");
  if (!consumers_.transitions->idle_native_source(v->source.id, dt,
                                                  processing)) {
    e = consumers_.transitions->error();
    return false;
  }
  e.clear();
  return true;
}
bool PodunkSceneScripts::landmark_recheck(FieldObjectId id, std::string &e) {
  Instance *v = nullptr;
  if (!actual(id, v, e) || !v->ready ||
      v->source.role != FieldSceneRole::FlagLandmark)
    return false;
  return lifecycle_.recheck_landmark(v->source.id, e);
}
} // namespace encore::ctr

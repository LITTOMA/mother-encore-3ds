#include "podunk_player_host.hpp"
#include <algorithm>
#include <cmath>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool identity(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.source_sha256 == b.source_sha256 &&
         a.upstream_commit == b.upstream_commit;
}
bool base_class(std::string_view c) {
  return c == "Node" || c == "Node2D" || c == "Position2D";
}
bool base_phase(FieldTreePhase p) {
  switch (p) {
  case FieldTreePhase::EnterNative:
  case FieldTreePhase::EnterScript:
  case FieldTreePhase::TreeEntered:
  case FieldTreePhase::NodeAdded:
  case FieldTreePhase::ChildEntered:
  case FieldTreePhase::PostEnterNative:
  case FieldTreePhase::TreeExiting:
  case FieldTreePhase::ExitNative:
  case FieldTreePhase::NodeRemoved:
  case FieldTreePhase::ChildExiting:
  case FieldTreePhase::TreeExited:
  case FieldTreePhase::VisibilityChanged:
  case FieldTreePhase::Hide:
  case FieldTreePhase::TransformChanged:
  case FieldTreePhase::LocalTransformChanged:
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
  case FieldTreePhase::ChildMoved:
  case FieldTreePhase::PathChanged:
  case FieldTreePhase::ReadySignal:
    return true;
  default:
    return false;
  }
}
} // namespace
const FieldGlobalRegistry *PodunkPlayerHost::registry() const {
  return services_.registry;
}
const FieldNodeTreeRuntime *PodunkPlayerHost::tree() const {
  return tree_.get();
}
bool PodunkPlayerHost::live(std::string &e) const {
  return prepared_ && !poisoned_ && tree_ && services_.registry
             ? true
             : fail(e, "Player owning host not prepared or poisoned");
}
bool PodunkPlayerHost::source_candidate(uint32_t id) const {
  return sources_.initialization &&
         sources_.initialization->recipe().record(id);
}
bool PodunkPlayerHost::owns(FieldObjectId id) const {
  return objects_.count(id) && services_.registry &&
         services_.registry->tree_owner(id).get() == tree_.get();
}
bool PodunkPlayerHost::native_world(std::string_view c) const {
  return c == "Area2D" || c == "CollisionShape2D" ||
         c == "CollisionPolygon2D" || c == "Camera2D";
}
bool PodunkPlayerHost::prepare(PodunkPlayerSources s, PodunkPlayerServices h,
                               std::shared_ptr<FieldNodeTreeRuntime> t,
                               const char *root, std::string &e) {
  if (prepared_ || !s.initialization || !s.ready || !s.motion || !s.visual ||
      !s.graphics || !s.fetchers || !s.children || !s.effects || !s.resources ||
      !s.initialization->valid() || !s.ready->valid() || !s.motion->valid() ||
      !s.visual->valid() || !s.graphics->valid() || !s.fetchers->valid() ||
      !s.children->valid() || !s.effects->valid() || !s.resources->valid() ||
      !h.registry || !h.global || !h.characters || !h.character_data ||
      !h.random || !h.audio || !h.map || !h.geometry || !t || !root ||
      h.registry->tree_owner(t->root()).get() != t.get())
    return fail(e, "Player assembly checked sources/live currentScene missing");
  if (h.world &&
      (h.world->tree() != t.get() || h.world->registry() != h.registry))
    return fail(e, "Player native world belongs to another ObjectDB/tree");
  sources_ = std::move(s);
  services_ = std::move(h);
  tree_ = std::move(t);
  if (!resources_.load(*sources_.resources, root, *services_.registry, e) ||
      !visual_.load(sources_.initialization, sources_.visual, sources_.graphics,
                    root, e) ||
      !timer_data_.load_player(*sources_.initialization, e) ||
      !timers_.initialize(
          timer_data_,
          [this](FieldObjectId id, std::string &x) { return timeout(id, x); },
          [this](FieldObjectId id) {
            return services_.registry->tree_owner(id).get();
          },
          e) ||
      !character_.initialize(*services_.characters, *services_.character_data,
                             *sources_.ready, *services_.registry, e))
    return false;
  if (services_.statuses &&
      !character_.bind_status_effects(*services_.statuses, e))
    return false;
  std::vector<uint32_t> sprites, audio;
  for (const auto &r : sources_.initialization->recipe().records()) {
    if (r.native_class == "AnimatedSprite" &&
        r.id != sources_.visual->shadow().id)
      sprites.push_back(r.id);
    if (r.native_class == "AudioStreamPlayer")
      audio.push_back(r.id);
  }
  // AudioServer, bus layout and global signal ownership are supplied by the
  // actual process owner. Our animation-start connections remain ordered here.
  auto mh = services_.audio_server;
  const auto external_emit = mh.emit;
  if (!external_emit)
    return fail(e, "Player AudioServer actual signal dispatcher missing");
  mh.emit = [this, external_emit](FieldObjectId id, std::string_view name,
                                  std::string &x) {
    return signal(id, name, {}, x) && external_emit(id, name, x);
  };
  if (!media_.prepare(*sources_.initialization, resources_, *tree_,
                      *services_.registry, *services_.audio, std::move(mh),
                      sprites, audio, e))
    return false;
  if (services_.effect_owners &&
      (!effects_.initialize(sources_.effects, sources_.initialization,
                            *services_.registry, *services_.random,
                            *services_.effect_owners, e) ||
       !effects_.load_preloads(e)))
    return false;
  prepared_ = true;
  return true;
}
bool PodunkPlayerHost::construct_source(FieldObjectId id,
                                        const FieldNodeDescriptor &d,
                                        const FieldIdentity &i,
                                        std::string &e) {
  if (!live(e) || !building_ ||
      !identity(i, sources_.initialization->recipe().identity()) ||
      !source_candidate(d.id) || !services_.registry->object_exists(id) ||
      services_.registry->tree_owner(id).get() != tree_.get())
    return fail(e,
                "Player source attachment is not the actual published factory");
  const auto *expected = sources_.initialization->recipe().record(d.id);
  const auto *state = tree_->state(id);
  if (!state || state->inside || state->parent || !state->name.empty() ||
      !expected || expected->native_class != d.native_class ||
      expected->script != d.script || expected->script_sha != d.script_sha ||
      constructed_.count(id))
    return fail(e, "Player source constructor cursor/order rejected");
  objects_.emplace(id, d.id);
  if (native_world(d.native_class) &&
      (!services_.world ||
       !services_.world->construct(id, d, *sources_.initialization, e)))
    return fail(e, "Player actual native physics/viewport constructor pending");
  if (d.id == sources_.initialization->recipe().identity().scene_id) {
    if (!body_.construct(
            *sources_.initialization, *tree_, id, *services_.characters,
            *services_.registry,
            [this](const PlayerInitializationField &f, FieldObjectId &out,
                   std::string &x) { return resources_.resolve(f, out, x); },
            e))
      return false;
    for (const auto &f : sources_.fetchers->records()) {
      auto owner = std::make_unique<PlayerFetcherRuntime>();
      if (!owner->prepare(*sources_.fetchers, *sources_.initialization, *tree_,
                          *services_.registry, *services_.global, *this, e))
        return false;
      fetchers_.emplace(f.id, std::move(owner));
    }
    auto fetcher = fetchers_.find(sources_.visual->bat().fetcher);
    if (fetcher == fetchers_.end() ||
        !visual_.prepare(*tree_, *services_.registry, *services_.global, body_,
                         *fetcher->second, e) ||
        !children_.prepare(*sources_.children, *sources_.initialization,
                           *sources_.ready, *tree_, *services_.registry, body_,
                           *this, e))
      return false;
  } else if (visual_.owns_source(d.id)) {
    if (!visual_.construct(id, d, e))
      return false;
  } else if (fetchers_.count(d.id)) {
    if (!fetchers_.at(d.id)->construct(id, d, e))
      return false;
  } else {
    bool child = false, effect = false;
    for (const auto &r : sources_.children->records())
      child |= r.id == d.id;
    for (const auto &r : sources_.effects->creators())
      effect |= r.id == d.id;
    if (child) {
      if (!children_.construct(id, d, e))
        return false;
    } else if (!d.script.empty() && !effect)
      return fail(e, "Player script attachment has no actual source consumer");
  }
  if (d.native_class == "Timer") {
    FieldNodeBinding timer;
    if (!(effect_timer(id) ? effects_.bind_timer(*tree_, id, timer, e)
                           : timers_.attach(*tree_, id, timer, e)))
      return false;
  } else if ((d.native_class == "AnimatedSprite" &&
              !visual_.owns_source(d.id)) ||
             d.native_class == "AudioStreamPlayer") {
    if (!media_.construct(id, e))
      return false;
  } else if (!native_world(d.native_class) && !base_class(d.native_class) &&
             d.native_class != "Sprite" && d.native_class != "AnimatedSprite" &&
             d.native_class != "AnimationPlayer" &&
             d.native_class != "AnimationTree" &&
             d.native_class != "KinematicBody2D" &&
             d.native_class != "RayCast2D")
    return fail(e, "Player native source subclass not implemented");
  constructed_.insert(id);
  return true;
}
bool PodunkPlayerHost::actual(uint32_t source, FieldObjectId &out,
                              std::string &e) const {
  const auto *r = sources_.initialization->recipe().record(source);
  FieldIdentity i;
  if (!r || !body_.constructed() ||
      !tree_->get_node(body_.object(), r->path, out, e) || !owns(out) ||
      !tree_->object_identity(out, i) ||
      !identity(i, sources_.initialization->recipe().identity()))
    return fail(e, "Player relative dynamic instance/source identity rejected");
  const auto *d = tree_->descriptor(out);
  return d && d->id == source && d->native_class == r->native_class &&
                 d->script_sha == r->script_sha
             ? true
             : fail(e, "Player dynamic relative descriptor mismatch");
}
bool PodunkPlayerHost::instantiate(const PlayerInitializationData &d,
                                   std::shared_ptr<FieldNodeTreeRuntime> &out,
                                   FieldObjectId &player, std::string &e) {
  if (!live(e) || building_ || assembled_ || body_.constructed() ||
      &d != sources_.initialization.get())
    return fail(e, "Player factory repeated or wrong checked source bundle");
  building_ = true;
  FieldObjectId actual_player = 0;
  if (!tree_->instantiate_recipe(d.recipe(), actual_player, e) ||
      actual_player != body_.object() ||
      constructed_.size() != d.recipe().records().size() ||
      !finish_factory(e)) {
    poisoned_ = true;
    building_ = false;
    return false;
  }
  building_ = false;
  assembled_ = true;
  out = tree_;
  player = actual_player;
  return true;
}
bool PodunkPlayerHost::finish_factory(std::string &e) {
  for (const auto &r : sources_.initialization->recipe().records()) {
    FieldObjectId id = 0;
    if (!actual(r.id, id, e) || !constructed_.count(id))
      return false;
  }
  if (!animation_.construct(*sources_.initialization, *sources_.ready, *tree_,
                            *services_.registry, body_.object(), *this, e) ||
      !PodunkPlayerPlayback::construct(sources_.initialization, sources_.ready,
                                       *services_.registry, playback_id_,
                                       playback_, e) ||
      !playback_->bind_tracks(animation_.graph_host(), e))
    return false;
  for (auto &f : fetchers_)
    if (!f.second->apply_exports(e))
      return false;
  if (!children_.connect_packed_signals(e) ||
      !arrow_native_.prepare(*sources_.children, *sources_.initialization,
                             *tree_, *services_.registry, body_.object(),
                             animation_, media_, e) ||
      !children_.bind_camera(*services_.random, services_.camera, e) ||
      !children_.bind_arrows(services_.arrows, arrow_native_, e))
    return false;
  PlayerReadyHost ready = services_.ready;
  character_.bind(ready);
  ready.resolve_resource = [this](const GlobalYamlValue &r, FieldObjectId &out,
                                  std::string &x) {
    return resolve_onready(r, out, x);
  };
  ready.animation_active = [this](FieldObjectId id, bool value,
                                  std::string &x) {
    FieldObjectId expected = 0;
    if (!tree_->get_node(body_.object(),
                         sources_.ready->binding(PlayerReadyBinding::TreePath),
                         expected, x) ||
        expected != id || !playback_->graph().set_active(value, x))
      return false;
    const auto group = sources_.ready->process_mode()
                           ? "idle_process_internal"
                           : "physics_process_internal";
    return value ? tree_->add_group(id, group, x)
                 : tree_->remove_group(id, group, x);
  };
  ready.sprite_playing = [this](FieldObjectId id, bool v, std::string &x) {
    return animated_playing(id, v, x);
  };
  ready.sprite_frame = [this](FieldObjectId id, uint32_t v, std::string &x) {
    if (media_.owns(id))
      return media_.set_frame(id, v, x);
    return animation_.sprite_frame(id, v, x);
  };
  ready.resource_exists = [this](std::string_view p, bool &v, std::string &x) {
    return resources_.resource_exists(p, v, x);
  };
  ready.texture_path = [this](FieldObjectId id, std::string &p,
                              std::string &x) {
    auto *s = animation_.sprite(id);
    return s ? resources_.source_path(s->texture, p, x)
             : fail(x, "Player source Sprite texture owner absent");
  };
  ready.load_texture = [this](FieldObjectId id, std::string_view p,
                              std::string &x) {
    FieldObjectId resource = 0;
    if (!resources_.load_texture(p, resource, x))
      return false;
    for (const auto &r : sources_.resources->resources())
      if (r.kind == 1 && r.source == p)
        return animation_.sprite_texture(id, r.id, x);
    return fail(x,
                "Player requested texture is outside checked resource closure");
  };
  ready.texture_height = [this](FieldObjectId id, uint32_t &v, std::string &x) {
    auto *s = animation_.sprite(id);
    return s ? resources_.height(s->texture, v, x)
             : fail(x, "Player source Sprite height owner absent");
  };
  ready.sprite_offset_y = [this](FieldObjectId id, float y, std::string &x) {
    auto *s = animation_.sprite(id);
    return s ? animation_.sprite_offset(id, {s->offset.x, y}, x)
             : fail(x, "Player source Sprite offset owner absent");
  };
  ready.shadow_animation = [this](FieldObjectId id, std::string_view p,
                                  std::string &x) {
    auto *s = visual_.native(id);
    return s ? s->play(p, x) : fail(x, "Player source Shadow owner absent");
  };
  if (!ready_.initialize(*sources_.ready, body_, *tree_, *services_.global,
                         *services_.characters, playback_->graph(),
                         std::move(ready), e) ||
      !kinematic_.initialize(*sources_.initialization, *sources_.motion, body_,
                             *tree_, *services_.registry, *services_.map,
                             *services_.geometry, services_.kinematic, e))
    return false;
  auto motion = services_.motion;
  motion.move_and_slide = [this](FieldObjectId id, Vec2 v, Vec2 &out,
                                 std::string &x) {
    return kinematic_.move_and_slide(id, v, out, x);
  };
  motion.ray_rotation = [this](FieldObjectId id, float v, std::string &x) {
    return kinematic_.ray_rotation(id, v, x);
  };
  motion.cached_ray = [this](FieldObjectId id, FieldObjectId &v,
                             std::string &x) {
    return kinematic_.cached_ray(id, v, x);
  };
  motion.timer_left = [this](FieldObjectId id, double &v, std::string &x) {
    if (!timers_.state(id))
      return fail(x, "Player source Timer owner absent");
    v = timers_.time_left(id);
    return true;
  };
  motion.timer_start = [this](FieldObjectId id, double v, std::string &x) {
    if (!std::isfinite(v) || !std::isfinite(float(v)))
      return fail(x, "Player Timer duration overflow");
    return timers_.start(id, float(v), x);
  };
  motion.timer_stop = [this](FieldObjectId id, std::string &x) {
    return timers_.stop(id, x);
  };
  motion.timer_wait = [this](FieldObjectId id, double v, std::string &x) {
    if (!std::isfinite(v) || !std::isfinite(float(v)))
      return fail(x, "Player Timer wait overflow");
    return timers_.set_wait(id, float(v), x);
  };
  motion.animation_play = [this](FieldObjectId id, std::string_view name,
                                 std::string &x) {
    return animation_.play(id, name, x);
  };
  motion.animation_current = [this](FieldObjectId id, std::string &v,
                                    std::string &x) {
    return animation_.assigned(id, v, x);
  };
  motion.incapacitated = [this](FieldObjectId id, bool &v, std::string &x) {
    return character_.is_incapacitated(id, v, x);
  };
  motion.shape_disabled = [this](FieldObjectId id, bool v, std::string &x) {
    return disabled(id, v, x);
  };
  return motion_.initialize(*sources_.motion, body_, ready_, playback_->graph(),
                            *tree_, *services_.global, *services_.registry,
                            *services_.random, std::move(motion), e);
}
bool PodunkPlayerHost::resolve_onready(const GlobalYamlValue &row,
                                       FieldObjectId &out, std::string &e) {
  const auto c = row.get("native"), k = row.get("kind"),
             p = row.get("property");
  if (c && c->kind == 4 && c->string == "AnimationNodeStateMachinePlayback" &&
      k && k->kind == 2 && k->integer == 3 && p && p->kind == 4 &&
      p->string == "parameters/playback" && playback_ &&
      playback_->tracks_bound()) {
    out = playback_id_;
    return true;
  }
  if (!services_.ready.resolve_resource)
    return fail(e, "Player onready PackedScene actual ResourceLoader pending");
  if (!services_.ready.resolve_resource(row, out, e) ||
      !services_.registry->source_resource(out))
    return fail(e, "Player onready resource has no actual Registry owner");
  return true;
}
bool PodunkPlayerHost::bind(FieldObjectId id, const FieldNodeDescriptor &d,
                            FieldNodeBinding &out, std::string &e) {
  if (!live(e) || !owns(id) || !assembled_ || !constructed_.count(id) ||
      objects_.at(id) != d.id)
    return fail(e, "Player typed bind requires full actual factory");
  FieldIdentity i;
  if (!tree_->object_identity(id, i) ||
      !identity(i, sources_.initialization->recipe().identity()))
    return fail(e, "Player typed bind source instance identity mismatch");
  out = {i, d.id, d.class_index, 0x454e0056u, 1, d.script_sha, d.native_class};
  return true;
}
bool PodunkPlayerHost::begin_frame(uint64_t epoch, float idle, float physics,
                                   bool paused, bool update, std::string &e) {
  if (!live(e) || !assembled_ || !epoch || epoch <= epoch_ ||
      !std::isfinite(idle) || !std::isfinite(physics) || idle < 0 ||
      physics < 0)
    return fail(e, "Player actual SceneTree frame cursor rejected");
  epoch_ = epoch;
  idle_delta_ = idle;
  physics_delta_ = physics;
  paused_ = paused;
  update_pending_ = update;
  return kinematic_.begin_physics(epoch, physics, e) &&
         media_.tree_pause(paused, e);
}
bool PodunkPlayerHost::script_phase(FieldObjectId id, FieldTreePhase p,
                                    const FieldNodeBinding &b, std::string &e) {
  const auto *d = tree_->descriptor(id);
  if (!d)
    return fail(e, "Player actual source script descriptor missing");
  if (d->script.empty())
    return true;
  if (id == body_.object()) {
    if (p == FieldTreePhase::ReadyScript)
      return ready_.ready(p, b, e);
    if (p == FieldTreePhase::Physics)
      return motion_.physics(physics_delta_, b, e);
    if (p == FieldTreePhase::Input)
      return input_event_
                 ? motion_.input(*input_event_, e)
                 : fail(e, "Player source Input has no actual Viewport event");
    if (p == FieldTreePhase::EnterScript || p == FieldTreePhase::ExitScript)
      return true;
    return fail(e,
                "Player root script notification requires typed source input");
  }
  if (visual_.owns_source(d->id)) {
    if (p == FieldTreePhase::ReadyScript ||
        (p == FieldTreePhase::Idle && d->id == sources_.visual->bat().id))
      return visual_.phase(id, p, idle_delta_, paused_, update_pending_, e);
    if (p == FieldTreePhase::EnterScript || p == FieldTreePhase::ExitScript)
      return true;
    return fail(e, "Player visual script notification unsupported");
  }
  if (fetchers_.count(d->id)) {
    auto &f = *fetchers_.at(d->id);
    if (p == FieldTreePhase::ReadyScript)
      return f.ready(p, b, e);
    if (p == FieldTreePhase::Idle)
      return f.process(p, paused_, e);
    if (p == FieldTreePhase::EnterScript || p == FieldTreePhase::ExitScript)
      return true;
    return fail(e, "Player fetcher script notification unsupported");
  }
  for (const auto &r : sources_.children->records())
    if (r.id == d->id) {
      if (p == FieldTreePhase::ReadyScript)
        return children_.ready(id, p, b, e);
      if (p == FieldTreePhase::ExitScript)
        return children_.exit(id, e);
      if (p == FieldTreePhase::EnterScript)
        return true;
      return children_.process(
          id, p, p == FieldTreePhase::Physics ? physics_delta_ : idle_delta_,
          paused_, e);
    }
  for (const auto &r : sources_.effects->creators())
    if (r.id == d->id) {
      if (p == FieldTreePhase::ReadyScript)
        return services_.effect_owners
                   ? effects_.ready(body_.object(), id, e)
                   : fail(e, "Player actual effect PackedScene owner pending");
      if (p == FieldTreePhase::EnterScript || p == FieldTreePhase::ExitScript)
        return true;
      return fail(e, "Player creator script notification unsupported");
    }
  return fail(e, "Player source script has no dispatch owner");
}
bool PodunkPlayerHost::phase(FieldObjectId id, const FieldNodeBinding &b,
                             FieldTreePhase p, std::string &e) {
  if (!live(e) || !assembled_ || !owns(id) || b.stable_id != objects_.at(id) ||
      !identity(b.identity, sources_.initialization->recipe().identity()))
    return fail(e, "Player phase crossed actual source instance");
  const auto *d = tree_->descriptor(id);
  if (!d || d->native_class != b.native_class || d->script_sha != b.script_sha)
    return fail(e, "Player source native/script receipt mismatch");
  const bool media = media_.owns(id), world = native_world(d->native_class);
  if (d->native_class == "RayCast2D" && p == FieldTreePhase::EnterNative) {
    auto rows = sources_.initialization->native_source()->get("nodes");
    bool found = false;
    if (!rows || rows->kind != 5)
      return fail(e, "Player native ray source nodes missing");
    for (const auto &row : rows->array) {
      auto path = row->get("path");
      if (!path || path->kind != 4 || path->string != d->path)
        continue;
      auto properties = row->get("properties");
      auto enabled = properties ? properties->get("enabled") : nullptr;
      if (!enabled || enabled->kind != 1)
        return fail(e, "Player native ray enabled source type invalid");
      if (enabled->boolean &&
          !tree_->add_group(id, "physics_process_internal", e))
        return false;
      found = true;
      break;
    }
    if (!found)
      return fail(e, "Player native ray actual source row missing");
  }
  if (d->native_class == "RayCast2D" && p == FieldTreePhase::ExitNative &&
      !tree_->remove_group(id, "physics_process_internal", e))
    return false;
  if (p == FieldTreePhase::Deleting)
    return release(id, e);
  if (p == FieldTreePhase::ReadySignal || p == FieldTreePhase::TreeEntered ||
      p == FieldTreePhase::TreeExiting || p == FieldTreePhase::TreeExited) {
    const char *name = p == FieldTreePhase::ReadySignal   ? "ready"
                       : p == FieldTreePhase::TreeEntered ? "tree_entered"
                       : p == FieldTreePhase::TreeExiting ? "tree_exiting"
                                                          : "tree_exited";
    return services_.audio_server.emit
               ? services_.audio_server.emit(id, name, e)
               : fail(e, "Player actual Node signal dispatcher missing");
  }
  if (p == FieldTreePhase::ReadyScript || p == FieldTreePhase::EnterScript ||
      p == FieldTreePhase::ExitScript || p == FieldTreePhase::Input ||
      p == FieldTreePhase::Idle || p == FieldTreePhase::Physics)
    return script_phase(id, p, b, e);
  if (world && !services_.world->phase(id, p,
                                       p == FieldTreePhase::PhysicsInternal
                                           ? physics_delta_
                                           : idle_delta_,
                                       paused_, e))
    return false;
  if (media && !media_.phase(id, p, idle_delta_, paused_, update_pending_, e))
    return false;
  if (p == FieldTreePhase::ReadyNative) {
    if (d->native_class == "Timer" &&
        !(effect_timer(id) ? effects_.native_timer_ready(id, e)
                           : timers_.ready(id, e)))
      return false;
    if (d->native_class == "AnimationPlayer" && !animation_.ready(id, p, b, e))
      return false;
    if (d->native_class == "Sprite" && !visual_.native(id) &&
        !animation_.sprite(id))
      return fail(e, "Player native Sprite actual property owner missing");
    if (d->native_class == "AnimationTree" &&
        (!playback_ || !playback_->tracks_bound()))
      return fail(e, "Player native AnimationTree actual graph missing");
    native_ready_.insert(id);
    return true;
  }
  if (p == FieldTreePhase::IdleInternal ||
      p == FieldTreePhase::PhysicsInternal) {
    if (media)
      return true;
    if (d->native_class == "Timer") {
      const float dt =
          p == FieldTreePhase::PhysicsInternal ? physics_delta_ : idle_delta_;
      return effect_timer(id) ? effects_.timer_process(id, p, dt, paused_, e)
                              : timers_.process(id, p, dt, paused_, e);
    }
    if (d->native_class == "AnimationPlayer")
      return animation_.process(id, p, idle_delta_, paused_, e);
    if (d->native_class == "AnimationTree") {
      const auto clock = sources_.ready->process_mode()
                             ? FieldTreePhase::IdleInternal
                             : FieldTreePhase::PhysicsInternal;
      if (p != clock || !tree_->can_process(id, paused_))
        return true;
      return playback_->graph().process(
          p == FieldTreePhase::IdleInternal ? idle_delta_ : physics_delta_, e);
    }
    if (d->id == sources_.visual->shadow().id)
      return p == FieldTreePhase::IdleInternal
                 ? visual_.phase(id, p, idle_delta_, paused_, update_pending_,
                                 e)
                 : fail(e, "Player Shadow wrong native clock");
    if (d->native_class == "RayCast2D")
      return p == FieldTreePhase::PhysicsInternal
                 ? kinematic_.ray_physics(id, epoch_, e)
                 : fail(e, "Player ray wrong native clock");
    return fail(e, "Player unexpected internal notification owner");
  }
  return base_phase(p) ? true
                       : fail(e, "Player unimplemented native notification");
}
bool PodunkPlayerHost::input(const PlayerInputEvent &v, std::string &e) {
  if (!live(e) || !ready_complete() || input_event_)
    return fail(e, "Player Viewport input owner/cursor unavailable");
  input_event_ = &v;
  const bool result = tree_->dispatch_input(body_.object(), 0, paused_, e);
  input_event_ = nullptr;
  return result;
}
bool PodunkPlayerHost::timeout(FieldObjectId id, std::string &e) {
  for (const auto &r : sources_.effects->creators()) {
    if (r.timer_path.empty())
      continue;
    FieldObjectId creator = 0, timer = 0;
    if (!actual(r.id, creator, e) ||
        !tree_->get_node(creator, r.timer_path, timer, e))
      return false;
    if (timer == id)
      return services_.effect_owners
                 ? effects_.timeout(id, e)
                 : fail(e, "Player effect timeout owner pending");
  }
  return motion_.timer_timeout(id, e);
}
bool PodunkPlayerHost::effect_timer(FieldObjectId id) const {
  const auto *d = tree_ ? tree_->descriptor(id) : nullptr;
  FieldIdentity i;
  return services_.effect_owners && d && tree_->object_identity(id, i) &&
         sources_.effects->timer_data().record(i, d->id);
}
bool PodunkPlayerHost::read(FieldObjectId id, PlayerFetcherSpriteState &out,
                            std::string &e) const {
  if (!live(e) || !owns(id))
    return fail(e, "Player Sprite read actual owner mismatch");
  if (auto *s = animation_.sprite(id)) {
    out = {s->texture, s->columns, s->rows, s->frame,
           tree_->visible_in_tree(id)};
    return true;
  }
  auto *v = const_cast<PodunkPlayerVisualBundle &>(visual_).native(id);
  PlayerVisualNativeState s;
  FieldObjectId texture = 0;
  if (!v || !v->state(s, e) || !s.constructed ||
      !const_cast<PodunkPlayerResources &>(resources_)
           .construct_resource(s.texture, 0, texture, e))
    return fail(e, "Player Sprite read native property owner absent");
  out = {texture, s.columns, s.rows, s.frame, tree_->visible_in_tree(id)};
  return true;
}
bool PodunkPlayerHost::sprite(FieldObjectId id, PlayerFetcherSpriteState &out,
                              std::string &e) const {
  return read(id, out, e);
}
bool PodunkPlayerHost::texture_height(FieldObjectId id, uint32_t &out,
                                      std::string &e) const {
  return resources_.height(id, out, e);
}
bool PodunkPlayerHost::connect_animation_started(
    FieldObjectId emitter, FieldObjectId receiver, std::string_view name,
    std::string_view method,
    std::function<bool(std::string_view, std::string &)> cb, std::string &e) {
  if (!owns(emitter) || !owns(receiver) || !cb ||
      receiver != children_.actual(1) ||
      name != sources_.children->emote().signal ||
      method != sources_.children->emote().method ||
      tree_->descriptor(emitter)->native_class != "AnimationPlayer")
    return fail(e, "Player packed animation connection source mismatch");
  FieldObjectId source_emitter = 0;
  if (!tree_->get_node(receiver, sources_.children->emote().animation_path,
                       source_emitter, e) ||
      emitter != source_emitter || !connections_.empty())
    return fail(
        e, "Player original one-slot packed connection source path mismatch");
  for (const auto &c : connections_)
    if (c.emitter == emitter && c.receiver == receiver && c.signal == name &&
        c.method == method)
      return fail(e, "Player duplicate packed source connection");
  connections_.push_back({emitter, receiver, std::string(name),
                          std::string(method), std::move(cb)});
  return true;
}
bool PodunkPlayerHost::signal(FieldObjectId id, std::string_view name,
                              std::string_view clip, std::string &e) {
  if (!owns(id))
    return fail(e, "Player signal actual emitter mismatch");
  // This source has exactly one persistent local slot. Multiple slots are
  // rejected, so VMap target/method-pointer ordering is unambiguous. Other
  // owners and signal flags belong to the actual shared ObjectDB SignalBus.
  const auto connections = connections_;
  for (const auto &c : connections)
    if (c.emitter == id && c.signal == name &&
        services_.registry->object_exists(c.receiver) && !c.callback(clip, e))
      return false;
  const auto *d = tree_->descriptor(id);
  if (d && d->native_class == "AnimationPlayer")
    return services_.emit_animation
               ? services_.emit_animation(id, name, clip, e)
               : fail(e, "Player actual animation SignalBus owner pending");
  return true;
}
bool PodunkPlayerHost::emit_tint(FieldObjectId id, std::string_view name,
                                 const FieldColor &color, std::string &e) {
  if (id != children_.actual(2) || name != sources_.children->tint().signal)
    return fail(e, "Player tint signal actual source mismatch");
  // Tint's concrete script setter has already applied to the same target nodes.
  for (float v : color)
    if (!std::isfinite(v))
      return fail(e, "Player tint nonfinite");
  return services_.emit_tint
             ? services_.emit_tint(id, name, color, e)
             : fail(e, "Player global tint signal bus unavailable");
}
bool PodunkPlayerHost::connect_tint(FieldObjectId emitter,
                                    FieldObjectId receiver,
                                    std::string_view name,
                                    std::string_view method, std::string &e) {
  return services_.audio_server.connect
             ? services_.audio_server.connect(emitter, name, receiver, method,
                                              0, e)
             : fail(e, "Player tint actual signal bus unavailable");
}
bool PodunkPlayerHost::admit(FieldObjectId id, std::string_view member,
                             bool method, std::string &e) const {
  if (!owns(id))
    return fail(e, "Player animation target actual owner missing");
  if (media_.owns(id))
    return media_.admit(id, member, method, e);
  const auto *d = tree_->descriptor(id);
  if (!method && d &&
      (d->native_class == "CollisionShape2D" ||
       d->native_class == "CollisionPolygon2D") &&
      member == "disabled" && services_.world)
    return true;
  if (!method && d && d->native_class == "Sprite" &&
      member.rfind("material:shader_param/", 0) == 0)
    return true; // The actual parameter type/value is checked by the Resource
                 // owner on assignment.
  if (!method && d && visual_.owns_source(d->id) &&
      (member == "frame" || member == "playing" || member == "offset"))
    return true;
  if (method && id == body_.object() &&
      (member ==
           sources_.motion->text(PlayerMotionText::AttackFinishedMethod) ||
       member == sources_.motion->text(PlayerMotionText::ShootMethod) ||
       member == sources_.motion->text(PlayerMotionText::CastMethod)))
    return true;
  return fail(e,
              "Player animation method/property concrete endpoint unavailable");
}
bool PodunkPlayerHost::disabled(FieldObjectId id, bool v, std::string &e) {
  const auto *d = owns(id) ? tree_->descriptor(id) : nullptr;
  if (!d || !services_.world || !services_.world->disabled(id, v, e))
    return fail(e, "Player actual shape owner pending");
  // The root polygon and the Kinematic solver own the same actual shape.
  // Bat's nested attack polygon belongs to a different native body.
  if (d->native_class == "CollisionPolygon2D" &&
      d->parent == sources_.initialization->recipe().identity().scene_id)
    return kinematic_.set_collider_disabled(v, e);
  return true;
}
bool PodunkPlayerHost::audio_playing(FieldObjectId id, bool v, std::string &e) {
  return media_.audio_playing(id, v, e);
}
bool PodunkPlayerHost::audio_stream(FieldObjectId id, uint32_t v,
                                    std::string &e) {
  return media_.audio_stream(id, v, e);
}
bool PodunkPlayerHost::animated_frame(FieldObjectId id, uint32_t v,
                                      std::string &e) {
  if (auto *s = visual_.native(id))
    return s->set_frame(v, e);
  return media_.set_frame(id, v, e);
}
bool PodunkPlayerHost::animated_playing(FieldObjectId id, bool v,
                                        std::string &e) {
  return media_.set_playing(id, v, e);
}
bool PodunkPlayerHost::native_offset(FieldObjectId id, Vec2 v, std::string &e) {
  return media_.set_offset(id, v, e);
}
bool PodunkPlayerHost::shader_number(FieldObjectId id, std::string_view p,
                                     double v, std::string &e) {
  const auto *s = animation_.sprite(id);
  if (!s || !std::isfinite(v) || !std::isfinite(float(v)))
    return fail(e, "Player ShaderMaterial actual Sprite/number invalid");
  return resources_.set_shader_parameter(s->material, p, 1, {float(v), 0, 0, 0},
                                         e);
}
bool PodunkPlayerHost::shader_color(FieldObjectId id, std::string_view p,
                                    FieldColor v, std::string &e) {
  const auto *s = animation_.sprite(id);
  return s ? resources_.set_shader_parameter(s->material, p, 2, v, e)
           : fail(e, "Player ShaderMaterial actual Sprite absent");
}
PodunkPlayerVisualNative *PodunkPlayerHost::visual(FieldObjectId id) {
  return visual_.native(id);
}
bool PodunkPlayerHost::texture(uint32_t id, FieldObjectId &out,
                               std::string &e) {
  return resources_.construct_resource(id, 0, out, e);
}
bool PodunkPlayerHost::material(uint32_t id, FieldObjectId &out,
                                std::string &e) {
  const auto *r = sources_.resources->resource(id);
  return r ? resources_.construct_resource(
                 id, r->instanced ? body_.object() : 0, out, e)
           : fail(e,
                  "Player Animation material outside checked resource closure");
}
bool PodunkPlayerHost::stream(uint32_t id, FieldObjectId &out, std::string &e) {
  return media_.stream(id, out, e);
}
bool PodunkPlayerHost::release(FieldObjectId id, std::string &e) {
  if (!owns(id))
    return fail(e, "Player release source owner mismatch");
  const auto *d = tree_->descriptor(id);
  if (media_.owns(id) && !media_.release(id, e))
    return false;
  if (d->native_class == "Timer" &&
      !(effect_timer(id) ? effects_.release_timer(id, e)
                         : timers_.release(id, e)))
    return false;
  if (native_world(d->native_class) && !services_.world->release(id, e))
    return false;
  for (const auto &r : sources_.children->records())
    if (r.id == d->id && !children_.deleting(id, e))
      return false;
  connections_.erase(std::remove_if(connections_.begin(), connections_.end(),
                                    [id](const Connection &c) {
                                      return c.emitter == id ||
                                             c.receiver == id;
                                    }),
                     connections_.end());
  native_ready_.erase(id);
  constructed_.erase(id);
  objects_.erase(id);
  return true;
}
bool PodunkPlayerHost::deferred(const FieldDeferredMessage &m, std::string &e) {
  if (!live(e) || !assembled_ || m.object != body_.object() ||
      m.kind != FieldDeferredKind::Call || !m.args.empty() ||
      !admit(m.object, m.member, true, e))
    return fail(e, "Player source deferred method not admitted");
  return motion_.native_callback(m.member, e);
}
bool PodunkPlayerHost::draw(FieldObjectId id, const FieldTransform &viewport,
                            bool snap, std::string &e) {
  if (!live(e) || !assembled_ || !owns(id))
    return fail(e, "Player compositor draw source instance mismatch");
  if (media_.owns(id))
    return media_.draw(id, viewport, snap, e);
  if (auto *v = visual_.native(id))
    return v->draw(viewport, snap, e);
  const auto *s = animation_.sprite(id);
  if (!s || !native_ready_.count(id))
    return fail(e, "Player Sprite actual GPU/property owner not Ready");
  if (!tree_->visible_in_tree(id))
    return true;
  C2D_Image image{};
  FieldTransform world;
  FieldColor color;
  if (!resources_.texture(s->texture, image, e) ||
      (s->material && !resources_.neutral_material(s->material, e)) ||
      !tree_->world_transform(id, world, e) ||
      !tree_->effective_color(id, color, e))
    return false;
  if (!image.tex || !image.subtex || Tex3DS_SubTextureRotated(image.subtex) ||
      !s->columns || !s->rows || s->frame >= uint64_t(s->columns) * s->rows ||
      image.subtex->width % s->columns || image.subtex->height % s->rows)
    return fail(e, "Player Sprite checked GPU grid rejected");
  auto transform = [](const FieldTransform &t, Vec2 v) {
    return Vec2{t[0].x * v.x + t[1].x * v.y + t[2].x,
                t[0].y * v.x + t[1].y * v.y + t[2].y};
  };
  Vec2 x = transform(viewport, world[0]), y = transform(viewport, world[1]),
       origin = transform(viewport, world[2]);
  x.x -= viewport[2].x;
  x.y -= viewport[2].y;
  y.x -= viewport[2].x;
  y.y -= viewport[2].y;
  float sx = std::hypot(x.x, x.y), sy = std::hypot(y.x, y.y);
  if (!sx || !sy)
    return true;
  if (std::abs(x.x * y.x + x.y * y.y) > 1e-5f * sx * sy)
    return fail(e, "Player source skew needs actual quad GPU endpoint");
  auto sub = *image.subtex;
  const float du = (sub.right - sub.left) / s->columns,
              dv = (sub.bottom - sub.top) / s->rows;
  sub.left += du * (s->frame % s->columns);
  sub.right = sub.left + du;
  sub.top += dv * (s->frame / s->columns);
  sub.bottom = sub.top + dv;
  sub.width /= s->columns;
  sub.height /= s->rows;
  if (s->flip_h)
    std::swap(sub.left, sub.right);
  if (s->flip_v != (x.x * y.y - x.y * y.x < 0))
    std::swap(sub.top, sub.bottom);
  Vec2 offset = s->offset;
  if (s->centered) {
    offset.x -= sub.width / 2.f;
    offset.y -= sub.height / 2.f;
  }
  if (snap) {
    offset.x = std::floor(offset.x);
    offset.y = std::floor(offset.y);
  }
  const Vec2 center{offset.x + sub.width / 2.f, offset.y + sub.height / 2.f};
  origin.x += x.x * center.x + y.x * center.y;
  origin.y += x.y * center.x + y.y * center.y;
  C2D_ImageTint tint;
  C2D_PlainImageTint(&tint,
                     C2D_Color32f(color[0], color[1], color[2], color[3]), 0);
  image.subtex = &sub;
  C3D_TexSetFilter(image.tex, GPU_NEAREST, GPU_NEAREST);
  return C2D_DrawImageAtRotated(image, origin.x, origin.y, 0,
                                std::atan2(x.y, x.x), &tint, sx, sy)
             ? true
             : fail(e, "Player Sprite actual GPU submission failed");
}
} // namespace encore::ctr

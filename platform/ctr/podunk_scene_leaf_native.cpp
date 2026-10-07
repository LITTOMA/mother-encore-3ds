#include "podunk_scene_leaf_native.hpp"
#include <cmath>
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
} // namespace
bool PodunkSceneLeafNative::prepare(
    const SceneLeafNativeData &d, const SceneLeafNativeSources &s,
    FieldNodeTreeRuntime &t, FieldGlobalRegistry &r, FieldObjectSignals &bus,
    PodunkSceneNative &native, FieldGameCameraRuntime &cam,
    FieldPlayerTransitionsRuntime &jump, FieldBirdRuntime &birds,
    FieldDroppedRuntime &drop, FieldPayphoneRuntime &phone,
    FieldMelodyBackgroundRuntime &melody, std::string &e) {
  if (data_ || !d.valid() || !s.tree.valid() ||
      !same(d.identity(), s.tree.identity()) || r.poisoned() ||
      bus.registry() != &r ||
      (t.object_domain() && t.object_domain() != r.kernel()))
    return fail(e, "Scene leaf source/domain/actual owners rejected");
  data_ = &d;
  sources_ = std::make_unique<SceneLeafNativeSources>(s);
  tree_ = &t;
  registry_ = &r;
  signals_ = &bus;
  native_ = &native;
  camera_ = &cam;
  transitions_ = &jump;
  birds_ = &birds;
  dropped_ = &drop;
  phone_ = &phone;
  melody_ = &melody;
  e.clear();
  return true;
}
bool PodunkSceneLeafNative::apply(FieldGameCameraHost &c, FieldBirdHost &b,
                                  FieldPayphoneHost &p,
                                  FieldMelodyBackgroundHost &m,
                                  std::string &e) {
  if (!data_ || applied_)
    return fail(e, "Scene leaf callbacks already applied/unprepared");
  c.animation_signal = [this](uint32_t id, uint32_t role, bool started,
                              std::string &e) {
    const auto *r = data_->record(id);
    if (!r || r->kind != SceneLeafKind::CameraAnimation || role < 1 ||
        role > r->clips.size())
      return fail(e, "Camera actual AP clip role rejected");
    return event(r->kind, r->owner, r->clips[role - 1].name, started ? 1 : 2,
                 e);
  };
  b.animation_signal = [this](uint32_t id, bool start, FieldBirdClipRole role,
                              std::string &e) {
    const auto *r = data_->record(id);
    const auto *o = r ? sources_->birds.record(r->owner) : nullptr;
    const auto *c = o ? sources_->birds.clip(o->profile, role) : nullptr;
    if (!r || !c)
      return fail(e, "Bird actual AP source clip absent");
    return event(r->kind, r->owner, c->name, start ? 1 : 2, e);
  };
  p.play_idle = [this](uint32_t owner, uint32_t frame, std::string &e) {
    return play_phone(owner, frame, e);
  };
  m.animation_started = [this](uint32_t id, std::string_view clip,
                               std::string &e) {
    const auto *r = data_->record(id);
    return r ? event(r->kind, r->owner, clip, 1, e)
             : fail(e, "Melody actual AP source absent");
  };
  m.animation_stopped = [this](uint32_t id, std::string &e) {
    const auto *r = data_->record(id);
    return r ? event(r->kind, r->owner, {}, 3, e)
             : fail(e, "Melody actual AP source absent");
  };
  if (!transitions_->bind_native_animation(
          [this](uint32_t id, std::string &e) {
            const auto *r = data_->owner(SceneLeafKind::JumpAnimation, id);
            return r ? event(r->kind, id, r->active_clip, 1, e)
                     : fail(e, "Jump actual source AP absent");
          },
          e))
    return false;
  if (!dropped_->bind_native_leaves(
          [this](uint32_t stable, bool playing, std::string &e) {
            const auto *r = data_->record(stable);
            if (!r || (r->kind != SceneLeafKind::DroppedAnimation &&
                       r->kind != SceneLeafKind::DroppedTween))
              return fail(e, "Dropped native playback identity rejected");
            if (r->kind == SceneLeafKind::DroppedAnimation)
              return event(r->kind, r->owner, r->active_clip, playing ? 1 : 3,
                           e);
            auto id = tree_->source_object(stable);
            if (!actual(id, e))
              return false;
            auto &l = leaves_.at(id);
            l.playing = playing;
            return playing
                       ? tree_->add_group(id, data_->internal_group(), e)
                       : tree_->remove_group(id, data_->internal_group(), e);
          },
          e))
    return false;
  applied_ = true;
  e.clear();
  return true;
}
bool PodunkSceneLeafNative::owns(const FieldNodeDescriptor &n) const {
  auto *r = data_ ? data_->record(n.id) : nullptr;
  return r && (uint32_t(r->kind) <= 8 || uint32_t(r->kind) >= 11) &&
         n.native_class == r->native_class && n.script.empty();
}
bool PodunkSceneLeafNative::owns(FieldObjectId id) const {
  return leaves_.count(id) != 0;
}
bool PodunkSceneLeafNative::actual(FieldObjectId id, std::string &e) const {
  auto i = leaves_.find(id);
  const auto *d = tree_ ? tree_->descriptor(id) : nullptr;
  const auto *s = tree_ ? tree_->state(id) : nullptr;
  FieldIdentity identity;
  if (!data_ || i == leaves_.end() || !d || !s || !s->alive ||
      registry_->poisoned() || !registry_->object_exists(id) ||
      registry_->tree_owner(id).get() != tree_ ||
      !tree_->object_identity(id, identity) ||
      !same(identity, data_->identity()) || d->id != i->second.source->id ||
      !owns(*d))
    return fail(e, "Scene leaf actual ObjectDB/source identity rejected");
  return true;
}
bool PodunkSceneLeafNative::construct(FieldObjectId id,
                                      const FieldNodeDescriptor &n,
                                      const FieldIdentity &identity,
                                      std::string &e) {
  if (!owns(n) || leaves_.count(id) || !same(identity, data_->identity()) ||
      !registry_->object_exists(id) ||
      registry_->tree_owner(id).get() != tree_ || !tree_->descriptor(id) ||
      tree_->descriptor(id)->id != n.id)
    return fail(e, "Scene leaf actual native constructor rejected");
  Leaf l;
  l.source = data_->record(n.id);
  l.binding = {identity, n.id,         n.class_index, 0x454e0070,
               1,        n.script_sha, n.native_class};
  leaves_.emplace(id, std::move(l));
  e.clear();
  return true;
}
bool PodunkSceneLeafNative::bind(FieldObjectId id, FieldNodeBinding &out,
                                 std::string &e) {
  if (!actual(id, e))
    return false;
  out = leaves_.at(id).binding;
  return true;
}
bool PodunkSceneLeafNative::event(SceneLeafKind kind, uint32_t owner,
                                  std::string_view clip, uint32_t ev,
                                  std::string &e) {
  const auto *r = data_->owner(kind, owner);
  auto id = r ? tree_->source_object(r->id) : 0;
  if (!id || !actual(id, e) || (ev != 3 && !r->clip(clip)))
    return fail(e, "Scene leaf actual AP/play source rejected");
  auto &l = leaves_.at(id);
  if (ev == 1) {
    l.playing = true;
    if (!tree_->add_group(id, data_->internal_group(), e))
      return false;
    return signals_->emit(id, data_->started_signal(), {std::string(clip)}, e);
  }
  if (ev != 2 && ev != 3)
    return fail(e, "Scene leaf native event rejected");
  l.playing = false;
  if (!tree_->remove_group(id, data_->internal_group(), e))
    return false;
  return ev == 3 ||
         signals_->emit(id, data_->finished_signal(), {std::string(clip)}, e);
}
bool PodunkSceneLeafNative::play_phone(uint32_t owner, uint32_t frame,
                                       std::string &e) {
  const auto *r = data_->owner(SceneLeafKind::PayphoneAnimation, owner);
  const auto *d = sources_->payphone.record(owner);
  if (!r || !d || frame != sources_->payphone.idle_frame())
    return fail(e, "Payphone actual Idle frame/source rejected");
  const auto *c = r->clip(r->active_clip);
  if (!c || c->loop)
    return fail(e, "Payphone source idle clip missing");
  auto id = tree_->source_object(r->id),
       sprite = tree_->source_object(r->target);
  if (!actual(id, e) || !sprite || !native_->owns(sprite))
    return fail(e, "Payphone real Sprite/AP unavailable");
  auto &l = leaves_.at(id);
  if (!l.assigned || l.elapsed == c->length)
    l.elapsed = 0;
  l.assigned = true;
  return event(r->kind, owner, c->name, 1, e);
}
bool PodunkSceneLeafNative::phase(FieldObjectId id, FieldTreePhase p, float dt,
                                  bool paused, bool, std::string &e) {
  if (!actual(id, e))
    return false;
  auto &l = leaves_.at(id);
  const auto *n = tree_->state(id);
  switch (p) {
  case FieldTreePhase::EnterNative:
    if (l.entered || !n->inside)
      return fail(e, "Scene leaf Enter rejected");
    l.entered = true;
    return true;
  case FieldTreePhase::ReadyNative:
    if (!l.entered || !n->ready_notified || n->ready_first)
      return fail(e, "Scene leaf native Ready cursor rejected");
    l.ready = true;
    return true;
  case FieldTreePhase::IdleInternal: {
    if (!applied_ || !finished_ || !l.ready || !l.entered || !n->inside ||
        !tree_->can_process(id, paused) || !std::isfinite(dt) || dt < 0)
      return fail(e, "Scene leaf actual process owner/delta rejected");
    const auto &r = *l.source;
    switch (r.kind) {
    case SceneLeafKind::CameraAnimation:
      if (!camera_->animation_idle(r.id, dt)) {
        e = camera_->error();
        return false;
      }
      return true;
    case SceneLeafKind::BirdAnimation:
      if (!birds_->idle_leaf(r.id, dt, true)) {
        e = birds_->error();
        return false;
      }
      return true;
    case SceneLeafKind::DroppedAnimation:
    case SceneLeafKind::DroppedTween: {
      if (!dropped_->idle_node(r.id, dt, true, e))
        return false;
      const auto *s = dropped_->state(r.owner);
      auto sprite = tree_->source_object(r.target);
      if (!s || !sprite || !native_->owns(sprite))
        return fail(e, "Dropped actual Sprite owner absent");
      if (r.kind == SceneLeafKind::DroppedAnimation)
        return tree_->set_visible(sprite, s->sprite_visible, e);
      auto t = tree_->state(sprite)->local;
      float angle = std::atan2(t[0].y, t[0].x),
            rotate = s->sprite_rotation - angle, c = std::cos(rotate),
            sn = std::sin(rotate);
      for (unsigned i = 0; i < 2; ++i) {
        auto v = t[i];
        t[i] = {c * v.x - sn * v.y, sn * v.x + c * v.y};
      }
      return tree_->set_local(sprite, t, e);
    }
    case SceneLeafKind::JumpAnimation:
      if (!transitions_->animation_native_source(r.owner, dt)) {
        e = transitions_->error();
        return false;
      }
      return true;
    case SceneLeafKind::PayphoneAnimation: {
      const auto *c = r.clip(r.active_clip);
      if (!c || !l.playing)
        return c != nullptr;
      l.elapsed = std::min(l.elapsed + dt, c->length);
      auto sprite = tree_->source_object(r.target);
      if (!native_->sprite_set_frame(sprite, sources_->payphone.idle_frame(),
                                     e))
        return false;
      return l.elapsed == c->length ? event(r.kind, r.owner, c->name, 2, e)
                                    : true;
    }
    case SceneLeafKind::EmptyAnimation:
      return fail(
          e, "Empty source AP cannot be processing without a playable clip");
    case SceneLeafKind::MelodyAnimation:
      return melody_->animation_idle(r.owner, dt, true, e);
    default:
      return fail(e, "Scene leaf unsupported native clock");
    }
  }
  case FieldTreePhase::ExitNative:
    if (!l.entered)
      return fail(e, "Scene leaf Exit rejected");
    l.entered = false;
    return true;
  case FieldTreePhase::Deleting:
    return release(id, e);
  case FieldTreePhase::PostEnterNative:
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
  case FieldTreePhase::ChildMoved:
  case FieldTreePhase::PathChanged:
    return true;
  default:
    return fail(e, "Scene leaf native phase unsupported");
  }
}
bool PodunkSceneLeafNative::deferred(const FieldDeferredMessage &,
                                     std::string &e) {
  return fail(e, "Scene leaf unknown native method rejected");
}
bool PodunkSceneLeafNative::release(FieldObjectId id, std::string &e) {
  if (!actual(id, e) || leaves_.at(id).entered)
    return fail(e, "Scene leaf release before Exit rejected");
  leaves_.erase(id);
  return true;
}
bool PodunkSceneLeafNative::declaration(FieldObjectId id,
                                        std::string_view signal,
                                        uint32_t &arity, std::string &e) const {
  if (!actual(id, e) ||
      uint32_t(leaves_.at(id).source->kind) >
          uint32_t(SceneLeafKind::MelodyAnimation) ||
      (signal != data_->started_signal() && signal != data_->finished_signal()))
    return fail(e, "Scene leaf unknown signal rejected");
  arity = 1;
  return true;
}
bool PodunkSceneLeafNative::finish_factory(std::string &e) {
  if (!data_ || !applied_ || finished_)
    return fail(e, "Scene leaf factory state rejected");
  for (const auto &r : data_->records())
    if (uint32_t(r.kind) <= 8 || uint32_t(r.kind) >= 11) {
      auto id = tree_->source_object(r.id);
      if (!id || !actual(id, e) || !tree_->state(id)->bound)
        return fail(e, "Scene leaf actual factory closure incomplete");
    }
  finished_ = true;
  e.clear();
  return true;
}
bool PodunkSceneLeafNative::owns_drawable(FieldObjectId id) const {
  auto i = leaves_.find(id);
  return i != leaves_.end() &&
         i->second.source->kind == SceneLeafKind::MelodyRect;
}
bool PodunkSceneLeafNative::appearance(const FieldCanvasRecord &r,
                                       FieldObjectId id,
                                       FieldCanvasAppearance &out,
                                       std::string &e) const {
  if (!actual(id, e) || !owns_drawable(id) || !leaves_.at(id).ready ||
      !leaves_.at(id).entered || r.id != leaves_.at(id).source->id ||
      !native_->canvas_data() || native_->canvas_data()->record(r.id) != &r)
    return fail(e, "Melody actual TextureRect appearance owner rejected");
  const auto *s = melody_->state(leaves_.at(id).source->owner);
  if (!s || !s->ready || !s->alive)
    return fail(e, "Melody same typed source state unavailable");
  FieldCanvasAppearance p;
  p.action = FieldCanvasAction::Delegate;
  p.texture = r.texture;
  p.hframes = r.hframes;
  p.vframes = r.vframes;
  p.frame = r.frame;
  p.offset = r.offset;
  p.size = r.size;
  p.centered = r.centered;
  p.flip_h = r.flip_h;
  p.flip_v = r.flip_v;
  out = p;
  e.clear();
  return true;
}
bool PodunkSceneLeafNative::draw_leaf(const FieldCanvasOrderSlot &,
                                      const FieldTransform &, bool,
                                      std::string &e) {
  return fail(e, "Melody TextureRect draws through checked same-order material "
                 "compositor only");
}
} // namespace encore::ctr

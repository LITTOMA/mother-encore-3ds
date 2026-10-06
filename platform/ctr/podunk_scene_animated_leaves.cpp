#include "podunk_scene_animated_leaves.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "field_camera_arrows_renderer.hpp"
#include "field_sparkles_renderer.hpp"
#pragma GCC diagnostic pop
#include "field_canvas_art_renderer.hpp"
#include <fstream>
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
bool bytes(const std::string &p, const std::array<uint8_t, 32> &hash,
           std::string &e) {
  std::ifstream f(p, std::ios::binary);
  if (!f)
    return fail(e, "Native leaf GPU asset unavailable");
  f.seekg(0, std::ios::end);
  const auto size = f.tellg();
  if (size <= 0 || size > 16 * 1024 * 1024)
    return fail(e, "Native leaf GPU asset size rejected");
  f.seekg(0);
  std::vector<uint8_t> b(size_t(size), uint8_t(0));
  if (!f.read(reinterpret_cast<char *>(b.data()), b.size()) ||
      field_canvas_art_renderer_detail::sha256(b.data(), b.size()) != hash)
    return fail(e, "Native leaf actual GPU bytes differ from checked source");
  return true;
}
} // namespace
PodunkSceneAnimatedLeaves::PodunkSceneAnimatedLeaves() = default;
PodunkSceneAnimatedLeaves::~PodunkSceneAnimatedLeaves() = default;
bool PodunkSceneAnimatedLeaves::prepare(
    const FieldNodeTreeData &s, FieldNodeTreeRuntime &t, FieldGlobalRegistry &r,
    FieldObjectSignals &bus, const FieldCameraArrowsData &a,
    FieldCameraArrowsRuntime &ar, const FieldSparklesData &sp,
    FieldSparklesRuntime &spr, FieldPresentRuntime &p, FieldDroppedRuntime &d,
    const char *prefix, std::string &e) {
  if (source_ || !s.valid() || !a.valid() || !sp.valid() ||
      !same(s.identity(), a.identity()) || !same(s.identity(), sp.identity()) ||
      ar.data() != &a || ar.borrowed_native() || spr.data() != &sp ||
      !p.content() || !d.content() || bus.registry() != &r ||
      t.object_domain() != r.kernel() || r.poisoned() || !prefix)
    return fail(e, "Native animated leaves require same checked source "
                   "bodies/ObjectDB/GPU owner");
  if (!validate_sparkles_owner_bridge(sp, *p.content(), *d.content(), e) ||
      !bytes(std::string(prefix) + a.asset().path, a.asset().output_sha, e) ||
      !bytes(std::string(prefix) + a.program().path, a.program().output_sha,
             e) ||
      !bytes(std::string(prefix) + sp.texture_path(), sp.texture_sha(), e))
    return false;
  auto ag = std::make_unique<FieldCameraArrowsRenderer>();
  auto sg = std::make_unique<FieldSparklesRenderer>();
  if (!ag->load(a, prefix, e) || !sg->load(sp, prefix, e))
    return false;
  source_ = &s;
  tree_ = &t;
  registry_ = &r;
  signals_ = &bus;
  arrows_data_ = &a;
  arrows_ = &ar;
  sparkles_data_ = &sp;
  sparkles_ = &spr;
  present_ = &p;
  dropped_ = &d;
  arrows_gpu_ = std::move(ag);
  sparkles_gpu_ = std::move(sg);
  return true;
}
bool PodunkSceneAnimatedLeaves::actual(FieldObjectId id, std::string &e) const {
  const auto i = instances_.find(id);
  const auto *n = tree_ ? tree_->descriptor(id) : nullptr;
  const auto *s = tree_ ? tree_->state(id) : nullptr;
  FieldIdentity identity;
  if (!source_ || i == instances_.end() || !n || !s || !s->alive ||
      registry_->poisoned() || !registry_->object_exists(id) ||
      registry_->tree_owner(id).get() != tree_ ||
      !tree_->object_identity(id, identity) ||
      !same(identity, source_->identity()) ||
      n->id != i->second.binding.stable_id ||
      n->native_class != i->second.binding.native_class ||
      n->script_sha != i->second.binding.script_sha)
    return fail(e, "Native animated leaf actual live source object rejected");
  return true;
}
bool PodunkSceneAnimatedLeaves::resolve(uint32_t source, FieldObjectId &out,
                                        std::string &e) const {
  auto i = objects_.find(source);
  if (i == objects_.end() || !actual(i->second, e))
    return fail(e, "Native animated leaf source ObjectID unavailable");
  out = i->second;
  return true;
}
bool PodunkSceneAnimatedLeaves::construct(FieldObjectId id,
                                          const FieldNodeDescriptor &n,
                                          const FieldIdentity &identity,
                                          std::string &e) {
  if (!source_ || finished_ || instances_.count(id) || objects_.count(n.id) ||
      !same(identity, source_->identity()) ||
      registry_->tree_owner(id).get() != tree_ || !registry_->object_exists(id))
    return fail(e, "Native animated leaf construction owner rejected");
  const auto *original = source_->record(n.id);
  const auto *actual = tree_->descriptor(id);
  if (!original || !actual || actual->id != n.id ||
      original->native_class != n.native_class ||
      original->script_sha != n.script_sha || original->path != n.path)
    return fail(e, "Native animated leaf descriptor differs from source");
  Instance i;
  i.binding = {identity, n.id,         n.class_index, 0,
               3,        n.script_sha, n.native_class};
  if (arrows_data_->sprite(n.id) && n.native_class == "AnimatedSprite" &&
      n.script.empty()) {
    i.kind = Kind::Arrow;
    i.binding.family = 0x454e0030;
  } else if (arrows_data_->player(n.id) &&
             n.native_class == "AnimationPlayer" && n.script.empty()) {
    i.kind = Kind::Player;
    i.binding.family = 0x454e0030;
  } else if (sparkles_data_->record(n.id) &&
             n.native_class == "AnimatedSprite" &&
             n.script == sparkles_data_->script() &&
             n.script_sha == sparkles_data_->script_sha()) {
    i.kind = Kind::Sparkles;
    i.binding.family = 0x454e0021;
  } else
    return fail(e,
                "Native animated leaf has no checked concrete source consumer");
  instances_.emplace(id, i);
  objects_.emplace(n.id, id);
  return true;
}
bool PodunkSceneAnimatedLeaves::bind(FieldObjectId id, FieldNodeBinding &out,
                                     std::string &e) const {
  if (!actual(id, e))
    return false;
  out = instances_.at(id).binding;
  return true;
}
bool PodunkSceneAnimatedLeaves::finish_factory(std::string &e) {
  if (!source_ || finished_)
    return fail(e, "Native animated leaf factory state rejected");
  FieldObjectId out = 0;
  for (const auto &s : arrows_data_->sprites())
    if (!resolve(s.id, out, e))
      return false;
  for (const auto &p : arrows_data_->players())
    if (!resolve(p.id, out, e))
      return false;
  for (const auto &s : sparkles_data_->records())
    if (!resolve(s.id, out, e))
      return false;
  if (!present_->bind_sparkles_leaf_owner(*this, e) ||
      !dropped_->bind_sparkles_leaf_owner(*this, e))
    return false;
  finished_ = true;
  return true;
}
bool PodunkSceneAnimatedLeaves::owns(FieldObjectId id) const {
  return instances_.count(id) != 0;
}
bool PodunkSceneAnimatedLeaves::drawable(FieldObjectId id) const {
  const auto i = instances_.find(id);
  return i != instances_.end() && i->second.kind != Kind::Player;
}
bool PodunkSceneAnimatedLeaves::schedule(FieldObjectId id, bool enabled,
                                         std::string &e) {
  return enabled ? tree_->add_group(id, "idle_process_internal", e)
                 : tree_->remove_group(id, "idle_process_internal", e);
}
bool PodunkSceneAnimatedLeaves::phase(FieldObjectId id, FieldTreePhase p,
                                      float dt, bool paused,
                                      bool update_pending, std::string &e) {
  if (!actual(id, e))
    return false;
  auto &i = instances_.at(id);
  const auto *s = tree_->state(id);
  const auto source = i.binding.stable_id;
  switch (p) {
  case FieldTreePhase::EnterNative:
    if (!finished_ || !s->inside || i.entered)
      return fail(e, "Native animated leaf Enter order rejected");
    i.entered = true;
    return true;
  case FieldTreePhase::ReadyNative: {
    if (!i.entered || !s->inside)
      return fail(e, "Native animated leaf Ready outside real Tree");
    bool playing = false;
    if (i.kind == Kind::Arrow) {
      const auto *v = arrows_->sprite_state(source);
      if (!v || !v->alive)
        return fail(e, "Arrow native body was not source constructed");
      playing = v->playing;
    } else if (i.kind == Kind::Player) {
      const auto *v = arrows_->player_state(source);
      if (!v || !v->alive)
        return fail(e, "Arrow AnimationPlayer body was not source constructed");
      playing = v->playing;
    } else {
      FieldSparklesInstance v;
      if (!sparkles_->snapshot(source, v) || !v.ready)
        return fail(e, "Sparkles same source script Ready not completed");
      playing = v.playing;
    }
    if (!schedule(id, playing, e))
      return false;
    i.ready = true;
    return true;
  }
  case FieldTreePhase::IdleInternal:
    if (!i.ready || !i.entered || !s->inside || !std::isfinite(dt) || dt < 0 ||
        dt > 1)
      return fail(e, "Native animated leaf process order/delta rejected");
    if (!tree_->can_process(id, paused))
      return true;
    if (i.kind != Kind::Player && !update_pending)
      return true;
    if (i.kind != Kind::Sparkles) {
      if (!arrows_->idle_leaf(source, dt)) {
        e = arrows_->error();
        return false;
      }
      return true;
    }
    switch (sparkles_data_->record(source)->owner) {
    case FieldSparklesOwner::Self:
      if (!sparkles_->idle_frame(source, dt)) {
        e = sparkles_->error();
        return false;
      }
      return true;
    case FieldSparklesOwner::Present:
      return present_->idle_sparkles_leaf(source, dt, true, e);
    case FieldSparklesOwner::Dropped:
      return dropped_->idle_sparkles_leaf(source, dt, true, e);
    }
    return fail(e, "Unknown native Sparkles source owner");
  case FieldTreePhase::ExitNative:
    if (!i.entered)
      return fail(e, "Native animated leaf Exit before Enter");
    i.entered = false;
    return true;
  case FieldTreePhase::Deleting:
    if (i.entered)
      return fail(e, "Native animated leaf delete before Exit");
    if (!schedule(id, false, e))
      return false;
    objects_.erase(source);
    instances_.erase(id);
    return true;
  case FieldTreePhase::PostEnterNative:
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
  case FieldTreePhase::ChildMoved:
  case FieldTreePhase::PathChanged:
  case FieldTreePhase::VisibilityChanged:
  case FieldTreePhase::Hide:
  case FieldTreePhase::TransformChanged:
  case FieldTreePhase::LocalTransformChanged:
    return true;
  default:
    return fail(e, "Native animated leaf unexpected script/native phase");
  }
}
bool PodunkSceneAnimatedLeaves::publish_arrow(uint32_t source,
                                              const FieldArrowSpriteState &v,
                                              std::string &e) {
  FieldObjectId id = 0;
  if (!resolve(source, id, e) || instances_.at(id).kind != Kind::Arrow ||
      v.id != source || arrows_->sprite_state(source) != &v)
    return fail(e, "Arrow publish does not own actual source body");
  auto t = tree_->state(id)->local;
  t[2] = v.position;
  if (!tree_->set_local(id, t, e) || !tree_->set_visible(id, v.visible, e))
    return false;
  return !instances_.at(id).entered || schedule(id, v.playing, e);
}
bool PodunkSceneAnimatedLeaves::animation_signal(uint32_t source, bool started,
                                                 uint32_t role,
                                                 std::string &e) {
  FieldObjectId id = 0;
  if (!resolve(source, id, e) || instances_.at(id).kind != Kind::Player)
    return fail(e, "Arrow animation emitter rejected");
  const auto *p = arrows_data_->player(source);
  const auto *c = arrows_data_->clip(p->profile, role);
  const auto *state = arrows_->player_state(source);
  if (!c || !state || !state->alive || state->assigned != role)
    return fail(e, "Arrow animation actual clip/body rejected");
  if (instances_.at(id).entered && !schedule(id, state->playing, e))
    return false;
  return signals_->emit(
      id, started ? "animation_started" : "animation_finished", {c->name}, e);
}
bool PodunkSceneAnimatedLeaves::sprite_signal(uint32_t source,
                                              FieldArrowSignal signal,
                                              std::string &e) {
  FieldObjectId id = 0;
  if (!resolve(source, id, e) || instances_.at(id).kind != Kind::Arrow)
    return fail(e, "Arrow sprite emitter rejected");
  if (signal != FieldArrowSignal::FrameChanged &&
      signal != FieldArrowSignal::AnimationFinished)
    return fail(e, "Unknown arrow native signal");
  return signals_->emit(id,
                        signal == FieldArrowSignal::FrameChanged
                            ? "frame_changed"
                            : "animation_finished",
                        {}, e);
}
bool PodunkSceneAnimatedLeaves::sparkle_signal(uint32_t source,
                                               FieldSparklesSignal signal,
                                               std::string &e) {
  FieldObjectId id = 0;
  if (!resolve(source, id, e) || instances_.at(id).kind != Kind::Sparkles)
    return fail(e, "Sparkles emitter rejected");
  if (signal != FieldSparklesSignal::FrameChanged &&
      signal != FieldSparklesSignal::AnimationFinished)
    return fail(e, "Unknown Sparkles native signal");
  return signals_->emit(id,
                        signal == FieldSparklesSignal::FrameChanged
                            ? "frame_changed"
                            : "animation_finished",
                        {}, e);
}
bool PodunkSceneAnimatedLeaves::admit(const FieldPresentData &d,
                                      const FieldPresentBinding &b,
                                      std::string &e) const {
  FieldObjectId id = 0;
  if (!present_ || present_->content() != &d || d.binding(b.id) != &b ||
      !resolve(b.sparkles_id, id, e))
    return fail(e, "Present native Sparkles actual data/child rejected");
  const auto *n = sparkles_data_->record(b.sparkles_id);
  return n && n->owner == FieldSparklesOwner::Present && n->parent_id == b.id
             ? true
             : fail(e, "Present native Sparkles authoritative parent differs");
}
bool PodunkSceneAnimatedLeaves::signal(uint32_t child,
                                       FieldPresentSparklesEvent event,
                                       std::string &e) {
  if (event != FieldPresentSparklesEvent::FrameChanged &&
      event != FieldPresentSparklesEvent::AnimationFinished)
    return fail(e, "Unknown Present native event");
  return sparkle_signal(child,
                        event == FieldPresentSparklesEvent::FrameChanged
                            ? FieldSparklesSignal::FrameChanged
                            : FieldSparklesSignal::AnimationFinished,
                        e);
}
bool PodunkSceneAnimatedLeaves::admit(const FieldDroppedData &data,
                                      const FieldDroppedBinding &binding,
                                      std::string &e) const {
  FieldObjectId id = 0;
  if (!dropped_ || dropped_->content() != &data ||
      data.binding(binding.id) != &binding ||
      !resolve(binding.sparkles_id, id, e))
    return fail(e, "Dropped native Sparkles actual data/child rejected");
  const auto *source = sparkles_data_->record(binding.sparkles_id);
  return source && source->owner == FieldSparklesOwner::Dropped &&
                 source->parent_id == binding.id
             ? true
             : fail(e, "Dropped native Sparkles authoritative parent differs");
}
bool PodunkSceneAnimatedLeaves::signal(uint32_t child,
                                      FieldDroppedSparklesEvent event,
                                      std::string &e) {
  if (event != FieldDroppedSparklesEvent::FrameChanged &&
      event != FieldDroppedSparklesEvent::AnimationFinished)
    return fail(e, "Unknown Dropped native event");
  return sparkle_signal(child,
                         event == FieldDroppedSparklesEvent::FrameChanged
                             ? FieldSparklesSignal::FrameChanged
                             : FieldSparklesSignal::AnimationFinished,
                         e);
}
bool PodunkSceneAnimatedLeaves::begin_draw(uint64_t epoch, std::string &e) {
  if (!finished_ || !epoch || (draw_started_ && epoch <= draw_epoch_))
    return fail(e, "Native leaf actual GPU frame/fence rejected");
  arrows_gpu_->begin_frame();
  draw_epoch_ = epoch;
  draw_started_ = true;
  return true;
}
bool PodunkSceneAnimatedLeaves::draw(FieldObjectId id, Vec2 camera,
                                     std::string &e) {
  if (!draw_started_ || !actual(id, e) || !drawable(id) ||
      !instances_.at(id).ready || !instances_.at(id).entered)
    return fail(e, "Native leaf draw before actual lifecycle/GPU frame");
  const auto source = instances_.at(id).binding.stable_id;
  if (instances_.at(id).kind == Kind::Arrow) {
    FieldArrowDraw pose;
    if (!arrows_->draw(source, pose)) {
      e = arrows_->error();
      return false;
    }
    return arrows_gpu_->draw(pose, camera.x, camera.y) ||
           fail(e, "Actual arrow GPU draw rejected");
  }
  FieldSparklesDraw pose;
  if (!sparkles_->draw(source, pose)) {
    e = sparkles_->error();
    return false;
  }
  return sparkles_gpu_->draw(pose, camera.x, camera.y) ||
         fail(e, "Actual Sparkles GPU draw rejected");
}
} // namespace encore::ctr

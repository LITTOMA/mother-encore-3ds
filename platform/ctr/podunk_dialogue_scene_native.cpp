#include "podunk_dialogue_scene_native.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

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
bool finite(Vec2 p) { return std::isfinite(p.x) && std::isfinite(p.y); }
} // namespace
bool PodunkDialogueSceneNative::prepare(PodunkDialogueNativeInput i,
                                        std::string &e) {
  if (in_.recipe || !i.recipe || !i.ui || !i.visual || !i.audio ||
      !i.timer_data || !i.registry || !i.signals || !i.timers || !i.global ||
      !i.root || !i.cameras || !i.player_camera || !i.audio_server ||
      !i.geometry || !i.physics || !i.sounds || !i.dialogue || !i.printer ||
      !i.house_renderer || !i.font || !i.source_font || i.asset_root.empty() ||
      !i.frame || !i.controls || !i.control_directions || !i.update_pending ||
      !i.recipe->valid() || !i.ui->valid() || !i.visual->valid() ||
      !i.audio->valid() || !i.timer_data->valid() ||
      i.signals->registry() != i.registry ||
      i.audio_server->registry() != i.registry ||
      i.sounds->registry() != i.registry ||
      i.player_camera->registry() != i.registry ||
      i.physics->registry() != i.registry ||
      !same(i.recipe->identity(), i.ui->identity()) ||
      !same(i.recipe->identity(), i.visual->identity()) ||
      !same(i.recipe->identity(), i.audio->identity()) ||
      i.ui->recipe_ir_sha() != i.recipe->ir_sha256() ||
      i.visual->recipe_sha() != i.recipe->ir_sha256() ||
      i.audio->recipe_sha() != i.recipe->ir_sha256())
    return fail(e, "Dialogue native checked source/actual owners incomplete");
  in_ = std::move(i);
  if (!admit(*in_.recipe, e)) {
    in_ = {};
    return false;
  }
  e.clear();
  return true;
}
bool PodunkDialogueSceneNative::admit(const FieldNodeRecipeData &r,
                                      std::string &e) const {
  if (!in_.recipe || &r != in_.recipe || !r.valid())
    return fail(e, "Dialogue native recipe differs from admitted source");
  for (const auto &n : r.records()) {
    unsigned count = 0;
    if (auto *ui = in_.ui->node(n.id)) {
      if (ui->kind != FieldDialogueUiKind::Pending)
        ++count;
    }
    if (in_.visual->node(n.id))
      ++count;
    if (in_.audio->node(n.id))
      ++count;
    if (in_.timer_data->record(r.identity(), n.id))
      ++count;
    if (count != 1)
      return fail(e, "Dialogue native subtree missing/duplicate typed owner");
    if (r.control(n.id) && (!in_.ui->node(n.id) || !in_.ui->control(n.id)))
      return fail(e, "Dialogue Control has no actual source layout consumer");
  }
  for (const auto &c : in_.visual->camera().records()) {
    if (c.zoom.x != 1 || c.zoom.y != 1 || !finite(c.shape_extents) ||
        c.shape_extents.x <= 0 || c.shape_extents.y <= 0 ||
        !r.record(c.area_id) || !r.record(c.shape_id))
      return fail(e, "Dialogue Camera source shape/zoom unsupported");
  }
  e.clear();
  return true;
}
PodunkDialogueSceneNative::Factory *
PodunkDialogueSceneNative::factory(FieldObjectId id) {
  auto b = bodies_.find(id);
  if (b == bodies_.end())
    return nullptr;
  auto f = factories_.find(b->second.root);
  return f == factories_.end() ? nullptr : &f->second;
}
const PodunkDialogueSceneNative::Factory *
PodunkDialogueSceneNative::factory(FieldObjectId id) const {
  auto b = bodies_.find(id);
  if (b == bodies_.end())
    return nullptr;
  auto f = factories_.find(b->second.root);
  return f == factories_.end() ? nullptr : &f->second;
}
const PodunkDialogueSceneNative::NativeBody *
PodunkDialogueSceneNative::body(FieldObjectId id, std::string &e) const {
  auto b = bodies_.find(id);
  auto *f = factory(id);
  const auto *s = f ? f->tree->state(id) : nullptr;
  const auto *d = f ? f->tree->descriptor(id) : nullptr;
  FieldIdentity identity;
  if (b == bodies_.end() || !s || !s->alive || !d ||
      d->id != b->second.source->id ||
      d->native_class != b->second.source->native_class ||
      d->script_sha != b->second.source->script_sha ||
      !f->tree->object_identity(id, identity) ||
      !same(identity, in_.recipe->identity())) {
    fail(e, "Dialogue native actual source object expired/mismatched");
    return nullptr;
  }
  return &b->second;
}
PodunkDialogueSceneNative::NativeBody *
PodunkDialogueSceneNative::body(FieldObjectId id, std::string &e) {
  return const_cast<NativeBody *>(
      static_cast<const PodunkDialogueSceneNative *>(this)->body(id, e));
}
bool PodunkDialogueSceneNative::source(FieldObjectId root, uint32_t stable,
                                       FieldObjectId &out,
                                       std::string &e) const {
  auto *f = factory(root);
  auto p = f ? f->ids.find(stable)
             : std::map<uint32_t, FieldObjectId>::const_iterator{};
  if (!f || p == f->ids.end() || !body(p->second, e))
    return fail(e, "Dialogue native source ID outside this actual factory");
  out = p->second;
  return true;
}
bool PodunkDialogueSceneNative::emit(FieldObjectId id, std::string_view n,
                                     const FieldDeferredValue &v,
                                     std::string &e) {
  if (!body(id, e))
    return false;
  std::vector<FieldDeferredValue> args;
  if (!std::holds_alternative<std::monostate>(v))
    args.push_back(v);
  return in_.signals->emit(id, n, args, e);
}
bool PodunkDialogueSceneNative::connect(FieldObjectId receiver,
                                        FieldObjectId emitter,
                                        std::string_view signal,
                                        std::function<bool()> callback,
                                        std::string &e, uint32_t flags,
                                        std::string *out_method) {
  auto *b = body(receiver, e);
  uint32_t arity = 0;
  if (!b || !callback || !next_ || !in_.registry->object_exists(emitter))
    return fail(e, "Dialogue native signal actual receiver/emitter absent");
  if (bodies_.count(emitter)) {
    if (!declaration(emitter, signal, arity, e))
      return false;
  } else if (emitter == in_.global->owner()) {
    // The actual global declaration is independently checked by SignalBus.
    // This source visual pack proves its locale_changed connection is nullary.
    if (signal != in_.visual->signals().at(10))
      return fail(e, "Dialogue visual unknown external global signal");
  } else
    return fail(e, "Dialogue signal foreign emitter unsupported");
  std::string method = "native_dialogue_slot_" + std::to_string(next_++);
  b->slots.emplace(method, std::make_pair(arity, std::move(callback)));
  if (!in_.signals->connect(emitter, signal, receiver, method, flags, {}, e)) {
    b->slots.erase(method);
    return false;
  }
  if (out_method)
    *out_method = method;
  return true;
}
bool PodunkDialogueSceneNative::frame(FieldObjectId id, PodunkDialogueFrame &o,
                                      std::string &e) const {
  auto *b = body(id, e);
  auto *f = factory(id);
  if (!b || !in_.registry->object_exists(id) ||
      in_.registry->tree_owner(id) != f->tree || !in_.frame(id, o, e) ||
      !std::isfinite(o.delta) || o.delta < 0)
    return fail(e, "Dialogue source frame must be actual registered dispatch");
  return true;
}
bool PodunkDialogueSceneNative::global_position(FieldObjectId id, Vec2 p,
                                                std::string &e) {
  auto *b = body(id, e);
  auto *f = factory(id);
  const auto *s = f ? f->tree->state(id) : nullptr;
  FieldTransform parent;
  if (!b || !s || !finite(p) || !s->parent ||
      !f->tree->world_transform(s->parent, parent, e))
    return false;
  float det = parent[0].x * parent[1].y - parent[1].x * parent[0].y;
  if (!std::isfinite(det) || !det)
    return fail(e, "Dialogue native position singular parent");
  Vec2 v{p.x - parent[2].x, p.y - parent[2].y};
  auto local = s->local;
  local[2] = {(parent[1].y * v.x - parent[1].x * v.y) / det,
              (-parent[0].y * v.x + parent[0].x * v.y) / det};
  return f->tree->set_local(id, local, e);
}
bool PodunkDialogueSceneNative::native_notification(
    FieldObjectId id, const FieldNodeDescriptor &n, FieldTreePhase p,
    std::string &e) {
  auto *b = body(id, e);
  auto *f = factory(id);
  const auto *s = f ? f->tree->state(id) : nullptr;
  if (!b || n.id != b->source->id ||
      n.native_class != b->source->native_class || !s ||
      in_.registry->tree_owner(id) != f->tree ||
      !in_.registry->object_exists(id))
    return fail(e, "Dialogue native notification wrong source/registry");
  if (p == FieldTreePhase::EnterNative) {
    if (b->entered || !s->inside || !in_.root->viewport().world_registered)
      return fail(e, "Dialogue native Enter requires actual viewport lifetime");
    for (const auto &camera : in_.visual->camera().records()) {
      if (camera.area_id == n.id) {
        if (!in_.geometry->reserve_dialogue_camera_owner(
                *in_.visual, *in_.recipe, *f->tree, *in_.registry, id, e))
          return false;
        b->geometry_reserved = true;
      }
      if (camera.shape_id == n.id) {
        FieldObjectId area = 0;
        if (!source(b->root, camera.area_id, area, e) ||
            !in_.geometry->register_dialogue_camera_shape(
                *in_.visual, *in_.recipe, *f->tree, *in_.registry, area, id, e))
          return false;
        b->shape_registered = true;
        std::vector<FieldGeometryContact> contacts;
        if (!in_.geometry->player_shapes(area, contacts, e) ||
            contacts.size() != 1 || contacts[0].actual_shape != id ||
            !in_.physics->admit_dialogue_camera_monitor(
                *in_.visual, *in_.recipe, area, contacts[0], e))
          return false;
        bodies_.at(area).monitor_registered = true;
      }
    }
    if (in_.visual->camera().record(n.id)) {
      if (!in_.cameras->register_external(
              id, in_.recipe->identity(),
              [this](auto actual, auto &out, auto &error) {
                auto *node = body(actual, error);
                auto *v = node ? in_.dialogue->visual(node->root) : nullptr;
                auto *state = v ? v->camera().state(node->source->id) : nullptr;
                if (!state)
                  return fail(error, "Dialogue actual Camera core absent");
                out = *state;
                return true;
              },
              [this](auto actual, bool selected, auto &error) {
                auto *node = body(actual, error);
                auto *v = node ? in_.dialogue->visual(node->root) : nullptr;
                if (!v)
                  return fail(error, "Dialogue Camera selection owner absent");
                if (!v->camera().native_current_changed(node->source->id,
                                                        selected)) {
                  error = v->camera().error();
                  return false;
                }
                return !selected ||
                       v->camera().native_update(node->source->id) ||
                       fail(error, v->camera().error().c_str());
              },
              e))
        return false;
      b->camera_registered = true;
    }
    b->entered = true;
    return true;
  }
  if (p == FieldTreePhase::ReadyNative) {
    if (!b->entered || !s->inside || !s->ready_notified || b->ready)
      return fail(e, "Dialogue native Ready outside actual source traversal");
    b->ready = true;
    return true;
  }
  if (p == FieldTreePhase::ExitNative) {
    if (b->camera_registered && !in_.cameras->unregister_external(id, e))
      return false;
    b->camera_registered = false;
    if (b->shape_registered) {
      if (!in_.geometry->remove_player_shape(id, e))
        return false;
      b->shape_registered = false;
    }
    if (b->monitor_registered) {
      if (!in_.physics->static_monitor_exit(id, e))
        return false;
      b->monitor_registered = false;
    }
    b->entered = false;
    b->ready = false;
    return true;
  }
  if (p == FieldTreePhase::Deleting) {
    if (b->camera_registered && !in_.cameras->unregister_external(id, e))
      return false;
    if (b->shape_registered || b->monitor_registered)
      return fail(e, "Dialogue geometry deletion before native Exit");
    if (b->geometry_reserved) {
      if (!in_.geometry->retire_player_owner(id, e))
        return false;
      b->geometry_reserved = false;
    }
    for (auto &t : tweens_)
      if (t.second.object == id)
        t.second.alive = false;
    waits_.erase(std::remove_if(waits_.begin(), waits_.end(),
                                [id](auto &w) { return w.object == id; }),
                 waits_.end());
    b->deleting = true;
    b->slots.clear();
    b->mix = {};
    return true;
  }
  switch (p) {
  case FieldTreePhase::PostEnterNative:
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
  case FieldTreePhase::PathChanged:
  case FieldTreePhase::TreeEntered:
  case FieldTreePhase::NodeAdded:
  case FieldTreePhase::ChildEntered:
  case FieldTreePhase::TreeExiting:
  case FieldTreePhase::NodeRemoved:
  case FieldTreePhase::ChildExiting:
  case FieldTreePhase::TreeExited:
  case FieldTreePhase::EnterScript:
  case FieldTreePhase::ExitScript:
  case FieldTreePhase::ChildMoved:
  case FieldTreePhase::TransformChanged:
  case FieldTreePhase::LocalTransformChanged:
    // These checked classes have no extra source body for these notifications;
    // Node/Canvas state was already executed by the one native Tree kernel.
    e.clear();
    return true;
  case FieldTreePhase::VisibilityChanged:
    return emit(id, "visibility_changed", {}, e);
  case FieldTreePhase::Hide:
    e.clear();
    return true;
  default:
    return fail(e, "Dialogue native notification has no typed body");
  }
}
bool PodunkDialogueSceneNative::declaration(FieldObjectId id,
                                            std::string_view n, uint32_t &arity,
                                            std::string &e) const {
  auto *b = body(id, e);
  if (!b)
    return false;
  if (n == "ready" || n == "visibility_changed" || n == "tree_entered" ||
      n == "tree_exiting" || n == "tree_exited") {
    arity = 0;
    return true;
  }
  if (b->source->native_class == "Area2D") {
    if (n == "body_entered" || n == "body_exited" || n == "area_entered" ||
        n == "area_exited") {
      arity = 1;
      return true;
    }
    if (n == "body_shape_entered" || n == "body_shape_exited" ||
        n == "area_shape_entered" || n == "area_shape_exited") {
      arity = 4;
      return true;
    }
  }
  if (in_.timer_data->record(in_.recipe->identity(), b->source->id) &&
      n == "timeout") {
    arity = 0;
    return true;
  }
  if (in_.audio->node(b->source->id) && n == in_.audio->finished_signal()) {
    arity = 0;
    return true;
  }
  if (auto *u = in_.ui->node(b->source->id)) {
    if (n == in_.ui->rect_signal() || n == in_.ui->range_signal()) {
      arity = 0;
      return true;
    }
    if (n == in_.ui->value_signal()) {
      arity = 1;
      return true;
    }
    if (u->kind == FieldDialogueUiKind::AnimationPlayer &&
        (n == in_.ui->started_signal() || n == in_.ui->finished_signal())) {
      arity = 1;
      return true;
    }
  }
  auto &signals = in_.visual->signals();
  if (in_.visual->cursor(b->source->id)) {
    for (size_t i = 0; i < signals.size(); ++i)
      if (n == signals[i]) {
        arity = (i == 1 || i == 2 || i == 3 || i == 4) ? 1 : 0;
        return true;
      }
  }
  if (b->source->native_class == "AnimationPlayer" &&
      (n == in_.ui->started_signal() || n == in_.ui->finished_signal())) {
    arity = 1;
    return true;
  }
  if (in_.visual->arrows().sprite(b->source->id) &&
      (n == signals[6] || n == signals[7])) {
    arity = 0;
    return true;
  }
  return fail(e, "Dialogue native signal outside actual source declaration");
}
bool PodunkDialogueSceneNative::arrows_observe(FieldObjectId root,
                                               uint32_t stable,
                                               FieldArrowObservation &o,
                                               std::string &e) {
  FieldObjectId id = 0;
  if (!source(root, stable, id, e))
    return false;
  auto *f = factory(root);
  auto *s = f->tree->state(id);
  auto *b = body(id, e);
  FieldTransform parent{{{1, 0}, {0, 1}, {0, 0}}};
  if (!s || !b || !f->tree->world_transform(id, o.world, e))
    return false;
  if (s->canvas_parent &&
      !f->tree->world_transform(s->canvas_parent, parent, e))
    return false;
  o.parent = parent;
  o.alive = s->alive;
  o.ancestors_admitted = true;
  for (auto p = s->parent; p && p != f->tree->state(root)->parent;) {
    if (!body(p, e))
      return false;
    auto *a = f->tree->state(p);
    if (!a)
      return false;
    p = a->parent;
  }
  o.descendants_ready = true;
  std::vector<FieldObjectId> children(s->children.begin(), s->children.end());
  while (!children.empty()) {
    auto child = children.back();
    children.pop_back();
    auto *a = f->tree->state(child);
    if (!a || !body(child, e))
      return false;
    o.descendants_ready = o.descendants_ready && a->inside &&
                          a->ready_notified && !a->ready_first;
    children.insert(children.end(), a->children.begin(), a->children.end());
  }
  PodunkDialogueFrame current;
  if (s->inside) {
    if (!in_.frame(id, current, e))
      return false;
  }
  o.can_process = f->tree->can_process(id, current.tree_paused);
  o.visible_in_tree = f->tree->visible_in_tree(id);
  if (!f->tree->effective_color(id, o.canvas_color, e))
    return false;
  // The checked arrow atlas has no source material and was validated by its
  // actual renderer before this factory was exposed.
  o.material_admitted = f->renderer != nullptr;
  o.listeners_admitted = true;
  return in_.update_pending(o.update_pending, e);
}
bool PodunkDialogueSceneNative::camera_observe(FieldObjectId root,
                                               uint32_t stable,
                                               FieldGameCameraObservation &o,
                                               std::string &e) {
  FieldObjectId id = 0;
  if (!source(root, stable, id, e))
    return false;
  auto *f = factory(root);
  auto *s = f->tree->state(id);
  auto *descriptor = in_.visual->camera().record(stable);
  if (!descriptor)
    for (const auto &c : in_.visual->camera().records())
      if (c.animation_id == stable)
        descriptor = &c;
  if (!descriptor || !s || !in_.player_camera->source_observation(o, e))
    return false;
  FieldObjectId actualCamera = 0, area = 0, shape = 0;
  if (!source(root, descriptor->id, actualCamera, e) ||
      !source(root, descriptor->area_id, area, e) ||
      !source(root, descriptor->shape_id, shape, e))
    return false;
  auto *areaBody = body(area, e);
  auto *shapeBody = body(shape, e);
  auto *actual = f->tree->state(actualCamera);
  if (!areaBody || !shapeBody || !actual ||
      !f->tree->world_transform(actual->parent, o.parent_world, e))
    return false;
  o.viewport = in_.root->viewport().size;
  o.alive = s->alive;
  o.ancestors_admitted = body(actual->parent, e) != nullptr;
  o.native_children_ready = true;
  for (auto child : actual->children) {
    const auto *v = f->tree->state(child);
    o.native_children_ready = o.native_children_ready && v && v->inside &&
                              v->ready_notified && !v->ready_first;
  }
  std::vector<FieldGeometryContact> shapes;
  if (!in_.geometry->player_shapes(area, shapes, e))
    return false;
  o.native_geometry_admitted =
      areaBody->geometry_reserved && areaBody->monitor_registered &&
      shapeBody->shape_registered && shapes.size() == 1 &&
      shapes[0].actual_shape == shape;
  o.listeners_admitted = true;
  o.parent_is_global_player = actual->parent == o.global_player;
  o.physics_interpolation_enabled = descriptor->physics_interpolation != 0;
  PodunkDialogueFrame current;
  if (s->inside && !in_.frame(id, current, e))
    return false;
  o.can_process = f->tree->can_process(id, current.tree_paused);
  FieldObjectId arrows = 0;
  if (!source(root, descriptor->arrows_id, arrows, e))
    return false;
  o.scope_visible = f->tree->visible_in_tree(arrows);
  return in_.global->object(FieldGlobalMemberRole::CurrentCamera,
                            o.current_camera, e);
}
bool PodunkDialogueSceneNative::factory_services(
    FieldObjectId root, const std::map<uint32_t, FieldObjectId> &ids,
    PodunkDialogueFactoryServices &h, std::string &e) {
  if (!in_.recipe || factories_.count(root) ||
      ids.size() != in_.recipe->records().size())
    return fail(e, "Dialogue native duplicate/incomplete factory");
  auto tree = in_.registry->tree_owner(in_.registry->stable_canvas());
  if (!tree || tree->object_domain() != in_.registry->kernel() ||
      !tree->state(root) || tree->state(root)->inside)
    return fail(e,
                "Dialogue native must share actual detached stableCanvas Tree");
  Factory f;
  f.tree = tree;
  f.ids = ids;
  f.renderer = std::make_unique<FieldDialogueVisualRenderer>();
  if (!f.renderer->load(*in_.visual, in_.asset_root.c_str(), e))
    return false;
  for (const auto &n : in_.recipe->records()) {
    auto i = ids.find(n.id);
    auto *s = i == ids.end() ? nullptr : tree->state(i->second);
    auto *d = s ? tree->descriptor(s->object) : nullptr;
    FieldIdentity identity;
    if (!s || !s->alive || s->inside || !d || d->id != n.id ||
        d->native_class != n.native_class || d->script_sha != n.script_sha ||
        bodies_.count(s->object) ||
        !tree->object_identity(s->object, identity) ||
        !same(identity, in_.recipe->identity()))
      return fail(e, "Dialogue native full factory source closure mismatch");
  }
  factories_.emplace(root, std::move(f));
  for (const auto &n : in_.recipe->records()) {
    NativeBody b;
    b.root = root;
    b.source = &n;
    bodies_.emplace(ids.at(n.id), std::move(b));
  }
  h.ui.font_minimum = [this](auto font, auto text, auto &out, auto &error) {
    return FieldDialogueUiRenderer::measure(*in_.ui, font, *in_.font,
                                            *in_.source_font, text, out, error);
  };
  h.ui.canvas_enter = [this](auto id, const auto &layer, auto &error) {
    auto *b = body(id, error);
    auto *f = factory(id);
    auto *source = b ? in_.recipe->canvas_layer(b->source->id) : nullptr;
    if (!b || !source || source != &layer || b->canvas_registered ||
        layer.custom_viewport || layer.follow_viewport ||
        layer.world_2d_binding || !in_.root->viewport().world_registered ||
        !f->tree->state(id)->inside)
      return fail(error, "Dialogue real CanvasLayer registration unsupported");
    b->canvas_registered = true;
    b->entered = true;
    return true;
  };
  h.ui.canvas_exit = [this](auto id, auto &error) {
    auto *b = body(id, error);
    if (!b || !b->canvas_registered)
      return false;
    b->canvas_registered = false;
    b->entered = false;
    return true;
  };
  h.ui.control_enter = [this](auto id, const auto &control, auto &error) {
    auto *b = body(id, error);
    auto *f = factory(id);
    if (!b || b->control_registered ||
        in_.recipe->control(b->source->id) != &control ||
        !f->tree->state(id)->inside || !in_.root->viewport().world_registered)
      return fail(error,
                  "Dialogue actual native Control registration rejected");
    b->control_registered = true;
    b->entered = true;
    return true;
  };
  h.ui.control_exit = [this](auto id, auto &error) {
    auto *b = body(id, error);
    if (!b || !b->control_registered)
      return false;
    b->control_registered = false;
    b->entered = false;
    return true;
  };
  h.ui.emit = [this](auto id, auto name, auto value, auto &error) {
    return emit(id, name,
                value.empty() ? FieldDeferredValue{}
                              : FieldDeferredValue(std::string(value)),
                error);
  };
  h.ui.rename = [this](auto id, auto name, auto &error) {
    auto *b = body(id, error);
    auto *f = factory(id);
    return b && f->tree->set_name(id, name, error);
  };
  h.ui.material_admit = [this](auto id, auto shader, auto texture,
                               auto &error) {
    return body(id, error) &&
           FieldDialogueUiRenderer::material(*in_.ui, shader, texture, error);
  };
  h.ui.range_value = [this](auto id, auto signal, double value, auto &error) {
    return std::isfinite(value) && emit(id, signal, value, error);
  };
  h.visual.native = [this](auto id, const auto &n, auto phase, auto &error) {
    auto *b = body(id, error);
    if (!b || b->source->id != n.id ||
        b->source->native_class != n.native_class)
      return false;
    return native_notification(id, *b->source, phase, error);
  };
  h.visual.observe = [this](auto id, bool &process, bool &pending,
                            auto &error) {
    PodunkDialogueFrame current;
    if (!frame(id, current, error))
      return false;
    process = factory(id)->tree->can_process(id, current.tree_paused);
    return in_.update_pending(pending, error);
  };
  h.visual.controls = [this](auto id, auto &value, auto &error) {
    return body(id, error) && in_.controls(value, error) && finite(value);
  };
  h.visual.connect = [this](auto receiver, auto emitter, auto name, auto cb,
                            auto &error) {
    return connect(receiver, emitter, name, std::move(cb), error);
  };
  h.visual.emit = [this](auto id, auto name, const auto &value, auto &error) {
    return emit(id, name, value, error);
  };
  h.visual.publish = [this](auto id, const FieldDialogueCursorState &state,
                            auto &error) {
    auto *b = body(id, error);
    auto *f = factory(id);
    if (!b || state.object != id || state.source != b->source->id ||
        !finite(state.position))
      return false;
    auto local = f->tree->state(id)->local;
    local[2] = state.position;
    return f->tree->set_local(id, local, error) &&
           f->tree->set_visible(id, state.visible, error);
  };
  h.visual.timer_left = [this](auto id, float &left, auto &error) {
    if (!body(id, error) || !in_.timers->state(id))
      return false;
    left = in_.timers->time_left(id);
    return true;
  };
  h.visual.timer_start = [this](auto id, auto &error) {
    return body(id, error) && in_.timers->start(id, -1, error);
  };
  h.visual.sound = [this](auto id, auto name, auto &error) {
    FieldObjectId voice = 0;
    return body(id, error) && in_.sounds->get_sfx(name, voice, error) &&
           in_.sounds->play(voice, error);
  };
  h.visual.tween_position = [this](auto id, uint64_t &token, Vec2 target,
                                   float length, auto trans, auto ease,
                                   auto &error) {
    auto *f = factory(id);
    FieldTransform world;
    if (!body(id, error) || !finite(target) || !std::isfinite(length) ||
        length <= 0 || trans != in_.visual->transition() ||
        ease != in_.visual->ease() || trans != "TRANS_QUART" ||
        ease != "EASE_OUT" || !next_ ||
        !f->tree->world_transform(id, world, error))
      return fail(error,
                  "Dialogue Cursor native Tween source operation rejected");
    token = next_++;
    tweens_.emplace(token, Tween{id, world[2], target, length, 0, true});
    return true;
  };
  h.visual.finish_tween = [this](uint64_t token, float custom, bool finish,
                                 auto &error) {
    auto i = tweens_.find(token);
    if (i == tweens_.end() || !i->second.alive)
      return fail(error, "Dialogue Cursor actual Tween missing");
    if (finish && !step_tween(i->second, custom, error))
      return false;
    i->second.alive = false;
    return true;
  };
  h.visual.await_idle = [this](auto id, auto cb, auto &error) {
    if (!body(id, error) || !cb)
      return false;
    waits_.push_back({id, std::move(cb)});
    return true;
  };
  h.visual.global = in_.global->owner();
  h.camera.bind = [this](const auto &d, auto &error) {
    return &d == &in_.visual->camera() ||
           fail(error, "Dialogue camera source substituted");
  };
  h.camera.observe = [this, root](auto id, auto &o, auto &error) {
    return camera_observe(root, id, o, error);
  };
  h.camera.publish = [this, root](auto stable, const auto &state, auto &error) {
    FieldObjectId id = 0;
    if (!source(root, stable, id, error))
      return false;
    auto *f = factory(root);
    auto local = f->tree->state(id)->local;
    local[2] = state.position;
    return f->tree->set_local(id, local, error);
  };
  h.camera.publish_canvas = [this, root](auto stable, Vec2 origin, Vec2,
                                         auto &error) {
    FieldObjectId id = 0, current = 0;
    if (!source(root, stable, id, error) ||
        !in_.cameras->native_current(current, error) || current != id)
      return fail(
          error,
          "Dialogue Camera canvas mutation requires actual current selection");
    return in_.root->set_canvas_transform(
        {{{1, 0}, {0, 1}, {-origin.x, -origin.y}}}, error);
  };
  h.camera.current_camera_snapshot = [this](auto, auto &out, auto &error) {
    FieldObjectId id = 0;
    return in_.global->object(FieldGlobalMemberRole::CurrentCamera, id,
                              error) &&
           in_.cameras->current_snapshot(id, out, error);
  };
  h.camera.make_current = [this, root](auto stable, auto &error) {
    FieldObjectId id = 0;
    return source(root, stable, id, error) &&
           in_.cameras->make_current(id, error);
  };
  h.camera.set_global_current = [this, root](auto stable, auto &error) {
    FieldObjectId id = 0;
    return source(root, stable, id, error) &&
           in_.global->set_object(FieldGlobalMemberRole::CurrentCamera, id,
                                  error);
  };
  h.camera.scope = [this, root](auto stable, FieldScopeOperation op, Vec2 value,
                                auto &error) {
    auto *v = in_.dialogue->visual(root);
    if (!v)
      return fail(error, "Dialogue actual scope core absent");
    bool ok = op == FieldScopeOperation::Show   ? v->arrows().show(stable)
              : op == FieldScopeOperation::Hide ? v->arrows().hide(stable)
              : op == FieldScopeOperation::GlobalPosition
                  ? v->arrows().global_position(stable, value)
                  : v->arrows().handle_input_events(stable);
    return ok || fail(error, v->arrows().error().c_str());
  };
  h.camera.arrow_visible = [this, root](auto stable, Vec2 direction,
                                        bool visible, auto &error) {
    auto *v = in_.dialogue->visual(root);
    return v && (v->arrows().set_arrow_visible(stable, direction, visible) ||
                 fail(error, v->arrows().error().c_str()));
  };
  auto playerCamera = in_.player_camera->source_host();
  h.camera.info_plates_hide = playerCamera.info_plates_hide;
  h.camera.player_exit_camera = playerCamera.player_exit_camera;
  h.camera.animation_signal = [this, root](auto stable, uint32_t role,
                                           bool started, auto &error) {
    FieldObjectId id = 0;
    if (!source(root, stable, id, error))
      return false;
    // Empty Camera clips are already consumed by the camera core's unique
    // animation clock. Their role is not an invented source animation name.
    if (role < 1 || role > in_.visual->camera().animation_lengths().size())
      return false;
    return fail(
        error, started
                   ? "Dialogue Camera clip names require checked source binding"
                   : "Dialogue Camera clip finished name binding missing");
  };
  h.arrows.bind = [this](const auto &d, auto &error) {
    return &d == &in_.visual->arrows() ||
           fail(error, "Dialogue arrows source substituted");
  };
  h.arrows.observe = [this, root](auto stable, auto &out, auto &error) {
    return arrows_observe(root, stable, out, error);
  };
  h.arrows.publish_root = [this, root](auto stable, const auto &state,
                                       auto &error) {
    FieldObjectId id = 0;
    if (!source(root, stable, id, error))
      return false;
    auto *f = factory(root);
    auto local = f->tree->state(id)->local;
    local[2] = state.position;
    return f->tree->set_local(id, local, error) &&
           f->tree->set_visible(id, state.visible, error);
  };
  h.arrows.publish_sprite = [this, root](auto stable, const auto &state,
                                         auto &error) {
    FieldObjectId id = 0;
    return source(root, stable, id, error) &&
           factory(root)->tree->set_visible(id, state.visible, error);
  };
  h.arrows.control_directions = in_.control_directions;
  h.arrows.animation_signal = [this, root](auto stable, bool started,
                                           uint32_t role, auto &error) {
    FieldObjectId id = 0;
    if (!source(root, stable, id, error))
      return false;
    auto *p = in_.visual->arrows().player(stable);
    auto *clip = p ? in_.visual->arrows().clip(p->profile, role) : nullptr;
    return clip &&
           emit(id,
                started ? in_.ui->started_signal() : in_.ui->finished_signal(),
                clip->name, error);
  };
  h.arrows.sprite_signal = [this, root](auto stable, FieldArrowSignal s,
                                        auto &error) {
    FieldObjectId id = 0;
    return source(root, stable, id, error) &&
           emit(id,
                in_.visual
                    ->signals()[s == FieldArrowSignal::FrameChanged ? 6 : 7],
                {}, error);
  };
  h.arrows.connect_animation_finished = [this, root](auto stable, auto callback,
                                                     auto &error) {
    FieldObjectId id = 0;
    if (!source(root, stable, id, error))
      return false;
    return connect(
        id, id, in_.ui->finished_signal(),
        [this, root, stable, callback] {
          auto *v = in_.dialogue->visual(root);
          auto *p = v ? v->arrows().player_state(stable) : nullptr;
          return p && callback(p->assigned);
        },
        error);
  };
  h.arrows.await_frame_changed = [this, root](auto stable, uint64_t token,
                                              auto cb, auto &error) {
    FieldObjectId id = 0;
    auto key = std::make_pair(root, token);
    if (!source(root, stable, id, error) || signal_waits_.count(key))
      return false;
    std::string method;
    auto signal = in_.visual->signals()[6];
    if (!connect(
            id, id, signal,
            [this, key, cb] {
              signal_waits_.erase(key);
              return cb();
            },
            error, FieldSignalOneShot, &method))
      return false;
    signal_waits_.emplace(key, SignalWait{id, id, signal, method});
    return true;
  };
  h.arrows.cancel_frame_changed = [this, root](auto, uint64_t token,
                                               auto &error) {
    auto i = signal_waits_.find({root, token});
    if (i == signal_waits_.end())
      return fail(error, "Dialogue actual frame waiter absent");
    auto w = i->second;
    if (!in_.signals->disconnect(w.emitter, w.signal, w.receiver, w.method,
                                 error))
      return false;
    bodies_.at(w.receiver).slots.erase(w.method);
    signal_waits_.erase(i);
    return true;
  };
  auto server = in_.audio_server->media_host();
  h.audio.add_audio_callback = [this, server](auto id, auto callback,
                                              auto &error) {
    auto *b = body(id, error);
    if (!b || b->mix || !callback)
      return false;
    b->mix = std::move(callback);
    b->audio_callback = {
        id, in_.registry, this,
        [](void *owner, FieldObjectId actual, std::string &err) {
          auto *self = static_cast<PodunkDialogueSceneNative *>(owner);
          auto *node = self->body(actual, err);
          return node && node->mix && node->mix();
        }};
    if (!server.add_audio_callback(
            id, {podunk_audio_stream_player_mix, &b->audio_callback}, error)) {
      b->mix = {};
      return false;
    }
    return true;
  };
  h.audio.remove_audio_callback = [this, server](auto id, auto &error) {
    auto *b = body(id, error);
    if (!b || !b->mix || !server.remove_audio_callback(id, error))
      return false;
    b->mix = {};
    return true;
  };
  h.audio.emit = [this](auto id, auto name, auto &error) {
    return emit(id, name, {}, error);
  };
  e.clear();
  return true;
}
bool PodunkDialogueSceneNative::deferred(const FieldDeferredMessage &m,
                                         std::string &e) {
  if (in_.physics && in_.physics->handles_callback(m))
    return in_.physics->deferred(m, e);
  auto *b = body(m.object, e);
  auto *f = factory(m.object);
  if (!b)
    return false;
  if (m.kind == FieldDeferredKind::Call) {
    auto slot = b->slots.find(m.member);
    if (slot != b->slots.end()) {
      if (m.args.size() != slot->second.first)
        return fail(e, "Dialogue native signal callback arity mismatch");
      auto callback = slot->second.second;
      return (callback && callback()) ||
             fail(e, "Dialogue native signal callback failed");
    }
    if (m.member == "show" && m.args.empty())
      return f->tree->set_visible(m.object, true, e);
    if (m.member == "hide" && m.args.empty())
      return f->tree->set_visible(m.object, false, e);
  }
  if (m.kind == FieldDeferredKind::Set && m.args.size() == 1) {
    if (m.member == "visible") {
      auto *v = std::get_if<bool>(&m.args[0]);
      return v && f->tree->set_visible(m.object, *v, e);
    }
    if (m.member == "position" || m.member == "global_position") {
      auto *v = std::get_if<Vec2>(&m.args[0]);
      if (!v || !finite(*v))
        return false;
      if (m.member == "global_position")
        return global_position(m.object, *v, e);
      auto local = f->tree->state(m.object)->local;
      local[2] = *v;
      return f->tree->set_local(m.object, local, e);
    }
  }
  return fail(
      e,
      "Dialogue native receiver lacks source method/property implementation");
}
bool PodunkDialogueSceneNative::step_tween(Tween &t, float delta,
                                           std::string &e) {
  if (!t.alive || !std::isfinite(delta) || delta < 0)
    return fail(e, "Dialogue actual Tween cursor invalid");
  float next = t.elapsed + delta;
  if (!std::isfinite(next))
    return fail(e, "Dialogue Tween overflow");
  t.elapsed = std::min(next, t.duration);
  float u = t.elapsed / t.duration;
  float value = 1 - std::pow(1 - u, 4.f);
  if (!global_position(t.object,
                       {t.initial.x + (t.target.x - t.initial.x) * value,
                        t.initial.y + (t.target.y - t.initial.y) * value},
                       e))
    return false;
  if (t.elapsed >= t.duration)
    t.alive = false;
  return true;
}
bool PodunkDialogueSceneNative::idle_tail(uint64_t epoch, float delta,
                                          bool paused, std::string &e) {
  if (!in_.recipe || !epoch || epoch <= idle_epoch_ || !std::isfinite(delta) ||
      delta < 0)
    return fail(e, "Dialogue native idle must follow unique actual traversal");
  idle_epoch_ = epoch;
  // The real Tree/Registry deletion and DialogueHost core retirement must
  // finish before disposing GPU owners or audio callback userdata.
  std::vector<FieldObjectId> retired;
  for (const auto &f : factories_) {
    bool complete = true;
    for (const auto &node : f.second.ids) {
      auto b = bodies_.find(node.second);
      const auto *state = f.second.tree->state(node.second);
      complete = complete && b != bodies_.end() && b->second.deleting &&
                 (!state || !state->alive) &&
                 !in_.registry->object_exists(node.second);
    }
    if (complete)
      retired.push_back(f.first);
  }
  for (auto root : retired) {
    for (const auto &node : factories_.at(root).ids)
      bodies_.erase(node.second);
    factories_.erase(root);
  }
  auto waits = std::move(waits_);
  waits_.clear();
  std::vector<uint64_t> tokens;
  for (const auto &t : tweens_)
    if (t.second.alive)
      tokens.push_back(t.first);
  for (auto token : tokens) {
    auto i = tweens_.find(token);
    if (i == tweens_.end() || !i->second.alive)
      continue;
    auto *f = factory(i->second.object);
    auto *b = body(i->second.object, e);
    if (!b || !f)
      return false;
    if (f->tree->can_process(i->second.object, paused) &&
        !step_tween(i->second, delta, e))
      return false;
  }
  for (auto &wait : waits) {
    if (!body(wait.object, e) || !wait.callback || !wait.callback())
      return fail(e, "Dialogue actual idle waiter receiver failed");
  }
  for (auto i = tweens_.begin(); i != tweens_.end();) {
    if (!i->second.alive)
      i = tweens_.erase(i);
    else
      ++i;
  }
  return true;
}
bool PodunkDialogueSceneNative::animation_playing(FieldObjectId id,
                                                  bool &playing,
                                                  std::string &e) const {
  auto *b = body(id, e);
  auto *f = factory(id);
  if (!b || b->source->native_class != "AnimationPlayer")
    return fail(e, "Dialogue animation query requires actual AnimationPlayer");
  if (auto *ui = in_.ui->node(b->source->id)) {
    if (ui->kind != FieldDialogueUiKind::AnimationPlayer)
      return false;
    // The two UI AnimationPlayers add exactly their one native group on play
    // and remove it on completion. This queries the actual owner-owned group.
    std::vector<FieldObjectId> processing;
    if (!f->tree->group("idle_process_internal", processing, e))
      return false;
    playing =
        std::find(processing.begin(), processing.end(), id) != processing.end();
    return true;
  }
  auto *visual = in_.dialogue->visual(b->root);
  if (!visual)
    return false;
  if (auto *p = visual->arrows().player_state(b->source->id)) {
    playing = p->playing;
    return true;
  }
  for (const auto &c : in_.visual->cursors())
    if (c.player == b->source->id) {
      auto actual = f->ids.find(c.id);
      auto *cursor = actual == f->ids.end()
                         ? nullptr
                         : visual->cursor_state(actual->second);
      if (!cursor)
        return false;
      playing = cursor->animation_playing;
      return true;
    }
  for (const auto &c : in_.visual->camera().records())
    if (c.animation_id == b->source->id) {
      auto *state = visual->camera().state(c.id);
      if (!state)
        return false;
      playing = state->animation_playing;
      return true;
    }
  return fail(e, "Dialogue native AnimationPlayer owner missing");
}
bool PodunkDialogueSceneNative::name_rect_changed(FieldObjectId id,
                                                  std::string &e) {
  auto *b = body(id, e);
  auto *n = b ? in_.ui->node(b->source->id) : nullptr;
  auto *ui = b ? in_.dialogue->ui(b->root) : nullptr;
  if (!n || n->role != FieldDialogueUiRole::Name || !ui || !ui->control(id))
    return fail(e, "Dialogue source name-size receiver mismatched");
  // HousePresentation is the actual existing source name-size Tween owner;
  // copy its current property result into the same native controls.
  return ui->sync_text(b->root, *in_.printer, e);
}
bool PodunkDialogueSceneNative::draw(uint64_t epoch, float width, float height,
                                     float x, float y, uint32_t color,
                                     std::string &e) {
  if (!in_.recipe || !epoch || !finite({width, height}) || width <= 0 ||
      height <= 0 || !finite({x, y}))
    return fail(e, "Dialogue draw requires actual viewport/frame");
  std::vector<std::pair<std::vector<int32_t>, FieldObjectId>> order;
  for (const auto &entry : factories_) {
    auto *s = entry.second.tree->state(entry.first);
    if (!s || !s->alive || !s->inside ||
        !entry.second.tree->visible_in_tree(entry.first))
      continue;
    std::vector<int32_t> key;
    for (auto current = s; current && current->parent;
         current = entry.second.tree->state(current->parent)) {
      auto *p = entry.second.tree->state(current->parent);
      if (!p)
        break;
      auto i =
          std::find(p->children.begin(), p->children.end(), current->object);
      if (i == p->children.end())
        return fail(e, "Dialogue Canvas tree draw order differs");
      key.push_back(int32_t(i - p->children.begin()));
    }
    std::reverse(key.begin(), key.end());
    order.emplace_back(std::move(key), entry.first);
  }
  std::sort(order.begin(), order.end());
  for (const auto &entry : order) {
    auto &f = factories_.at(entry.second);
    auto *ui = in_.dialogue->ui(entry.second);
    auto *visual = in_.dialogue->visual(entry.second);
    if (!ui || !visual || f.draw_epoch == epoch)
      return fail(e, "Dialogue actual renderer missing/duplicate frame");
    f.draw_epoch = epoch;
    f.renderer->begin_frame();
    FieldDialogueUiRenderer renderer;
    if (!renderer.draw(*ui, entry.second, *in_.house_renderer, *in_.font,
                       *in_.source_font, width, height, x, y, color, e))
      return false;
    for (const auto &c : in_.visual->cursors()) {
      auto id = f.ids.find(c.id);
      FieldArrowDraw pose;
      if (id == f.ids.end() || !visual->cursor_draw(id->second, pose, e) ||
          !f.renderer->draw(pose, -x, -y))
        return false;
    }
    for (const auto &s : in_.visual->arrows().sprites()) {
      FieldArrowDraw pose;
      if (!visual->arrows().draw(s.id, pose) || !f.renderer->draw(pose, -x, -y))
        return fail(e, "Dialogue actual scope arrow GPU/source draw failed");
    }
  }
  e.clear();
  return true;
}
bool PodunkDialogueSceneNative::observes_closed_printer(const HousePresentation &p,std::string &e)const{
 if(!in_.registry||in_.registry->poisoned()||!in_.recipe||!in_.recipe->valid()||
    in_.printer!=&p||!p.source_frame_closed()||!bodies_.empty()||!factories_.empty()||
    !waits_.empty()||!tweens_.empty()||!signal_waits_.empty())
  return fail(e,"Dialogue native printer rebind rejects live/pending native callback owners");
 e.clear();return true;
}
bool PodunkDialogueSceneNative::admit_printer_rebind(const HousePresentation &old,
    const HousePresentation &next,std::string &e)const{
 if(&old==&next||!observes_closed_printer(old,e)||!next.source_frame_closed()||
    old.callback_bindings().random!=next.callback_bindings().random)
  return fail(e,"Dialogue native actual printer destination differs");
 e.clear();return true;
}
bool PodunkDialogueSceneNative::rebind_printer(const HousePresentation &old,
    HousePresentation &next,std::string &e){
 if(!admit_printer_rebind(old,next,e))return false;
 in_.printer=&next;e.clear();return true;
}
bool PodunkDialogueSceneNative::apply(PodunkDialogueServices &s,
                                      std::string &e) {
  if (!in_.recipe || s.factory_services || s.admit_native_services ||
      s.native_notification || s.native_deferred || s.frame)
    return fail(e, "Dialogue native endpoints already owned/unprepared");
  s.factory_services = [this](auto root, const auto &ids, auto &out,
                              auto &error) {
    return factory_services(root, ids, out, error);
  };
  s.admit_native_services = [this](const auto &recipe, auto &error) {
    return admit(recipe, error);
  };
  s.native_notification = [this](auto id, const auto &n, auto p, auto &error) {
    return native_notification(id, n, p, error);
  };
  s.native_deferred = [this](const auto &m, auto &error) {
    return deferred(m, error);
  };
  s.frame = [this](auto id, auto &out, auto &error) {
    return frame(id, out, error);
  };
  e.clear();
  return true;
}
} // namespace encore::ctr

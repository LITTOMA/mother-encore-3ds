#include "podunk_player_camera.hpp"
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
bool finite(Vec2 v) { return std::isfinite(v.x) && std::isfinite(v.y); }
bool identity(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.source_sha256 == b.source_sha256 &&
         a.upstream_commit == b.upstream_commit;
}
} // namespace
bool PodunkPlayerCamera::prepare(
    const PlayerChildScriptsData &d, const PlayerInitializationData &p,
    const PlayerMotionData &m, FieldNodeTreeRuntime &t, FieldGlobalRegistry &r,
    PodunkNativeRoot &root, FieldGlobalConstructorRuntime &g,
    PlayerInitializationBody &b, PlayerChildScriptsRuntime &children,
    PodunkPlayerAnimation &animation, PodunkCameraViewport v,
    PodunkPlayerCameraPorts ports, std::string &e) {
  if (data_ || !d.valid() || !p.valid() || !m.valid() ||
      d.player_ir_sha256() != p.ir_sha256() ||
      !identity(d.identity(), p.recipe().identity()) ||
      !identity(m.identity(), p.recipe().identity()) ||
      d.camera().records().size() != 1 || !d.record(3) ||
      d.record(3)->id != d.camera().records().front().id ||
      t.object_domain() != r.kernel() || root.kernel_object() != r.kernel() ||
      root.viewport_object() != r.root() || !g.data() ||
      g.data()->identity().upstream_commit != d.identity().upstream_commit ||
      !finite(v.logical_size) || v.logical_size.x <= 0 || v.logical_size.y <= 0)
    return fail(e, "Player Camera checked sources/domain/viewport rejected");
  data_ = &d;
  player_data_ = &p;
  motion_data_ = &m;
  tree_ = &t;
  registry_ = &r;
  root_ = &root;
  global_ = &g;
  body_ = &b;
  children_ = &children;
  animation_ = &animation;
  viewport_ = v;
  ports_ = std::move(ports);
  if (!viewport(e)) {
    data_ = nullptr;
    return false;
  }
  const auto physical = root_->viewport().size;
  display_offset_ = {(physical.x - v.logical_size.x) * .5f,
                     (physical.y - v.logical_size.y) * .5f};
  e.clear();
  return true;
}
bool PodunkPlayerCamera::viewport(std::string &e) const {
  const auto &v = root_->viewport();
  if (!v.initialized || v.failed ||
      !registry_->object_exists(root_->viewport_object()) || !finite(v.size) ||
      v.size.x <= 0 || v.size.y <= 0 || viewport_.logical_size.x > v.size.x ||
      viewport_.logical_size.y > v.size.y ||
      (!viewport_.reference_explicit && (v.size.x != viewport_.logical_size.x ||
                                         v.size.y != viewport_.logical_size.y)))
    return fail(
        e, "Player Camera logical view is not actual Viewport/reference mode");
  return true;
}
bool PodunkPlayerCamera::live(std::string &e) const {
  if (!data_ || !object_ || !registry_->object_exists(object_) ||
      registry_->tree_owner(object_).get() != tree_ || !tree_->state(object_) ||
      !tree_->state(object_)->alive || !viewport(e))
    return fail(e, "Player Camera actual native owner unavailable");
  return true;
}
bool PodunkPlayerCamera::actual_ui(std::string &e) const {
  const auto *ns = ports_.ui_namespace;
  if (!registry_ || !data_ || !ports_.ui || !ns || !ns->valid() ||
      ns->identity().upstream_commit != data_->identity().upstream_commit)
    return fail(e, "Player Camera actual UI namespace/source unavailable");
  const auto binding = ports_.ui->binding();
  const auto &spec = binding.source;
  const auto &autoloads = ns->autoloads();
  const auto expected =
      std::find_if(autoloads.begin(), autoloads.end(),
                   [&](const auto &a) { return a.id == ns->ui_autoload(); });
  FieldGlobalExternalState state;
  std::array<uint8_t, 32> proof{};
  if (expected == autoloads.end() || !binding.object ||
      registry_->external_object(binding.object) != ports_.ui ||
      !identity(spec.identity, ns->identity()) || spec.role != 3 ||
      spec.stable_id != expected->id || spec.name != expected->name ||
      spec.native_class != expected->native_class ||
      spec.source != expected->path || spec.script != expected->script ||
      spec.source_sha != expected->source_sha ||
      spec.script_sha != expected->script_sha ||
      !ns->source_hash(spec.script, proof) || proof != spec.script_sha ||
      !ports_.ui->state(state, e) || !state.inside ||
      state.parent != registry_->root())
    return fail(e,
                "Player Camera actual UI identity/attachment/source rejected");
  // No Ready bit is synthesized or required. The concrete callback must read
  // this owning UI's real battlefield state, including transition boundaries.
  e.clear();
  return true;
}
bool PodunkPlayerCamera::deferred(const FieldDeferredMessage &m,
                                  std::string &e) {
  if (!live(e) || m.object != object_ || m.kind != FieldDeferredKind::Call ||
      m.member != "_update_scroll" || !m.args.empty())
    return fail(e, "Camera native method/signature/owner rejected");
  return entered_ ? update_native(e) : true;
}
bool PodunkPlayerCamera::construct(FieldObjectId id,
                                   const FieldNodeDescriptor &d,
                                   std::string &e) {
  const auto *expected = data_ ? player_data_->recipe().record(d.id) : nullptr;
  const auto *source = data_ ? data_->camera().record(d.id) : nullptr;
  const auto *s = data_ ? tree_->state(id) : nullptr;
  FieldIdentity actual_identity{};
  if (object_ || !expected || !source || !s || s->inside || s->parent ||
      !s->name.empty() || d.native_class != "Camera2D" ||
      expected->native_class != d.native_class ||
      expected->script != d.script || expected->script_sha != d.script_sha ||
      d.script != data_->camera().script() ||
      d.script_sha != data_->camera().script_sha() ||
      !tree_->object_identity(id, actual_identity) ||
      !identity(actual_identity, player_data_->recipe().identity()) ||
      registry_->tree_owner(id).get() != tree_ || !registry_->object_exists(id))
    return fail(e, "Player Camera native constructor/source cursor rejected");
  object_ = id;
  native_.id = source->id;
  native_.position = source->position;
  native_.offset = source->offset;
  native_.limits = source->limits;
  native_.current = (source->flags & 1) != 0;
  e.clear();
  return true;
}
bool PodunkPlayerCamera::actual(uint32_t source, FieldObjectId &out,
                                std::string &e) const {
  if (!live(e) || !body_->constructed() || body_->tree() != tree_)
    return fail(e, "Player Camera actual parent body unavailable");
  const auto *d = player_data_->recipe().record(source);
  if (!d || !tree_->get_node(body_->object(), d->path, out, e))
    return false;
  const auto *live_source = tree_->descriptor(out);
  FieldIdentity i{};
  if (!live_source || live_source->id != d->id ||
      live_source->native_class != d->native_class ||
      live_source->script != d->script ||
      live_source->script_sha != d->script_sha ||
      registry_->tree_owner(out).get() != tree_ ||
      !registry_->object_exists(out) || !tree_->object_identity(out, i) ||
      !identity(i, player_data_->recipe().identity()))
    return fail(e, "Player Camera NodePath crossed actual instance/source");
  return true;
}
bool PodunkPlayerCamera::global_player(FieldObjectId &out,
                                       std::string &e) const {
  std::shared_ptr<const GlobalLoadObjectArray> objects;
  if (!global_->array(FieldGlobalMemberRole::PartyObjects, objects, e) ||
      !objects || objects->values.empty())
    return fail(e, "Source global.get_player actual first party object absent");
  out = objects->values.front();
  if (!registry_->object_exists(out))
    return fail(e, "Source global player is dead");
  return true;
}
bool PodunkPlayerCamera::all_children_ready(FieldObjectId id) const {
  const auto *s = tree_->state(id);
  if (!s || !s->alive || !s->inside || !s->bound)
    return false;
  for (auto child : s->children) {
    const auto *c = tree_->state(child);
    if (!c || !c->ready_notified || !all_children_ready(child))
      return false;
  }
  return true;
}
bool PodunkPlayerCamera::observe(uint32_t source,
                                 FieldGameCameraObservation &out,
                                 std::string &e) {
  if (!live(e) || source != native_.id || !entered_ ||
      !ports_.listeners_admitted || !ports_.geometry_admitted)
    return fail(e, "Player Camera actual source observations unavailable");
  const auto *s = tree_->state(object_);
  if (!s->inside || !s->bound || !s->parent || s->parent != body_->object())
    return fail(e, "Player Camera source parent/entry rejected");
  for (auto id = object_; id;) {
    const auto *node = tree_->state(id);
    if (!node || !node->inside || !node->bound || !node->alive)
      return fail(e, "Player Camera actual ancestor not admitted");
    id = node->parent;
  }
  FieldObjectId area = 0, shape = 0, player = 0, current = 0, arrows = 0;
  const auto &d = data_->camera().records().front();
  if (!actual(d.area_id, area, e) || !actual(d.shape_id, shape, e) ||
      !actual(d.arrows_id, arrows, e) || !global_player(player, e) ||
      !global_->object(FieldGlobalMemberRole::CurrentCamera, current, e) ||
      !ports_.listeners_admitted(object_, data_->camera(), e) ||
      !ports_.geometry_admitted(area, shape, e))
    return false;
  FieldGameCameraObservation o{};
  o.alive = true;
  o.ancestors_admitted = true;
  o.native_children_ready = all_children_ready(object_);
  o.listeners_admitted = true;
  o.native_geometry_admitted = true;
  o.physics_interpolation_enabled = d.physics_interpolation != 0;
  o.parent_is_global_player = player == s->parent;
  o.global_player = player;
  o.current_camera = current == object_ ? native_.id : 0;
  o.viewport = viewport_.logical_size;
  o.can_process = tree_->can_process(object_, paused_);
  o.scope_visible = (tree_->state(arrows)->flags & 2) != 0;
  if (!tree_->world_transform(s->parent, o.parent_world, e))
    return false;
  // These source getters read the same owning Player fields on every call,
  // including after Player's earlier Physics body mutates them in this frame.
  PlayerInitializationMember state, damaged;
  if (!body_->member(motion_data_->field(PlayerMotionField::State), state, e) ||
      state.kind != 2 || !state.value || state.value->kind != 2 ||
      state.value->integer < 0 ||
      uint64_t(state.value->integer) > std::numeric_limits<uint32_t>::max() ||
      !body_->member(motion_data_->field(PlayerMotionField::ContinuousDamage),
                     damaged, e) ||
      damaged.kind != 1 || !damaged.value || damaged.value->kind != 1)
    return fail(e, "Player Camera actual state/damage getters rejected");
  o.player_state = uint32_t(state.value->integer);
  o.player_damaged = damaged.value->boolean;
  const auto *script_state = children_->camera().state(source);
  if (script_state && script_state->ready && !native_observation_) {
    if (!frame_valid_)
      return fail(e,
                  "Player Camera UI/Input actual frame has not been observed");
    o.in_battle = frame_.in_battle;
    o.controls = frame_.controls;
    o.scope_pressed = frame_.scope_pressed;
    o.scope_just_pressed = frame_.scope_just_pressed;
    o.scope_just_released = frame_.scope_just_released;
  }
  // Before script Ready these UI/Input fields are not evaluated by source
  // create/native_update/_ready. No UI getter or default Ready is substituted.
  out = o;
  e.clear();
  return true;
}
bool PodunkPlayerCamera::publish(uint32_t source, const FieldGameCameraState &s,
                                 std::string &e) {
  if (!live(e) || source != native_.id || s.id != source ||
      !finite(s.position) || !finite(s.offset))
    return fail(e, "Player Camera native property publication rejected");
  auto t = tree_->state(object_)->local;
  t[2] = s.position;
  if (!tree_->set_local(object_, t, e))
    return false;
  native_.position = s.position;
  native_.offset = s.offset;
  native_.limits = s.limits;
  // current is a native Viewport selection. The script's copied bit cannot
  // select a camera independently of its owning Viewport registry.
  e.clear();
  return true;
}
bool PodunkPlayerCamera::canvas(uint32_t source, Vec2 origin, Vec2 center,
                                std::string &e) {
  FieldObjectId current = 0;
  if (!live(e) || source != native_.id || !entered_ || !ports_.native_current ||
      !ports_.native_current(current, e) || current != object_ ||
      !finite(origin) || !finite(center))
    return fail(e, "Player Camera canvas is not actual native current camera");
  const FieldTransform t{
      Vec2{1, 0}, Vec2{0, 1},
      Vec2{display_offset_.x - origin.x, display_offset_.y - origin.y}};
  if (!root_->set_canvas_transform(t, e))
    return false;
  native_.canvas_origin = origin;
  native_.screen_center = center;
  return true;
}
bool PodunkPlayerCamera::update_native(std::string &e) {
  FieldObjectId current = 0;
  if (!live(e) || !entered_ || !ports_.native_current ||
      !ports_.native_current(current, e))
    return false;
  if (current != object_)
    return true;
  const auto *source_state = children_->camera().state(native_.id);
  if (source_state) {
    native_observation_ = true;
    const auto local = tree_->state(object_)->local[2];
    const bool changed = source_state->position.x != local.x ||
                         source_state->position.y != local.y;
    const bool ok = changed
                        ? children_->camera().set_position(native_.id, local)
                        : children_->camera().native_update(native_.id);
    native_observation_ = false;
    if (!ok)
      e = children_->camera().error();
    return ok;
  }
  // Native Enter/transform updates precede the script's Ready/create cursor.
  // Use the actual Node2D world transform and native Camera2D fields here.
  FieldTransform world{};
  if (!tree_->world_transform(object_, world, e) || world[0].x != 1 ||
      world[0].y != 0 || world[1].x != 0 || world[1].y != 1)
    return fail(e, "Player Camera native rotated/scaled transform unsupported");
  Vec2 origin{world[2].x - viewport_.logical_size.x * .5f,
              world[2].y - viewport_.logical_size.y * .5f};
  if (origin.x < native_.limits[1])
    origin.x = float(native_.limits[1]);
  if (origin.x + viewport_.logical_size.x > native_.limits[2])
    origin.x = float(native_.limits[2]) - viewport_.logical_size.x;
  if (origin.y + viewport_.logical_size.y > native_.limits[3])
    origin.y = float(native_.limits[3]) - viewport_.logical_size.y;
  if (origin.y < native_.limits[0])
    origin.y = float(native_.limits[0]);
  origin.x += native_.offset.x;
  origin.y += native_.offset.y;
  Vec2 center{origin.x + viewport_.logical_size.x * .5f,
              origin.y + viewport_.logical_size.y * .5f};
  return canvas(native_.id, origin, center, e);
}
bool PodunkPlayerCamera::select(uint32_t source, std::string &e) {
  if (!live(e) || source != native_.id || !entered_ || !ports_.make_current ||
      !ports_.make_current(object_, e))
    return false;
  native_.current = true;
  return true;
}
bool PodunkPlayerCamera::begin_frame(uint64_t epoch, bool paused,
                                     std::string &e) {
  frame_valid_ = false;
  if (!live(e) || !epoch || epoch <= epoch_ || !actual_ui(e) ||
      !ports_.in_battle || !ports_.controls || !ports_.input ||
      !ports_.in_battle(ports_.ui->binding().object, frame_.in_battle, e) ||
      !ports_.controls(frame_.controls, e) || !finite(frame_.controls) ||
      !ports_.input(data_->camera().scope_action(), PlayerInputQuery::Held,
                    frame_.scope_pressed, e) ||
      !ports_.input(data_->camera().scope_action(),
                    PlayerInputQuery::JustPressed, frame_.scope_just_pressed,
                    e) ||
      !ports_.input(data_->camera().scope_action(),
                    PlayerInputQuery::JustReleased, frame_.scope_just_released,
                    e))
    return fail(e, "Player Camera actual UI/Input frame owner unavailable");
  epoch_ = epoch;
  paused_ = paused;
  frame_valid_ = true;
  e.clear();
  return true;
}
bool PodunkPlayerCamera::phase(FieldObjectId id, FieldTreePhase p, bool paused,
                               std::string &e) {
  if (!live(e) || id != object_)
    return fail(e, "Player Camera native phase identity rejected");
  paused_ = paused;
  const auto *s = tree_->state(id);
  switch (p) {
  case FieldTreePhase::EnterNative:
    if (entered_ || !s->inside || !s->bound)
      return fail(e, "Player Camera duplicate/native Enter rejected");
    entered_ = true;
    if (!root_->connect_signal(false, "size_changed", id, "_update_scroll", e))
      return false;
    // Official _update_process_mode disables both internal clocks when
    // smoothing and physics interpolation are inactive (this checked scope).
    // Following is driven by actual Canvas transform notifications and the
    // source offset setter in _physics_process, never an extra frame timer.
    if (!tree_->set_transform_notification(id, false, true, e))
      return false;
    if (native_.current && !select(native_.id, e))
      return false;
    return update_native(e);
  case FieldTreePhase::ReadyNative:
    if (ready_ || !entered_ || !all_children_ready(id))
      return fail(e, "Player Camera native Ready/postorder rejected");
    ready_ = true;
    return update_native(e);
  case FieldTreePhase::PhysicsInternal:
  case FieldTreePhase::IdleInternal:
    return fail(
        e, "Camera no-smoothing/no-interpolation native clock is disabled");
  case FieldTreePhase::TransformChanged:
    return entered_ ? update_native(e) : true;
  case FieldTreePhase::ExitNative: {
    if (!entered_ || !s->inside)
      return fail(e, "Player Camera native Exit order rejected");
    FieldObjectId current = 0;
    if (!ports_.native_current || !ports_.native_current(current, e) ||
        (current == object_ &&
         (!ports_.make_current || !ports_.make_current(0, e))))
      return false;
    if (current == object_ &&
        !root_->set_canvas_transform(
            FieldTransform{Vec2{1, 0}, Vec2{0, 1}, Vec2{0, 0}}, e))
      return false;
    if (!root_->disconnect_signal(false, "size_changed", id, "_update_scroll",
                                  e))
      return false;
    entered_ = false;
    frame_valid_ = false;
    return true;
  }
  case FieldTreePhase::Deleting:
    if (entered_)
      return fail(e, "Player Camera deleted while in actual Viewport");
    object_ = 0;
    return true;
  case FieldTreePhase::PostEnterNative:
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
  case FieldTreePhase::LocalTransformChanged:
  case FieldTreePhase::VisibilityChanged:
  case FieldTreePhase::Hide:
  case FieldTreePhase::PathChanged:
  case FieldTreePhase::ChildMoved:
  case FieldTreePhase::TreeEntered:
  case FieldTreePhase::TreeExiting:
  case FieldTreePhase::TreeExited:
  case FieldTreePhase::NodeAdded:
  case FieldTreePhase::NodeRemoved:
  case FieldTreePhase::ChildEntered:
  case FieldTreePhase::ChildExiting:
  case FieldTreePhase::ReadySignal:
    e.clear();
    return true;
  default:
    return fail(e, "Player Camera unsupported native notification");
  }
}
bool PodunkPlayerCamera::script_process(FieldObjectId id, FieldTreePhase p,
                                        float delta, bool paused,
                                        std::string &e) {
  if (!live(e) || id != object_ || !frame_valid_ || !epoch_ ||
      !std::isfinite(delta) || delta < 0 ||
      (p != FieldTreePhase::Idle && p != FieldTreePhase::Physics &&
       p != FieldTreePhase::Input))
    return fail(e, "Camera actual source frame/clock unavailable");
  if (p == FieldTreePhase::Physics) {
    if (physics_epoch_ == epoch_)
      return fail(e, "Camera duplicate source Physics clock");
    physics_epoch_ = epoch_;
  }
  return children_->process(id, p, delta, paused, e);
}
bool PodunkPlayerCamera::rebind_tree(FieldNodeTreeRuntime &t, std::string &e) {
  if (!data_ || !object_ || body_->tree() != &t ||
      t.object_domain() != registry_->kernel() ||
      registry_->tree_owner(object_).get() != &t || !t.state(object_) ||
      !t.descriptor(object_) || t.descriptor(object_)->id != native_.id)
    return fail(e, "Player Camera persistent owner migration rejected");
  tree_ = &t;
  frame_valid_ = false;
  e.clear();
  return true;
}
FieldGameCameraHost PodunkPlayerCamera::source_host() {
  FieldGameCameraHost h;
  h.bind = [this](const FieldGameCameraData &d, std::string &e) {
    if (!live(e) || &d != &data_->camera() || !actual_ui(e) ||
        !ports_.connect_player || !ports_.listeners_admitted ||
        !ports_.geometry_admitted || !ports_.native_current ||
        !ports_.make_current)
      return fail(e, "Player Camera actual full native/Ready owners missing");
    return true;
  };
  h.observe = [this](uint32_t id, FieldGameCameraObservation &o,
                     std::string &e) { return observe(id, o, e); };
  h.publish = [this](uint32_t id, const FieldGameCameraState &s,
                     std::string &e) { return publish(id, s, e); };
  h.publish_canvas = [this](uint32_t id, Vec2 origin, Vec2 center,
                            std::string &e) {
    return canvas(id, origin, center, e);
  };
  h.connect_player = [this](uint32_t id, std::function<bool()> stop,
                            std::function<bool()> pause, std::string &e) {
    FieldObjectId player = 0;
    return live(e) && actual_ui(e) && id == native_.id &&
           global_player(player, e) &&
           ports_.connect_player(object_, player, ports_.ui->binding().object,
                                 data_->camera(), std::move(stop),
                                 std::move(pause), e);
  };
  h.scope = [this](uint32_t id, FieldScopeOperation op, Vec2 v,
                   std::string &e) {
    if (!live(e) || id != data_->camera().records().front().arrows_id)
      return fail(e, "Camera actual ScopeArrows source rejected");
    auto &r = children_->arrows();
    bool ok = false;
    switch (op) {
    case FieldScopeOperation::Show:
      ok = r.show(id);
      break;
    case FieldScopeOperation::Hide:
      ok = r.hide(id);
      break;
    case FieldScopeOperation::GlobalPosition:
      ok = r.global_position(id, v);
      break;
    case FieldScopeOperation::Input:
      ok = r.handle_input_events(id);
      break;
    }
    if (!ok)
      e = r.error();
    return ok;
  };
  h.arrow_visible = [this](uint32_t id, Vec2 dir, bool v, std::string &e) {
    if (!live(e) || id != data_->camera().records().front().arrows_id)
      return fail(e, "Camera actual arrow source rejected");
    auto &r = children_->arrows();
    bool ok = r.set_arrow_visible(id, dir, v);
    if (!ok)
      e = r.error();
    return ok;
  };
  h.info_plates_hide = [this](std::string &e) {
    if (!actual_ui(e))
      return false;
    return ports_.info_plates_hide
               ? ports_.info_plates_hide(ports_.ui->binding().object, e)
               : fail(e, "Camera actual UI info plates owner pending");
  };
  h.player_exit_camera = [this](std::string &e) {
    FieldObjectId player = 0;
    return global_player(player, e) &&
           (ports_.player_exit_camera
                ? ports_.player_exit_camera(player, e)
                : fail(e, "Camera actual Player.exit_camera owner pending"));
  };
  h.animation_signal = [this](uint32_t source, uint32_t role, bool started,
                              std::string &e) {
    if (!live(e))
      return false;
    FieldObjectId id = 0;
    const auto &camera = data_->camera().records().front();
    const auto *a = data_->arrows().player_for_target(camera.arrows_id);
    const auto *clip = a ? data_->arrows().clip(a->profile, role) : nullptr;
    if (!started || source != camera.animation_id || !clip || role < 1 ||
        role > data_->camera().animation_lengths().size() ||
        clip->length != data_->camera().animation_lengths()[role - 1] ||
        !actual(source, id, e))
      return fail(e, "Camera actual AnimationPlayer role/source rejected");
    return animation_->play(id, clip->name, e);
  };
  h.current_camera_snapshot = [this](uint32_t id, FieldGameCameraState &out,
                                     std::string &e) {
    FieldObjectId current = 0;
    if (id != native_.id ||
        !global_->object(FieldGlobalMemberRole::CurrentCamera, current, e) ||
        !current || !registry_->object_exists(current))
      return fail(e, "Camera source currentCamera object absent");
    if (current == object_) {
      const auto *s = children_->camera().state(id);
      if (!s)
        return fail(e, "Camera current script body absent");
      out = *s;
      return true;
    }
    return ports_.current_snapshot
               ? ports_.current_snapshot(current, out, e)
               : fail(e, "Camera previous actual camera owner pending");
  };
  h.make_current = [this](uint32_t id, std::string &e) {
    return select(id, e);
  };
  h.set_global_current = [this](uint32_t id, std::string &e) {
    if (!live(e) || id != native_.id)
      return fail(e, "Camera actual global assignment source rejected");
    return global_->set_object(FieldGlobalMemberRole::CurrentCamera, object_,
                               e);
  };
  // Explicitly unsupported factories are encountered only when source calls
  // them. No fake Reference/ObjectID or copied local tween queue is published.
  h.register_tween = [](uint32_t, uint64_t, std::function<bool(float)>,
                        std::string &e) {
    return fail(e, "Camera SceneTreeTween actual Reference factory pending");
  };
  h.kill_tween = [](uint64_t, std::string &e) {
    return fail(e, "Camera SceneTreeTween actual lifetime owner pending");
  };
  h.tween_signal = [](uint64_t, uint32_t, bool, std::string &e) {
    return fail(e, "Camera actual Tween signal owner pending");
  };
  h.coroutine_completed = [](uint64_t, std::string &e) {
    return fail(e, "Camera actual coroutine owner pending");
  };
  h.create_shaker = [](uint32_t, uint64_t, std::function<bool(float)>,
                       std::string &e) {
    return fail(e, "Camera Shaker checked dynamic Node factory pending");
  };
  h.shaker_finished = [](uint64_t, std::string &e) {
    return fail(e, "Camera Shaker actual signal owner pending");
  };
  h.queue_free_shaker = [](uint64_t, std::string &e) {
    return fail(e, "Camera Shaker actual delete queue owner pending");
  };
  h.await_idle_frame = [](uint64_t, std::function<bool()>, std::string &e) {
    return fail(e, "Camera actual SceneTree idle_frame waiter pending");
  };
  h.cancel_idle_frame = [](uint64_t, std::string &e) {
    return fail(e, "Camera actual idle_frame waiter lifetime pending");
  };
  h.stopped_shaking = [](uint32_t, std::string &e) {
    return fail(e, "Camera actual stopped_shaking signal owner pending");
  };
  return h;
}
} // namespace encore::ctr

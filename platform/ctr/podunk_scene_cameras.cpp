#include "podunk_scene_cameras.hpp"
#include <algorithm>
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
bool finite(Vec2 v) { return std::isfinite(v.x) && std::isfinite(v.y); }
} // namespace
bool PodunkSceneCameras::prepare(
    const SceneLeafNativeData &d, const FieldNodeTreeData &s,
    FieldNodeTreeRuntime &t, FieldGlobalRegistry &r, PodunkNativeRoot &root,
    FieldGlobalConstructorRuntime &g, FieldGameCameraRuntime &core,
    FieldCameraArrowsRuntime &arrows, PodunkCameraViewport v, std::string &e) {
  if (data_ || !d.valid() || !s.valid() || !same(d.identity(), s.identity()) ||
      r.poisoned() || root.kernel_object() != r.kernel() ||
      root.viewport_object() != r.root() || !g.data() ||
      g.data()->identity().upstream_commit != d.identity().upstream_commit ||
      !finite(v.logical_size) || v.logical_size.x <= 0 || v.logical_size.y <= 0)
    return fail(e, "Scene cameras actual Viewport/source owners rejected");
  const auto &actual = root.viewport();
  if (!actual.initialized || actual.failed ||
      v.logical_size.x > actual.size.x || v.logical_size.y > actual.size.y ||
      (!v.reference_explicit && (v.logical_size.x != actual.size.x ||
                                 v.logical_size.y != actual.size.y)))
    return fail(
        e,
        "Scene cameras logical size lacks actual Viewport reference adapter");
  data_ = &d;
  source_ = &s;
  tree_ = &t;
  registry_ = &r;
  root_ = &root;
  global_ = &g;
  core_ = &core;
  arrows_ = &arrows;
  viewport_ = v;
  e.clear();
  return true;
}
bool PodunkSceneCameras::register_player(PodunkPlayerCamera &p,
                                         std::string &e) {
  if (!data_ || player_ || p.registry() != registry_ || p.tree() != tree_)
    return fail(e, "Scene camera Player owner crossed actual Registry/tree");
  player_ = &p;
  e.clear();
  return true;
}
bool PodunkSceneCameras::actual_external(FieldObjectId id,std::string&e)const{
 auto i=external_.find(id);auto tree=registry_?registry_->tree_owner(id):nullptr;
 FieldIdentity identity;const auto*n=tree?tree->state(id):nullptr;const auto*d=tree?tree->descriptor(id):nullptr;
 if(i==external_.end()||!n||!n->alive||!d||d->native_class!="Camera2D"||
    !tree->object_identity(id,identity)||!same(identity,i->second.identity)||
    identity.upstream_commit!=data_->identity().upstream_commit)
  return fail(e,"External Camera2D actual recipe/Registry owner rejected");
 e.clear();return true;
}
bool PodunkSceneCameras::register_external(FieldObjectId id,const FieldIdentity&identity,
 ExternalSnapshot snapshot,ExternalSelect select,std::string&e){
 if(!data_||!id||!snapshot||!select||cameras_.count(id)||external_.count(id)||
    (player_&&player_->owns(id)))return fail(e,"External Camera2D registration owner rejected");
 external_.emplace(id,ExternalCamera{identity,std::move(snapshot),std::move(select)});
 if(!actual_external(id,e)){external_.erase(id);return false;}e.clear();return true;
}
bool PodunkSceneCameras::unregister_external(FieldObjectId id,std::string&e){
 if(!actual_external(id,e))return false;
 if(current_==id&&!make_current(0,e))return false;
 external_.erase(id);e.clear();return true;
}
bool PodunkSceneCameras::owns(const FieldNodeDescriptor &d) const {
  auto *r = data_ ? data_->record(d.id) : nullptr;
  return r &&
         (r->kind == SceneLeafKind::GameCamera ||
          r->kind == SceneLeafKind::IntroCamera) &&
         d.native_class == r->native_class;
}
bool PodunkSceneCameras::owns(FieldObjectId id) const {
  return cameras_.count(id) != 0;
}
bool PodunkSceneCameras::actual(FieldObjectId id, std::string &e) const {
  auto i = cameras_.find(id);
  auto *s = tree_ ? tree_->state(id) : nullptr;
  auto *d = tree_ ? tree_->descriptor(id) : nullptr;
  FieldIdentity identity;
  if (!data_ || i == cameras_.end() || !s || !s->alive || !d ||
      !registry_->object_exists(id) ||
      registry_->tree_owner(id).get() != tree_ ||
      !tree_->object_identity(id, identity) ||
      !same(identity, data_->identity()) || !owns(*d) ||
      i->second.source->id != d->id)
    return fail(e, "Scene camera actual ObjectDB/native body rejected");
  return true;
}
bool PodunkSceneCameras::source(uint32_t stable, FieldObjectId &out,
                                std::string &e) const {
  for (const auto &i : cameras_)
    if (i.second.source->id == stable) {
      if (!actual(i.first, e))
        return false;
      out = i.first;
      return true;
    }
  return fail(e, "Scene camera source has not been allocated");
}
bool PodunkSceneCameras::construct(FieldObjectId id,
                                   const FieldNodeDescriptor &d,
                                   const FieldIdentity &identity,
                                   std::string &e) {
  if (!owns(d) || cameras_.count(id) || !same(identity, data_->identity()) ||
      !registry_->object_exists(id) ||
      registry_->tree_owner(id).get() != tree_ || !tree_->state(id))
    return fail(e, "Scene Camera2D real constructor rejected");
  const auto &r = *data_->record(d.id);
  if (r.camera.zoom.x != 1 || r.camera.zoom.y != 1)
    return fail(e, "Scene Camera2D unsupported source zoom");
  Camera c;
  c.source = &r;
  c.binding = {identity, d.id,         d.class_index, 0x454e0070,
               1,        d.script_sha, d.native_class};
  c.body.id = d.id;
  c.body.position = tree_->state(id)->local[2];
  c.body.offset = r.camera.offset;
  c.body.current = r.camera.current;
  c.body.limits = {r.camera.limits[1], r.camera.limits[0], r.camera.limits[2],
                   r.camera.limits[3]};
  cameras_.emplace(id, std::move(c));
  e.clear();
  return true;
}
bool PodunkSceneCameras::bind(FieldObjectId id, FieldNodeBinding &out,
                              std::string &e) {
  if (!actual(id, e))
    return false;
  out = cameras_.at(id).binding;
  return true;
}
bool PodunkSceneCameras::native_current(FieldObjectId &out,
                                        std::string &e) const {
  if (!data_ || registry_->poisoned() ||
      !registry_->object_exists(root_->viewport_object()))
    return fail(e, "Camera Viewport owner not live");
  if (current_ &&
      (!(player_ && player_->owns(current_)) &&
       !(external_.count(current_) ? actual_external(current_,e) : actual(current_,e))))
    return false;
  if (current_ && !registry_->object_exists(current_))
    return fail(e, "Viewport current Camera2D has been freed");
  out = current_;
  e.clear();
  return true;
}
bool PodunkSceneCameras::make_current(FieldObjectId id, std::string &e) {
  if (!data_)
    return fail(e, "Camera Viewport registry unprepared");
  bool external = player_ && player_->owns(id);
  bool foreign = external_.count(id)!=0;
  if (id && !external && !(foreign ? actual_external(id,e) : actual(id, e)))
    return false;
  auto selected_tree=id?registry_->tree_owner(id):nullptr;
  const auto *s = selected_tree ? selected_tree->state(id) : nullptr;
  if (id && (!s || !s->inside))
    return fail(e, "Camera selection requires actual entered node");
  current_ = id;
  for (auto &i : cameras_) {
    if (!i.second.entered)
      continue;
    i.second.body.current = i.first == id;
    if (i.second.source->kind == SceneLeafKind::GameCamera &&
        core_->state(i.second.source->id) &&
        !core_->native_current_changed(i.second.source->id, i.first == id)) {
      e = core_->error();
      return false;
    }
  }
  if (player_ && player_->object() && tree_->state(player_->object())->inside &&
      !player_->native_select(external, e))
    return false;
  for(auto&i:external_){
    auto owner=registry_->tree_owner(i.first);const auto*node=owner?owner->state(i.first):nullptr;
    if(node&&node->inside&&!i.second.select(i.first,i.first==id,e))return false;
  }
  if (id && !external && !foreign)
    return update(id, e);
  e.clear();
  return true;
}
bool PodunkSceneCameras::current_snapshot(FieldObjectId id,
                                          FieldGameCameraState &out,
                                          std::string &e) const {
  if (!id)
    return fail(e, "Camera snapshot cannot use null ObjectID");
  if (player_ && player_->owns(id))
    return player_->native_snapshot(out, e);
  if(external_.count(id))return actual_external(id,e)&&external_.at(id).snapshot(id,out,e);
  if (!actual(id, e))
    return false;
  out = cameras_.at(id).body;
  if (auto *s = core_->state(out.id))
    out.camareas = s->camareas;
  e.clear();
  return true;
}
bool PodunkSceneCameras::all_ready(FieldObjectId id) const {
  const auto *s = tree_->state(id);
  if (!s || !s->alive || !s->bound || !s->inside)
    return false;
  for (auto child : s->children) {
    const auto *n = tree_->state(child);
    if (!n || !n->ready_notified || n->ready_first || !all_ready(child))
      return false;
  }
  return true;
}
bool PodunkSceneCameras::update(FieldObjectId id, std::string &e) {
  if (!actual(id, e))
    return false;
  auto &c = cameras_.at(id);
  if (!c.entered || current_ != id) {
    e.clear();
    return true;
  }
  FieldTransform world;
  if (!tree_->world_transform(id, world, e))
    return false;
  if (world[0].x != 1 || world[0].y != 0 || world[1].x != 0 || world[1].y != 1)
    return fail(e,
                "Camera native no-smoothing source transform is unsupported");
  Vec2 origin{world[2].x - viewport_.logical_size.x * .5f,
              world[2].y - viewport_.logical_size.y * .5f};
  if (origin.x < c.body.limits[1])
    origin.x = float(c.body.limits[1]);
  if (origin.x + viewport_.logical_size.x > c.body.limits[2])
    origin.x = float(c.body.limits[2]) - viewport_.logical_size.x;
  if (origin.y + viewport_.logical_size.y > c.body.limits[3])
    origin.y = float(c.body.limits[3]) - viewport_.logical_size.y;
  if (origin.y < c.body.limits[0])
    origin.y = float(c.body.limits[0]);
  origin.x += c.body.offset.x;
  origin.y += c.body.offset.y;
  c.body.canvas_origin = origin;
  c.body.screen_center = {origin.x + viewport_.logical_size.x * .5f,
                          origin.y + viewport_.logical_size.y * .5f};
  const auto physical = root_->viewport().size;
  return root_->set_canvas_transform(
      {Vec2{1, 0}, Vec2{0, 1},
       Vec2{(physical.x - viewport_.logical_size.x) * .5f - origin.x,
            (physical.y - viewport_.logical_size.y) * .5f - origin.y}},
      e);
}
bool PodunkSceneCameras::publish(uint32_t stable, const FieldGameCameraState &s,
                                 std::string &e) {
  FieldObjectId id;
  if (!source(stable, id, e) || s.id != stable || !finite(s.position) ||
      !finite(s.offset))
    return fail(e, "Scene Camera2D property assignment source rejected");
  auto &c = cameras_.at(id);
  c.body.offset = s.offset;
  c.body.limits = s.limits;
  c.body.base = s.base;
  c.body.shake = s.shake;
  c.body.camarea = s.camarea;
  c.body.camareas = s.camareas;
  c.body.position = s.position;
  auto t = tree_->state(id)->local;
  t[2] = s.position;
  if (!tree_->set_local(id, t, e))
    return false;
  return update(id, e);
}
bool PodunkSceneCameras::observe(uint32_t stable,
                                 FieldGameCameraObservation &out,
                                 std::string &e) {
  const auto *binding = data_->record(stable);
  if (!binding)
    return fail(e, "Scene camera source observation missing");
  FieldObjectId id;
  if (!source(binding->kind == SceneLeafKind::CameraAnimation ? binding->owner
                                                              : stable,
              id, e))
    return false;
  auto &c = cameras_.at(id);
  const auto *n = tree_->state(id);
  if (!c.entered || !n->bound || !n->inside)
    return fail(e, "Scene camera actual entered source required");
  for (auto p = id; p;) {
    const auto *s = tree_->state(p);
    if (!s || !s->alive || !s->inside || !s->bound)
      return fail(e, "Scene camera live ancestor unbound");
    p = s->parent;
  }
  FieldGameCameraObservation o{};
  o.alive = o.ancestors_admitted = o.listeners_admitted = true;
  o.native_children_ready = all_ready(id);
  o.viewport = viewport_.logical_size;
  o.can_process =
      tree_->can_process(binding->kind == SceneLeafKind::CameraAnimation
                             ? tree_->source_object(stable)
                             : id,
                         paused_);
  if (!tree_->world_transform(n->parent, o.parent_world, e))
    return false;
  std::shared_ptr<const GlobalLoadObjectArray> party;
  if (!global_->array(FieldGlobalMemberRole::PartyObjects, party, e) ||
      !party || party->values.empty() ||
      !registry_->object_exists(party->values.front()))
    return fail(e, "Scene camera real global.get_player unavailable");
  o.global_player = party->values.front();
  o.parent_is_global_player = n->parent == o.global_player;
  const auto *d = core_->data() ? core_->data()->record(c.body.id) : nullptr;
  if (!d || o.parent_is_global_player)
    return fail(e, "Scene camera source parent is not audited JumpArea");
  auto area = tree_->source_object(d->area_id),
       shape = tree_->source_object(d->shape_id),
       arrows = tree_->source_object(d->arrows_id);
  if (!area || !shape || !arrows || !all_ready(area) ||
      !tree_->state(shape)->ready_notified)
    return fail(e, "Scene camera actual Area/shape/arrows closure missing");
  o.native_geometry_admitted = true;
  o.scope_visible = (tree_->state(arrows)->flags & 2) != 0;
  FieldObjectId current;
  if (!global_->object(FieldGlobalMemberRole::CurrentCamera, current, e))
    return false;
  auto *current_source = tree_->descriptor(current);
  o.current_camera = current_source ? current_source->id : 0;
  const auto *state = core_->state(c.body.id);
  if (state && state->ready) {
    FieldGameCameraObservation player;
    if (!player_ || !player_->source_observation(player, e))
      return fail(
          e, "Scene camera actual Player/UI/Input getter owner unavailable");
    o.in_battle = player.in_battle;
    o.player_state = player.player_state;
    o.player_damaged = player.player_damaged;
    o.controls = player.controls;
    o.scope_pressed = player.scope_pressed;
    o.scope_just_pressed = player.scope_just_pressed;
    o.scope_just_released = player.scope_just_released;
  }
  out = o;
  e.clear();
  return true;
}
bool PodunkSceneCameras::apply(FieldGameCameraHost &h, std::string &e) {
  if (!data_ || applied_)
    return fail(e, "Scene camera actual source Host already bound/unprepared");
  h.bind = [this](const FieldGameCameraData &d, std::string &e) {
    if (!d.valid() || !same(d.identity(), data_->identity()))
      return fail(e, "Camera typed source identity rejected");
    e.clear();
    return true;
  };
  h.observe = [this](uint32_t id, FieldGameCameraObservation &o,
                     std::string &e) { return observe(id, o, e); };
  h.publish = [this](uint32_t id, const FieldGameCameraState &s,
                     std::string &e) { return publish(id, s, e); };
  h.publish_canvas = [this](uint32_t stable, Vec2 origin, Vec2 center,
                            std::string &e) {
    FieldObjectId id;
    if (!source(stable, id, e) || current_ != id || !finite(origin) ||
        !finite(center))
      return fail(e, "Camera canvas assignment has no actual selected owner");
    auto &c = cameras_.at(id);
    c.body.canvas_origin = origin;
    c.body.screen_center = center;
    auto v = root_->viewport().size;
    return root_->set_canvas_transform(
        {Vec2{1, 0}, Vec2{0, 1},
         Vec2{(v.x - viewport_.logical_size.x) * .5f - origin.x,
              (v.y - viewport_.logical_size.y) * .5f - origin.y}},
        e);
  };
  h.current_camera_snapshot = [this](uint32_t, FieldGameCameraState &s,
                                     std::string &e) {
    FieldObjectId current;
    if (!global_->object(FieldGlobalMemberRole::CurrentCamera, current, e))
      return false;
    return current_snapshot(current, s, e);
  };
  h.make_current = [this](uint32_t stable, std::string &e) {
    FieldObjectId id;
    return source(stable, id, e) && make_current(id, e);
  };
  h.set_global_current = [this](uint32_t stable, std::string &e) {
    FieldObjectId id;
    return source(stable, id, e) &&
           global_->set_object(FieldGlobalMemberRole::CurrentCamera, id, e);
  };
  h.scope = [this](uint32_t id, FieldScopeOperation op, Vec2 p,
                   std::string &e) {
    bool ok = false;
    switch (op) {
    case FieldScopeOperation::Show:
      ok = arrows_->show(id);
      break;
    case FieldScopeOperation::Hide:
      ok = arrows_->hide(id);
      break;
    case FieldScopeOperation::GlobalPosition:
      ok = arrows_->global_position(id, p);
      break;
    case FieldScopeOperation::Input:
      ok = arrows_->handle_input_events(id);
      break;
    }
    if (!ok)
      e = arrows_->error();
    return ok;
  };
  h.arrow_visible = [this](uint32_t id, Vec2 d, bool v, std::string &e) {
    if (!arrows_->set_arrow_visible(id, d, v)) {
      e = arrows_->error();
      return false;
    }
    return true;
  };
  h.connect_player = [](uint32_t, std::function<bool()>, std::function<bool()>,
                        std::string &e) {
    return fail(
        e, "JumpArea Camera2D cannot impersonate Player Camera signal owner");
  };
  applied_ = true;
  e.clear();
  return true;
}
bool PodunkSceneCameras::phase(FieldObjectId id, FieldTreePhase p, float,
                               bool paused, bool, std::string &e) {
  if (!actual(id, e))
    return false;
  paused_ = paused;
  auto &c = cameras_.at(id);
  auto *s = tree_->state(id);
  switch (p) {
  case FieldTreePhase::EnterNative:
    if (c.entered || !s->inside || !s->bound)
      return fail(e, "Camera actual Enter rejected");
    if (!root_->connect_signal(false, data_->viewport_size_signal(), id,
                               data_->camera_scroll_method(), e))
      return false;
    c.entered = true;
    return c.body.current ? make_current(id, e) : true;
  case FieldTreePhase::ReadyNative:
    if (!c.entered || !s->ready_notified || s->ready_first || !all_ready(id))
      return fail(e, "Camera actual native Ready rejected");
    c.ready = true;
    return update(id, e);
  case FieldTreePhase::TransformChanged:
    return update(id, e);
  case FieldTreePhase::ExitNative:
    if (!c.entered)
      return fail(e, "Camera actual Exit rejected");
    if (current_ == id) {
      current_ = 0;
      if (!root_->set_canvas_transform({Vec2{1, 0}, Vec2{0, 1}, Vec2{}}, e))
        return false;
    }
    if (!root_->disconnect_signal(false, data_->viewport_size_signal(), id,
                                  data_->camera_scroll_method(), e))
      return false;
    c.entered = false;
    return true;
  case FieldTreePhase::Deleting:
    return release(id, e);
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
  case FieldTreePhase::ChildMoved:
  case FieldTreePhase::PathChanged:
  case FieldTreePhase::PostEnterNative:
    return true;
  default:
    return fail(e, "Camera no-smoothing source native clock unsupported");
  }
}
bool PodunkSceneCameras::method(FieldObjectId id, std::string_view name,
                                const std::vector<FieldDeferredValue> &args,
                                std::string &e) {
  if (!actual(id, e) || name != data_->camera_scroll_method() || !args.empty())
    return fail(e, "Unknown Camera2D native method or arguments");
  return update(id, e);
}
bool PodunkSceneCameras::deferred(const FieldDeferredMessage &m,
                                  std::string &e) {
  return method(m.object, m.member, m.args, e);
}
bool PodunkSceneCameras::release(FieldObjectId id, std::string &e) {
  if (!actual(id, e) || cameras_.at(id).entered)
    return fail(e, "Camera release before actual Exit");
  cameras_.erase(id);
  return true;
}
bool PodunkSceneCameras::finish_factory(std::string &e) const {
  if (!data_ || !applied_)
    return fail(e, "Camera native source Host not installed");
  for (const auto &r : data_->records())
    if (r.kind == SceneLeafKind::GameCamera ||
        r.kind == SceneLeafKind::IntroCamera) {
      auto id = tree_->source_object(r.id);
      if (!id || !actual(id, e) || !tree_->state(id)->bound)
        return fail(e, "Camera actual source factory incomplete");
    }
  e.clear();
  return true;
}
} // namespace encore::ctr

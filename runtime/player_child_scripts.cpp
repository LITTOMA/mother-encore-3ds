#include "encore/player_child_scripts.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool nullable(const FieldNodeTreeRuntime &t, FieldObjectId from,
              std::string_view path, FieldObjectId &out, std::string &e) {
  out = 0;
  if (path.empty() || path.front() == '/' || path.find(':') != path.npos)
    return fail(e, "Fetcher relative native NodePath unsupported");
  auto at = from;
  size_t begin = 0;
  while (begin <= path.size()) {
    auto end = path.find('/', begin);
    if (end == path.npos)
      end = path.size();
    auto part = path.substr(begin, end - begin);
    auto *s = t.state(at);
    if (!s || !s->alive)
      return fail(e, "Fetcher NodePath actual base is absent");
    if (part == "..") {
      at = s->parent;
      if (!at) {
        out = 0;
        e.clear();
        return true;
      }
    } else if (!part.empty() && part != ".") {
      FieldObjectId found = 0;
      for (auto child : s->children) {
        auto *c = t.state(child);
        if (!c || !c->alive || c->parent != at)
          return fail(e, "Fetcher native child ownership corrupt");
        if (c->name == part) {
          if (found)
            return fail(e, "Fetcher NodePath ambiguous actual names");
          found = child;
        }
      }
      if (!found) {
        out = 0;
        e.clear();
        return true;
      }
      at = found;
    }
    if (end == path.size())
      break;
    begin = end + 1;
  }
  out = at;
  e.clear();
  return true;
}

} // namespace
bool PlayerChildScriptsRuntime::prepare(
    const PlayerChildScriptsData &d, const PlayerInitializationData &p,
    const PlayerReadyData &ready, FieldNodeTreeRuntime &t,
    FieldGlobalRegistry &r, PlayerInitializationBody &body,
    PlayerChildScriptNative &native, std::string &e) {
  if (data_ || !d.valid() || !p.valid() || !ready.valid() ||
      d.player_ir_sha256() != p.ir_sha256() ||
      ready.initialization_ir_sha256() != p.ir_sha256() ||
      !body.constructed() || body.data() != &p || body.tree() != &t ||
      native.registry() != &r || native.tree() != &t ||
      t.object_domain() != r.kernel())
    return fail(e, "Player child actual owner preparation rejected");
  data_ = &d;
  player_data_ = &p;
  ready_data_ = &ready;
  tree_ = &t;
  registry_ = &r;
  body_ = &body;
  native_ = &native;
  tint_ = d.tint().color;
  return true;
}
bool PlayerChildScriptsRuntime::construct(FieldObjectId id,
                                          const FieldNodeDescriptor &descriptor,
                                          std::string &e) {
  if (!data_ || poisoned_ || !id ||
      (!tree_->descriptor(id) || tree_->descriptor(id)->id != descriptor.id ||
       tree_->descriptor(id)->script != descriptor.script ||
       tree_->descriptor(id)->script_sha != descriptor.script_sha))
    return fail(e, "Player child source attachment absent");
  auto i = std::find_if(data_->records().begin(), data_->records().end(),
                        [&](const auto &x) { return x.id == descriptor.id; });
  if (i == data_->records().end() || objects_.count(i->role) ||
      descriptor.script != i->script ||
      descriptor.script_sha != i->script_sha ||
      descriptor.native_class != i->native_class)
    return fail(e, "Player child exact script attachment rejected");
  FieldIdentity identity{};
  if (!tree_->object_identity(id, identity) ||
      identity.scene_id != data_->identity().scene_id ||
      identity.source_sha256 != data_->identity().source_sha256 ||
      identity.upstream_commit != data_->identity().upstream_commit)
    return fail(e, "Player child attachment source identity rejected");
  auto *s = tree_->state(id);
  if (!s || s->inside || s->bound)
    return fail(e, "Player child attachment cursor rejected");
  objects_.emplace(i->role, id);
  return true;
}
bool PlayerChildScriptsRuntime::resolve(const PlayerChildScriptRecord &r,
                                        FieldObjectId &id,
                                        std::string &e) const {
  if (!tree_->get_node(body_->object(), r.path, id, e))
    return false;
  auto *d = tree_->descriptor(id);
  if (!d || d->id != r.id || d->script != r.script ||
      d->script_sha != r.script_sha || d->native_class != r.native_class)
    return fail(e, "Player child relative instance lookup rejected");
  return true;
}
bool PlayerChildScriptsRuntime::live(FieldObjectId id, std::string &e) const {
  if (!data_ || poisoned_ || !id || !registry_->object_exists(id) ||
      registry_->tree_owner(id).get() != tree_)
    return fail(e, "Player child actual ObjectDB lifetime rejected");
  auto *s = tree_->state(id);
  FieldIdentity identity{};
  if (!s || !s->alive || !tree_->object_identity(id, identity) ||
      identity.scene_id != data_->identity().scene_id ||
      identity.source_sha256 != data_->identity().source_sha256 ||
      identity.upstream_commit != data_->identity().upstream_commit)
    return fail(e, "Player child actual scene binding rejected");
  return true;
}
FieldObjectId PlayerChildScriptsRuntime::actual(uint32_t role) const {
  auto i = objects_.find(role);
  return i == objects_.end() ? 0 : i->second;
}
bool PlayerChildScriptsRuntime::onready_complete(uint32_t role) const {
  return ready_roles_.count(role) && !poisoned_;
}
bool PlayerChildScriptsRuntime::connect_packed_signals(std::string &e) {
  if (!data_ || connected_ || objects_.size() != data_->records().size())
    return fail(e, "Player child full factory signal cursor rejected");
  auto id = actual(1);
  FieldObjectId expected = 0;
  if (!resolve(*data_->record(1), expected, e) || expected != id ||
      !live(id, e) ||
      !tree_->get_node(id, data_->emote().animation_path, emote_animation_, e))
    return false;
  auto *d = tree_->descriptor(emote_animation_);
  if (!d || d->native_class != "AnimationPlayer" || !live(emote_animation_, e))
    return fail(e,
                "Player emote actual AnimationPlayer signal source rejected");
  if (!native_->connect_animation_started(
          emote_animation_, id, data_->emote().signal, data_->emote().method,
          [this](std::string_view clip, std::string &e) {
            return animation_started(clip, e);
          },
          e)) {
    poisoned_ = true;
    return false;
  }
  connected_ = true;
  return true;
}
bool PlayerChildScriptsRuntime::bind_camera(SourceRandom &random,
                                            FieldGameCameraHost h,
                                            std::string &e) {
  if (!data_ || camera_bound_ ||
      !camera_.initialize(data_->camera(), random, std::move(h), e))
    return false;
  camera_bound_ = true;
  return true;
}
bool PlayerChildScriptsRuntime::bind_arrows(
    FieldCameraArrowsHost h, FieldCameraArrowsNativeOwner &native,
    std::string &e) {
  if (!data_ || arrows_bound_ ||
      !arrows_.initialize_borrowed(data_->arrows(), std::move(h), native, e))
    return false;
  arrows_bound_ = true;
  return true;
}
bool PlayerChildScriptsRuntime::tint_targets(std::string &e) {
  auto id = actual(2);
  if (!live(id, e))
    return false;
  for (const auto &path : data_->tint().paths) {
    FieldObjectId target = 0;
    if (!nullable(*tree_, id, path, target, e))
      return false;
    if (!target)
      continue;
    if (!live(target, e))
      return false;
    auto *s = tree_->state(target);
    if (!s || (s->flags & 2) == 0)
      return fail(e, "Player tint source target is not actual CanvasItem");
    targets_.push_back(target);
  }
  return true;
}
bool PlayerChildScriptsRuntime::ready(FieldObjectId id, FieldTreePhase phase,
                                      const FieldNodeBinding &binding,
                                      std::string &e) {
  if (phase != FieldTreePhase::ReadyScript || !live(id, e))
    return fail(e, "Player child actual SourceReady notification required");
  auto *d = tree_->descriptor(id);
  auto *s = tree_->state(id);
  auto i = std::find_if(data_->records().begin(), data_->records().end(),
                        [&](const auto &x) { return x.id == d->id; });
  if (i == data_->records().end() || actual(i->role) != id ||
      ready_roles_.count(i->role) || !s->inside || s->ready_first ||
      !s->ready_notified || !s->bound || binding.stable_id != d->id ||
      binding.native_class != d->native_class ||
      binding.class_index != d->class_index ||
      binding.script_sha != d->script_sha ||
      binding.identity.scene_id != data_->identity().scene_id ||
      binding.identity.source_sha256 != data_->identity().source_sha256 ||
      binding.identity.upstream_commit != data_->identity().upstream_commit)
    return fail(e, "Player child source Ready lifecycle rejected");
  bool ok = false;
  if (i->role == 1) {
    if (!connected_)
      return fail(e, "Player emote original packed connection missing");
    if (!tree_->get_node(id, data_->emote().animation_path, emote_animation_,
                         e))
      return false;
    if (!nullable(*tree_, id, data_->emote().object_path, emote_object_, e))
      return false;
    ok = true;
  } else if (i->role == 2)
    ok = tint_targets(e);
  else if (i->role == 3) {
    if (!camera_bound_)
      return fail(e, "Player actual Camera native/source host pending");
    if (!camera_created_) {
      if (!camera_.create(i->id)) {
        e = camera_.error();
        poisoned_ = true;
        return false;
      }
      camera_created_ = true;
    }
    ok = camera_.ready(i->id);
    if (!ok)
      e = camera_.error();
  } else if (i->role == 4) {
    if (!arrows_bound_)
      return fail(e, "Player actual MapArrows native/source host pending");
    if (!arrows_created_) {
      if (!arrows_.create(i->id)) {
        e = arrows_.error();
        poisoned_ = true;
        return false;
      }
      arrows_created_ = true;
    }
    ok = arrows_.ready(i->id);
    if (!ok)
      e = arrows_.error();
  }
  if (!ok) {
    poisoned_ = true;
    return false;
  }
  ready_roles_.insert(i->role);
  return true;
}
bool PlayerChildScriptsRuntime::animation_started(std::string_view clip,
                                                  std::string &e) {
  auto id = actual(1);
  if (!connected_ || !live(id, e))
    return fail(e, "Player emote original animation signal owner absent");
  auto &policy = data_->emote();
  const bool sensitive =
      std::find(policy.sensitive_clips.begin(), policy.sensitive_clips.end(),
                clip) != policy.sensitive_clips.end();
  bool write = !sensitive;
  float x = policy.other_scale;
  if (sensitive) {
    auto *s = tree_->state(id);
    FieldObjectId ancestor = s ? s->parent : 0;
    bool found = false;
    while (ancestor) {
      if (!live(ancestor, e))
        return false;
      if (ancestor == body_->object()) {
        found = true;
        break;
      }
      auto *d = tree_->descriptor(ancestor);
      auto *source = d ? player_data_->recipe().record(d->id) : nullptr;
      if (!d || !source || d->script != source->script ||
          d->script_sha != source->script_sha)
        return fail(e, "Player emote ancestor method admission pending");
      ancestor = tree_->state(ancestor)->parent;
    }
    if (!found)
      return fail(e, "Player emote get_direction ancestor missing");
    PlayerInitializationMember value;
    if (!body_->member(policy.direction_member, value, e) || value.kind != 7)
      return fail(e, "Player emote actual direction body missing");
    if (value.vector[0] < 0) {
      x = policy.negative_scale;
      write = true;
    }
  }
  if (!write)
    return true;
  auto local = tree_->state(id)->local;
  if (local[0].y != 0 || local[1].x != 0)
    return fail(e, "Player emote rotated scale setter pending");
  local[0].x = x;
  return tree_->set_local(id, local, e);
}
bool PlayerChildScriptsRuntime::set_bubble_offset(std::string &e) {
  auto id = actual(1);
  if (!onready_complete(1) || !live(id, e) || !live(emote_object_, e))
    return fail(e, "Player emote object onready missing");
  PlayerFetcherSpriteState value;
  uint32_t height = 0;
  if (!native_->sprite(emote_object_, value, e) || !value.rows ||
      !native_->texture_height(value.texture, height, e))
    return false;
  auto local = tree_->state(id)->local;
  local[2].y = -(float(height) / float(value.rows) + data_->emote().padding);
  return tree_->set_local(id, local, e);
}
bool PlayerChildScriptsRuntime::set_tint(const FieldColor &color,
                                         std::string &e) {
  if (!data_ || !live(actual(2), e) ||
      !std::all_of(color.begin(), color.end(),
                   [](float x) { return std::isfinite(x); }))
    return fail(e, "Player tint actual color rejected");
  tint_ = color;
  if (targets_.empty() && !tint_targets(e))
    return false;
  for (auto id : targets_)
    if (!live(id, e) || !tree_->set_modulate(id, tint_, true, e)) {
      poisoned_ = true;
      return false;
    }
  if (!native_->emit_tint(actual(2), data_->tint().signal, tint_, e)) {
    poisoned_ = true;
    return false;
  }
  return true;
}
bool PlayerChildScriptsRuntime::connect_tint(PlayerChildScriptsRuntime &other,
                                             std::string &e) {
  if (!data_ || !other.data_ || registry_ != other.registry_ ||
      !live(actual(2), e) || !other.live(other.actual(2), e))
    return fail(e, "Player tint target same ObjectDB required");
  if (!native_->connect_tint(actual(2), other.actual(2), data_->tint().signal,
                             other.data_->tint().method, e))
    return false;
  return other.set_tint(tint_, e);
}
bool PlayerChildScriptsRuntime::process(FieldObjectId id, FieldTreePhase phase,
                                        float delta, bool paused,
                                        std::string &e) {
  if (!live(id, e) || !std::isfinite(delta) || delta < 0)
    return fail(e, "Player child source process invalid");
  if (!tree_->can_process(id, paused))
    return true;
  if (id != actual(3) || !onready_complete(3))
    return fail(e, "Player child unsupported source process notification");
  bool ok =
      phase == FieldTreePhase::Idle ? camera_.idle(data_->record(3)->id, delta)
      : phase == FieldTreePhase::Physics
          ? camera_.physics(data_->record(3)->id, delta)
      : phase == FieldTreePhase::Input ? camera_.input(data_->record(3)->id)
                                       : false;
  if (!ok) {
    e = camera_.error();
    poisoned_ = true;
  }
  return ok;
}
bool PlayerChildScriptsRuntime::exit(FieldObjectId id, std::string &e) {
  // None of these source scripts has an _exit_tree body. Native owner exit
  // handles Canvas/viewport/process groups; source fields/waits remain alive.
  return live(id, e);
}
bool PlayerChildScriptsRuntime::deleting(FieldObjectId id, std::string &e) {
  if (!live(id, e))
    return false;
  if (id == actual(3) && camera_created_) {
    if (!camera_.exit_tree(data_->record(3)->id)) {
      e = camera_.error();
      return false;
    }
  } else if (id == actual(4) && arrows_created_) {
    if (!arrows_.exit_tree(data_->record(4)->id)) {
      e = arrows_.error();
      return false;
    }
  }
  return true;
}
bool PlayerChildScriptsRuntime::rebind_tree(FieldNodeTreeRuntime &t,
                                            std::string &e) {
  if (!data_ || poisoned_ || body_->tree() != &t || native_->tree() != &t ||
      t.object_domain() != registry_->kernel())
    return fail(e,
                "Player child actual native/body tree ownership not rebound");
  for (const auto &entry : objects_) {
    auto id = entry.second;
    auto *source = data_->record(entry.first);
    auto *d = t.descriptor(id);
    if (!registry_->object_exists(id) ||
        registry_->tree_owner(id).get() != &t || !source || !d ||
        d->id != source->id || d->script != source->script ||
        d->script_sha != source->script_sha)
      return fail(
          e, "Player child persistent same ObjectID/source transfer rejected");
  }
  for (auto id : targets_)
    if (!registry_->object_exists(id) || registry_->tree_owner(id).get() != &t)
      return fail(e, "Player tint persistent target transfer rejected");
  if (emote_animation_ && registry_->tree_owner(emote_animation_).get() != &t)
    return fail(e, "Player emote persistent animation transfer rejected");
  if (emote_object_ && registry_->tree_owner(emote_object_).get() != &t)
    return fail(e, "Player emote persistent object transfer rejected");
  tree_ = &t;
  return true;
}
} // namespace encore::upstream

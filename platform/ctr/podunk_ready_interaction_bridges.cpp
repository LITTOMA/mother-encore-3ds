#include "podunk_ready_interaction_bridges.hpp"
#include "podunk_global_host.hpp"
#include <algorithm>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
bool PodunkReadyInteractionBridges::prepare(PodunkReadyInteractionInput i,
                                            std::string &e) {
  if (prepared_ || !i.scene.sources || !i.scene.sources->valid() ||
      !i.scene.continuation || !i.scene.tree || !i.scene.native ||
      !i.scene.player || !i.scene.animated || !i.consumers.sparkles ||
      !i.consumers.openable || !i.consumers.prompt || !i.physics || !i.audio ||
      !i.motion || !i.motion->valid() || !i.source || !i.frame)
    return fail(e, "Ready interaction concrete owners incomplete");
  auto *r = i.scene.continuation->registry();
  auto *bus = i.scene.continuation->signals();
  // Preparation precedes the actual Player and native leaf factories.
  // Check their ownership when source callbacks execute, after construction.
  if (!r || !bus || bus->registry() != r ||
      i.scene.native->canvas_tree() != i.scene.tree ||
      i.scene.native->canvas_data() != &i.scene.sources->canvas() ||
      i.scene.sources->openable().area_connections().size() != 2)
    return fail(e, "Ready interaction source/Tree/ObjectDB owners differ or "
                   "source Area symbols absent");
  input_ = std::move(i);
  prepared_ = true;
  e.clear();
  return true;
}
bool PodunkReadyInteractionBridges::source(uint32_t id, FieldObjectId &out,
                                           std::string &e, bool entered) const {
  if (!prepared_ || !input_.source(id, out, e))
    return false;
  auto *r = input_.scene.continuation->registry();

  auto *n = input_.scene.tree->state(out);
  auto *d = input_.scene.tree->descriptor(out);
  auto *proof = input_.scene.sources->tree().record(id);
  FieldIdentity identity{};
  bool has_identity = input_.scene.tree->object_identity(out, identity);
  if (!n || !n->alive || n->queued || !d || !proof || d->id != id ||
      d->script != proof->script || d->script_sha != proof->script_sha ||
      d->class_index != proof->class_index || !has_identity ||
      identity.scene_id != input_.scene.sources->tree().identity().scene_id ||
      identity.source_sha256 !=
          input_.scene.sources->tree().identity().source_sha256 ||
      identity.upstream_commit !=
          input_.scene.sources->tree().identity().upstream_commit ||
      !r->object_exists(out) || r->tree_owner(out).get() != input_.scene.tree ||
      (entered && (!n->inside || !n->bound)))
    return fail(e, "Ready interaction actual source node/lifecycle differs");
  e.clear();
  return true;
}
bool PodunkReadyInteractionBridges::position(uint32_t id, Vec2 p,
                                             std::string &e) {
  FieldObjectId a = 0;
  if (!source(id, a, e))
    return false;
  auto s = input_.scene.tree->state(a);
  auto local = s->local;
  local[2] = p;
  return input_.scene.tree->set_local(a, local, e);
}
bool PodunkReadyInteractionBridges::observe_sparkle(uint32_t id,
                                                    FieldSparklesObservation &o,
                                                    std::string &e) {
  FieldObjectId a = 0;
  if(input_.scene.animated->registry()!=input_.scene.continuation->registry()||
     input_.scene.animated->tree()!=input_.scene.tree||
     input_.scene.animated->source()!=&input_.scene.sources->tree())
    return fail(e,"Sparkles observation precedes its actual native leaf construction");
  if (!source(id, a, e, true) || !input_.scene.sources->sparkles().record(id) ||
      !input_.scene.animated->owns(a))
    return fail(e, "Sparkles source/native leaf owner absent");
  auto *tree = input_.scene.tree;
  const auto *s = tree->state(a);
  bool paused = false, update = false;
  if (!input_.frame(paused, update, e) ||
      !tree->world_transform(a, o.world, e) ||
      !tree->effective_color(a, o.canvas_color, e))
    return false;
  // Validate every actual ancestor, without requiring a parent's later Ready.
  for (FieldObjectId p = a; p;) {
    auto q = tree->state(p);
    if (!q) {
      if (!input_.scene.continuation->registry()->object_exists(p))
        return fail(e, "Sparkles external ancestor is not live");
      break;
    }
    if (!q->alive || !q->inside || !q->bound || q->queued)
      return fail(e, "Sparkles ancestor native/script ownership absent");
    p = q->parent;
  }
  // Original Sparkles has no material; inherited material is followed only when
  // the actual CanvasItem's use_parent_material property requests it.
  for (FieldObjectId p = a; p;) {
    auto q = tree->state(p);
    if (!q)
      return fail(e, "Sparkles external material owner unsupported");
    if (q->flags & 32)
      return fail(e, "Sparkles actual custom material unsupported");
    if (!(q->flags & 16))
      break;
    p = q->canvas_parent;
  }
  o.alive = s->alive;
  o.ancestors_admitted = true;
  o.can_process = tree->can_process(a, paused);
  o.update_pending = update;
  o.visible_in_tree = tree->visible_in_tree(a);
  o.material_admitted = true;
  // The concrete leaf owns both native signals on this same bus. Delivery to
  // unknown source receivers remains a synchronous ObjectDB rejection.
  o.listeners_admitted = input_.scene.animated->registry() ==
                         input_.scene.continuation->signals()->registry();
  e.clear();
  return true;
}
bool PodunkReadyInteractionBridges::observe_door(uint32_t id,
                                                 FieldOpenableObservation &o,
                                                 std::string &e) {
  if(input_.scene.player->registry()!=input_.scene.continuation->registry()||
     input_.physics->registry()!=input_.scene.continuation->registry()||
     input_.physics->tree()!=input_.scene.tree)
    return fail(e,"Openable player observation precedes its actual Player/Physics construction");
  const auto *d = input_.scene.sources->openable().record(id);
  FieldObjectId root = 0, anim = 0, timer = 0;
  if (!d || !source(id, root, e, true) ||
      !source(d->children[12], anim, e, true) ||
      !source(d->children[11], timer, e, true))
    return false;
  auto &body = input_.scene.player->body();
  PlayerInitializationMember p, r, v;
  if (!body.constructed() ||
      !body.member(input_.motion->field(PlayerMotionField::Paused), p, e) ||
      !body.member(input_.motion->field(PlayerMotionField::Running), r, e) ||
      !body.member(input_.motion->field(PlayerMotionField::Direction), v, e))
    return false;
  if (!p.value || p.value->kind != 1 || !r.value || r.value->kind != 1 ||
      v.kind != 7)
    return fail(e, "Openable actual Player source fields have wrong types");
  bool paused = false, update = false;
  if (!input_.frame(paused, update, e))
    return false;
  o.player_paused = p.value->boolean;
  o.player_running = r.value->boolean;
  o.player_direction = {float(v.vector[0]), float(v.vector[1])};
  if (!input_.scene.continuation->global()->core().boolean(
          FieldGlobalMemberRole::EnteringDoor, o.entering_door, e))
    return false;
  o.animation_process = input_.scene.tree->can_process(anim, paused);
  o.timer_process = input_.scene.tree->can_process(timer, paused);
  e.clear();
  return true;
}
bool PodunkReadyInteractionBridges::connect_area(
    uint32_t id, std::function<bool(uint64_t)> enter,
    std::function<bool(uint64_t)> exit, std::string &e) {
  const FieldOpenableDescriptor *d = nullptr;
  for (const auto &v : input_.scene.sources->openable().records())
    if (v.children[1] == id)
      d = &v;
  FieldObjectId area = 0, target = 0;
  if (!d || !enter || !exit || !source(id, area, e, true) ||
      !source(d->id, target, e, true))
    return fail(e, "Openable source Area connection owner absent");
  for (const auto &c : input_.scene.sources->openable().area_connections()) {
    auto key = std::make_pair(target, c.method);
    if (methods_.count(key))
      return fail(e, "Openable Area source connection duplicated");
    auto cb = c.role == 1 ? enter : exit;
    methods_[key] = [this, cb](const Args &args, std::string &err) {
      if (args.size() != 1)
        return fail(err, "Openable body signal argument count differs");
      auto *ref = std::get_if<FieldObjectRef>(&args[0]);
      if (!ref || !ref->id ||
          !input_.scene.continuation->registry()->object_exists(ref->id))
        return fail(err, "Openable body signal is not a live actual Node");
      if (!cb(ref->id)) {
        err = input_.consumers.openable->error();
        return false;
      }
      err.clear();
      return true;
    };
    if (!input_.scene.continuation->signals()->connect(area, c.signal, target,
                                                       c.method, 0, {}, e)) {
      methods_.erase(key);
      return false;
    }
    connections_.push_back({area, target, c.signal, c.method});
  }
  e.clear();
  return true;
}
bool PodunkReadyInteractionBridges::audio_playing(uint32_t id, bool value,
                                                  std::string &e) {
  FieldObjectId a = 0;
  if (!source(id, a, e, true) || !input_.audio->owns(a))
    return fail(e, "Openable source AudioStreamPlayer owner absent");
  return value ? input_.audio->play(a, 0, e) : input_.audio->stop(a, e);
}
bool PodunkReadyInteractionBridges::apply(PodunkSceneMechanismOwners &o,
                                          std::string &e) {
  if (!prepared_)
    return fail(e, "Ready interaction bridge not prepared");
  o.sparkles.bind = [this](const FieldSparklesData &d, std::string &err) {
    if (&d != &input_.scene.sources->sparkles() || !d.valid())
      return fail(err, "Sparkles actual checked source owner differs");
    err.clear();
    return true;
  };
  o.sparkles.observe = [this](uint32_t id, FieldSparklesObservation &v,
                              std::string &err) {
    return observe_sparkle(id, v, err);
  };
  o.sparkles.publish = [this](uint32_t id, const FieldSparklesInstance &v,
                              std::string &err) {
    FieldObjectId a = 0;
    if(input_.scene.animated->registry()!=input_.scene.continuation->registry()||
       input_.scene.animated->tree()!=input_.scene.tree||
       input_.scene.animated->source()!=&input_.scene.sources->tree())
      return fail(err,"Sparkles publication precedes its actual native leaf construction");
    if (v.id != id || !source(id, a, err) || !input_.scene.animated->owns(a))
      return fail(err, "Sparkles actual native publication absent");
    if (!input_.scene.tree->set_visible(a, v.visible, err))
      return false;
    return v.playing
               ? input_.scene.tree->add_group(a, "idle_process_internal", err)
               : input_.scene.tree->remove_group(a, "idle_process_internal",
                                                 err);
  };
  o.sparkles.emit = [this](uint32_t id, FieldSparklesSignal s,
                           std::string &err) {
    return input_.scene.animated->sparkle_signal(id, s, err);
  };
  o.openable.bind = [this](const FieldOpenableDoorData &d, std::string &err) {
    if (&d != &input_.scene.sources->openable() ||
        d.area_connections().size() != 2)
      return fail(err, "Openable actual source connections absent");
    err.clear();
    return true;
  };
  o.openable.observe = [this](uint32_t id, FieldOpenableObservation &v,
                              std::string &err) {
    return observe_door(id, v, err);
  };
  o.openable.connect_area = [this](uint32_t id, auto a, auto b,
                                   std::string &err) {
    return connect_area(id, std::move(a), std::move(b), err);
  };
  o.openable.read_flag = [this](std::string_view k, bool &p, bool &v,
                                std::string &err) {
    return input_.scene.continuation->characters()->flags().read(false, k, p, v,
                                                                 err);
  };
  o.openable.write_flag = [this](std::string_view k, bool v, std::string &err) {
    return input_.scene.continuation->characters()->flags().write(false, k, v,
                                                                  err);
  };
  o.openable.sprite_texture = [this](uint32_t id, const FieldOpenableTexture &t,
                                     std::string &err) {
    FieldObjectId a = 0;
    if (!source(id, a, err))
      return false;
    const auto *art = input_.scene.native->canvas_data();
    const FieldCanvasAsset *asset = nullptr;
    for (const auto &v : art->textures())
      if (v.source == t.source && v.source_sha == t.source_sha &&
          t.region[0] == 0 && t.region[1] == 0 && t.region[2] == v.width &&
          t.region[3] == v.height) {
        if (asset)
          return fail(err, "Openable source Canvas texture is ambiguous");
        asset = &v;
      }
    if (!asset)
      return fail(
          err,
          "Openable cropped texture needs an actual checked GPU region owner");
    return input_.scene.native->sprite_set_texture(a, asset->id, err);
  };
  o.openable.publish = [this](uint32_t id, const FieldOpenableInstance &v,
                              std::string &err) {
    const auto *d = input_.scene.sources->openable().record(id);
    if (!d || v.id != id || !position(d->children[0], v.sprite_position, err))
      return false;
    for (size_t j = 0; j < 4; ++j)
      if (!position(d->children[j + 1], v.positions[j], err))
        return false;
    FieldObjectId a = 0;
    return source(d->children[0], a, err) &&
           input_.scene.tree->set_visible(a, v.sprite_visible, err);
  };
  o.openable.property = [this](uint32_t id, FieldOpenableProperty p, bool v,
                               std::string &err) {
    FieldObjectId a = 0;
    if (!source(id, a, err))
      return false;
    switch (p) {
    case FieldOpenableProperty::SpriteVisible:
      return input_.scene.tree->set_visible(a, v, err);
    case FieldOpenableProperty::BodyDisabled:
    case FieldOpenableProperty::NonPlayerDisabled:
      return input_.scene.native->set_disabled(a, v, err);
    case FieldOpenableProperty::AudioPlaying:
      return audio_playing(id, v, err);
    }
    return fail(err, "Unknown Openable source property");
  };
  o.openable.defer_disabled =
      [this](uint32_t id, bool v, std::function<bool()> cb, std::string &err) {
        FieldObjectId a = 0;
        if (!cb || !source(id, a, err))
          return false;
        FieldDeferredMessage m;
        m.object = a;
        m.kind = FieldDeferredKind::Set;
        m.member = "disabled";
        m.args = {v};
        if (!input_.scene.continuation->registry()->enqueue(std::move(m), err))
          return false;
        disabled_[a].push_back({v, std::move(cb)});
        return true;
      };
  o.openable.prompt_assign_enabled = [this](uint32_t id, bool v,
                                            std::string &err) {
    FieldObjectId a = 0;
    if (!input_.scene.sources->prompt().record(id) || !source(id, a, err, true))
      return false;
    if (!input_.consumers.prompt->assign_enabled(id, v)) {
      err = input_.consumers.prompt->error();
      return false;
    }
    err.clear();
    return true;
  };
  o.openable.prompt_hide = [this](uint32_t id, std::string &err) {
    FieldObjectId a = 0;
    if (!source(id, a, err, true))
      return false;
    if (!input_.consumers.prompt->canvas_hide(id)) {
      err = input_.consumers.prompt->error();
      return false;
    }
    err.clear();
    return true;
  };
  o.openable.audio_stream = [this](uint32_t id, const FieldOpenableSound &sound,
                                   float db, std::string_view bus,
                                   std::string &err) {
    FieldObjectId a = 0;
    if (!source(id, a, err, true) || !input_.audio->owns(a))
      return false;
    const FieldSceneAudioStream *stream = nullptr;
    for (const auto &s : input_.scene.sources->audio().streams())
      if (s.source == sound.source && s.source_sha == sound.sha) {
        if (stream)
          return fail(err, "Openable source audio is ambiguous");
        stream = &s;
      }
    if (!stream)
      return fail(err, "Openable source stream not in actual checked bank");
    return input_.audio->set_stream(a, stream->id, err) &&
           input_.audio->set_volume(a, db, err) &&
           input_.audio->set_bus(a, bus, err);
  };
  o.openable.audio_playing = [this](uint32_t id, bool v, std::string &err) {
    return audio_playing(id, v, err);
  };
  o.openable.overlapping_bodies = [this](uint32_t id,
                                         std::vector<FieldOpenableBody> &out,
                                         std::string &err) {
    FieldObjectId a = 0;
    if (!source(id, a, err, true))
      return false;
    std::vector<FieldObjectId> bodies;
    if (!input_.physics->overlapping(a, false, bodies, err))
      return false;
    std::shared_ptr<const GlobalLoadObjectArray> party;
    if (!input_.scene.continuation->global()->core().array(
            FieldGlobalMemberRole::PartyObjects, party, err) ||
        !party)
      return fail(err, "Openable actual PartyObjects source Array absent");
    out.clear();
    for (auto b : bodies)
      out.push_back({b, std::find(party->values.begin(), party->values.end(),
                                  b) != party->values.end()});
    err.clear();
    return true;
  };
  // Later interaction operations are not replaced by successful no-ops.
  if (!o.openable.camera_shake)
    o.openable.camera_shake = [](float, float, Vec2, std::string &err) {
      return fail(err, "Openable source Camera shake owner is not bound");
    };
  if (!o.openable.party_inventories)
    o.openable.party_inventories = [](auto &, auto &, std::string &err) {
      return fail(err, "Openable source party Inventory search is not bound");
    };
  if (!o.openable.inventory_find)
    o.openable.inventory_find = [](uint64_t, std::string_view, uint64_t &,
                                   std::string &err) {
      return fail(err, "Openable source Inventory.find_item is not bound");
    };
  if (!o.openable.item_name)
    o.openable.item_name = [](uint64_t, std::string &, std::string &err) {
      return fail(err, "Openable source Item body is not bound");
    };
  if (!o.openable.item_owner)
    o.openable.item_owner = [](uint64_t, uint64_t &, std::string &err) {
      return fail(err, "Openable source Item owner search is not bound");
    };
  if (!o.openable.inventory_drop)
    o.openable.inventory_drop = [](uint64_t, uint64_t, bool &,
                                   std::string &err) {
      return fail(err, "Openable source Inventory.drop_item is not bound");
    };
  if (!o.openable.set_global_item)
    o.openable.set_global_item = [](uint64_t, std::string &err) {
      return fail(err, "Openable source global.item assignment is not bound");
    };
  if (!o.openable.open_dialogue)
    o.openable.open_dialogue = [](uint32_t, const FieldOpenableDialogue &,
                                  std::string &err) {
      return fail(err, "Openable source dialogue owner is not bound");
    };
  if (!o.openable.emit_flags)
    o.openable.emit_flags = o.scene.emit_flags;
  if (!o.openable.emit_flags || !o.openable.connect_flags)
    return fail(
        e, "Openable actual source flags signal bridge must be applied first");
  e.clear();
  return true;
}
bool PodunkReadyInteractionBridges::handles(
    const FieldDeferredMessage &m) const {
  return prepared_ && ((m.kind == FieldDeferredKind::Call &&
                        methods_.count({m.object, m.member})) ||
                       (m.kind == FieldDeferredKind::Set &&
                        m.member == "disabled" && disabled_.count(m.object)));
}
bool PodunkReadyInteractionBridges::deferred(const FieldDeferredMessage &m,
                                             std::string &e) {
  if (!handles(m))
    return fail(e, "Ready interaction message has no actual source slot");
  if (m.kind == FieldDeferredKind::Call)
    return methods_.at({m.object, m.member})(m.args, e);
  auto &q = disabled_.at(m.object);
  auto *v = m.args.size() == 1 ? std::get_if<bool>(&m.args[0]) : nullptr;
  if (q.empty() || !v || q.front().value != *v)
    return fail(e, "Openable deferred property source FIFO differs");
  auto cb = q.front().commit;
  q.pop_front();
  if (q.empty())
    disabled_.erase(m.object);
  if (!cb()) {
    e = input_.consumers.openable->error();
    return false;
  }
  e.clear();
  return true;
}
bool PodunkReadyInteractionBridges::release(FieldObjectId id, std::string &e) {
  if (!prepared_)
    return fail(e, "Ready interaction bridge not prepared");
  auto &bus = *input_.scene.continuation->signals();
  auto *r = input_.scene.continuation->registry();
  for (auto it = connections_.begin(); it != connections_.end();) {
    if (it->emitter != id && it->target != id) {
      ++it;
      continue;
    }
    if (r->object_exists(it->emitter) && r->object_exists(it->target)) {
      bool connected = false;
      if (!bus.connected(it->emitter, it->signal, it->target, it->method,
                         connected, e) ||
          (connected &&
           !bus.disconnect(it->emitter, it->signal, it->target, it->method, e)))
        return false;
    }
    methods_.erase({it->target, it->method});
    it = connections_.erase(it);
  }
  disabled_.erase(id);
  e.clear();
  return true;
}
} // namespace encore::ctr

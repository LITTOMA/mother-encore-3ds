#include "podunk_scene_loop.hpp"
#include "podunk_player_physics_world.hpp"
#include <algorithm>
#include <cmath>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const std::string &message) {
  e = message;
  return false;
}
bool source_phase(FieldTreePhase p) {
  return p == FieldTreePhase::EnterScript || p == FieldTreePhase::ReadyScript ||
         p == FieldTreePhase::ExitScript || p == FieldTreePhase::Idle ||
         p == FieldTreePhase::Physics || p == FieldTreePhase::Input ||
         p == FieldTreePhase::UnhandledInput ||
         p == FieldTreePhase::UnhandledKeyInput;
}
bool native_phase(FieldTreePhase p) {
  return !source_phase(p) && p != FieldTreePhase::TreeEntered &&
         p != FieldTreePhase::NodeAdded && p != FieldTreePhase::ChildEntered &&
         p != FieldTreePhase::ReadySignal && p != FieldTreePhase::TreeExiting &&
         p != FieldTreePhase::NodeRemoved && p != FieldTreePhase::ChildExiting &&
         p != FieldTreePhase::TreeExited;
}
bool step_delta(float v) { return std::isfinite(v) && v >= 0 && v <= 1; }
} // namespace
PodunkSceneNativeMechanism *PodunkSceneLoop::mechanism(
    const FieldNodeDescriptor &d) const {
  PodunkSceneNativeMechanism *found = nullptr;
  for (auto *owner : input_.mechanisms) {
    if (owner && owner->owns(d)) {
      if (found) return nullptr; // Ambiguous owners do not grant admission.
      found = owner;
    }
  }
  return found;
}
bool PodunkSceneLoop::construct(PodunkSceneLoopInput input, std::string &e) {
  if (attempted_ || !input.sources || !input.sources->valid() ||
      !input.continuation || !input.continuation->initialized() || !input.tree ||
      input.tree->object_count() || !input.native || !input.player ||
      !input.physics || !input.map || !input.geometry || input.asset_root.empty() ||
      !input.source_signals || !input.source_methods || !input.source_method_owned ||
      !input.allocation_observed)
    return fail(e, "Scene loop requires actual checked destination and session owners");
  attempted_ = true;
  input_ = std::move(input);
  auto &d = *input_.sources;
  auto &r = *input_.continuation->registry();
  // Reject before allocating nodes or consuming RNG. A missing native class
  // is a concrete dependency, never an empty notification implementation.
  for (const auto &row : d.tree().records()) {
    // Data rows retain the checked class opcode. Runtime descriptors resolve
    // its name during instantiation; preflight uses that same schema.
    auto n = row;
    if (n.class_index >= d.tree().classes().size())
      return fail(e, "Scene native class opcode outside checked schema");
    n.native_class = d.tree().classes()[n.class_index];
    const bool leaf = n.native_class == "Timer" ||
        n.native_class == "AnimatedSprite";
    bool arrows_player = false;
    for (const auto &arrow : d.arrows().players())
      if (arrow.id == n.id) arrows_player = true;
    if (!input_.native->owns(n) && !leaf && !arrows_player && !mechanism(n))
      missing_.push_back(n.native_class + ": " + n.path);
  }
  if (!missing_.empty())
    return fail(e, "Scene native owner missing: " + missing_.front());
  if (!scripts_.prepare(d, input_.consumers, input_.scene_ops, *input_.tree, r,
                        *input_.continuation->signals(), input_.grass, e) ||
      !input_.native->prepare(d.tree(), *input_.tree, r,
          *input_.continuation->native_root(), *input_.map, *input_.geometry,
          d.canvas(), input_.asset_root.c_str(), input_.canvas, input_.materials, e) ||
      !input_.native->bind_sprite_signals(*input_.continuation->signals(), e) ||
      !input_.continuation->bind_scene_signal_declarations(
          [this](auto id, auto name, auto &arity, auto &error) {
            return signal_declaration(id, name, arity, error);
          }, e))
    return false;
  FieldNodeTreeHost host;
  host.object_domain = r.kernel();
  host.allocate_object = [&r](auto &id, auto &error) {
    return r.allocate_object(id, error);
  };
  host.allocate_fast_name = [&r](auto &id, auto &error) {
    return r.allocate_fast_name(id, error);
  };
  host.object_exists = [&r](auto id) { return r.object_exists(id); };
  host.native_allocated = [this](auto id, const auto &n, const auto &identity,
                                 auto &error) {
    return native_allocated(id, n, identity, error);
  };
  host.construct_source = [this](auto id, const auto &n, const auto &identity,
                                 auto &error) {
    if (input_.player->source_candidate(n.id))
      return input_.player->construct_source(id, n, identity, error);
    if (n.script.empty()) { error.clear(); return true; }
    return scripts_.construct_source(id, n, identity, error);
  };
  host.bind = [this](auto id, const auto &n, auto &binding, auto &error) {
    return bind(id, n, binding, error);
  };
  host.dispatch = [this](auto id, const auto &binding, auto phase, auto &error) {
    const bool ok = dispatch(id, binding, phase, error);
    poisoned_ |= !ok;
    return ok;
  };
  host.deferred = [this](const auto &m, auto &error) { return deferred(m, error); };
  host.enqueue_global = [&r](auto m, auto &error) {
    return r.enqueue(std::move(m), error);
  };
  host.flush_global = [&r](auto &error) { return r.flush_messages(error); };
  host.input_registration = [this](auto id, auto kind, auto enabled, auto &error) {
    return input_.continuation->native_root()->input_registration(id, kind, enabled, error);
  };
  // The original Viewport inherits the root's default pause behavior. This
  // query is used only when SceneTree is paused and no ancestor overrides it.
  host.external_pause_process = [](auto) { return false; };
  host.external_path = [&r](auto id, auto path, auto &out, auto &error) {
    return r.resolve_path(id, path, out, error);
  };
  host.release = [this](auto id, const auto &binding, auto &error) {
    return release(id, binding, error);
  };
  if (!input_.tree->initialize(d.tree(), std::move(host), e) ||
      !timers_.finish_factory(e) || !animated_.finish_factory(e) ||
      !input_.native->finish_factory(e) ||
      !input_.native->bind_animated_leaves(animated_, e)) {
    poisoned_ = true;
    return false;
  }
  constructed_ = true;
  e.clear();
  return true;
}
bool PodunkSceneLoop::prepare_leaves(std::string &e) {
  if (leaves_prepared_) return true;
  const auto &d = *input_.sources;
  auto &c = input_.consumers;
  if (!c.arrows || !c.sparkles || !c.present || !c.dropped ||
      !timers_.prepare(d.timers(), d.tree(), *input_.tree,
          *input_.continuation->registry(), *input_.continuation->signals(), e) ||
      !animated_.prepare(d.tree(), *input_.tree, *input_.continuation->registry(),
          *input_.continuation->signals(), d.arrows(), *c.arrows,
          d.sparkles(), *c.sparkles, *c.present, *c.dropped,
          input_.asset_root.c_str(), e))
    return false;
  leaves_prepared_ = true;
  return true;
}
bool PodunkSceneLoop::native_allocated(FieldObjectId id,
    const FieldNodeDescriptor &d, const FieldIdentity &identity, std::string &e) {
  auto &r = *input_.continuation->registry();
  if (!r.publish_allocated_node(input_.tree, id,
      [this](const auto &message, auto &error) { return deferred(message, error); }, e))
    return false;
  if (input_.player->source_candidate(d.id)) return true;
  if (!input_.allocation_observed(id, d, identity, e)) return false;
  if (!prepare_leaves(e)) return false;
  if (input_.native->owns(d)) return input_.native->construct(id, d, identity, e);
  if (d.native_class == "Timer") return timers_.construct(id, d, identity, e);
  if (d.native_class == "AnimatedSprite" || d.native_class == "AnimationPlayer") {
    // This concrete owner accepts only its checked Arrow/Sparkles roster.
    bool arrow = d.native_class == "AnimatedSprite";
    for (const auto &a : input_.sources->arrows().players())
      if (a.id == d.id) arrow = true;
    if (arrow) return animated_.construct(id, d, identity, e);
  }
  auto *owner = mechanism(d);
  return owner ? owner->construct(id, d, identity, e)
               : fail(e, "Unowned actual native constructor: " + d.path);
}
bool PodunkSceneLoop::bind(FieldObjectId id, const FieldNodeDescriptor &d,
                           FieldNodeBinding &out, std::string &e) {
  if (input_.player->source_candidate(d.id)) return input_.player->bind(id, d, out, e);
  FieldNodeBinding native;
  if (input_.native->owns(id)) {
    native = {input_.sources->tree().identity(), d.id, d.class_index,
              0x454e003c, 3, d.script_sha, d.native_class};
  } else if (timers_.owns(id)) {
    if (!timers_.bind(id, native, e)) return false;
  } else if (animated_.owns(id)) {
    if (!animated_.bind(id, native, e)) return false;
  } else {
    auto *owner = mechanism(d);
    if (!owner || !owner->bind(id, native, e)) return false;
  }
  if (!d.script.empty()) {
    if (!scripts_.bind(id, d, out, e)) return false;
  } else out = native;
  return !input_.native->owns(id) || input_.native->bind(id, out, e);
}
bool PodunkSceneLoop::dispatch(FieldObjectId id, const FieldNodeBinding &b,
                               FieldTreePhase p, std::string &e) {
  auto &r = *input_.continuation->registry();
  const auto *n = input_.tree->descriptor(id);
  if (!n || r.tree_owner(id) != input_.tree || !r.object_exists(id))
    return fail(e, "Scene phase has no actual owning node");
  auto &root = *input_.continuation->native_root();
  if (p == FieldTreePhase::NodeAdded || p == FieldTreePhase::NodeRemoved ||
      p == FieldTreePhase::ChildEntered || p == FieldTreePhase::ChildExiting ||
      p == FieldTreePhase::ReadyNative || p == FieldTreePhase::ReadyScript)
    if (!root.node_notification(id, p, e)) return false;
  if (input_.player->owns(id)) return input_.player->phase(id, b, p, e);
  if (source_phase(p)) {
    if (!n->script.empty())
      return scripts_.phase(id, b, p,
          p == FieldTreePhase::Physics ? physics_delta_ : idle_delta_, e);
    // Source-less nodes have no script callback; Tree retains base state.
    e.clear(); return true;
  }
  if (native_phase(p)) {
    if (input_.native->owns(id)) {
      if (!input_.native->phase(id, p, e)) return false;
    } else if (timers_.owns(id)) {
      if (!timers_.phase(id, p,
          p == FieldTreePhase::PhysicsInternal ? physics_delta_ : idle_delta_, paused_, e))
        return false;
    } else if (animated_.owns(id)) {
      if (!animated_.phase(id, p, idle_delta_, paused_, update_pending_, e)) return false;
    } else {
      auto *owner = mechanism(*n);
      if (!owner || !owner->phase(id, p,
          p == FieldTreePhase::PhysicsInternal ? physics_delta_ : idle_delta_, paused_, update_pending_, e))
        return false;
    }
  }
  // Engine signal declarations are software schema, independently of game
  // callbacks. Delivery uses the same synchronous ObjectDB bus/queue.
  std::string_view signal;
  switch (p) {
  case FieldTreePhase::TreeEntered: signal = "tree_entered"; break;
  case FieldTreePhase::TreeExiting: signal = "tree_exiting"; break;
  case FieldTreePhase::TreeExited: signal = "tree_exited"; break;
  case FieldTreePhase::ReadySignal: signal = "ready"; break;
  case FieldTreePhase::VisibilityChanged: signal = "visibility_changed"; break;
  default: break;
  }
  return signal.empty() || input_.continuation->signals()->emit(id, signal, {}, e);
}
bool PodunkSceneLoop::attach_scene(std::string &e) {
  if (!constructed_ || attached_ || poisoned_)
    return fail(e, "Scene attachment precedes actual complete factory");
  auto &r = *input_.continuation->registry();
  if (!r.attach_scene(input_.tree->root(), e) ||
      !scripts_.lifecycle().scene_ready()) {
    poisoned_ = true;
    return false;
  }
  attached_ = true;
  source_ready_ = true;
  e.clear(); return true;
}
bool PodunkSceneLoop::activate_after_player(std::string &e) {
  if (!attached_ || !source_ready_ || monitors_ready_ || poisoned_ ||
      !input_.player->ready_complete())
    return fail(e, "Scene monitors precede actual persistent Player attachment/Ready");
  const auto player=input_.player->body().object();
  const auto *state=input_.tree->state(player);
  if (!state || !state->alive || !state->inside || !state->ready_notified ||
      !input_.native->activate_monitors(scripts_.lifecycle(), *input_.physics, e)) {
    poisoned_=true; return false;
  }
  monitors_ready_=true;
  e.clear(); return true;
}
bool PodunkSceneLoop::ready() const {
  return attached_ && source_ready_ && monitors_ready_ && !poisoned_ &&
         input_.player->ready_complete();
}
bool PodunkSceneLoop::physics_frame(uint64_t epoch, float dt, bool paused,
                                    std::string &e) {
  if (!ready() || !epoch || epoch <= physics_epoch_ || !step_delta(dt))
    return fail(e, "Scene actual physics cursor rejected");
  physics_epoch_ = epoch; physics_delta_ = dt; paused_ = paused;
  if (!input_.player->begin_frame(epoch, 0, dt, paused, update_pending_, e) ||
      (physics_sampled_ && !input_.physics->flush_queries(e)) ||
      !input_.tree->process(true, paused, e) ||
      !input_.continuation->registry()->flush_messages(e) ||
      !input_.tree->flush_transform_notifications(e) ||
      !input_.physics->physics_step(epoch, e)) {
    poisoned_ = true; return false;
  }
  physics_sampled_ = true;
  e.clear(); return true;
}
bool PodunkSceneLoop::idle_frame(uint64_t epoch, float dt, bool paused,
                                 bool update, std::string &e) {
  if (!ready() || !epoch || epoch <= idle_epoch_ || !step_delta(dt))
    return fail(e, "Scene actual idle cursor rejected");
  idle_epoch_ = epoch; idle_delta_ = dt; paused_ = paused; update_pending_ = update;
  if (!input_.player->begin_frame(epoch, dt, 0, paused, update, e) ||
      !input_.tree->process(false, paused, e) ||
      !transition_jobs(dt, paused, e) ||
      !input_.continuation->registry()->flush_messages(e) ||
      !input_.tree->flush_transform_notifications(e) ||
      !input_.tree->flush_delete_queue(e) || !scripts_.collect_deleted(e)) {
    poisoned_ = true; return false;
  }
  input_.continuation->registry()->collect_dead_tree_objects();
  for (auto id : released_signals_)
    if (!input_.continuation->signals()->release(id, e)) {
      poisoned_ = true; return false;
    }
  released_signals_.clear();
  e.clear(); return true;
}
bool PodunkSceneLoop::transition_jobs(float dt, bool paused, std::string &e) {
  // Script _process and native Tween/Timer continuations each advance once.
  // Only the checked Jump roster owns these delayed jobs.
  const auto &source = input_.sources->lifecycle();
  for (uint32_t i = 0; i < source.ready_count(); ++i) {
    const auto row = source.ready(i);
    if (row.role != FieldSceneRole::JumpArea) continue;
    const auto id = input_.tree->source_object(row.id);
    const auto *node = input_.tree->state(id);
    if (!node || !node->alive || !node->inside || node->queued) continue;
    if (!scripts_.transition_native_idle(id, dt,
          input_.tree->can_process(id, paused), e)) return false;
  }
  e.clear();
  return true;
}
bool PodunkSceneLoop::input(uint32_t kind, const PlayerInputEvent &event,
                            bool accept, bool paused, std::string &e) {
  if (!ready() || kind > 2) return fail(e, "Scene input before actual Ready");
  std::vector<FieldObjectId> receivers;
  if (!input_.continuation->native_root()->input_objects(kind, receivers, e)) return false;
  for (auto id : receivers) {
    if (!input_.tree->state(id) || !input_.tree->can_process(id, paused)) continue;
    if (input_.player->owns(id)) {
      if (kind != 0 || id != input_.player->body().object())
        return fail(e, "Player input subgroup has no source handler");
      if (!input_.player->input(event, e)) return false;
    } else {
      bool jump_input = false;
      if (scripts_.owns(id)) {
        const auto *d = input_.tree->descriptor(id);
        for (uint32_t i = 0; i < input_.sources->lifecycle().ready_count(); ++i) {
          const auto row = input_.sources->lifecycle().ready(i);
          if (d && row.id == d->id && row.role == FieldSceneRole::JumpArea) {
            jump_input = true;
            if (accept && !scripts_.transition_accept(id, true, e)) return false;
          }
        }
      }
      if (!jump_input && !input_.tree->dispatch_input(id, kind, paused, e)) return false;
    }
  }
  e.clear(); return true;
}
bool PodunkSceneLoop::draw(uint64_t epoch, float dt, std::string &e) {
  if (!ready() || !step_delta(dt)) return fail(e, "Scene draw before actual Ready");
  if (!animated_.begin_draw(epoch, e) || !input_.native->begin_draw(epoch, dt, e)) return false;
  return input_.native->draw([this](uint32_t source) {
    return scripts_.lifecycle().gate(source);
  }, e);
}
bool PodunkSceneLoop::deferred(const FieldDeferredMessage &m, std::string &e) {
  const auto *n = input_.tree ? input_.tree->descriptor(m.object) : nullptr;
  const auto *s = input_.tree ? input_.tree->state(m.object) : nullptr;
  if (!n || !s || !s->alive) return fail(e, "Scene queued method target not live");
  if (m.kind == FieldDeferredKind::Notification) {
    const auto p = FieldTreePhase(m.notification);
    if (!m.args.empty() || (p != FieldTreePhase::Parented &&
        p != FieldTreePhase::Unparented && p != FieldTreePhase::ChildMoved))
      return fail(e, "Scene external native notification unsupported");
    return dispatch(m.object, s->binding, p, e);
  }
  if (input_.physics->handles_callback(m)) return input_.physics->deferred(m, e);
  if (input_.player->owns(m.object)) return input_.player->deferred(m, e);
  // A native body can also be a scripted receiver. Route only an explicitly
  // owned source method before its native property's dispatcher.
  if (input_.source_method_owned(m)) return input_.source_methods(m,e);
  if (timers_.owns(m.object)) return timers_.deferred(m, e);
  if (auto *owner = mechanism(*n); owner && owner->owns(m.object))
    return owner->deferred(m, e);
  return input_.source_methods(m, e);
}
bool PodunkSceneLoop::release(FieldObjectId id, const FieldNodeBinding &b,
                              std::string &e) {
  if (input_.player->owns(id)) return input_.player->release(id, e);
  if (scripts_.owns(id) && !scripts_.release(id, b, e)) return false;
  const auto *d = input_.tree->descriptor(id);
  if (d) if (auto *owner = mechanism(*d); owner && owner->owns(id))
    if (!owner->release(id, e)) return false;
  // The Tree invokes release while the actual ObjectDB node is still alive.
  // Connection cleanup follows the completed native deletion boundary.
  released_signals_.push_back(id);
  e.clear(); return true;
}
bool PodunkSceneLoop::signal_declaration(FieldObjectId id, std::string_view name,
                                        uint32_t &arity, std::string &e) const {
  const auto *d = input_.tree ? input_.tree->descriptor(id) : nullptr;
  if (!d || input_.continuation->registry()->tree_owner(id) != input_.tree)
    return fail(e, "Scene signal lacks actual owning node");
  if (name == "tree_entered" || name == "tree_exiting" || name == "tree_exited" ||
      name == "ready") { arity = 0; e.clear(); return true; }
  if (name == "child_entered_tree" || name == "child_exiting_tree") {
    arity = 1; e.clear(); return true;
  }
  if ((d->flags & 1) && name == "visibility_changed") {
    arity = 0; e.clear(); return true;
  }
  if (d->native_class == "Timer" && name == "timeout") {
    arity = 0; e.clear(); return true;
  }
  if ((d->native_class == "Sprite" || d->native_class == "AnimatedSprite") &&
      (name == "frame_changed" || name == "texture_changed" ||
       (d->native_class == "AnimatedSprite" && name == "animation_finished"))) {
    arity = 0; e.clear(); return true;
  }
  return input_.source_signals(id, name, arity, e);
}
} // namespace encore::ctr

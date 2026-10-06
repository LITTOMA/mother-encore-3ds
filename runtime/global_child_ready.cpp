#include "encore/global_child_ready.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
bool GlobalChildReadyRuntime::poison(std::string &e) {
  poisoned_ = true;
  if (e.empty())
    e = "Global child actual native execution failed";
  return false;
}
bool GlobalChildReadyRuntime::initialize(
    const GlobalChildReadyData &data, const FieldGlobalConstructorData &ctor,
    FieldGlobalRegistry &registry, GlobalChildEngine &engine,
    GlobalChildInput &input, GlobalChildAudio *audio, std::string &e) {
  if (data_ || !data.valid() || !ctor.valid() || registry.poisoned() ||
      data.constructor_ir_sha256() != ctor.ir_sha256())
    return fail(
        e, "Global child actual constructor/Registry prerequisite rejected");
  double scale = 0;
  if (!engine.time_scale(scale, e) || !std::isfinite(scale))
    return false;
  data_ = &data;
  constructor_ = &ctor;
  registry_ = &registry;
  engine_ = &engine;
  input_ = &input;
  audio_ = audio;
  admitted_ir_ = data.ir_sha256();
  e.clear();
  return true;
}
bool GlobalChildReadyRuntime::construct(FieldNodeTreeRuntime &tree,
                                        FieldObjectId id,
                                        const FieldNodeDescriptor &descriptor,
                                        std::string &e) {
  if (!data_ || !data_->valid() || data_->ir_sha256() != admitted_ir_ ||
      poisoned_ || !id || trees_.count(id) ||
      tree.object_domain() != registry_->kernel())
    return fail(e, "Global child actual source constructor unavailable");
  auto row = std::find_if(data_->nodes().begin(), data_->nodes().end(),
                          [&](const auto &r) { return r.id == descriptor.id; });
  const auto *actual = tree.descriptor(id);
  const auto *state = tree.state(id);
  if (row == data_->nodes().end() || !actual || !state || state->inside ||
      descriptor.native_class != "Node" || actual->id != descriptor.id ||
      row->script != descriptor.script ||
      row->script_sha != descriptor.script_sha ||
      row->class_index != descriptor.class_index ||
      row->methods != descriptor.script_methods)
    return fail(e, "Global child true source Node identity differs");
  auto fields = std::find_if(constructor_->child_fields().begin(),
                             constructor_->child_fields().end(),
                             [&](const auto &f) { return f.id == row->id; });
  if (fields == constructor_->child_fields().end() ||
      fields->fields.size() != 4)
    return fail(e, "Global child source initializer absent");
  if (row->kind == 1) {
    if (slow_id_)
      return fail(e, "Duplicate actual Slowmo owner");
    slow_id_ = id;
    slow_.with_pitch = fields->fields[3].boolean;
  } else {
    if (mouse_id_)
      return fail(e, "Duplicate actual MouseHider owner");
    mouse_id_ = id;
    mouse_.speed = {float(fields->fields[0].vector[0]),
                    float(fields->fields[0].vector[1])};
    mouse_.shown = fields->fields[1].number;
    mouse_.hidden = fields->fields[2].number;
    mouse_.idle = fields->fields[3].number;
  }
  trees_[id] = &tree;
  e.clear();
  return true;
}
bool GlobalChildReadyRuntime::live(FieldObjectId id, uint32_t kind,
                                   FieldNodeTreeRuntime *&tree,
                                   std::string &e) const {
  if (!data_ || !data_->valid() || data_->ir_sha256() != admitted_ir_ ||
      poisoned_ || !registry_ || registry_->poisoned() ||
      id != (kind == 1 ? slow_id_ : mouse_id_))
    return fail(e, "Global child actual live source owner rejected");
  auto owned = registry_->tree_owner(id);
  auto found = trees_.find(id);
  if (!owned || found == trees_.end() || owned.get() != found->second ||
      owned->object_domain() != registry_->kernel())
    return fail(e, "Global child different Registry/Tree owner rejected");
  tree = owned.get();
  const auto *n = tree->state(id);
  const auto *d = tree->descriptor(id);
  auto r = std::find_if(data_->nodes().begin(), data_->nodes().end(),
                        [&](const auto &x) { return x.kind == kind; });
  if (!n || !n->alive || n->queued || !n->inside || !n->bound || !d ||
      r == data_->nodes().end() || d->id != r->id ||
      d->script_sha != r->script_sha || n->binding.stable_id != r->id ||
      n->binding.script_sha != r->script_sha)
    return fail(e, "Global child actual lifecycle/script binding expired");
  return true;
}
bool GlobalChildReadyRuntime::ready_script(FieldNodeTreeRuntime &actual,
                                           FieldObjectId id, std::string &e) {
  const uint32_t kind = id == slow_id_ ? 1 : 2;
  FieldNodeTreeRuntime *tree = nullptr;
  if (!live(id, kind, tree, e) || tree != &actual ||
      !tree->state(id)->ready_notified || tree->state(id)->ready_first ||
      (kind == 1 ? slow_.ready : mouse_.ready))
    return fail(
        e, "Global child ReadyScript requires real first native Ready cursor");
  if (kind == 1) {
    if (!tree->set_process(id, false, false, e))
      return poison(e);
    slow_.processing = false;
    if (!engine_->set_time_scale(data_->policy().slow_end, e))
      return poison(e);
    slow_.ready = true;
  } else {
    if (!input_->set_mouse_mode(data_->policy().hidden, e))
      return poison(e);
    if (!input_->mouse_speed(mouse_.speed, e))
      return poison(e);
    mouse_.ready = true;
    mouse_.processing = true;
  }
  e.clear();
  return true;
}
bool GlobalChildReadyRuntime::start_slowmo(FieldObjectId id, double speed,
                                           double length, bool pitch,
                                           std::string &e) {
  FieldNodeTreeRuntime *tree = nullptr;
  if (!live(id, 1, tree, e) || !slow_.ready || !std::isfinite(speed) ||
      !std::isfinite(length) ||
      !std::isfinite(length * data_->policy().length_scale))
    return fail(e, "Slowmo actual start arguments/source owner rejected");
  if (pitch) {
    std::array<uint8_t, 32> a{}, b{};
    if (!audio_ || !audio_->source_hash(data_->audio_source(), a) ||
        !data_->source_hash(data_->audio_source(), b) || a != b)
      return fail(e, "Slowmo actual Sfx pitch owner pending");
  }
  uint64_t now = 0;
  if (!engine_->ticks_msec(now, e) ||
      now > uint64_t(std::numeric_limits<int64_t>::max()))
    return poison(e);
  slow_.start = now;
  slow_.length = length * data_->policy().length_scale;
  slow_.value = speed;
  slow_.with_pitch = pitch;
  slow_.started = true;
  if (!engine_->set_time_scale(speed, e) ||
      !tree->set_process(id, false, true, e))
    return poison(e);
  slow_.processing = true;
  e.clear();
  return true;
}
bool GlobalChildReadyRuntime::start_slowmo(FieldObjectId id, double speed,
                                           double length, std::string &e) {
  if (!data_)
    return fail(e, "Slowmo source default unavailable");
  return start_slowmo(id, speed, length, data_->policy().default_pitch, e);
}
bool GlobalChildReadyRuntime::idle(FieldObjectId id, double delta,
                                   std::string &e) {
  const uint32_t kind = id == slow_id_ ? 1 : 2;
  FieldNodeTreeRuntime *tree = nullptr;
  if (!live(id, kind, tree, e) || !std::isfinite(delta))
    return fail(e, "Global child idle source owner/delta rejected");
  if (kind == 1) {
    if (!slow_.ready || !slow_.started || !slow_.processing)
      return fail(e, "Slowmo idle outside actual process cursor");
    uint64_t now = 0;
    if (!engine_->ticks_msec(now, e) || now < slow_.start ||
        now > uint64_t(std::numeric_limits<int64_t>::max()))
      return poison(e);
    const double elapsed = double(now - slow_.start),
                 fraction = elapsed / slow_.length;
    double value =
        -data_->policy().slow_end * (std::sqrt(1 - fraction * fraction) - 1) +
        slow_.value;
    if (elapsed >= slow_.length) {
      if (!tree->set_process(id, false, false, e))
        return poison(e);
      slow_.processing = false;
      value = data_->policy().slow_end;
    }
    if (!std::isfinite(value))
      return fail(e, "Slowmo source circular result not representable");
    if (!engine_->set_time_scale(value, e))
      return poison(e);
    if (slow_.with_pitch &&
        (!audio_ || !audio_->set_sfx_pitch(double(float(value)), e)))
      return poison(e);
  } else {
    if (!mouse_.ready || !mouse_.processing)
      return fail(e, "MouseHider idle outside actual process cursor");
    Vec2 speed{};
    if (!input_->mouse_speed(speed, e))
      return poison(e);
    uint32_t mode = 0;
    if (mouse_.speed.x != speed.x || mouse_.speed.y != speed.y) {
      mouse_.shown = 0;
      mouse_.idle = 0;
      if (!input_->mouse_mode(mode, e))
        return poison(e);
      if (mode == data_->policy().hidden) {
        mouse_.hidden += delta;
        if (mouse_.hidden >= delta * data_->policy().moving_frames) {
          if (!input_->set_mouse_mode(data_->policy().visible, e))
            return poison(e);
          mouse_.hidden = 0;
        }
      }
    } else
      mouse_.idle += delta;
    if (!input_->mouse_mode(mode, e))
      return poison(e);
    if (mode == data_->policy().visible)
      mouse_.shown += delta;
    if (!input_->mouse_speed(mouse_.speed, e))
      return poison(e);
    if (mouse_.idle >= data_->policy().idle_reset)
      mouse_.hidden = 0;
    if (mouse_.shown >= data_->policy().shown_hide) {
      if (!input_->set_mouse_mode(data_->policy().hidden, e))
        return poison(e);
      mouse_.shown = 0;
    }
    if (!std::isfinite(mouse_.shown) || !std::isfinite(mouse_.hidden) ||
        !std::isfinite(mouse_.idle))
      return poison(e);
  }
  e.clear();
  return true;
}
bool GlobalChildReadyRuntime::input(FieldObjectId id, std::string &e) {
  FieldNodeTreeRuntime *tree = nullptr;
  if (!live(id, 2, tree, e) || !mouse_.ready ||
      !tree->state(id)->input_enabled[0])
    return fail(e, "MouseHider actual input cursor unavailable");
  uint32_t mask = 0;
  if (!input_->mouse_buttons(mask, e))
    return poison(e);
  if (mask) {
    if (!input_->set_mouse_mode(data_->policy().visible, e))
      return poison(e);
    mouse_.hidden = mouse_.shown = mouse_.idle = 0;
  }
  e.clear();
  return true;
}
bool GlobalChildReadyRuntime::notification(FieldObjectId id, uint32_t what,
                                           std::string &e) {
  FieldNodeTreeRuntime *tree = nullptr;
  if (!live(id, 2, tree, e))
    return false;
  if (what == data_->policy().mouse_enter &&
      !input_->set_mouse_mode(data_->policy().visible, e))
    return poison(e);
  e.clear();
  return true;
}
bool GlobalChildReadyRuntime::set_active(FieldObjectId id, bool active,
                                         std::string &e) {
  FieldNodeTreeRuntime *tree = nullptr;
  if (!live(id, 2, tree, e) || !mouse_.ready)
    return fail(e, "MouseHider set_active before Ready rejected");
  if (!tree->set_process(id, false, active, e))
    return poison(e);
  mouse_.processing = active;
  if (!active && !input_->set_mouse_mode(data_->policy().visible, e))
    return poison(e);
  e.clear();
  return true;
}
bool GlobalChildReadyRuntime::release(FieldObjectId id, std::string &e) {
  if (!trees_.erase(id))
    return fail(e, "Global child release unknown owner");
  if (id == slow_id_) {
    slow_id_ = 0;
    slow_ = GlobalSlowmoState{};
  } else if (id == mouse_id_) {
    mouse_id_ = 0;
    mouse_ = GlobalMouseHiderState{};
  } else
    return fail(e, "Global child release identity differs");
  e.clear();
  return true;
}
} // namespace encore::upstream

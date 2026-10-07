#include "podunk_scene_signal_callbacks.hpp"
#include <limits>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
class PodunkSceneSignalCallbacks::Wait final
    : public FieldGlobalNativeReference {
public:
  FieldGlobalRegistry *owner = nullptr;
  const FieldSceneSignalCallbacksData *data = nullptr;
  FieldGlobalExternalBinding proof{};
  std::function<bool()> resume;
  bool pending = true;
  FieldGlobalExternalBinding binding() const override { return proof; }
  const char *native_class() const override {
    return proof.source.native_class.c_str();
  }
  const FieldGlobalRegistry *registry() const override { return owner; }
  bool checked_source_hash(std::string_view p,
                           std::array<uint8_t, 32> &h) const override {
    if (!data || p != data->wait_source())
      return false;
    h = data->wait_sha();
    return true;
  }
  bool dispatch(const FieldDeferredMessage &m, std::string &e) override {
    const auto *symbol =
        data ? data->symbol(SceneSignalSymbol::FunctionCallback) : nullptr;
    if (!pending || !symbol || m.object != proof.object ||
        m.kind != FieldDeferredKind::Call || m.member != symbol->name ||
        m.args.size() != 1 ||
        !std::holds_alternative<FieldObjectRef>(m.args[0]) ||
        std::get<FieldObjectRef>(m.args[0]).id != proof.object || !resume)
      return fail(
          e, "Actual frame wait FunctionState signature/lifetime rejected");
    // Source yield(frame_changed): no emitted arguments and the final bind is
    // the actual FunctionState self Reference, then resume exactly once.
    pending = false;
    if (!resume())
      return fail(e, "Source frame wait resume rejected");
    e.clear();
    return true;
  }
};
PodunkSceneSignalCallbacks::~PodunkSceneSignalCallbacks() {
  std::string ignored;
  if (data_)
    disconnect(ignored);
}
bool PodunkSceneSignalCallbacks::prepare(const FieldSceneSignalCallbacksData &d,
                                         PodunkSceneConsumerInput i,
                                         std::string &e) {
  if (data_ || !d.valid() || !i.sources || !i.tree || !i.continuation ||
      !i.continuation->initialized() || !i.player ||
      i.sources->tree().identity().scene_id != d.identity().scene_id ||
      i.sources->tree().identity().source_sha256 !=
          d.identity().source_sha256 ||
      i.sources->tree().identity().upstream_commit !=
          d.identity().upstream_commit ||
      i.continuation->signals()->registry() != i.continuation->registry())
    return fail(
        e, "Scene signal same source/continuation/ObjectDB owners rejected");
  data_ = &d;
  input_ = i;
  e.clear();
  return true;
}
bool PodunkSceneSignalCallbacks::observe_allocated(
    FieldObjectId actual, const FieldNodeDescriptor &node,
    const FieldIdentity &identity, std::string &e) {
  if (!data_ || identity.upstream_commit != data_->identity().upstream_commit ||
      identity.scene_id != data_->identity().scene_id ||
      identity.source_sha256 != data_->identity().source_sha256 ||
      input_.continuation->registry()->tree_owner(actual).get() != input_.tree)
    return fail(e, "Scene signal constructor allocation owner/source rejected");
  const auto *d = input_.tree->descriptor(actual);
  const auto *state = input_.tree->state(actual);
  if (!d || !state || !state->alive || state->inside || d->id != node.id ||
      d->script != node.script || d->script_sha != node.script_sha ||
      !allocated_.emplace(node.id, actual).second)
    return fail(e, "Scene signal constructor source mapping rejected");
  e.clear();
  return true;
}
FieldObjectId PodunkSceneSignalCallbacks::area_object() const {
  if (!data_)
    return 0;
  const auto stable = input_.sources->lifecycle().area().id;
  auto actual = input_.tree->source_object(stable);
  if (actual)
    return actual;
  auto early = allocated_.find(stable);
  return early == allocated_.end() ? 0 : early->second;
}
bool PodunkSceneSignalCallbacks::source(uint32_t id, FieldObjectId &out,
                                        std::string &e) const {
  if (!data_)
    return fail(e, "Scene signal owner not prepared");
  auto actual = input_.tree->source_object(id);
  if (!actual) {
    auto early = allocated_.find(id);
    if (early != allocated_.end())
      actual = early->second;
  }
  auto *d = actual ? input_.tree->descriptor(actual) : nullptr;
  auto *s = actual ? input_.tree->state(actual) : nullptr;
  if (!d || !s || d->id != id || !s->alive || s->queued ||
      input_.continuation->registry()->tree_owner(actual).get() != input_.tree)
    return fail(e, "Scene signal source node not actual in same ObjectDB");
  out = actual;
  e.clear();
  return true;
}
bool PodunkSceneSignalCallbacks::connect(uint32_t node, SceneCallbackRole role,
                                         FieldObjectId sender,
                                         SceneSignalSymbol signal,
                                         Callback callback,
                                         std::vector<FieldDeferredValue> binds,
                                         std::string &e) {
  const auto *b = data_ ? data_->callback(node, role) : nullptr;
  const auto *s = data_ ? data_->symbol(signal) : nullptr;
  FieldObjectId receiver = 0;
  if (!b || !s || !callback || !source(node, receiver, e))
    return fail(e, "Source callback typed binding unavailable");
  const auto *descriptor = input_.tree->descriptor(receiver);
  if (descriptor->script != b->script || descriptor->script_sha != b->leaf_sha)
    return fail(e, "Source callback leaf script identity differs");
  auto key = std::make_pair(receiver, b->method);
  bool inserted = methods_.emplace(key, std::move(callback)).second;
  if (!input_.continuation->signals()->connect(
          sender, s->name, receiver, b->method, 0, std::move(binds), e)) {
    if (inserted)
      methods_.erase(key);
    return false;
  }
  connections_.push_back({sender, receiver, s->name, b->method});
  e.clear();
  return true;
}
bool PodunkSceneSignalCallbacks::flags(uint32_t id, FieldSceneSignalSlot slot,
                                       std::string &e) {
  SceneCallbackRole role =
      data_ && data_->callback(id, SceneCallbackRole::UpdateDoor)
          ? SceneCallbackRole::UpdateDoor
          : SceneCallbackRole::CheckFlags;
  return connect(
      id, role, input_.continuation->global()->core().owner(),
      SceneSignalSymbol::Flags,
      [slot](const Args &a, std::string &err) {
        return a.empty() && slot && slot(err);
      },
      {}, e);
}
bool PodunkSceneSignalCallbacks::area(uint32_t id, FieldSceneAreaSlot slot,
                                      std::string &e) {
  return connect(
      id, SceneCallbackRole::LeaveArea, area_object(),
      SceneSignalSymbol::AreaLeft,
      [slot](const Args &a, std::string &err) {
        if (a.size() != 1 || !std::holds_alternative<bool>(a[0]) || !slot)
          return fail(err, "Source area callback boolean signature rejected");
        return slot(std::get<bool>(a[0]), err);
      },
      {}, e);
}
bool PodunkSceneSignalCallbacks::emit_flags(std::string &e) {
  if (!data_)
    return fail(e, "Scene flags signal owner absent");
  return input_.continuation->signals()->emit(
      input_.continuation->global()->core().owner(),
      data_->symbol(SceneSignalSymbol::Flags)->name, {}, e);
}
bool PodunkSceneSignalCallbacks::declaration(FieldObjectId id,
                                             std::string_view name,
                                             uint32_t &arity,
                                             std::string &e) const {
  if (!data_ || !input_.continuation->registry()->object_exists(id))
    return fail(e, "Scene signal emitter unavailable");
  for (const auto role :
       {SceneSignalSymbol::Flags, SceneSignalSymbol::InputsChanged,
        SceneSignalSymbol::LocaleChanged}) {
    auto *b = data_->symbol(role);
    if (id == input_.continuation->global()->core().owner() &&
        name == b->name) {
      arity = b->declaration_arguments;
      e.clear();
      return true;
    }
  }
  for (const auto role :
       {SceneSignalSymbol::AreaLeft, SceneSignalSymbol::Switches}) {
    auto *b = data_->symbol(role);
    if (id == area_object() && name == b->name) {
      arity = b->declaration_arguments;
      e.clear();
      return true;
    }
  }
  auto player = input_.player->body().object();
  for (const auto role :
       {SceneSignalSymbol::NearbyEntered, SceneSignalSymbol::NearbyExited,
        SceneSignalSymbol::Paused, SceneSignalSymbol::Unpaused}) {
    auto *b = data_->symbol(role);
    if (id == player && player && name == b->name) {
      arity = b->declaration_arguments;
      e.clear();
      return true;
    }
  }
  // Native declarations are admitted only on the actual leaves belonging to
  // their checked typed pack, never on an arbitrary node with a similar name.
  const auto *d = input_.tree->descriptor(id);
  if (!d)
    return fail(e, "Scene native signal leaf descriptor absent");
  auto *f = data_->symbol(SceneSignalSymbol::FrameChanged);
  if (name == f->name && (input_.sources->arrows().sprite(d->id) ||
                          input_.sources->sprite().record(d->id))) {
    arity = 0;
    e.clear();
    return true;
  }
  auto *h = data_->symbol(SceneSignalSymbol::Hide);
  if (name == h->name && input_.sources->prompt().record(d->id)) {
    arity = 0;
    e.clear();
    return true;
  }
  auto *a = data_->symbol(SceneSignalSymbol::AnimationFinished);
  if (name == a->name && input_.sources->arrows().player(d->id)) {
    arity = a->declaration_arguments;
    e.clear();
    return true;
  }
  return fail(e, "Scene source/native signal declaration unsupported");
}
bool PodunkSceneSignalCallbacks::emission(FieldObjectId id,
                                          std::string_view name, size_t count,
                                          std::string &e) const {
  const auto *b = data_ ? data_->symbol(SceneSignalSymbol::AreaLeft) : nullptr;
  if (!b || id != area_object() || name != b->name ||
      count != b->emission_arguments)
    return fail(e,
                "Scene extra signal arguments lack actual source emit proof");
  e.clear();
  return true;
}
bool PodunkSceneSignalCallbacks::handles(const FieldDeferredMessage &m) const {
  return data_ && m.kind==FieldDeferredKind::Call &&
         methods_.count({m.object,m.member});
}
bool PodunkSceneSignalCallbacks::dispatch(const FieldDeferredMessage &m,
                                          std::string &e) {
  if (!data_ || m.kind != FieldDeferredKind::Call)
    return fail(e, "Scene source callback kind unsupported");
  auto i = methods_.find({m.object, m.member});
  const auto *d = input_.tree->descriptor(m.object);
  const auto *s = input_.tree->state(m.object);
  if (i == methods_.end() || !d || !s || !s->alive || s->queued ||
      input_.continuation->registry()->tree_owner(m.object).get() !=
          input_.tree)
    return fail(e, "Scene actual callback method not connected/alive");
  const SceneCallbackBinding *b = nullptr;
  for (const auto &v : data_->callbacks())
    if (v.node == d->id && v.method == m.member) {
      b = &v;
      break;
    }
  if (!b || b->leaf_sha != d->script_sha || b->script != d->script ||
      m.args.size() != b->arguments)
    return fail(e, "Scene source callback signature/leaf proof rejected");
  if (!i->second(m.args, e)) {
    if (e.empty())
      e = "Source connected callback execution rejected";
    return false;
  }
  e.clear();
  return true;
}
bool PodunkSceneSignalCallbacks::wait(uint32_t sender, uint64_t token,
                                      std::function<bool()> resume,
                                      std::string &e) {
  // Previously resumed FunctionStates have already lost their one-shot
  // signal. Release their real References before allocating another waiter.
  for (auto i = waits_.begin(); i != waits_.end();) {
    if (i->second->pending) {
      ++i;
      continue;
    }
    auto object = i->second->proof.object;
    i = waits_.erase(i);
    if (!input_.continuation->registry()->retire_object(object, e))
      return false;
  }
  FieldObjectId actual = 0;
  if (!data_ || !token || !resume || !input_.sources->arrows().sprite(sender) ||
      waits_.count({sender, token}) || !source(sender, actual, e))
    return fail(e, "Source frame waiter sender/token rejected");
  auto &r = *input_.continuation->registry();
  auto state = std::make_shared<Wait>();
  FieldObjectId object = 0;
  if (!r.allocate_object(object, e))
    return false;
  state->owner = &r;
  state->data = data_;
  state->resume = std::move(resume);
  auto &binding = state->proof;
  binding.object = object;
  binding.family = 0x454e0068;
  binding.capability = 1;
  auto &spec = binding.source;
  spec.identity = data_->identity();
  spec.identity.source_sha256 = data_->wait_sha();
  spec.stable_id = data_->wait_id();
  spec.role = 5;
  spec.source = spec.script = data_->wait_source();
  spec.source_sha = spec.script_sha = data_->wait_sha();
  spec.native_class = data_->symbol(SceneSignalSymbol::FunctionClass)->name;
  if (!r.publish_native_reference(spec, object, state, e)) {
    r.retire_object(object, e);
    return false;
  }
  auto *signal = data_->symbol(SceneSignalSymbol::FrameChanged);
  auto *method = data_->symbol(SceneSignalSymbol::FunctionCallback);
  if (!input_.continuation->signals()->connect(
          actual, signal->name, object, method->name, data_->wait_flags(),
          {FieldObjectRef{object}}, e)) {
    state.reset();
    std::string ignored;
    r.retire_object(object, ignored);
    return false;
  }
  waits_.emplace(std::make_pair(sender, token), std::move(state));
  e.clear();
  return true;
}
bool PodunkSceneSignalCallbacks::cancel(uint32_t sender, uint64_t token,
                                        std::string &e) {
  auto i = waits_.find({sender, token});
  if (i == waits_.end())
    return fail(e, "Source frame wait cancellation has no token");
  FieldObjectId actual = 0;
  if (!source(sender, actual, e))
    return false;
  auto signal = data_->symbol(SceneSignalSymbol::FrameChanged);
  auto method = data_->symbol(SceneSignalSymbol::FunctionCallback);
  bool connected = false;
  if (!input_.continuation->signals()->connected(actual, signal->name,
                                                 i->second->proof.object,
                                                 method->name, connected, e))
    return false;
  if (connected &&
      !input_.continuation->signals()->disconnect(
          actual, signal->name, i->second->proof.object, method->name, e))
    return false;
  auto object = i->second->proof.object;
  waits_.erase(i);
  return input_.continuation->registry()->retire_object(object, e);
}
bool PodunkSceneSignalCallbacks::disconnect(std::string &e) {
  if (!data_)
    return fail(e, "Scene callbacks not prepared");
  auto &bus = *input_.continuation->signals();
  auto &registry = *input_.continuation->registry();
  for (auto &i : connections_) {
    if (!registry.object_exists(i.sender) || !registry.object_exists(i.target))
      continue;
    bool connected = false;
    if (!bus.connected(i.sender, i.signal, i.target, i.method, connected, e) ||
        (connected &&
         !bus.disconnect(i.sender, i.signal, i.target, i.method, e)))
      return false;
  }
  while (!waits_.empty()) {
    auto i = waits_.begin();
    if (!cancel(i->first.first, i->first.second, e))
      return false;
  }
  connections_.clear();
  methods_.clear();
  allocated_.clear();
  arrow_finished_.clear();
  data_ = nullptr;
  e.clear();
  return true;
}
bool PodunkSceneSignalCallbacks::apply(PodunkSceneMechanismOwners &o,
                                       std::string &e) {
  if (!data_)
    return fail(e, "Scene callbacks not prepared");
  o.scene.connect_flags = [this](uint32_t id, FieldSceneSignalSlot cb,
                                 std::string &err) {
    return flags(id, std::move(cb), err);
  };
  o.scene.emit_flags = [this](std::string &err) { return emit_flags(err); };
  o.scene.connect_area_left = [this](uint32_t id, FieldSceneAreaSlot cb,
                                     std::string &err) {
    return area(id, std::move(cb), err);
  };
  o.scene.emit_area_left = [this](bool changed, std::string &err) {
    return input_.continuation->signals()->emit(
        area_object(), data_->symbol(SceneSignalSymbol::AreaLeft)->name,
        {changed}, err);
  };
  o.scene.connect_switches = [this](uint32_t id, std::function<void(bool)> cb,
                                    std::string &err) {
    return connect(
        id, SceneCallbackRole::Switches, area_object(),
        SceneSignalSymbol::Switches,
        [this, cb](const Args &a, std::string &error) {
          if (a.size() != 3 || !std::holds_alternative<FieldObjectRef>(a[0]) ||
              !std::holds_alternative<bool>(a[1]) ||
              !std::holds_alternative<bool>(a[2]) || !cb)
            return fail(error,
                        "Switch signal actual object/bool signature rejected");
          auto object = std::get<FieldObjectRef>(a[0]).id;
          if (object) {
            const auto *d = input_.tree->descriptor(object);
            const auto *type = data_->symbol(SceneSignalSymbol::SwitchSource);
            auto proof = type ? data_->sources().find(type->name)
                              : data_->sources().end();
            if (!d || !type || proof == data_->sources().end() ||
                d->script != type->name || d->script_sha != proof->second ||
                input_.continuation->registry()->tree_owner(object).get() !=
                    input_.tree)
              return fail(error, "Switch signal source typed emitter rejected");
          }
          cb(std::get<bool>(a[1]));
          return true;
        },
        {}, err);
  };
  o.interact.connect_flags = [this](uint32_t id, FieldSceneSignalSlot cb,
                                    std::string &err) {
    return flags(id, std::move(cb), err);
  };
  o.openable.connect_flags = [this](uint32_t id, std::function<bool()> cb,
                                    std::string &err) {
    return flags(id, [cb](std::string &) { return cb && cb(); }, err);
  };
  o.payphone.connect_flags = o.openable.connect_flags;
  o.prompt.connect = [this](uint32_t id,
                            std::function<bool(uint32_t, bool)> near,
                            std::function<bool()> pause,
                            std::function<bool()> key, std::string &err) {
    auto player = input_.player->body().object();
    auto global = input_.continuation->global()->core().owner();
    Callback nearby = [this, id, near](const Args &a, std::string &error) {
      if (a.size() != 2 || !std::holds_alternative<FieldObjectRef>(a[0]) ||
          !std::holds_alternative<bool>(a[1]) || !near)
        return fail(error, "Prompt source nearby signature rejected");
      const auto *d = input_.sources->prompt().record(id);
      FieldObjectId parent = 0;
      if (!d || !source(d->parent_id, parent, error))
        return false;
      // Native signal arguments are actual ObjectIDs. The typed prompt consumer
      // accepts its source descriptor key only after actual parent identity
      // agrees.
      if (std::get<FieldObjectRef>(a[0]).id != parent) {
        error.clear();
        return true;
      }
      return near(d->parent_id, std::get<bool>(a[1]));
    };
    Callback paused = [pause](const Args &a, std::string &) {
      return a.empty() && pause && pause();
    };
    Callback keys = [key](const Args &a, std::string &) {
      return a.empty() && key && key();
    };
    return connect(id, SceneCallbackRole::Nearby, player,
                   SceneSignalSymbol::NearbyEntered, nearby, {true}, err) &&
           connect(id, SceneCallbackRole::Nearby, player,
                   SceneSignalSymbol::NearbyExited, nearby, {false}, err) &&
           connect(id, SceneCallbackRole::Pause, player,
                   SceneSignalSymbol::Paused, paused, {}, err) &&
           connect(id, SceneCallbackRole::Pause, player,
                   SceneSignalSymbol::Unpaused, paused, {}, err) &&
           connect(id, SceneCallbackRole::KeyName, global,
                   SceneSignalSymbol::InputsChanged, keys, {}, err) &&
           connect(id, SceneCallbackRole::KeyName, global,
                   SceneSignalSymbol::LocaleChanged, keys, {}, err);
  };
  o.prompt.hide_signal = [this](uint32_t id, std::string &err) {
    FieldObjectId actual = 0;
    return source(id, actual, err) &&
           input_.continuation->signals()->emit(
               actual, data_->symbol(SceneSignalSymbol::Hide)->name, {}, err);
  };
  o.arrows.await_frame_changed =
      [this](uint32_t id, uint64_t token, std::function<bool()> cb,
             std::string &err) { return wait(id, token, std::move(cb), err); };
  o.arrows.cancel_frame_changed = [this](uint32_t id, uint64_t token,
                                         std::string &err) {
    return cancel(id, token, err);
  };
  o.arrows.connect_animation_finished = [this](uint32_t id,
                                               std::function<bool(uint32_t)> cb,
                                               std::string &err) {
    const auto *p = input_.sources->arrows().player(id);
    const auto *a =
        p ? input_.sources->arrows().sprite(p->target_root) : nullptr;
    FieldObjectId actual = 0, arrow = 0;
    if (!p || !a || !cb || !source(id, actual, err) ||
        !source(a->id, arrow, err))
      return fail(err, "MapArrow finished actual source leaves rejected");
    if (!arrow_finished_
             .emplace(arrow, std::make_pair(p->profile, std::move(cb)))
             .second)
      return fail(err, "MapArrow actual finished receiver duplicated");
    return connect(
        a->root_id, SceneCallbackRole::ArrowFinished, actual,
        SceneSignalSymbol::AnimationFinished,
        [this](const Args &args, std::string &error) {
          if (args.size() != 2 ||
              !std::holds_alternative<std::string>(args[0]) ||
              !std::holds_alternative<FieldObjectRef>(args[1]))
            return fail(
                error, "MapArrow animation finished source signature rejected");
          auto item =
              arrow_finished_.find(std::get<FieldObjectRef>(args[1]).id);
          if (item == arrow_finished_.end())
            return fail(error, "MapArrow source bound actual arrow missing");
          for (uint32_t role = 1; role <= 3; ++role) {
            auto *c = input_.sources->arrows().clip(item->second.first, role);
            if (c && c->name == std::get<std::string>(args[0]))
              return item->second.second(role);
          }
          return fail(error, "MapArrow finished unknown source clip");
        },
        {FieldObjectRef{arrow}}, err);
  };
  e.clear();
  return true;
}
} // namespace encore::ctr

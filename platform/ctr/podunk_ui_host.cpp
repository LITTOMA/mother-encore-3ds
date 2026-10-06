#include "podunk_ui_host.hpp"
#include <3ds.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <limits>
#include <set>
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
bool same(const FieldGlobalExternalSpec &a, const FieldGlobalExternalSpec &b) {
  return same(a.identity, b.identity) && a.stable_id == b.stable_id &&
         a.role == b.role && a.name == b.name &&
         a.native_class == b.native_class && a.source == b.source &&
         a.script == b.script && a.source_sha == b.source_sha &&
         a.script_sha == b.script_sha;
}
bool same(const FieldNodeBinding &a, const FieldNodeBinding &b) {
  return same(a.identity, b.identity) && a.stable_id == b.stable_id &&
         a.class_index == b.class_index && a.native_class == b.native_class &&
         a.script_sha == b.script_sha && a.family == b.family &&
         a.capability == b.capability;
}
} // namespace
bool PodunkUiHost::initialize(const FieldGlobalRegistryData &registry_data,
                              FieldGlobalRegistry &registry,
                              SourceRandom &random, PodunkUiHostData data,
                              PodunkUiHostServices services, std::string &e) {
  if (registry_ || !registry_data.valid() || !data.ui || !data.ui->valid() ||
      !data.preloads || !data.preloads->valid() || !data.dialogue ||
      !data.dialogue->valid() || !data.backgrounds ||
      !data.backgrounds->valid() || !data.timer_data ||
      !data.timer_data->valid() || !services.timers)
    return fail(
        e,
        "UI owning host complete source resource/actual Timer owners absent");
  auto source = std::find_if(
      registry_data.autoloads().begin(), registry_data.autoloads().end(),
      [&](const auto &a) { return a.id == registry_data.ui_autoload(); });
  std::array<uint8_t, 32> hash;
  if (source == registry_data.autoloads().end() ||
      source->script != data.ui->source_script() ||
      source->script_sha != data.ui->identity().source_sha256 ||
      registry_data.identity().upstream_commit !=
          data.ui->identity().upstream_commit ||
      data.backgrounds->identity().upstream_commit !=
          data.ui->identity().upstream_commit ||
      !data.backgrounds->source_hash(data.ui->source_script(), hash) ||
      hash != source->script_sha)
    return fail(e, "UI owning host exact source autoload/pin proof rejected");
  if (!preloads_.initialize(data.preloads, *data.ui, data.dialogue, e))
    return false;
  registry_data_ = &registry_data;
  registry_ = &registry;
  random_ = &random;
  data_ = std::move(data);
  services_ = services;
  tick_origin_ = svcGetSystemTick();
  clock_started_ = true;
  e.clear();
  return true;
}
FieldGlobalRegistryHost PodunkUiHost::registry_host() {
  FieldGlobalRegistryHost h;
  h.construct = [this](FieldObjectId id, const FieldGlobalExternalSpec &s,
                       std::unique_ptr<FieldGlobalExternalObject> &out,
                       std::string &e) { return construct(id, s, out, e); };
  return h;
}
bool PodunkUiHost::connect_tree(std::shared_ptr<FieldNodeTreeRuntime> tree,
                                FieldGlobalRegistry::NodeDispatch dispatch,
                                std::string &e) {
  if (!registry_ || tree_ || !registry_->kernel() || !tree ||
      tree->object_domain() != registry_->kernel() || !tree->root() ||
      !tree->state(tree->root()) || !dispatch)
    return fail(e, "UI actual shared registry/Tree dispatch domain absent");
  tree_ = std::move(tree);
  node_dispatch_ = std::move(dispatch);
  e.clear();
  return true;
}
FieldUiManagerRuntime *PodunkUiHost::ui() const {
  return registry_ && ui_object_ && registry_->object_exists(ui_object_)
             ? ui_
             : nullptr;
}
bool PodunkUiHost::construct(FieldObjectId id, const FieldGlobalExternalSpec &s,
                             std::unique_ptr<FieldGlobalExternalObject> &out,
                             std::string &e) {
  if (!registry_ || !id || out)
    return fail(e, "UI external factory actual allocated slot rejected");
  if (s.role != 3 || s.stable_id != registry_data_->ui_autoload()) {
    if (!services_.external)
      return fail(e, "UI factory actual SceneTree/Viewport/preceding autoload "
                     "owner pending");
    if (!services_.external->construct(id, s, out, e) || !out)
      return false;
    auto b = out->binding();
    FieldGlobalExternalState state;
    if (b.object != id || !b.family || !b.capability || !same(b.source, s) ||
        !out->state(state, e) || state.name != s.name || state.parent ||
        !state.children.empty() || state.inside || state.ready)
      return fail(e, "UI delegated external object live out-of-tree "
                     "constructor receipt rejected");
    e.clear();
    return true;
  }
  if (ui() || !services_.signals || !tree_ || !node_dispatch_ ||
      !registry_->kernel() || tree_->object_domain() != registry_->kernel())
    return fail(
        e,
        "UI source singleton duplicate or actual source factory Tree pending");
  if (!backgrounds_bound_) {
    if (!backgrounds_.initialize(data_.backgrounds, *registry_, e))
      return false;
    backgrounds_bound_ = true;
  }
  FieldUiManagerHost host;
  host.tree = tree_;
  host.dispatch = node_dispatch_;
  host.backgrounds = &backgrounds_;
  host.load_recipe = [this](const FieldUiPreload &p,
                            std::shared_ptr<const FieldNodeRecipeData> &r,
                            std::string &e) {
    return preloads_.load_recipe(p, r, e);
  };
  host.clock = [this](uint64_t &seconds, uint64_t &ticks, std::string &e) {
    return clock(seconds, ticks, e);
  };
  host.menu_flavor = [this](std::string &name, std::string &e) {
    return flavor(name, e);
  };
  host.emit_menu_flavor_updated = [this](std::string &e) {
    auto *owner = ui();
    FieldGlobalExternalState state;
    if (!owner || !services_.signals || !owner->state(state, e) ||
        !state.inside)
      return fail(e,
                  "UI actual source emitter/inside menu signal owner pending");
    return services_.signals->menu_flavor_updated(ui_object_, e);
  };
  FieldGlobalExternalBinding binding;
  binding.object = id;
  binding.source = s;
  binding.family = 0x454e0045;
  binding.capability = 1;
  auto owner = std::make_unique<FieldUiManagerRuntime>();
  if (!owner->initialize(*data_.ui, *registry_, *random_, binding,
                         std::move(host), e) ||
      !owner->initialize_fields(e))
    return false;
  if (!services_.signals->attach_ui(binding, e))
    return false;
  ui_ = owner.get();
  ui_object_ = id;
  out = std::move(owner);
  e.clear();
  return true;
}
bool PodunkUiHost::construct_instances(std::string &e) {
  auto *owner = ui();
  if (!owner || !services_.native_ui || !tree_ || owner->ready_cursor() != 1)
    return fail(e, "UI actual seven onready instances before randomize absent");
  const auto &instances = owner->instance_objects();
  if (instances.size() != data_.ui->instances().size())
    return fail(e, "UI complete source instance roster differs");
  for (const auto &a : data_.ui->instances()) {
    auto found = instances.find(a.name);
    auto *r = data_.ui->recipe(a.recipe);
    if (found == instances.end() || !r)
      return fail(e, "UI checked actual onready instance missing");
    const auto *root = tree_->state(found->second);
    if (!root || !root->alive || root->parent || root->inside)
      return fail(e, "UI onready constructor actual detached root rejected");
    std::vector<FieldObjectId> stack{found->second};
    std::set<FieldObjectId> seen;
    std::set<uint32_t> sources;
    while (!stack.empty()) {
      auto id = stack.back();
      stack.pop_back();
      const auto *state = tree_->state(id);
      const auto *n = tree_->descriptor(id);
      FieldIdentity identity;
      FieldNodeBinding binding;
      if (!seen.insert(id).second || !state || !state->alive || state->inside ||
          !n || !sources.insert(n->id).second || !r->record(n->id) ||
          !tree_->object_identity(id, identity) ||
          !same(identity, r->identity()) || !bind(id, *n, binding, e))
        return fail(e,
                    "UI original native/script detached constructor pending");
      for (auto it = state->children.rbegin(); it != state->children.rend();
           ++it) {
        const auto *child = tree_->state(*it);
        if (!child || child->parent != id)
          return fail(e, "UI original constructor child ownership rejected");
        stack.push_back(*it);
      }
    }
    if (sources.size() != r->records().size())
      return fail(e, "UI original full recipe constructor coverage rejected");
  }
  e.clear();
  return true;
}
bool PodunkUiHost::clock(uint64_t &seconds, uint64_t &ticks, std::string &e) {
  if (!construct_instances(e))
    return false;
  if (!clock_started_ || !registry_ || !registry_->kernel())
    return fail(e, "UI actual 3DS monotonic clock epoch pending");
  auto unix_time = std::time(nullptr);
  if (unix_time < 0)
    return fail(e, "UI actual 3DS wall clock unavailable");
  auto now = svcGetSystemTick();
  if (now < tick_origin_)
    return fail(e, "UI 3DS monotonic tick epoch invalid");
  auto elapsed = now - tick_origin_;
  auto whole = elapsed / SYSCLOCK_ARM11;
  if (whole > std::numeric_limits<uint64_t>::max() / 1000000)
    return fail(e, "UI source microsecond clock overflow");
  seconds = uint64_t(unix_time);
  ticks =
      whole * 1000000 + (elapsed % SYSCLOCK_ARM11) * 1000000 / SYSCLOCK_ARM11;
  e.clear();
  return true;
}
bool PodunkUiHost::flavor(std::string &out, std::string &e) const {
  if (!services_.globaldata || !registry_data_ || !registry_)
    return fail(e, "UI full actual globaldata menu-flavor owner pending");
  auto binding = services_.globaldata->binding();
  auto source = std::find_if(
      registry_data_->autoloads().begin(), registry_data_->autoloads().end(),
      [&](const auto &a) { return a.id == binding.source.stable_id; });
  std::array<uint8_t, 32> proof;
  if (source == registry_data_->autoloads().end() ||
      source->id == registry_data_->ui_autoload() || binding.source.role != 3 ||
      !binding.object || !binding.family || !binding.capability ||
      source->name != binding.source.name ||
      source->native_class != binding.source.native_class ||
      source->path != binding.source.source ||
      source->source_sha != binding.source.source_sha ||
      source->script != binding.source.script ||
      source->script_sha != binding.source.script_sha ||
      binding.source.identity.upstream_commit !=
          data_.ui->identity().upstream_commit ||
      !data_.ui->source_hash(source->script, proof) ||
      proof != source->script_sha)
    return fail(e, "UI actual globaldata source binding rejected");
  FieldObjectId actual = 0;
  FieldGlobalExternalState state;
  if (!registry_->lookup_absolute(
          "/" + registry_data_->root_name() + "/" + source->name, actual, e) ||
      actual != binding.object || !services_.globaldata->state(state, e) ||
      state.name != source->name || !state.inside || !state.ready ||
      state.parent != registry_->root())
    return fail(
        e, "UI menu-flavor getter actual completed globaldata object absent");
  return services_.globaldata->menu_flavor(out, e);
}
bool PodunkUiHost::entered(FieldObjectId parent, std::string &e) {
  auto *owner = ui();
  if (!owner || parent != registry_->root())
    return fail(e, "UI actual registered singleton/Viewport enter pending");
  return owner->entered(parent, e);
}
bool PodunkUiHost::advance_ready(std::string &e) {
  auto *owner = ui();
  if (!owner)
    return fail(e, "UI actual singleton constructor not published");
  if (owner->ready_cursor() == 0) {
    if (!services_.native_ui)
      return fail(e, "UI seven complete native/script constructors pending");
    for (const auto &a : data_.ui->instances()) {
      const auto *r = data_.ui->recipe(a.recipe);
      if (!r || !services_.native_ui->admit_constructor(*r, e))
        return false;
    }
  }
  return owner->advance_ready(e);
}
bool PodunkUiHost::exited(std::string &e) {
  auto *owner = ui();
  if (!owner)
    return fail(e, "UI actual registered exit owner absent");
  return owner->exited(e);
}
bool PodunkUiHost::state(FieldGlobalExternalState &out, std::string &e) const {
  auto *owner = ui();
  if (!owner)
    return fail(e, "UI actual registered source object absent");
  return owner->state(out, e);
}
const FieldNodeRecipeData *PodunkUiHost::recipe(const FieldIdentity &id) const {
  if (!data_.ui)
    return nullptr;
  if (same(data_.dialogue->identity(), id))
    return data_.dialogue.get();
  if (auto *r = data_.ui->recipe(id.scene_id); r && same(r->identity(), id))
    return r;
  for (const auto &entry : data_.preloads->entries())
    if (entry.recipe && same(entry.recipe->identity(), id))
      return entry.recipe.get();
  return nullptr;
}
bool PodunkUiHost::verify_binding(FieldObjectId id,
                                  const FieldNodeDescriptor &n,
                                  const FieldNodeBinding &b,
                                  std::string &e) const {
  auto tree = registry_->tree_owner(id);
  const auto *actual = tree ? tree->descriptor(id) : nullptr;
  FieldIdentity identity;
  if (!actual || actual->id != n.id || actual->path != n.path ||
      actual->name != n.name || actual->native_class != n.native_class ||
      actual->script != n.script || actual->script_sha != n.script_sha ||
      actual->class_index != n.class_index ||
      tree->object_domain() != registry_->kernel() ||
      !tree->object_identity(id, identity) || !same(identity, b.identity) ||
      b.stable_id != n.id || b.class_index != n.class_index ||
      b.native_class != n.native_class || b.script_sha != n.script_sha ||
      !b.family || !b.capability)
    return fail(e, "UI actual node/native source binding rejected");
  auto *r = recipe(identity);
  auto *record = r ? r->record(n.id) : nullptr;
  if (!record || record->path != n.path || record->name != n.name ||
      record->script != n.script || record->native_class != n.native_class ||
      record->script_sha != n.script_sha)
    return fail(e, "UI node outside original checked recipe source");
  return true;
}
bool PodunkUiHost::bind(FieldObjectId id, const FieldNodeDescriptor &n,
                        FieldNodeBinding &out, std::string &e) {
  if (!registry_)
    return fail(e, "UI uninitialized actual node binding rejected");
  auto existing = owned_.find(id);
  if (existing != owned_.end()) {
    if (existing->second.deleting ||
        !verify_binding(id, n, existing->second.binding, e))
      return fail(e, "UI existing constructor source binding rejected");
    out = existing->second.binding;
    e.clear();
    return true;
  }
  auto tree = registry_->tree_owner(id);
  FieldIdentity identity;
  if (!tree || !tree->object_identity(id, identity) || !recipe(identity))
    return fail(e, "UI actual registered source subtree owner absent");
  FieldNodeBinding binding;
  Owner kind;
  if (same(identity, data_.dialogue->identity())) {
    if (!services_.dialogue || !services_.dialogue->bind(id, n, binding, e))
      return false;
    kind = Owner::Dialogue;
  } else if (n.native_class == "Timer") {
    if (!n.script.empty() || !data_.timer_data->record(identity, n.id) ||
        !services_.native_ui)
      return fail(e, "UI actual source native Timer/script consumer absent");
    binding.identity = identity;
    binding.stable_id = n.id;
    binding.class_index = n.class_index;
    binding.native_class = n.native_class;
    binding.script_sha = n.script_sha;
    binding.family = 0x454e0044;
    binding.capability = 1;
    PodunkUiNativeState state;
    if (!services_.native_ui->bind_timer_node(*tree, id, n, binding, e) ||
        !services_.native_ui->state(id, state, e) || state.object != id ||
        !state.constructed || !state.script_constructed ||
        !same(state.binding, binding))
      return fail(e, "UI actual Timer Node constructor pending");
    FieldNodeBinding actual;
    if (!services_.timers->attach(*tree, id, actual, e) ||
        !same(actual, binding))
      return fail(e, "UI actual Timer state constructor rejected");
    kind = Owner::Timer;
  } else {
    if (!services_.native_ui ||
        !services_.native_ui->bind(*tree, id, n, binding, e))
      return fail(e,
                  "UI complete actual native/script node constructor pending");
    PodunkUiNativeState state;
    if (!services_.native_ui->state(id, state, e) || state.object != id ||
        !state.constructed || !state.script_constructed ||
        !same(state.binding, binding))
      return fail(e, "UI actual native/source constructor state rejected");
    kind = Owner::Native;
  }
  if (!verify_binding(id, n, binding, e))
    return false;
  owned_.emplace(id, Owned{kind, binding});
  out = binding;
  e.clear();
  return true;
}
bool PodunkUiHost::dispatch(FieldObjectId id, const FieldNodeBinding &b,
                            FieldTreePhase phase, std::string &e) {
  auto i = owned_.find(id);
  auto tree = registry_ ? registry_->tree_owner(id) : nullptr;
  auto *n = tree ? tree->descriptor(id) : nullptr;
  if (i == owned_.end() || !n || !same(i->second.binding, b) ||
      !verify_binding(id, *n, b, e))
    return fail(e, "UI native notification actual owner/source rejected");
  if (i->second.kind == Owner::Dialogue) {
    if (!services_.dialogue->dispatch(id, b, phase, e))
      return false;
    if (phase == FieldTreePhase::Deleting)
      i->second.deleting = true;
    return true;
  }
  if (i->second.kind == Owner::Timer) {
    if (phase == FieldTreePhase::ReadyNative)
      return services_.timers->ready(id, e);
    if (phase == FieldTreePhase::IdleInternal ||
        phase == FieldTreePhase::PhysicsInternal) {
      PodunkUiFrame frame;
      if (!services_.native_ui || !services_.native_ui->frame(id, frame, e) ||
          frame.phase != phase || !std::isfinite(frame.delta) ||
          frame.delta < 0)
        return fail(e, "UI source Timer actual traversal delta/pause pending");
      return services_.timers->process(id, phase, frame.delta,
                                       frame.tree_paused, e);
    }

    // Common native Node/signals are performed by the actual tree owner;
    // this delegates no Timer-specific Ready/process a second time.
    if (!services_.native_ui)
      return fail(e, "UI native Timer common Node/signal owner pending");
    if (!services_.native_ui->phase(id, b, phase, e))
      return false;
    if (phase == FieldTreePhase::Deleting)
      i->second.deleting = true;
    return true;
  }
  if (!services_.native_ui || !services_.native_ui->phase(id, b, phase, e))
    return false;
  if (phase == FieldTreePhase::Deleting) {
    i->second.deleting = true;
    e.clear();
    return true;
  }
  PodunkUiNativeState actual;
  if (!services_.native_ui->state(id, actual, e) || !same(actual.binding, b) ||
      actual.object != id || !actual.constructed ||
      !actual.script_constructed ||
      (phase == FieldTreePhase::ReadyNative && !actual.native_ready) ||
      (phase == FieldTreePhase::ReadyScript && !actual.script_ready))
    return fail(e, "UI actual source lifecycle state did not advance");
  e.clear();
  return true;
}
bool PodunkUiHost::deferred(const FieldDeferredMessage &m, std::string &e) {
  if (ui() && m.object == ui_object_)
    return ui_->deferred(m, e);
  auto i = owned_.find(m.object);
  if (i == owned_.end())
    return fail(e, "UI deferred message actual object owner absent");
  if (i->second.kind == Owner::Dialogue)
    return services_.dialogue->deferred(m, e);
  if (i->second.kind == Owner::Timer)
    return fail(e, "UI native Timer deferred member needs typed method owner");
  if (!services_.native_ui)
    return fail(e, "UI deferred native/source method owner pending");
  return services_.native_ui->deferred(m, e);
}
bool PodunkUiHost::release(FieldObjectId id, const FieldNodeBinding &binding,
                           std::string &e) {
  auto i = owned_.find(id);
  if (i == owned_.end() || !i->second.deleting ||
      !same(binding, i->second.binding))
    return fail(
        e, "UI final source object release before actual PREDELETE rejected");
  if (i->second.kind == Owner::Timer) {
    if (!i->second.timer_released) {
      if (!services_.timers->release(id, e))
        return false;
      i->second.timer_released = true;
    }
    if (!services_.native_ui || !services_.native_ui->release(id, binding, e))
      return false;
  } else if (i->second.kind == Owner::Native) {
    if (!services_.native_ui || !services_.native_ui->release(id, binding, e))
      return false;
  }
  // Dialogue PREDELETE already executes its actual shared owner cleanup.
  owned_.erase(i);
  e.clear();
  return true;
}
std::vector<std::string> PodunkUiHost::pending_owners() const {
  std::vector<std::string> out;
  if (!registry_data_) {
    out.emplace_back("UI host checked data/ownership not initialized");
    return out;
  }
  if (!services_.external) {
    out.push_back(registry_data_->kernel_native() + " actual native owner");
    out.push_back(registry_data_->root_native() + " actual native owner");
    for (const auto &a : registry_data_->autoloads())
      if (a.id != registry_data_->ui_autoload())
        out.push_back(a.name + " full source constructor/native enter/Ready");
  }
  if (!services_.globaldata)
    out.emplace_back("Complete globaldata menu_flavor getter owner (flags "
                     "component alone insufficient)");
  if (!services_.signals)
    out.emplace_back("Actual UiManager source signal registry");
  if (!services_.native_ui)
    out.emplace_back("Complete native/script constructors and Ready for seven "
                     "original UI trees");
  if (!services_.dialogue)
    out.emplace_back("Existing DialogueBox native/source factory host");
  if (!tree_)
    out.emplace_back("Actual shared source Tree/ObjectDB dispatch");
  out.emplace_back("UiManager source Ready tail seven canvas "
                   "additions/Fade/deferred Camera");
  return out;
}
} // namespace encore::ctr

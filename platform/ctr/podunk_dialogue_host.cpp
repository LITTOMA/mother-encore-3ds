#include "podunk_dialogue_host.hpp"
#include <algorithm>
#include <cmath>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *message) {
  e = message;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
bool notification(FieldTreePhase p) {
  switch (p) {
  case FieldTreePhase::TreeEntered:
  case FieldTreePhase::NodeAdded:
  case FieldTreePhase::ChildEntered:
  case FieldTreePhase::PostEnterNative:
  case FieldTreePhase::TreeExiting:
  case FieldTreePhase::NodeRemoved:
  case FieldTreePhase::ChildExiting:
  case FieldTreePhase::TreeExited:
  case FieldTreePhase::VisibilityChanged:
  case FieldTreePhase::Hide:
  case FieldTreePhase::TransformChanged:
  case FieldTreePhase::LocalTransformChanged:
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
  case FieldTreePhase::ChildMoved:
    return true;
  default:
    return false;
  }
}
} // namespace

bool PodunkDialogueHost::owner(const FieldNodeDescriptor &n, Owner &out,
                               std::string &e) const {
  const auto *r = recipe_ ? recipe_->record(n.id) : nullptr;
  if (!r || n.native_class != r->native_class ||
      n.script_sha != r->script_sha || n.script != r->script ||
      n.parent != r->parent || n.ready != r->ready ||
      n.class_index != r->class_index)
    return fail(e, "Dialogue owning host source recipe differs");
  unsigned count = 0;
  const auto *ui_node = ui_data_->node(n.id);
  if (ui_node && ui_node->kind != FieldDialogueUiKind::Pending) {
    out = Owner::Ui;
    ++count;
  }
  if (visual_data_->node(n.id)) {
    out = Owner::Visual;
    ++count;
  }
  if (audio_data_->node(n.id)) {
    if (n.native_class != "AudioStreamPlayer" || !n.script.empty())
      return fail(e, "Dialogue audio native/script binding differs");
    out = Owner::Audio;
    ++count;
  }
  if (const auto *t = timer_data_->record(recipe_->identity(), n.id)) {
    if (n.native_class != "Timer" || t->script_sha != n.script_sha ||
        !n.script.empty())
      return fail(e, "Dialogue Timer native/script binding differs");
    out = Owner::Timer;
    ++count;
  }
  if (count != 1)
    return fail(e, "Dialogue full native ownership missing or overlapping");
  e.clear();
  return true;
}
bool PodunkDialogueHost::admit_factory(std::string &e) const {
  if (!recipe_ || !life_ || !printer_ || !services_.root_script || !registry_ ||
      registry_->poisoned() ||
      !services_.root_script->admit(*life_, *recipe_, *printer_, e) ||
      !services_.admit_native_services(*recipe_, e))
    return fail(e, "Dialogue full native/script source owners not admitted");
  std::array<size_t, 4> counts{};
  for (const auto &n : recipe_->records()) {
    Owner o{};
    if (!owner(n, o, e))
      return false;
    ++counts[size_t(o)];
    if (!n.script.empty() && n.id != recipe_->identity().scene_id &&
        o != Owner::Visual)
      return fail(e, "Dialogue script has no checked actual source consumer");
  }
  const auto ui_count = size_t(std::count_if(
      ui_data_->nodes().begin(), ui_data_->nodes().end(),
      [](const auto &n) { return n.kind != FieldDialogueUiKind::Pending; }));
  if (counts[0] != ui_count || counts[1] != visual_data_->nodes().size() ||
      counts[3] != audio_data_->nodes().size() ||
      counts[0] + counts[1] + counts[2] + counts[3] !=
          life_->factory_node_count())
    return fail(e, "Dialogue independently checked native roster incomplete");
  e.clear();
  return true;
}
bool PodunkDialogueHost::initialize(
    const FieldDialogueLifecycleData &life,
    const FieldProgrammeData &programmes, const FieldNodeRecipeData &recipe,
    const FieldDialogueUiData &ui, const FieldDialogueVisualData &visual,
    const FieldDialogueAudioData &audio, const FieldNativeTimerData &timer,
    FieldGlobalRegistry &registry, FieldNativeTimers &timers,
    FieldNpcRuntime &npcs, HouseView house, HousePresentation &printer,
    SourceRandom &random, FieldDialogueAudioPlayer &audio_player,
    PodunkDialogueServices services, std::string &e) {
  if (initialized_ || recipe_ || !life.valid() || !programmes.valid() ||
      !recipe.valid() || !ui.valid() || !visual.valid() || !audio.valid() ||
      !timer.valid() || !same(ui.identity(), recipe.identity()) ||
      !same(visual.identity(), recipe.identity()) ||
      !same(audio.identity(), recipe.identity()) ||
      ui.recipe_ir_sha() != recipe.ir_sha256() ||
      visual.recipe_sha() != recipe.ir_sha256() ||
      audio.recipe_sha() != recipe.ir_sha256() ||
      life.factory_ir_sha() != recipe.ir_sha256() ||
      !services.factory_services || !services.admit_native_services ||
      !services.root_script || !services.native_notification ||
      !services.native_deferred || !services.emit || !services.frame ||
      !services.menu_sound || !services.input_sound ||
      !services.lifecycle.admit_factory || !services.lifecycle.admit_parent ||
      !services.lifecycle.connect_ready || !services.lifecycle.native ||
      !ui.verify_house(house, e))
    return fail(e, "Dialogue owning host checked data/live endpoints absent");
  auto tree = registry.tree_owner(registry.stable_canvas());
  if (!tree || tree->object_domain() != registry.kernel())
    return fail(e, "Dialogue actual persistent canvas shared Tree absent");
  auto backend = audio_player.backend();
  if (!backend.bank || !audio.verify_bank(*backend.bank, e))
    return fail(e, "Dialogue actual NDSP bank not admitted");
  life_ = &life;
  programmes_ = &programmes;
  recipe_ = &recipe;
  ui_data_ = &ui;
  visual_data_ = &visual;
  audio_data_ = &audio;
  timer_data_ = &timer;
  registry_ = &registry;
  timers_ = &timers;
  npcs_ = &npcs;
  house_ = house;
  printer_ = &printer;
  random_ = &random;
  audio_player_ = &audio_player;
  services_ = std::move(services);
  lifecycle_tree_ = std::move(tree);
  if (!admit_factory(e))
    return false;
  auto host = services_.lifecycle;
  host.admit_factory = [this](const FieldDialogueLifecycleData &d,
                              std::string &error) {
    return &d == life_ && admit_factory(error) &&
           services_.lifecycle.admit_factory(d, error);
  };
  host.admit_parent = [this](FieldObjectId parent, std::string &error) {
    if (parent != registry_->stable_canvas() ||
        registry_->tree_owner(parent) != lifecycle_tree_)
      return fail(
          error,
          "Dialogue source parent changed Tree; lifecycle rebind required");
    return services_.lifecycle.admit_parent(parent, error);
  };
  host.connect_ready =
      [this](FieldObjectId root, uint32_t generation, std::string_view signal,
             std::function<bool()> callback, std::string &error) {
        if (!attach(root, error))
          return false;
        return services_.lifecycle.connect_ready(root, generation, signal,
                                                 std::move(callback), error);
      };
  host.native = [this](const FieldDialogueStep &step, FieldObjectId id,
                       std::string &error) {
    return native_step(step, id, error);
  };
  host.play_animation = [this](FieldObjectId id, const FieldDialogueClip &clip,
                               std::string &error) {
    return play_animation(id, clip, error);
  };
  if (!lifecycle_.initialize(life, programmes, recipe, *lifecycle_tree_, npcs,
                             std::move(host), e))
    return false;
  initialized_ = true;
  e.clear();
  return true;
}
PodunkDialogueHost::Factory *PodunkDialogueHost::factory(FieldObjectId id) {
  auto o = owners_.find(id);
  if (o == owners_.end())
    return nullptr;
  auto f = factories_.find(o->second);
  return f == factories_.end() ? nullptr : f->second.get();
}
const PodunkDialogueHost::Factory *
PodunkDialogueHost::factory(FieldObjectId id) const {
  auto o = owners_.find(id);
  if (o == owners_.end())
    return nullptr;
  auto f = factories_.find(o->second);
  return f == factories_.end() ? nullptr : f->second.get();
}
bool PodunkDialogueHost::attach(FieldObjectId root, std::string &e) {
  if (!initialized_ || factories_.count(root) || factories_.size() >= 64)
    return fail(e, "Dialogue duplicate/bounded factory owner rejected");
  auto tree = lifecycle_tree_;
  const auto *state = tree ? tree->state(root) : nullptr;
  if (!state || !state->alive || state->inside || state->ready_notified ||
      state->source != recipe_->identity().scene_id)
    return fail(e, "Dialogue full detached source factory required");
  auto f = std::make_unique<Factory>();
  f->root = root;
  f->tree = tree;
  std::vector<FieldObjectId> pending{root};
  while (!pending.empty()) {
    auto id = pending.back();
    pending.pop_back();
    const auto *s = tree->state(id);
    const auto *d = tree->descriptor(id);
    FieldIdentity actual;
    Owner o{};
    if (!s || !s->alive || s->inside || !d ||
        !tree->object_identity(id, actual) ||
        !same(actual, recipe_->identity()) || !owner(*d, o, e) ||
        !f->objects.emplace(s->source, id).second || owners_.count(id))
      return fail(e, "Dialogue actual complete source factory differs");
    pending.insert(pending.end(), s->children.rbegin(), s->children.rend());
  }
  if (f->objects.size() != recipe_->records().size())
    return fail(e, "Dialogue actual factory omitted original descendants");
  PodunkDialogueFactoryServices native;
  if (!services_.factory_services(root, f->objects, native, e) ||
      !registry_->publish_branch(
          tree, root,
          [this](const FieldDeferredMessage &m, std::string &error) {
            return deferred(m, error);
          },
          e))
    return false;
  auto *owned = f.get();
  native.audio.backend = audio_player_->backend();
  // Actual shared Tree groups, not another audio/native processing loop.
  native.audio.internal_process = [tree](FieldObjectId id, bool enabled,
                                         std::string &error) {
    return enabled ? tree->add_group(id, "idle_process_internal", error)
                   : tree->remove_group(id, "idle_process_internal", error);
  };
  native.visual.timer_left = [this](FieldObjectId id, float &left,
                                    std::string &error) {
    if (!timers_->state(id))
      return fail(error, "Dialogue Cursor actual Timer missing");
    left = timers_->time_left(id);
    return true;
  };
  native.visual.timer_start = [this](FieldObjectId id, std::string &error) {
    return timers_->start(id, -1, error);
  };
  native.visual.menu = [this, owned](FieldObjectId id,
                                     FieldDialogueCursorMenu &menu,
                                     std::string &error) {
    const auto *s = owned->tree->state(id);
    const auto *n = s ? ui_data_->node(s->source) : nullptr;
    const auto *c = s ? ui_data_->control(s->source) : nullptr;
    FieldTransform world;
    if (!n || n->kind != FieldDialogueUiKind::GridContainer || !c ||
        !owned->ui.control(id) ||
        !owned->tree->world_transform(id, world, error))
      return fail(error, "Dialogue actual source Options Grid absent");
    menu = {};
    menu.object = id;
    menu.columns = c->columns;
    menu.position = world[2];
    for (auto child : s->children) {
      const auto *control = owned->ui.control(child);
      const auto *child_state = owned->tree->state(child);
      const auto *source =
          child_state ? ui_data_->node(child_state->source) : nullptr;
      if (!control || !source || source->kind != FieldDialogueUiKind::Label ||
          !owned->tree->world_transform(child, world, error))
        return fail(error, "Dialogue actual Options child owner differs");
      menu.items.push_back({child,
                            world[2],
                            {control->rect.z, control->rect.w},
                            control->visible,
                            true,
                            control->text});
    }
    error.clear();
    return true;
  };
  if (!f->ui.initialize(*ui_data_, *recipe_, house_, *tree,
                        std::move(native.ui), e) ||
      !f->visual.initialize(
          *visual_data_, *recipe_, *tree, std::move(native.visual), *random_,
          std::move(native.camera), std::move(native.arrows), e) ||
      !f->audio.initialize(*audio_data_, *recipe_, *tree, *random_,
                           std::move(native.audio), e) ||
      !f->ui.attach(root, {visual_data_, audio_data_, timer_data_}, e))
    return false;
  for (const auto &entry : f->objects) {
    if (!timer_data_->record(recipe_->identity(), entry.first))
      continue;
    FieldNodeBinding binding;
    if (!timers_->attach(*tree, entry.second, binding, e))
      return false;
    f->timer_attached.insert(entry.second);
  }
  // Publish owners before source connections, which can synchronously invoke
  // native layout/visibility/deferred callbacks into this same object owner.
  for (const auto &entry : f->objects)
    owners_.emplace(entry.second, root);
  factories_.emplace(root, std::move(f));
  if (!owned->visual.attach(root, e) || !owned->audio.attach(root, e))
    return false;
  e.clear();
  return true;
}
bool PodunkDialogueHost::bind(FieldObjectId id, const FieldNodeDescriptor &n,
                              FieldNodeBinding &out, std::string &e) {
  auto *f = factory(id);
  Owner o{};
  if (!initialized_ || !f || registry_->tree_owner(id) != f->tree ||
      !f->tree->state(id) || f->tree->state(id)->source != n.id ||
      !owner(n, o, e))
    return fail(e, "Dialogue bind actual source/object owner rejected");
  if (id == f->root) {
    if (!services_.root_script->construct(id, n, *printer_, e) ||
        !script_state(*f, false, e))
      return false;
  }
  const uint32_t families[] = {0x454e0041, 0x454e0046, 0x454e0044, 0x454e0049};
  out = {recipe_->identity(), n.id, n.class_index,
         families[size_t(o)], 1,    n.script_sha,
         n.native_class};
  e.clear();
  return true;
}
bool PodunkDialogueHost::references(const Factory &f,
                                    std::array<FieldObjectId, 12> &out,
                                    std::string &e) const {
  for (uint32_t role = 1; role <= out.size(); ++role) {
    const auto *r = life_->reference(role);
    auto i = r ? f.objects.find(r->id) : f.objects.end();
    const auto *s = i == f.objects.end() ? nullptr : f.tree->state(i->second);
    const auto *d = s ? f.tree->descriptor(s->object) : nullptr;
    if (!s || !s->alive || !d || d->native_class != r->native_class ||
        d->ready != r->ready)
      return fail(e, "Dialogue actual onready source reference absent");
    out[role - 1] = s->object;
  }
  return true;
}
bool PodunkDialogueHost::script_state(const Factory &f, bool ready,
                                      std::string &e) const {
  PodunkDialogueScriptState observed;
  std::array<FieldObjectId, 12> refs{};
  if (!services_.root_script->state(f.root, observed, e) ||
      !observed.constructed || observed.object != f.root ||
      observed.printer != printer_ ||
      !same(observed.identity, recipe_->identity()) ||
      (ready && (!observed.entered || !observed.ready ||
                 !references(f, refs, e) || observed.references != refs)))
    return fail(e,
                "Dialogue actual original script/printer/onready owner absent");
  e.clear();
  return true;
}
bool PodunkDialogueHost::full_ready(const Factory &f, std::string &e) const {
  for (const auto &n : recipe_->records()) {
    auto i = f.objects.find(n.id);
    const auto *s = i == f.objects.end() ? nullptr : f.tree->state(i->second);
    if (!s || !s->alive || !s->inside || !s->ready_notified ||
        !f.entered.count(s->object) || !f.native_ready.count(s->object) ||
        (!n.script.empty() && !f.script_ready.count(s->object)))
      return fail(e, "Dialogue full actual native/script Ready incomplete");
  }
  return script_state(f, true, e);
}
bool PodunkDialogueHost::process(Factory &f, FieldObjectId id, Owner o,
                                 FieldTreePhase phase, std::string &e) {
  const auto *d = f.tree->descriptor(id);
  PodunkDialogueFrame frame;
  if (!d || !services_.frame(id, frame, e) || frame.phase != phase ||
      !std::isfinite(frame.delta) || frame.delta < 0 || frame.delta > 1e6)
    return fail(e,
                "Dialogue actual shared Tree frame/input observation absent");
  if (!f.tree->can_process(id, frame.tree_paused))
    return fail(e,
                "Dialogue process dispatched outside source pause ownership");
  if (id == f.root) {
    std::array<FieldObjectId, 12> refs{};
    return script_state(f, true, e) && references(f, refs, e) &&
           services_.root_script->phase(id, phase, refs, e) &&
           f.ui.sync_text(f.root, *printer_, e);
  }
  if (o == Owner::Timer)
    return timers_->process(id, phase, frame.delta, frame.tree_paused, e);
  if (o == Owner::Audio && phase == FieldTreePhase::IdleInternal)
    return f.audio.tree_pause(id, frame.tree_paused,
                              f.tree->can_process(id, frame.tree_paused), e) &&
           f.audio.internal_process(id, e);
  if (o == Owner::Ui && phase == FieldTreePhase::IdleInternal)
    return f.ui.animation_process(id, frame.delta, e);
  if (o != Owner::Visual)
    return fail(e, "Dialogue source body has no requested process method");
  if (visual_data_->cursor(d->id)) {
    if (phase == FieldTreePhase::Physics)
      return f.visual.physics_cursor(id, e);
    if (phase == FieldTreePhase::Input)
      return f.visual.input_cursor(id, frame.action, frame.pressed, e);
    if (phase == FieldTreePhase::IdleInternal)
      return f.visual.idle_cursor(id, frame.delta, e);
  }
  for (const auto &cursor : visual_data_->cursors())
    if (cursor.player == d->id && phase == FieldTreePhase::IdleInternal)
      return f.visual.idle_cursor(id, frame.delta, e);
  if (visual_data_->camera().record(d->id)) {
    bool ok = phase == FieldTreePhase::Physics
                  ? f.visual.camera().physics(d->id, frame.delta)
              : phase == FieldTreePhase::Idle
                  ? f.visual.camera().idle(d->id, frame.delta)
              : phase == FieldTreePhase::Input ? f.visual.camera().input(d->id)
                                               : false;
    return ok || fail(e, f.visual.camera().error().c_str());
  }
  if (phase == FieldTreePhase::IdleInternal) {
    for (const auto &camera : visual_data_->camera().records())
      if (camera.animation_id == d->id)
        return f.visual.camera().animation_idle(camera.id, frame.delta) ||
               fail(e, f.visual.camera().error().c_str());
    if (visual_data_->arrows().sprite(d->id) ||
        visual_data_->arrows().player(d->id))
      return f.visual.arrows().idle_leaf(d->id, frame.delta) ||
             fail(e, f.visual.arrows().error().c_str());
  }
  return fail(e, "Dialogue visual actual source process method unsupported");
}
bool PodunkDialogueHost::dispatch(FieldObjectId id, const FieldNodeBinding &b,
                                  FieldTreePhase phase, std::string &e) {
  auto *f = factory(id);
  const auto *d = f ? f->tree->descriptor(id) : nullptr;
  Owner o{};
  const uint32_t families[] = {0x454e0041, 0x454e0046, 0x454e0044, 0x454e0049};
  if (!f || !d || !same(b.identity, recipe_->identity()) ||
      b.stable_id != d->id || b.native_class != d->native_class ||
      b.script_sha != d->script_sha || b.class_index != d->class_index ||
      !owner(*d, o, e) || b.family != families[size_t(o)] ||
      b.capability != 1 || registry_->tree_owner(id) != f->tree)
    return fail(e, "Dialogue phase source/ObjectDB ownership rejected");
  switch (phase) {
  case FieldTreePhase::EnterNative: {
    if (f->entered.count(id))
      return fail(e, "Dialogue duplicate actual native enter");
    bool ok = o == Owner::Ui       ? f->ui.enter_native(id, e)
              : o == Owner::Visual ? f->visual.enter_native(id, e)
              : o == Owner::Audio  ? f->audio.enter_native(id, e)
                                   : f->timer_attached.count(id) != 0;
    if (!ok)
      return false;
    f->entered.insert(id);
    return true;
  }
  case FieldTreePhase::ReadyNative: {
    if (!f->entered.count(id) || f->native_ready.count(id))
      return fail(e, "Dialogue actual native Ready lifetime rejected");
    bool ok = o == Owner::Ui       ? f->ui.ready_native(id, e)
              : o == Owner::Visual ? f->visual.ready_native(id, e)
              : o == Owner::Audio  ? f->audio.ready_native(id, e)
                                   : timers_->ready(id, e);
    if (!ok)
      return false;
    f->native_ready.insert(id);
    return true;
  }
  case FieldTreePhase::EnterScript:
  case FieldTreePhase::ReadyScript:
  case FieldTreePhase::ExitScript:
    // Empty script is a checked source fact for these particular records,
    // never a blanket approval of unknown classes/scripts or native Ready.
    if (d->script.empty()) {
      e.clear();
      return true;
    }
    if (id == f->root) {
      if (phase == FieldTreePhase::ReadyScript)
        for (const auto &entry : f->objects) {
          const auto *child = f->tree->state(entry.second);
          if (entry.second != f->root &&
              (!child || !child->inside || !child->ready_notified ||
               !f->native_ready.count(entry.second)))
            return fail(e, "Dialogue script onready precedes descendant Ready");
        }
      std::array<FieldObjectId, 12> refs{};
      if (!references(*f, refs, e) ||
          !services_.root_script->phase(id, phase, refs, e) ||
          !script_state(*f, phase == FieldTreePhase::ReadyScript, e))
        return false;
    } else if (o == Owner::Visual) {
      if (phase == FieldTreePhase::ReadyScript &&
          !f->visual.ready_script(id, e))
        return false;
      // The checked Cursor/Camera/Scope scripts have no _enter_tree or
      // _exit_tree method; their native server notifications still execute.
      if (phase != FieldTreePhase::ReadyScript &&
          !services_.native_notification(id, *d, phase, e))
        return false;
    } else
      return fail(e, "Dialogue unowned source script notification");
    if (phase == FieldTreePhase::ReadyScript)
      f->script_ready.insert(id);
    if (phase == FieldTreePhase::ExitScript)
      f->script_ready.erase(id);
    return true;
  case FieldTreePhase::ReadySignal:
    if (id == f->root && !full_ready(*f, e))
      return false;
    return services_.emit(id, life_->ready_signal(), std::monostate{}, e);
  case FieldTreePhase::ExitNative: {
    if (!f->entered.count(id))
      return fail(e, "Dialogue actual native exit without enter");
    bool ok = o == Owner::Ui       ? f->ui.exit_native(id, e)
              : o == Owner::Visual ? f->visual.exit_native(id, e)
              : o == Owner::Audio
                  ? f->audio.exit_native(id, e)
                  : services_.native_notification(id, *d, phase, e);
    if (!ok)
      return false;
    f->entered.erase(id);
    f->native_ready.erase(id);
    return true;
  }
  case FieldTreePhase::Deleting:
    return erase(*f, id, e);
  case FieldTreePhase::Idle:
  case FieldTreePhase::Physics:
  case FieldTreePhase::IdleInternal:
  case FieldTreePhase::PhysicsInternal:
  case FieldTreePhase::Input:
  case FieldTreePhase::UnhandledInput:
  case FieldTreePhase::UnhandledKeyInput:
    return process(*f, id, o, phase, e);
  default:
    if (!notification(phase))
      return fail(e, "Dialogue unknown native notification rejected");
    return services_.native_notification(id, *d, phase, e);
  }
}
bool PodunkDialogueHost::deferred(const FieldDeferredMessage &m,
                                  std::string &e) {
  auto *f = factory(m.object);
  const auto *d = f ? f->tree->descriptor(m.object) : nullptr;
  if (!d || !f->tree->state(m.object)->alive)
    return fail(e, "Dialogue deferred actual source receiver missing");
  const auto *n = ui_data_->node(d->id);
  if (m.kind == FieldDeferredKind::Call && m.member == "_sort_children" &&
      m.args.empty() && n &&
      (n->kind == FieldDialogueUiKind::GridContainer ||
       n->kind == FieldDialogueUiKind::HBoxContainer))
    return f->ui.sort_children(m.object, e);
  if (m.object == f->root && !d->script.empty())
    return services_.root_script->deferred(m, e);
  // Actual native receiver verifies the method/property/argument signature.
  // No generic script/property interpreter is installed here.
  return services_.native_deferred(m, e);
}
bool PodunkDialogueHost::erase(Factory &f, FieldObjectId id, std::string &e) {
  if (f.deleting.count(id))
    return fail(e, "Dialogue duplicate source object deletion");
  const auto *descriptor = f.tree->descriptor(id);
  if (!descriptor || !services_.native_notification(
                         id, *descriptor, FieldTreePhase::Deleting, e))
    return false;
  if (f.timer_attached.count(id) && !timers_->release(id, e))
    return false;
  f.deleting.insert(id);
  // The actual Tree deletes children before emitting its final Deleting
  // phase for the parent. Keep their complete owning instance until that
  // traversal has covered all original objects.
  if (f.deleting.size() != f.objects.size()) {
    e.clear();
    return true;
  }
  const auto root = f.root;
  if (!lifecycle_.deleted(root, e) || !f.audio.release(root, e) ||
      !f.ui.release(root, e) || !services_.root_script->release(root, e))
    return false;
  for (const auto &entry : f.objects)
    owners_.erase(entry.second);
  factories_.erase(root);
  e.clear();
  return true;
}
bool PodunkDialogueHost::open(const FieldProgrammeData &p, uint32_t programme,
                              const FieldProgrammeContext &ctx,
                              uint32_t generation, FieldObjectId &out,
                              std::string &e) {
  if (!initialized_ ||
      registry_->tree_owner(registry_->stable_canvas()) != lifecycle_tree_)
    return fail(e,
                "Dialogue actual current canvas lifecycle owner unavailable");
  return lifecycle_.open(p, programme, ctx, generation, out, e);
}
bool PodunkDialogueHost::admit(const DialogueAction &a,
                               const FieldProgrammeContext &ctx,
                               std::string &e) {
  return initialized_ ? lifecycle_.admit(a, ctx, e)
                      : fail(e, "Dialogue source host not initialized");
}
bool PodunkDialogueHost::apply(const DialogueAction &a,
                               const FieldProgrammeContext &ctx,
                               std::string &e) {
  return initialized_ ? lifecycle_.apply(a, ctx, e)
                      : fail(e, "Dialogue source host not initialized");
}
bool PodunkDialogueHost::admit_ready(FieldObjectId root, uint32_t generation,
                                     std::string &e) const {
  const auto *f = factory(root);
  return f && full_ready(*f, e) && lifecycle_.admit_ready(root, generation, e);
}
bool PodunkDialogueHost::presented_text(FieldObjectId root,
                                        const FieldProgrammeText &text,
                                        std::string &e) {
  auto *f = factory(root);
  const auto *source = programmes_ ? programmes_->text(text.id) : nullptr;
  const auto *voice = life_ ? life_->reference(9) : nullptr;
  if (!f || !source || source != &text || !voice || !full_ready(*f, e))
    return fail(e, "Dialogue presented phrase actual programme owner differs");
  return f->audio.phrase_sound(f->objects.at(voice->id), text.voice, e) &&
         f->ui.sync_text(root, *printer_, e);
}
bool PodunkDialogueHost::sync_choices(FieldObjectId root,
                                      const DialogueChoices &choices,
                                      const LocaleSelection *locale,
                                      std::string &e) {
  auto *f = factory(root);
  if (!f || !full_ready(*f, e) || !f->ui.sync_choices(root, choices, locale, e))
    return false;
  const auto *arrow = life_->reference(6);
  auto id = arrow ? f->objects.find(arrow->id) : f->objects.end();
  if (id == f->objects.end())
    return fail(e, "Dialogue source choice cursor missing");
  return f->visual.refresh_cursor(id->second, false, e);
}
bool PodunkDialogueHost::audio_event(FieldObjectId root,
                                     const HouseAudioEvent &event,
                                     std::string &e) {
  auto *f = factory(root);
  const auto *voice = life_ ? life_->reference(9) : nullptr;
  if (!f || !voice || !std::isfinite(event.pitch) || !full_ready(*f, e))
    return fail(e, "Dialogue actual printed audio event owner absent");
  auto id = f->objects.at(voice->id);
  switch (event.kind) {
  case HouseAudioKind::VoiceStart: {
    const auto *current = f->audio.state(id);
    const auto *asset =
        event.voice.empty()
            ? nullptr
            : audio_data_->asset(audio_data_->text_prefix() + event.voice +
                                 audio_data_->extension());
    if (!current || (!event.voice.empty() && !asset) ||
        current->stream != (asset ? asset->id : 0) ||
        event.pitch < audio_data_->pitch_range()[0] ||
        event.pitch > audio_data_->pitch_range()[1])
      return fail(e, "Dialogue printed voice/source pitch receipt differs");
    // HousePresentation has already drawn this exact source pitch. Calling
    // character_sound() here would consume the shared random stream twice.
    return f->audio.set_pitch(id, float(event.pitch), e) &&
           f->audio.play(id, 0, e);
  }
  case HouseAudioKind::VoiceStop:
    return f->audio.stop(id, e);
  case HouseAudioKind::Confirm: {
    FieldObjectId input = 0;
    if (!services_.input_sound(root, input, *audio_data_, e) || input == id ||
        !f->audio.state(input) ||
        !f->objects.count(f->audio.state(input)->source))
      return fail(e, "Dialogue actual source InputSound binding absent");
    return f->audio.play(input, 0, e);
  }
  case HouseAudioKind::MenuOpen:
  case HouseAudioKind::MenuClose:
    return services_.menu_sound(root, event, e);
  }
  return fail(e, "Dialogue unknown printer audio event rejected");
}
bool PodunkDialogueHost::native_step(const FieldDialogueStep &step,
                                     FieldObjectId id, std::string &e) {
  auto *f = factory(id);
  if (!f)
    return fail(e, "Dialogue source native method receiver absent");
  switch (step.op) {
  case FieldDialogueOp::TextClear:
  case FieldDialogueOp::BulletClear:
  case FieldDialogueOp::VisibleCharacters:
  case FieldDialogueOp::TextHide:
    return f->ui.native_step(step, id, e);
  case FieldDialogueOp::ArrowHide:
    if (!f->visual.cursor_state(id))
      return fail(e, "Dialogue source Arrow hide typed receiver rejected");
    return f->tree->set_visible(id, false, e);
  case FieldDialogueOp::VoiceVolume:
    return std::isfinite(step.value) &&
           f->audio.set_volume(id, float(step.value), e);
  default:
    return services_.lifecycle.native(step, id, e);
  }
}
bool PodunkDialogueHost::play_animation(FieldObjectId id,
                                        const FieldDialogueClip &clip,
                                        std::string &e) {
  auto *f = factory(id);
  const auto *d = f ? f->tree->descriptor(id) : nullptr;
  if (!d)
    return fail(e, "Dialogue actual AnimationPlayer receiver absent");
  const FieldDialogueUiAnimation *source = nullptr;
  for (const auto &a : ui_data_->animations())
    if (a.owner == d->id && a.clip.name == clip.name)
      source = &a;
  if (!source || source->clip.path != clip.path ||
      source->clip.length != clip.length ||
      source->clip.resource != clip.resource)
    return fail(e, "Dialogue lifecycle animation checked source differs");
  return f->ui.play(id, clip.name, e);
}
FieldDialogueUiRuntime *PodunkDialogueHost::ui(FieldObjectId root) {
  auto *f = factory(root);
  return f && f->root == root ? &f->ui : nullptr;
}
FieldDialogueVisualRuntime *PodunkDialogueHost::visual(FieldObjectId root) {
  auto *f = factory(root);
  return f && f->root == root ? &f->visual : nullptr;
}
FieldDialogueAudioRuntime *PodunkDialogueHost::audio(FieldObjectId root) {
  auto *f = factory(root);
  return f && f->root == root ? &f->audio : nullptr;
}
} // namespace encore::ctr

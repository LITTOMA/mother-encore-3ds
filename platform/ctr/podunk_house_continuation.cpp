#include "podunk_house_continuation.hpp"
#include "encore/global_load.hpp"
#include "encore/player_ready.hpp"
#include <algorithm>
#include <cstdio>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *text) {
  e = text;
  return false;
}
bool same_spec(const FieldGlobalExternalSpec &a,
               const FieldGlobalExternalSpec &b) {
  return a.identity.scene_id == b.identity.scene_id &&
         a.identity.upstream_commit == b.identity.upstream_commit &&
         a.identity.source_sha256 == b.identity.source_sha256 &&
         a.stable_id == b.stable_id && a.role == b.role && a.name == b.name &&
         a.native_class == b.native_class && a.source == b.source &&
         a.script == b.script && a.source_sha == b.source_sha &&
         a.script_sha == b.script_sha;
}
} // namespace
struct PodunkHouseContinuation::State {
  PodunkHouseContinuationInput input;
  FieldGlobalRegistryData registry_data;
  FieldNativeRootData root_data;
  FieldGlobalDataData members;
  GlobalYamlCachesData caches;
  GlobalPackedDirectoryData directories;
  GlobalYamlFileData files;
  GlobalDataConstructorData constructor;
  FieldGlobalFlagsData flags;
  FieldItemDefinitions all_items, field_items;
  FieldCharacterLoadData characters;
  GlobalLoadData cold_load;
  std::shared_ptr<FieldGlobalConstructorData> global_data =
      std::make_shared<FieldGlobalConstructorData>();
  std::shared_ptr<GlobalChildReadyData> child_data =
      std::make_shared<GlobalChildReadyData>();
  FieldUiManagerData ui_data;
  std::shared_ptr<HouseUiContinuationData> continuation_ui =
      std::make_shared<HouseUiContinuationData>();
  HouseGlobalBridgeData bridge_data;
  FieldInventoryData inventory;
  std::shared_ptr<PlayerInitializationData> player_initialization =
      std::make_shared<PlayerInitializationData>();
  std::shared_ptr<PlayerReadyData> player_ready =
      std::make_shared<PlayerReadyData>();
  HouseStatusEffectsData status_effects;
  PodunkNativeRoot root;
  PodunkGlobalNative native;
  PodunkGlobalHost global;
  PodunkGlobalDataFactory data_factory;
  FieldGlobalRegistry registry;
  PodunkHouseGlobalBridge bridge;
  HouseStatusEffectsRuntime status_runtime;
  HouseUiContinuation *ui = nullptr;
  FieldObjectSignals::DeclarationQuery scene_signals;
  bool complete = false;

  bool original_spec(const FieldGlobalAutoload &a, FieldGlobalExternalSpec &out,
                     std::string &e) const {
    auto it = std::find_if(registry_data.autoloads().begin(),
                           registry_data.autoloads().end(),
                           [&](const auto &v) { return v.id == a.id; });
    if (it == registry_data.autoloads().end() || it->name != a.name ||
        it->path != a.path || it->native_class != a.native_class ||
        it->script != a.script || it->source_sha != a.source_sha ||
        it->script_sha != a.script_sha)
      return fail(e, "Continuation original autoload source roster differs");
    out = {registry_data.identity(),
           a.id,
           3,
           a.name,
           a.native_class,
           a.path,
           a.script,
           a.source_sha,
           a.script_sha};
    return true;
  }
  template <typename Loader>
  bool read(PodunkPackRole role, Loader load, std::string &e) {
    const auto &bundle = input.destination->bundle();
    const auto *record = bundle.entry(role);
    if (!record)
      return fail(e, "Continuation bundle mandatory typed role absent");
    std::vector<uint8_t> bytes;
    if (!bundle.read(role, input.romfs_root, bytes, e) ||
        !load(bytes.data(), bytes.size(), record->identity, e)) {
      e = record->path + ": " + e;
      return false;
    }
    return true;
  }
  bool load(std::string &e) {
    if (!read(
            PodunkPackRole::GlobalRegistry,
            [&](auto p, auto n, const auto &id, auto &err) {
              return registry_data.load(p, n, id, err);
            },
            e) ||
        !read(
            PodunkPackRole::NativeRoot,
            [&](auto p, auto n, const auto &, auto &err) {
              return root_data.load(p, n, registry_data, err);
            },
            e) ||
        !read(
            PodunkPackRole::GlobalDataMembers,
            [&](auto p, auto n, const auto &id, auto &err) {
              return members.load(p, n, id, err);
            },
            e))
      return false;
    auto actual = std::find_if(
        registry_data.autoloads().begin(), registry_data.autoloads().end(),
        [&](const auto &a) { return a.script == members.owner_source(); });
    if (actual == registry_data.autoloads().end())
      return fail(e, "Continuation globalData source roster entry absent");
    FieldGlobalExternalSpec spec;
    if (!original_spec(*actual, spec, e))
      return false;
    if (!read(
            PodunkPackRole::YamlCaches,
            [&](auto p, auto n, const auto &, auto &err) {
              return caches.load(p, n, spec, err);
            },
            e) ||
        !read(
            PodunkPackRole::GlobalDataConstructor,
            [&](auto p, auto n, const auto &, auto &err) {
              return constructor.load(p, n, members, caches, err);
            },
            e) ||
        !read(
            PodunkPackRole::PackedDirectory,
            [&](auto p, auto n, const auto &, auto &err) {
              return directories.load(p, n, caches, registry_data, err);
            },
            e) ||
        !read(
            PodunkPackRole::YamlFile,
            [&](auto p, auto n, const auto &, auto &err) {
              return files.load(p, n, caches, err);
            },
            e) ||
        !read(
            PodunkPackRole::GlobalFlags,
            [&](auto p, auto n, const auto &, auto &err) {
              return flags.load(p, n, err);
            },
            e) ||
        !read(
            PodunkPackRole::GlobalItemDefinitions,
            [&](auto p, auto n, const auto &, auto &err) {
              return all_items.load(p, n, err);
            },
            e) ||
        !read(
            PodunkPackRole::FieldItemDefinitions,
            [&](auto p, auto n, const auto &, auto &err) {
              return field_items.load(p, n, err);
            },
            e) ||
        !read(
            PodunkPackRole::CharacterLoad,
            [&](auto p, auto n, const auto &id, auto &err) {
              return characters.load(p, n, id, err);
            },
            e) ||
        !read(
            PodunkPackRole::GlobalLoad,
            [&](auto p, auto n, const auto &, auto &err) {
              return cold_load.load(p, n, constructor, characters, flags,
                                    all_items, err);
            },
            e) ||
        !read(
            PodunkPackRole::GlobalNodeConstructor,
            [&](auto p, auto n, const auto &id, auto &err) {
              return global_data->load(p, n, id, err);
            },
            e) ||
        !read(
            PodunkPackRole::GlobalChildReady,
            [&](auto p, auto n, const auto &, auto &err) {
              return child_data->load(p, n, *global_data, err);
            },
            e) ||
        !read(
            PodunkPackRole::UiManager,
            [&](auto p, auto n, const auto &id, auto &err) {
              return ui_data.load(p, n, id, err);
            },
            e) ||
        !read(
            PodunkPackRole::UiContinuation,
            [&](auto p, auto n, const auto &, auto &err) {
              return continuation_ui->load(p, n, ui_data, err);
            },
            e) ||
        !read(
            PodunkPackRole::HouseGlobalBridge,
            [&](auto p, auto n, const auto &, auto &err) {
              return bridge_data.load(p, n, characters, cold_load,
                                      *input.session, err);
            },
            e) ||
        !read(
            PodunkPackRole::Inventory,
            [&](auto p, auto n, const auto &, auto &err) {
              return inventory.load(p, n, err);
            },
            e) ||
        !inventory.bind_definitions(field_items, e) ||
        !read(
            PodunkPackRole::PlayerInitialization,
            [&](auto p, auto n, const auto &, auto &err) {
              return player_initialization->load(p, n, constructor, err);
            },
            e) ||
        !read(
            PodunkPackRole::PlayerReady,
            [&](auto p, auto n, const auto &, auto &err) {
              return player_ready->load(p, n, *player_initialization, err);
            },
            e) ||
        !read(
            PodunkPackRole::HouseStatusEffects,
            [&](auto p, auto n, const auto &, auto &err) {
              return status_effects.load(p, n, bridge_data, *player_ready, err);
            },
            e))
      return false;
    if (!bridge_data.constructor_continuation())
      return fail(e, "Continuation bundle lacks existing-session capability");
    return true;
  }
};

PodunkHouseContinuation::PodunkHouseContinuation() = default;
PodunkHouseContinuation::~PodunkHouseContinuation() = default;

bool PodunkHouseContinuation::construct(
    FieldObjectId id, const FieldGlobalExternalSpec &spec,
    std::unique_ptr<FieldGlobalExternalObject> &out, std::string &e) {
  if (!state_ || spec.role != 3)
    return fail(e, "Continuation actual autoload factory scope rejected");
  auto &s = *state_;
  const auto &rows = s.bridge_data.continuation_autoloads();
  auto row = std::find_if(rows.begin(), rows.end(), [&](const auto &a) {
    return a.source.id == spec.stable_id;
  });
  FieldGlobalExternalSpec expected;
  if (row == rows.end() || !s.original_spec(row->source, expected, e) ||
      !same_spec(spec, expected))
    return fail(e, "Continuation factory differs from checked source autoload");
  if (row->role == 1)
    return s.data_factory.construct(id, spec, out, e);
  if (row->role == 2)
    return s.global.construct(id, spec, out, e);
  if (row->role != 3 || s.ui)
    return fail(e, "Continuation factory unknown or duplicated source role");
  auto ui = std::make_unique<HouseUiContinuation>();
  HouseUiContinuationSources models{
      s.input.battle,        s.input.outcome,
      s.input.commands,      &s.input.house->world,
      &s.input.house->house, &s.input.house->presentation};
  if (!ui->initialize(s.continuation_ui, s.registry_data, s.registry, s.root,
                      *s.input.signals, {id, spec, 0x454e0064, 1}, models, e))
    return false;
  s.ui = ui.get();
  out = std::move(ui);
  return true;
}

bool PodunkHouseContinuation::initialize(PodunkHouseContinuationInput input,
                                         std::string &e) {
  if (attempted_ || !input.destination || !input.destination->valid() ||
      input.romfs_root.empty() || !input.house || !input.battle ||
      !input.outcome || !input.commands || !input.session || !input.snapshot ||
      !input.played_random || !input.uid_ledger || !input.target ||
      !input.audio || !input.signals || input.signals->registry() ||
      !input.clock || !input.locale)
    return fail(
        e,
        "House continuation requires actual live owners and unused signal bus");
  attempted_ = true;
  state_ = std::make_unique<State>();
  auto &s = *state_;
  s.input = std::move(input);
  if (!s.load(e))
    return false;
  if (!s.root.initialize(s.root_data, s.registry_data, s.registry,
                         s.input.target, this, e))
    return false;
  FieldGlobalRegistryHost host;
  host.construct = [&s](auto id, const auto &spec, auto &out, auto &error) {
    return s.root.construct(id, spec, out, error);
  };
  if (!s.registry.initialize(s.registry_data, std::move(host), e) ||
      !s.input.signals->initialize(
          s.registry,
          [this](auto id, auto name, auto &arity, auto &error) {
            return signal_declaration(id, name, arity, error);
          },
          e) ||
      !s.root.bind_object_signals(*s.input.signals, e) ||
      !s.root.initialize_tree(e) ||
      !s.native.initialize(*s.global_data, s.registry, s.root, e) ||
      !s.global.initialize(s.registry_data, s.registry, s.global_data,
                           s.child_data, s.root, s.native, nullptr, e))
    return false;
  PodunkGlobalDataSingletonData resources{&s.members,  &s.constructor, &s.flags,
                                          &s.caches,   &s.directories, &s.files,
                                          &s.all_items};
  PodunkGlobalDataSingletonServices services;
  services.registry = &s.registry;
  services.root = &s.root;
  services.random = s.input.played_random;
  services.uid_ledger = s.input.uid_ledger;
  services.clock = s.input.clock;
  services.flags_updated =
      s.input.flags_updated
          ? s.input.flags_updated
          : FieldGlobalFlagsRuntime::Emit{[](auto &error) {
              return fail(error,
                          "Actual source flags_updated consumer not bound");
            }};
  services.warning = [](const auto &message, auto &error) {
    if (std::fprintf(stderr, "%s\n", message.c_str()) < 0)
      return fail(error, "Source cache warning output failed");
    error.clear();
    return true;
  };
  services.locale = s.input.locale;
  services.signal = [&s](auto id, auto name, auto &error) {
    return s.input.signals->emit(id, name, {}, error);
  };
  if (!s.data_factory.initialize(s.registry_data, resources, services, e))
    return false;
  for (const auto &row : s.bridge_data.continuation_autoloads()) {
    if (!s.bridge.construct_continuation_autoload(
            row.source.id, s.bridge_data, *s.input.session, s.input.room,
            s.input.house_data, s.input.round, s.input.legacy_items,
            *s.input.snapshot, s.registry, *s.input.played_random,
            *s.input.uid_ledger, e))
      return false;
    if (row.role == 1 && !s.data_factory.register_native_root(e))
      return false;
    if (row.role == 3 &&
        (!s.ui || !s.root.register_external_child(*s.ui, *s.ui, e)))
      return false;
  }
  auto *characters = s.data_factory.globaldata();
  if (!characters || !s.ui)
    return fail(e, "Continuation actual source owners were not published");
  if (!s.bridge.prepare_continuation(s.bridge_data, characters->host(),
                                     s.global.core(), s.registry, e) ||
      !s.bridge.adopt(
          s.bridge_data, *s.input.session, s.input.room, s.input.house_data,
          s.input.round, s.input.legacy_items, *s.input.snapshot, s.inventory,
          s.field_items, s.all_items, characters->host(), s.global.core(),
          s.registry, *s.input.played_random, *s.input.uid_ledger, e) ||
      !characters->bind_continuation(s.bridge.core(), s.global.core(), e) ||
      !s.global.bind_continuation(s.bridge.core(), characters->host().runtime(),
                                  e))
    return false;
  for (const auto &row : s.bridge_data.continuation_autoloads()) {
    const FieldObjectId id = s.registry.autoload_object(row.source.id);
    if (!id || !s.root.add_continuation_child(id, e))
      return false;
    FieldGlobalExternalState actual;
    const auto *object = s.registry.external_object(id);
    if (!object || !object->state(actual, e) || !actual.inside ||
        actual.ready || actual.parent != s.registry.root())
      return fail(e, "Continuation native attachment did not preserve source "
                     "Ready boundary");
  }
  if (!s.bridge.core().binds_source_owners(characters->host().runtime(),
                                           s.global.core(), s.registry))
    return fail(e, "Continuation imported owning references are not live");
  if (!s.status_runtime.initialize(
          s.status_effects, characters->host().runtime(), s.registry,
          [&s](auto character, auto status, auto &out, auto &error) {
            return s.bridge.status_data(character, status, out, error);
          },
          e))
    return false;
  s.complete = true;
  e.clear();
  return true;
}

bool PodunkHouseContinuation::signal_declaration(FieldObjectId id,
                                                 std::string_view name,
                                                 uint32_t &arity,
                                                 std::string &e) const {
  if (!state_ || !state_->registry.object_exists(id))
    return fail(e, "Continuation signal actual ObjectDB owner absent");
  const auto &s = *state_;
  if (id == s.registry.kernel() || id == s.registry.root())
    return s.root.signal_declaration(id, name, arity, e);
  const auto *object = s.registry.external_object(id);
  if (object) {
    const auto binding = object->binding();
    const auto &rows = s.bridge_data.continuation_autoloads();
    auto row = std::find_if(rows.begin(), rows.end(), [&](const auto &a) {
      return a.source.id == binding.source.stable_id;
    });
    if (row != rows.end()) {
      FieldGlobalExternalSpec expected;
      if (!s.original_spec(row->source, expected, e) ||
          !same_spec(binding.source, expected))
        return false;
      // Native Node declarations: engine schema, independent of game content.
      if (name == "tree_entered" || name == "tree_exiting" ||
          name == "tree_exited" || name == "ready") {
        arity = 0;
        e.clear();
        return true;
      }
      if (name == "child_entered_tree" || name == "child_exiting_tree") {
        arity = 1;
        e.clear();
        return true;
      }
      if (row->role == 2 && std::find(s.global_data->signals().begin(),
                                      s.global_data->signals().end(),
                                      name) != s.global_data->signals().end()) {
        arity = 0;
        e.clear();
        return true;
      }
      if (row->role == 3)
        for (const auto &signal : s.continuation_ui->signals())
          if (signal.name == name) {
            arity = signal.arity;
            e.clear();
            return true;
          }
    }
  }
  if (s.scene_signals)
    return s.scene_signals(id, name, arity, e);
  return fail(e, "Continuation signal requires another actual source owner");
}
bool PodunkHouseContinuation::bind_scene_signal_declarations(
    FieldObjectSignals::DeclarationQuery query, std::string &e) {
  if (!initialized() || !query || state_->scene_signals)
    return fail(e, "Continuation actual scene signal owner cannot be replaced");
  state_->scene_signals = std::move(query);
  e.clear();
  return true;
}
bool PodunkHouseContinuation::initialized() const {
  return state_ && state_->complete && !state_->registry.poisoned();
}
FieldGlobalRegistry *PodunkHouseContinuation::registry() {
  return initialized() ? &state_->registry : nullptr;
}
PodunkNativeRoot *PodunkHouseContinuation::native_root() {
  return initialized() ? &state_->root : nullptr;
}
PodunkGlobalHost *PodunkHouseContinuation::global() {
  return initialized() ? &state_->global : nullptr;
}
PodunkGlobalDataHost *PodunkHouseContinuation::characters() {
  auto *v = initialized() ? state_->data_factory.globaldata() : nullptr;
  return v ? &v->host() : nullptr;
}
HouseUiContinuation *PodunkHouseContinuation::ui() {
  return initialized() ? state_->ui : nullptr;
}
PodunkHouseGlobalBridge *PodunkHouseContinuation::bridge() {
  return initialized() ? &state_->bridge : nullptr;
}
const FieldInventoryData *PodunkHouseContinuation::inventory_data() const {
  return initialized() ? &state_->inventory : nullptr;
}
const FieldItemDefinitions *PodunkHouseContinuation::item_definitions() const {
  return initialized() ? &state_->field_items : nullptr;
}
const FieldItemDefinitions *
PodunkHouseContinuation::global_item_definitions() const {
  return initialized() ? &state_->all_items : nullptr;
}
const PlayerInitializationData *
PodunkHouseContinuation::player_initialization() const {
  return initialized() ? state_->player_initialization.get() : nullptr;
}
const PlayerReadyData *PodunkHouseContinuation::player_ready() const {
  return initialized() ? state_->player_ready.get() : nullptr;
}
const FieldCharacterLoadData *PodunkHouseContinuation::character_data() const {
  return initialized() ? &state_->characters : nullptr;
}
std::shared_ptr<const PlayerInitializationData>
PodunkHouseContinuation::player_initialization_owner() const {
  return initialized() ? state_->player_initialization : nullptr;
}
std::shared_ptr<const PlayerReadyData>
PodunkHouseContinuation::player_ready_owner() const {
  return initialized() ? state_->player_ready : nullptr;
}
const HouseStatusEffectsData *PodunkHouseContinuation::status_effects() const {
  return initialized() ? &state_->status_effects : nullptr;
}
const HouseStatusEffectsRuntime *
PodunkHouseContinuation::status_runtime() const {
  return initialized() ? &state_->status_runtime : nullptr;
}
AudioPlayer *PodunkHouseContinuation::audio() const {
  return initialized() ? state_->input.audio : nullptr;
}
SourceRandom *PodunkHouseContinuation::random() const {
  return initialized() ? state_->input.played_random : nullptr;
}
std::vector<uint32_t> *PodunkHouseContinuation::uid_ledger() const {
  return initialized() ? state_->input.uid_ledger : nullptr;
}
FieldObjectSignals *PodunkHouseContinuation::signals() const {
  return initialized() ? state_->input.signals : nullptr;
}
bool PodunkHouseContinuation::inventory_snapshot(PodunkInventorySnapshot &out,
                                                 std::string &e) const {
  if (!initialized())
    return fail(e, "Continuation actual inventory not initialized");
  return state_->bridge.snapshot(state_->inventory, out, e);
}
} // namespace encore::ctr

#include "podunk_global_ready.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fstream>
namespace encore::ctr {
using namespace upstream;
namespace {
inline uint32_t rotate(uint32_t v, unsigned n) {
  return (v >> n) | (v << (32 - n));
}
// Integrity machinery only. Bind the loaded native input bytes to the exact
// checked 3DS adaptation resource; no input tuning is compiled into this code.
inline std::array<uint8_t, 32> sha256(const uint8_t *data, size_t size) {
  static constexpr uint32_t constants[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
      0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
      0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
      0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
      0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
      0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
      0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
      0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  const size_t blocks = (size + 9 + 63) / 64;
  for (size_t block = 0; block < blocks; ++block) {
    uint8_t raw[64]{};
    for (size_t j = 0; j < 64; ++j) {
      const size_t at = block * 64 + j;
      if (at < size)
        raw[j] = data[at];
      else if (at == size)
        raw[j] = 0x80;
    }
    if (block + 1 == blocks) {
      const uint64_t bits = uint64_t(size) * 8;
      for (unsigned j = 0; j < 8; ++j)
        raw[63 - j] = uint8_t(bits >> (j * 8));
    }
    uint32_t w[64];
    for (unsigned j = 0; j < 16; ++j)
      w[j] = uint32_t(raw[j * 4]) << 24 | uint32_t(raw[j * 4 + 1]) << 16 |
             uint32_t(raw[j * 4 + 2]) << 8 | raw[j * 4 + 3];
    for (unsigned j = 16; j < 64; ++j) {
      const auto a = w[j - 15], b = w[j - 2];
      w[j] = w[j - 16] + (rotate(a, 7) ^ rotate(a, 18) ^ (a >> 3)) + w[j - 7] +
             (rotate(b, 17) ^ rotate(b, 19) ^ (b >> 10));
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5],
             g = h[6], v = h[7];
    for (unsigned j = 0; j < 64; ++j) {
      const auto t1 = v + (rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25)) +
                      ((e & f) ^ (~e & g)) + constants[j] + w[j],
                 t2 = (rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22)) +
                      ((a & b) ^ (a & c) ^ (b & c));
      v = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += v;
  }
  std::array<uint8_t, 32> out{};
  for (unsigned i = 0; i < 8; ++i)
    for (unsigned j = 0; j < 4; ++j)
      out[i * 4 + j] = uint8_t(h[i] >> (24 - j * 8));
  return out;
}

bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
bool same_tuning(const NativeInputTuning &a, const NativeInputTuning &b) {
  return a.circle_nominal_radius == b.circle_nominal_radius &&
         a.circle_activate == b.circle_activate &&
         a.circle_release == b.circle_release &&
         a.angular_hysteresis_degrees == b.angular_hysteresis_degrees &&
         a.touch_activate == b.touch_activate &&
         a.touch_release == b.touch_release &&
         a.tap_max_travel == b.tap_max_travel &&
         a.tap_max_seconds == b.tap_max_seconds &&
         a.base_radius == b.base_radius && a.thumb_radius == b.thumb_radius &&
         a.screen_width == b.screen_width &&
         a.screen_height == b.screen_height && a.base_rgba == b.base_rgba &&
         a.thumb_rgba == b.thumb_rgba;
}
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  while (n--) {
    c ^= *p++;
    for (unsigned i = 0; i < 8; ++i)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
// One actual native Reference at the original File.new source cursor, held by
// this call frame. It never claims Node lifecycle or source encrypted loading.
class SettingsFile final : public FieldGlobalNativeReference {
public:
  SettingsFile(const GlobalReadyData &data, FieldGlobalRegistry &registry,
               FieldObjectId id)
      : data_(data), registry_(registry),
        binding_{id, data.settings_file_spec(), 0x454e005d, 1} {}
  ~SettingsFile() override {
    std::string ignored;
    registry_.retire_object(binding_.object, ignored);
  }
  FieldGlobalExternalBinding binding() const override { return binding_; }
  const char *native_class() const override {
    return binding_.source.native_class.c_str();
  }
  const FieldGlobalRegistry *registry() const override { return &registry_; }
  bool checked_source_hash(std::string_view source,
                           std::array<uint8_t, 32> &out) const override {
    if (source != binding_.source.source)
      return false;
    out = binding_.source.source_sha;
    return data_.valid() && out == data_.identity().source_sha256;
  }
  bool file_exists(std::string_view source, bool &out, std::string &e) const {
    if (!data_.valid() || registry_.poisoned() ||
        registry_.native_reference(binding_.object).get() != this ||
        source != data_.policy().settings_source_path)
      return fail(e, "Global settings actual File source/owner rejected");
    errno = 0;
    auto *f = std::fopen(data_.policy().settings_native_path.c_str(), "rb");
    if (f) {
      if (std::fclose(f))
        return fail(e, "Global settings existence close failed");
      out = true;
      e.clear();
      return true;
    }
    if (errno != ENOENT)
      return fail(e, "Global source settings existence read failed");
    out = false;
    e.clear();
    return true;
  }

private:
  const GlobalReadyData &data_;
  FieldGlobalRegistry &registry_;
  FieldGlobalExternalBinding binding_;
};
} // namespace
bool PodunkGlobalReadyPreferences::initialize(
    const GlobalReadyData &d, const char *root, FieldGlobalRegistry &registry,
    const NativeInputData &i, NativeInputAdapter &a,
    const StartupSettingsData &s, const SessionSettings &session,
    const LocaleCatalog &locales, LocaleSelection &selection, uint32_t slots,
    uint32_t &selected, SelectNativeLocale select, std::string &e) {
  if (data_ || !d.valid() || !root || !*root || registry.poisoned() ||
      !registry.kernel() || !i.valid() || !s.valid() || !s.supports(session) ||
      !locales.valid() || selection.catalog() != &locales || !slots ||
      selected > slots || !select)
    return fail(
        e, "Global Ready actual native input/preferences owners unavailable");
  data_ = &d;
  registry_ = &registry;
  root_ = root;
  input_ = &i;
  adapter_ = &a;
  settings_ = &s;
  session_ = &session;
  locales_ = &locales;
  selection_ = &selection;
  max_slots_ = slots;
  selected_slot_ = &selected;
  select_ = std::move(select);
  e.clear();
  return true;
}
bool PodunkGlobalReadyPreferences::localized_inputs(std::string &e) {
  if (!data_ || inputs_complete_ || !input_->valid())
    return fail(e, "Global localized input source cursor rejected");
  const auto &p = data_->policy();
  auto path = root_;
  if (path.back() != '/' && path.back() != ':')
    path += '/';
  path += p.input_path;
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file || file.tellg() != std::streamoff(p.input_bytes))
    return fail(
        e, "Global localized input exact native resource missing/size differs");
  std::vector<uint8_t> bytes(p.input_bytes);
  file.seekg(0);
  if (!file.read(reinterpret_cast<char *>(bytes.data()), bytes.size()) ||
      crc(bytes.data(), bytes.size()) != p.input_crc ||
      sha256(bytes.data(), bytes.size()) != p.input_sha)
    return fail(e, "Global localized input native resource integrity differs");
  NativeInputData checked;
  if (!checked.load(bytes.data(), bytes.size(), e) ||
      !same_tuning(checked.tuning(), input_->tuning()))
    return fail(e, "Global localized input actual borrowed data differs");
  // Native resource is the explicit 3DS adaptation. No desktop InputEventKey
  // is created and no OS keyboard-layout branch is reported as executed.
  if (!adapter_->configure(*input_, e))
    return false;
  inputs_complete_ = true;
  e.clear();
  return true;
}
bool PodunkGlobalReadyPreferences::load_settings(std::string &e) {
  if (!data_ || !inputs_complete_ || settings_complete_ ||
      !settings_->supports(*session_) || selection_->catalog() != locales_)
    return fail(
        e, "Global settings actual source/input/preferences cursor rejected");
  const auto &p = data_->policy();
  FieldObjectId file_id = 0;
  if (!registry_->allocate_object(file_id, e))
    return false;
  auto file = std::make_shared<SettingsFile>(*data_, *registry_, file_id);
  if (!registry_->publish_native_reference(data_->settings_file_spec(), file_id,
                                           file, e))
    return false;
  bool exists = false;
  if (!file->file_exists(p.settings_source_path, exists, e))
    return false;
  if (exists)
    return fail(e, "Original encrypted settings.save branch is not migrated; "
                   "native locale/slot preferences are separate");
  std::string locale(selection_->code());
  const auto result = read_locale_preference(
      *locales_, p.locale_preference_path.c_str(), locale, e);
  if (result == LocalePreferenceRead::Rejected)
    return false;
  auto supported = [&](const std::string &code) {
    auto index = locales_->locale_index(code);
    return index >= 0 && locales_->locales()[size_t(index)].source_enabled &&
           locales_->locales()[size_t(index)].native_ready &&
           std::find(p.languages.begin(), p.languages.end(), code) !=
               p.languages.end();
  };
  if (!supported(locale)) {
    if (result == LocalePreferenceRead::Loaded)
      return fail(e, "Global native persisted language differs from "
                     "source-supported languages");
    locale = p.source_language_default;
  }
  if (!supported(locale))
    return fail(
        e, "Global source default language has no actual native locale owner");
  uint32_t slot = *selected_slot_;
  auto slot_result =
      read_slot_preference(p.slot_preference_path.c_str(), max_slots_, slot, e);
  if (slot_result == SlotPreferenceReadResult::Error)
    return false;
  // Existing GPU-safe application method, not LocaleSelection::select alone.
  // Never overwrite persistence during a source cold bootstrap.
  if (!select_(locale, false, e) || selection_->catalog() != locales_ ||
      selection_->code() != locale)
    return fail(e, "Global language service did not update the actual GPU-safe "
                   "locale owner");
  *selected_slot_ = slot;
  settings_complete_ = true;
  e.clear();
  return true;
}
bool PodunkGlobalReady::initialize(
    PodunkGlobalReadyDomain domain, const GlobalReadyData &data,
    const GlobalLoadData &cold, FieldGlobalConstructorRuntime &global,
    FieldGlobalDataRuntime &characters, FieldGlobalRegistry &registry,
    GlobalLoadGlobalOwner &actual, PlayerInitializationRuntime &player,
    PodunkGlobalLoadHost &load, PodunkGlobalReadyPreferences &prefs,
    std::string &e) {
  if (data_ || domain != PodunkGlobalReadyDomain::ColdSourceBootstrap ||
      !data.valid() || !cold.valid() ||
      data.load_ir_sha256() != cold.ir_sha256() || !global.data() ||
      data.constructor_ir_sha256() != global.data()->ir_sha256() ||
      !same(data.identity(), global.data()->identity()) || !registry.kernel() ||
      registry.poisoned() || !characters.constructor_complete() ||
      !player.data() || player.data()->ir_sha256() != data.player_ir_sha256())
    return fail(e, "Global Ready cold source owners/dependencies unavailable");
  if (registry.external_object(global.owner()) != &actual.external() ||
      actual.external().binding().object != global.owner())
    return fail(e, "Global Ready requires the actual Registry global object");
  data_ = &data;
  load_data_ = &cold;
  global_ = &global;
  characters_ = &characters;
  registry_ = &registry;
  actual_ = &actual;
  player_ = &player;
  load_ = &load;
  preferences_ = &prefs;
  if (!owner(e)) {
    data_ = nullptr;
    return false;
  }
  e.clear();
  return true;
}
bool PodunkGlobalReady::owner(std::string &e) const {
  if (!data_ || !global_->data() || registry_->poisoned() ||
      registry_->external_object(global_->owner()) != &actual_->external() ||
      data_->constructor_ir_sha256() != global_->data()->ir_sha256() ||
      data_->load_ir_sha256() != load_data_->ir_sha256() ||
      !characters_->constructor_complete() || !player_->data() ||
      player_->data()->ir_sha256() != data_->player_ir_sha256())
    return fail(e, "Global Ready actual owning source lifetime differs");
  auto tree = registry_->tree_owner(global_->owner());
  const auto *n = tree ? tree->state(global_->owner()) : nullptr;
  FieldIdentity identity;
  if (!tree || tree->object_domain() != registry_->kernel() || !n ||
      !n->alive || !tree->object_identity(global_->owner(), identity) ||
      !same(identity, data_->identity()))
    return fail(e, "Global Ready actual source Tree differs");
  e.clear();
  return true;
}
bool PodunkGlobalReady::binds(const FieldGlobalConstructorRuntime &global,
                              const FieldGlobalRegistry &registry) const {
  std::string error;
  return data_ && &global == global_ && &registry == registry_ && actual_ &&
         registry.external_object(global.owner()) == &actual_->external() &&
         owner(error);
}
bool PodunkGlobalReady::bind_cold_loader(
    const FieldCharacterLoadData &data, FieldUiManagerRuntime &ui,
    PodunkGlobalDataHost &owner_host, SourceRandom &random,
    std::vector<uint32_t> &uids, LoadRngClockProvider clock,
    FieldGlobalDataStatSignal signal, std::string &e) {
  if (!owner(e) || characters_data_ || running_ || cursor_ || !data.valid() ||
      data.ir_sha256() != load_data_->characters_ir_sha256() ||
      &owner_host.runtime() != characters_ ||
      registry_->external_object(ui.binding().object) != &ui || !clock ||
      !signal)
    return fail(
        e,
        "Global Ready cold LOAD actual constructor-live dependencies rejected");
  characters_data_ = &data;
  ui_ = &ui;
  data_host_ = &owner_host;
  random_ = &random;
  uids_ = &uids;
  clock_ = std::move(clock);
  stat_signal_ = std::move(signal);
  e.clear();
  return true;
}
bool PodunkGlobalReady::reject(std::string &e, const char *s) {
  poisoned_ = true;
  running_ = false;
  if (e.empty())
    e = s;
  return false;
}
bool PodunkGlobalReady::source_ready(FieldNodeTreeRuntime &tree,
                                     FieldObjectId id,
                                     const FieldNodeBinding &b,
                                     std::string &e) {
  if (!owner(e) || running_ || complete_ || poisoned_ || cursor_ ||
      id != global_->owner() || registry_->tree_owner(id).get() != &tree)
    return fail(e, "Global Ready original method cursor/lifetime rejected");
  const auto *n = tree.state(id);
  const auto *d = tree.descriptor(id);
  if (!n || !n->inside || !n->ready_notified || n->ready_first || !d ||
      !(d->script_methods & 1) || b.stable_id != d->id ||
      b.script_sha != d->script_sha || !same(b.identity, data_->identity()))
    return fail(
        e, "Global Ready actual child-first Node script Ready cursor absent");
  if (!characters_->ready_complete() || !characters_data_ || !ui_ ||
      !data_host_ || !random_ || !uids_ || !clock_ || !stat_signal_)
    return fail(e, "Global Ready precedes actual globalData Ready/cold LOAD "
                   "owner binding");
  // Data binding creates no gameplay objects or UID draws. The actual source
  // File/Item/Character work remains at the later _load_default_save cursor.
  if (!load_->initialize(*load_data_, *characters_data_, *this, *ui_,
                         *data_host_, *registry_, *random_, *uids_, clock_,
                         stat_signal_, e))
    return reject(e,
                  "Global Ready actual cold LOAD owner initialization failed");
  running_ = true;
  ready_tree_ = &tree;
  for (const auto step : data_->policy().steps) {
    if (uint32_t(step) != cursor_ + 1)
      return reject(e, "Global Ready unknown/reordered opcode");
    switch (step) {
    case GlobalReadyStep::LocalizedInputs:
      if (!preferences_->localized_inputs(e) ||
          !preferences_->inputs_complete())
        return reject(e, "Global native localized inputs did not execute");
      break;
    case GlobalReadyStep::Settings:
      if (!preferences_->load_settings(e) || !preferences_->settings_complete())
        return reject(e, "Global native settings did not execute");
      break;
    case GlobalReadyStep::PartySpace: {
      std::shared_ptr<const FieldGlobalPartySpaceArray> before, after;
      if (!global_->party_space(before, e) ||
          !global_->resize_party_space(data_->policy().party_space, e) ||
          !global_->party_space(after, e) || before != after ||
          after->values.size() != data_->policy().party_space)
        return reject(e, "Global partySpace actual Array resize differs");
      for (const auto &value : after->values)
        if (value.is_vector)
          return reject(e, "Global cold partySpace resize did not preserve "
                           "source nil values");
      break;
    }
    case GlobalReadyStep::Transition: {
      FieldObjectId transition = 0;
      if (!global_->object(FieldGlobalMemberRole::SceneTransition, transition,
                           e))
        return reject(e, "Global actual scene_transition missing");
      auto original = registry_->tree_owner(transition);
      const auto *state = original ? original->state(transition) : nullptr;
      FieldIdentity identity;
      if (!state || state->parent || state->inside || state->ready_notified ||
          original->root() != transition ||
          !original->object_identity(transition, identity) ||
          !same(identity, global_->data()->transition_identity()) ||
          !registry_->persistent_reparent(transition, id, e))
        return reject(e,
                      "Global original add_child transition cursor rejected");
      state = tree.state(transition);
      if (registry_->tree_owner(transition).get() != &tree || !state ||
          state->parent != id || !state->inside || !state->ready_notified)
        return reject(
            e, "Global transition add_child did not synchronously enter/Ready");
      break;
    }
    case GlobalReadyStep::Player:
      if (!player_->run(e) || !player_->complete() ||
          !registry_->object_exists(player_->player()))
        return reject(e, "Global actual _init_player incomplete");
      break;
    case GlobalReadyStep::ColdDefault:
      if (!load_->load_cold_default(e) || !load_->complete())
        return reject(e, "Global actual default LOAD incomplete");
      break;
    default:
      return reject(e, "Global Ready unknown source opcode");
    }
    ++cursor_;
  }
  running_ = false;
  complete_ = cursor_ == data_->policy().steps.size();
  e.clear();
  return complete_;
}
const FieldGlobalExternalObject &PodunkGlobalReady::external() const {
  return actual_->external();
}
bool PodunkGlobalReady::admit_cold_load_cursor(
    const GlobalLoadData &load, const FieldGlobalDataRuntime &characters,
    std::string &e) const {
  if (!owner(e) || poisoned_ || !running_ ||
      cursor_ != size_t(GlobalReadyStep::Player) ||
      &characters != characters_ ||
      load.ir_sha256() != load_data_->ir_sha256() || !player_->complete() ||
      !preferences_->inputs_complete() || !preferences_->settings_complete())
    return fail(e, "Global cold LOAD precedes actual Ready prefix");
  const auto *n = ready_tree_ ? ready_tree_->state(global_->owner()) : nullptr;
  if (!n || !n->inside || !n->ready_notified || n->ready_first)
    return fail(e, "Global cold LOAD escaped actual Node Ready cursor");
  e.clear();
  return true;
}
bool PodunkGlobalReady::party_array(
    std::string_view member, std::shared_ptr<const GlobalLoadObjectArray> &out,
    std::string &e) const {
  return owner(e) && global_->party_array(member, out, e);
}
bool PodunkGlobalReady::clear_party(std::string_view member, std::string &e) {
  if (!owner(e) || !running_ || cursor_ != size_t(GlobalReadyStep::Player))
    return fail(e, "Global party clear is outside the actual cold LOAD cursor");
  return global_->clear_party(member, e);
}
bool PodunkGlobalReady::append_party(std::string_view member, FieldObjectId id,
                                     std::string &e) {
  if (!owner(e) || !running_ || cursor_ != size_t(GlobalReadyStep::Player))
    return fail(e,
                "Global party append is outside the actual cold LOAD cursor");
  return global_->append_party(member, id, e);
}
} // namespace encore::ctr

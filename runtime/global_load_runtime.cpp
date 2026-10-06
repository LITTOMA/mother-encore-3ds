#include "encore/global_load_runtime.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool equal(const GlobalYamlValue &a, const GlobalYamlValue &b,
           size_t depth = 0) {
  if (depth > 64 || a.kind != b.kind)
    return false;
  switch (a.kind) {
  case 0:
    return true;
  case 1:
    return a.boolean == b.boolean;
  case 2:
    return a.integer == b.integer;
  case 3:
    return a.real == b.real;
  case 4:
    return a.string == b.string;
  case 5:
    if (a.array.size() != b.array.size())
      return false;
    for (size_t i = 0; i < a.array.size(); ++i)
      if (!a.array[i] || !b.array[i] ||
          !equal(*a.array[i], *b.array[i], depth + 1))
        return false;
    return true;
  case 6:
    if (a.dictionary.size() != b.dictionary.size())
      return false;
    for (size_t i = 0; i < a.dictionary.size(); ++i)
      if (a.dictionary[i].first != b.dictionary[i].first ||
          !a.dictionary[i].second || !b.dictionary[i].second ||
          !equal(*a.dictionary[i].second, *b.dictionary[i].second, depth + 1))
        return false;
    return true;
  default:
    return false;
  }
}
// Executes the reviewed recursive source mutation on the actual returned
// Dictionaries. Replacement keeps the override object root and its aliases.
bool merge(GlobalYamlValue &save, const GlobalYamlValue &overrides,
           std::string &e, size_t depth = 0) {
  if (depth > 64 || save.kind != 6 || overrides.kind != 6)
    return fail(e, "Source save override requires Dictionary");
  for (const auto &row : overrides.dictionary) {
    if (!row.second)
      return fail(e, "Source save override contains null storage");
    auto old =
        std::find_if(save.dictionary.begin(), save.dictionary.end(),
                     [&](const auto &x) { return x.first == row.first; });
    if (old == save.dictionary.end()) {
      save.dictionary.push_back(row);
      continue;
    }
    if (!old->second)
      return fail(e, "Source save contains null storage");
    if (old->second->kind == 6 && row.second->kind == 6) {
      if (!merge(*old->second, *row.second, e, depth + 1))
        return false;
    } else if (old->second->kind == 5 && row.second->kind == 5) {
      bool replace = row.second->array.empty();
      if (!old->second->array.empty()) {
        const auto &p = old->second->array.front();
        if (!p)
          return fail(e, "Source save Array contains null storage");
        replace = replace || p->kind == 5 || p->kind == 6;
      }
      if (replace)
        old->second = row.second;
      else
        for (const auto &v : row.second->array) {
          if (!v)
            return fail(e, "Source override Array contains null storage");
          if (std::none_of(old->second->array.begin(), old->second->array.end(),
                           [&](const auto &x) { return x && equal(*x, *v); }))
            old->second->array.push_back(v);
        }
    } else
      old->second = row.second;
  }
  return true;
}
bool integer(const GlobalYamlValue &v, int64_t &out) {
  if (v.kind == 2) {
    out = v.integer;
    return true;
  }
  if (v.kind == 1) {
    out = v.boolean;
    return true;
  }
  // Avoid out-of-range floating conversions, including rounded INT64_MAX.
  if (v.kind == 3 && std::isfinite(v.real) &&
      v.real >= -9223372036854775808.0 && v.real < 9223372036854775808.0) {
    out = int64_t(v.real);
    return true;
  }
  return false;
}
bool real(const GlobalYamlValue &v, double &out) {
  if (v.kind == 2)
    out = double(v.integer);
  else if (v.kind == 3)
    out = v.real;
  else
    return false;
  return std::isfinite(out);
}
std::shared_ptr<GlobalYamlValue> empty(uint32_t kind) {
  auto v = std::make_shared<GlobalYamlValue>();
  v->kind = kind;
  return v;
}
bool witness(const std::shared_ptr<GlobalYamlValue> &actual,
             const std::shared_ptr<const GlobalYamlValue> &expected) {
  return actual && expected && equal(*actual, *expected);
}
std::shared_ptr<GlobalYamlValue> literal(const GlobalYamlValue &v) {
  auto result = std::make_shared<GlobalYamlValue>();
  result->kind = v.kind;
  result->boolean = v.boolean;
  result->integer = v.integer;
  result->real = v.real;
  result->string = v.string;
  for (const auto &x : v.array)
    result->array.push_back(literal(*x));
  for (const auto &x : v.dictionary)
    result->dictionary.emplace_back(x.first, literal(*x.second));
  return result;
}
} // namespace
bool GlobalLoadRuntime::initialize(
    const GlobalLoadData &d, FieldGlobalDataRuntime &c, FieldGlobalRegistry &r,
    GlobalItemCache &items, SourceRandom &random, std::vector<uint32_t> &uids,
    LoadRngClockProvider clock, GlobalLoadHost host, std::string &e) {
  if (data_ || !d.valid() || !c.ready_complete() || c.global_load_started_ ||
      c.global_load_poisoned_ || c.global_load_complete_ || r.poisoned() ||
      items.owner() != c.globaldata_object() || !items.directory_admitted() ||
      !clock || !host.global || !host.ui || !host.files || !host.characters ||
      !host.characters_complete || !host.new_item ||
      (!c.global_load_bound_to(d) && !c.initialize_global_load(d, e)))
    return fail(e, "Global LOAD real dependencies/owners missing");
  data_ = &d;
  core_ = &c;
  registry_ = &r;
  items_ = &items;
  random_ = &random;
  uids_ = &uids;
  clock_ = std::move(clock);
  host_ = std::move(host);
  ir_ = d.ir_sha256();
  e.clear();
  return true;
}
bool GlobalLoadRuntime::available(std::string &e) const {
  if (!data_ || !data_->valid() || data_->ir_sha256() != ir_ || !core_ ||
      !core_->global_load_bound_to(*data_) || !registry_ ||
      registry_->poisoned() || poisoned_ || !items_->directory_admitted() ||
      host_.files->poisoned())
    return fail(e, "Global LOAD source owner unavailable/reloaded");
  return true;
}
bool GlobalLoadRuntime::owner_cursor(std::string &e) const {
  if (!available(e))
    return false;
  const auto &external = host_.global->external();
  const auto b = external.binding();
  const auto ui = host_.ui->binding();
  std::array<uint8_t, 32> h{}, uh{};
  FieldGlobalExternalState state;
  const FieldUiMenuShader *shader = nullptr;
  if (registry_->external_object(b.object) != &external ||
      b.source.script != data_->owner_source() ||
      b.source.identity.upstream_commit != data_->identity().upstream_commit ||
      !data_->source_hash(b.source.script, h) || h != b.source.script_sha ||
      !external.state(state, e) || !state.inside ||
      registry_->external_object(ui.object) != host_.ui ||
      !data_->source_hash(ui.source.script, uh) || uh != ui.source.script_sha ||
      ui.source.identity.upstream_commit != data_->identity().upstream_commit ||
      !host_.ui->checked_menu_shader(shader, e) || !shader ||
      registry_->source_resource(shader->binding().object) != shader)
    return fail(
        e, "Global LOAD actual global/UI constructor source owner rejected");
  // Concrete source global owner must prove execution at this method cursor;
  // inside/ID alone never grants prefix settings/player/bootstrap capability.
  return host_.global->admit_cold_load_cursor(*data_, *core_, e);
}
bool GlobalLoadRuntime::assignment(const GlobalLoadAssignment &a,
                                   const std::shared_ptr<GlobalYamlValue> &save,
                                   std::string &e) {
  std::shared_ptr<const GlobalYamlValue> fallback;
  if (a.fallback_kind == 1 && a.fallback)
    fallback = literal(*a.fallback);
  else if (a.fallback_kind == 2) {
    FieldGlobalDataMemberState v;
    if (!core_->read_global_member(a.fallback_member, v, e))
      return false;
    fallback = v.value;
  } else if (a.fallback_kind == 3) {
    FieldGlobalDataMemberState v;
    if (!core_->read_global_member(a.constant, v, e) || !v.value ||
        v.value->kind != 5 || a.constant_index >= v.value->array.size())
      return fail(e, "Global LOAD source constant fallback rejected");
    fallback = v.value->array[a.constant_index];
  } else
    return fail(e, "Global LOAD unknown fallback operation");
  if (!fallback)
    return fail(e, "Global LOAD missing source fallback value");
  auto value = save->get(a.key);
  if (!value)
    value = std::const_pointer_cast<GlobalYamlValue>(fallback);
  if (a.kind == 2) {
    auto y = save->get(a.key_y);
    if (!y)
      y = std::const_pointer_cast<GlobalYamlValue>(fallback);
    double xvalue = 0, yvalue = 0;
    if (!real(*value, xvalue) || !real(*y, yvalue) ||
        std::abs(xvalue) > std::numeric_limits<float>::max() ||
        std::abs(yvalue) > std::numeric_limits<float>::max())
      return fail(e, "Global LOAD Vector2 conversion bounds rejected");
    std::array<double, 2> position{double(float(xvalue)),
                                   double(float(yvalue))};
    return core_->assign_global_load_member(a.member, value, &position, e);
  }
  if (a.kind != 1)
    return fail(e, "Global LOAD unknown source assignment");
  if (a.coercion) {
    int64_t n = 0;
    if (a.coercion != 1 || !integer(*value, n))
      return fail(e, "Global LOAD int conversion unsupported");
    auto converted = empty(2);
    converted->integer = n;
    value = std::move(converted);
  }
  return core_->assign_global_load_member(a.member, value, nullptr, e);
}
bool GlobalLoadRuntime::inventory(size_t index,
                                  const std::shared_ptr<GlobalYamlValue> &save,
                                  std::string &e) {
  if (index >= data_->inventories().size())
    return fail(e, "Global LOAD Inventory index rejected");
  const auto &spec = data_->inventories()[index];
  const auto &keys = data_->saved_item_keys();
  auto source = save->get(spec.source_key);
  if (!source)
    source = empty(5);
  if (source->kind != 5 || source->array.size() != spec.items.size() ||
      keys.size() != 4)
    return fail(e, "Global LOAD Inventory actual serialized source differs");
  FieldObjectId inv = 0;
  if (!core_->global_load_inventory(index, inv, e))
    return false;
  std::vector<FieldGlobalDataItemReference> loaded;
  for (size_t i = 0; i < source->array.size(); ++i) {
    const auto &entry = source->array[i];
    const auto &reviewed = spec.items[i];
    if (!entry || entry->kind != 6)
      return fail(e, "Global LOAD serialized Item is not Dictionary");
    auto name = entry->get(keys[0]);
    auto equipped = entry->get(keys[1]);
    auto doses = entry->get(keys[2]);
    auto uid = entry->get(keys[3]);
    const bool eq =
        equipped ? equipped->truthy() : data_->saved_item_default_equipped();
    int64_t count = data_->saved_item_default_doses(), saved_uid = 0;
    if (!name || name->kind != 4 || name->string != reviewed.name ||
        eq != reviewed.equipped || (doses && !integer(*doses, count)) ||
        count < 0 || count > std::numeric_limits<uint32_t>::max() ||
        uint32_t(count) != reviewed.doses || bool(uid) != reviewed.has_uid ||
        (uid && (!integer(*uid, saved_uid) || saved_uid < 0 ||
                 saved_uid > std::numeric_limits<uint32_t>::max() ||
                 uint32_t(saved_uid) != reviewed.uid)))
      return fail(
          e, "Global LOAD serialized Item is outside checked cold capability");
    auto definition = items_->definitions()->definition(name->string);
    if (!definition)
      return fail(e, "Global LOAD Item definition missing");
    // Source data.get("uid",randi()) eagerly draws even for explicit uid.
    std::vector<LoadUidAllocation> trace;
    if (!apply_load_uid_allocations(*random_, *uids_, {{spec.id, 1}}, clock_, e,
                                    &trace))
      return false;
    FieldOwnedItem value{
        definition->id, uid ? uint32_t(saved_uid) : trace.front().generated_uid,
        uint32_t(count), eq};
    FieldGlobalDataItemReference actual;
    if (!host_.new_item(inv, value, actual, e))
      return false;
    loaded.push_back(std::move(actual));
  }
  return core_->publish_global_load_inventory(index, loaded, e);
}
bool GlobalLoadRuntime::load_cold_default(std::string &e) {
  if (begun_ || complete_ || (core_ && core_->global_load_started_))
    return fail(e, "Global LOAD cursor already consumed");
  if (!owner_cursor(e))
    return false;
  // Gate first. A missing true source bootstrap owner consumes no File or RNG.
  begun_ = true;
  core_->global_load_started_ = true;
  std::shared_ptr<GlobalYamlValue> save;
  auto reject = [&]() {
    poisoned_ = true;
    core_->global_load_poisoned_ = true;
    return false;
  };
  if (!host_.files->actual_global_load(*data_, 0, save, e) ||
      !witness(save, data_->cold_original())) {
    if (e.empty())
      e = "Global LOAD actual cold File result differs";
    return reject();
  }
  for (const auto &step : data_->steps()) {
    bool ok = false;
    switch (step.kind) {
    case 1: {
      std::shared_ptr<GlobalYamlValue> overrides;
      ok = host_.files->actual_global_load(*data_, 2, overrides, e) &&
           witness(overrides, data_->overrides()) &&
           merge(*save, *overrides, e) && witness(save, data_->cold_merged());
      break;
    }
    case 2:
      ok = step.index < data_->assignments().size() &&
           assignment(data_->assignments()[step.index], save, e);
      break;
    case 3:
      ok = inventory(step.index, save, e);
      break;
    case 4:
      ok = true;
      for (const auto &row : data_->characters()) {
        auto parsed = save->get(row.name);
        if (!parsed)
          parsed = empty(6);
        if (!witness(parsed, row.saved)) {
          ok = false;
          break;
        }
      }
      if (ok)
        ok = host_.characters(save, e) && host_.characters_complete();
      if (ok)
        for (const auto &row : data_->characters()) {
          FieldCharacterLoadState actual;
          if (!core_->read_character_load(row.id, actual, e)) {
            ok = false;
            break;
          }
        }
      break;
    case 5:
    case 6: {
      std::shared_ptr<const GlobalLoadObjectArray> before, after;
      ok = host_.global->party_array(step.member, before, e) && before &&
           host_.global->clear_party(step.member, e) &&
           host_.global->party_array(step.member, after, e) &&
           before == after && after->values.empty();
      break;
    }
    case 7: {
      auto party = save->get(data_->party_source_key());
      if (!party || party->kind != 5 ||
          party->array.size() != data_->party().size())
        break;
      ok = true;
      for (size_t i = 0; i < party->array.size(); ++i) {
        const auto &p = party->array[i];
        const auto &r = data_->party()[i];
        if (!p || p->kind != 4 || p->string != r.name) {
          ok = false;
          break;
        }
        const bool playable =
            std::find(data_->playable_names().begin(),
                      data_->playable_names().end(),
                      p->string) != data_->playable_names().end();
        if (playable != r.playable) {
          ok = false;
          break;
        }
        auto clear = std::find_if(
            data_->steps().begin(), data_->steps().end(),
            [&](const auto &s) { return s.kind == (playable ? 5u : 6u); });
        FieldCharacterLoadState actual;
        std::shared_ptr<const GlobalLoadObjectArray> before, after;
        if (clear == data_->steps().end() ||
            !core_->read_character_load(r.id, actual, e) ||
            !registry_->object_exists(actual.object) ||
            !host_.global->party_array(clear->member, before, e) || !before) {
          ok = false;
          break;
        }
        auto prior = before->values;
        if (!host_.global->append_party(clear->member, actual.object, e) ||
            !host_.global->party_array(clear->member, after, e) ||
            before != after) {
          ok = false;
          break;
        }
        prior.push_back(actual.object);
        if (after->values != prior) {
          ok = false;
          break;
        }
      }
      break;
    }
    case 8: {
      std::string flavor;
      const FieldUiMenuShader *shader = nullptr;
      ok = core_->menu_flavor(flavor, e) &&
           host_.ui->set_menu_flavors(flavor, e) &&
           host_.ui->checked_menu_shader(shader, e) && shader &&
           registry_->source_resource(shader->binding().object) == shader;
      break;
    }
    case 9: {
      auto flags = save->get(data_->normal_flags_source_key());
      if (!flags)
        flags = empty(6);
      FieldGlobalDataMemberState actual;
      if (flags->kind != 6 ||
          !core_->read_global_member(data_->normal_flags_member(), actual, e) ||
          !actual.flags)
        break;
      const auto registered = actual.flags->state().normal;
      ok = true;
      for (const auto &flag : registered) {
        auto v = flags->get(flag.first);
        if (v && v->kind != 1) {
          ok = false;
          break;
        }
        if (!core_->global_load_normal_flag(flag.first, v && v->boolean, e)) {
          ok = false;
          break;
        }
      }
      break;
    }
    case 10:
      ok = !data_->goto_game_admitted();
      break;
    default:
      break;
    }
    if (!ok) {
      if (e.empty())
        e = "Global LOAD actual source step rejected";
      return reject();
    }
    ++cursor_;
  }
  core_->global_load_global_ = host_.global->external().binding().object;
  core_->global_load_ui_ = host_.ui->binding().object;
  core_->global_load_complete_ = true;
  complete_ = true;
  e.clear();
  return true;
}
bool GlobalLoadRuntime::complete() const {
  std::string e;
  return complete_ && available(e) && cursor_ == data_->steps().size() &&
         core_->load_complete() && host_.characters_complete();
}
} // namespace encore::upstream

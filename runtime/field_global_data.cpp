#include "encore/field_global_data.hpp"
#include <algorithm>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool same(const FieldOwnedItem &a, const FieldOwnedItem &b) {
  return a.definition == b.definition && a.uid == b.uid && a.doses == b.doses &&
         a.equipped == b.equipped;
}
} // namespace
FieldGlobalDataRuntime::~FieldGlobalDataRuntime() {
  items_.clear();
  if (registry_) {
    std::string error;
    for (auto i = objects_.rbegin(); i != objects_.rend(); ++i)
      registry_->retire_object(i->object, error);
  }
}
bool FieldGlobalDataRuntime::construct_members(
    const FieldGlobalDataData &data, const FieldGlobalExternalSpec &spec,
    FieldObjectId actual, FieldGlobalRegistry &registry, std::string &e) {
  std::array<uint8_t, 32> proof{};
  if (data_ || poisoned_ || !actual || !data.valid() ||
      spec.identity.upstream_commit != data.identity().upstream_commit ||
      spec.source != data.owner_source() || spec.script != spec.source ||
      !spec.stable_id || !data.source_hash(spec.source, proof) ||
      spec.source_sha != proof || spec.script_sha != proof)
    return fail(e, "globalData actual autoload source binding rejected");
  // actual may be the source constructor's reserved ObjectDB slot, before the
  // registry publishes its external owner. It is never fabricated here.
  data_ = &data;
  admitted_ir_ = data.ir_sha256();
  registry_ = &registry;
  owner_ = actual;
  for (const auto &row : data.declarations()) {
    FieldObjectId id = 0;
    if (!registry.allocate_object(id, e)) {
      poisoned_ = true;
      return false;
    }
    objects_.push_back(
        {id, 0, row.id, row.kind, row.role, row.defaults, {}, {}, {}});
  }
  e.clear();
  return true;
}
bool FieldGlobalDataRuntime::object_exists(FieldObjectId id) const {
  if (!id || poisoned_ || !data_ || !data_->valid() ||
      data_->ir_sha256() != admitted_ir_)
    return false;
  for (const auto &o : objects_)
    if (o.object == id)
      return true;
  for (const auto &i : items_)
    if (i.object == id && i.registry == registry_ && i.actual_owner)
      return true;
  return false;
}
bool FieldGlobalDataRuntime::read_constructed_object(FieldObjectId id,
                                                     FieldGlobalDataObject &out,
                                                     std::string &e) const {
  if (!id || !registry_ || !data_ || poisoned_ || !data_->valid() ||
      data_->ir_sha256() != admitted_ir_)
    return fail(e, "globalData current source object unavailable");
  auto object =
      std::find_if(objects_.begin(), objects_.end(),
                   [&](const auto &value) { return value.object == id; });
  if (object == objects_.end())
    return fail(e, "globalData current source object not owned");
  out = *object;
  e.clear();
  return true;
}
bool FieldGlobalDataRuntime::check_owners(
    const FieldInventoryData &inv,
    const std::vector<std::pair<uint32_t, FieldObjectId>> &owners,
    std::string &e) const {
  if (!data_ || poisoned_ || !prefix_ || !data_->valid() ||
      data_->ir_sha256() != admitted_ir_ ||
      inv.identity() != data_->inventory_sha256() ||
      inv.source_pin() != data_->identity().upstream_commit ||
      owners.size() != data_->inventory_owners().size())
    return fail(e, "globalData source inventory ownership unavailable");
  std::set<uint32_t> seen;
  for (const auto &o : owners) {
    auto expected = std::find_if(
        data_->inventory_owners().begin(), data_->inventory_owners().end(),
        [&](const auto &r) { return r.id == o.first; });
    if (expected == data_->inventory_owners().end() ||
        !seen.insert(o.first).second || !object_exists(o.second))
      return fail(e, "globalData inventory owner ObjectDB rejected");
    auto source = std::find_if(
        data_->declarations().begin(), data_->declarations().end(),
        [&](const auto &r) { return r.name == expected->source_name; });
    if (source == data_->declarations().end())
      return fail(e, "globalData source declaration absent");
    auto object =
        std::find_if(objects_.begin(), objects_.end(), [&](const auto &r) {
          return r.declaration == source->id;
        });
    if (object == objects_.end() ||
        (expected->role ? object->object : object->inventory) != o.second)
      return fail(e, "globalData live source inventory reference differs");
  }
  e.clear();
  return true;
}
bool FieldGlobalDataRuntime::load_inventory_prefix(
    const FieldInventoryData &inv, const FieldItemDefinitions &defs,
    const FieldInventoryState *saved, SourceRandom &random,
    std::vector<uint32_t> &ledger, LoadRngClockProvider clock,
    FieldGlobalDataItemFactory factory, FieldGlobalDataStatSignal signal,
    std::string &e) {
  if (!data_ || poisoned_ || prefix_ || !factory || !signal ||
      !data_->valid() || data_->ir_sha256() != admitted_ir_ ||
      !data_->bind_inventory(inv, defs, e))
    return fail(e, "globalData source LOAD prefix unavailable");
  if (saved)
    return fail(e, "AwaitingCharacterFields: complete saved Character/Status "
                   "Reference owner is required before global.LOAD");
  const auto &policy = data_->character_policy();
  if (policy.exp_levels.size() != inv.levels().size() || !inv.initial_level() ||
      inv.initial_level() > policy.exp_levels.size())
    return fail(e, "globalData character experience/stat resource differs");
  FieldInventoryState next;
  if (saved) {
    FieldInventoryRuntime check;
    if (!check.initialize(inv, defs, *saved, e))
      return false;
    next = *saved;
  } else {
    next.level = inv.initial_level();
    next.cash = inv.initial_cash();
    for (const auto &o : inv.owners())
      next.items.inventories.push_back({o.id, o.role, {}});
    next.items.party_order = {inv.role(0)->id};
  }
  // Preflight all source item policies before irreversible clock/ObjectDB
  // calls.
  for (auto role : data_->load_roles()) {
    const auto *o = inv.role(role);
    if (!o)
      return fail(e, "globalData LOAD source owner absent");
    size_t count = 0;
    auto check_item = [&](const FieldOwnedItem &i) {
      const auto *d = defs.definition(i.definition);
      return d && i.doses && i.doses <= d->doses &&
             (!i.equipped || (role == 0 && !d->slot.empty())) &&
             (role != 1 || d->keyitem()) && (role != 0 || !d->keyitem());
    };
    if (saved) {
      for (const auto &r : next.items.inventories)
        if (r.owner == o->id)
          for (const auto &i : r.items) {
            if (!check_item(i))
              return fail(e, "globalData saved source item rejected");
            ++count;
          }
    } else {
      for (const auto &i : inv.initial())
        if (i.owner == o->id) {
          if (!check_item(i.item))
            return fail(e, "globalData initial source item rejected");
          ++count;
        }
    }
    if (!defs.unbounded(role) && count > defs.capacity(role))
      return fail(e, "globalData source inventory capacity exceeded");
  }
  auto candidate_random = random;
  auto candidate_ledger = ledger;
  std::vector<FieldGlobalDataItemReference> newitems;
  FieldObjectId newinventory = 0;
  auto abort = [&]() {
    newitems.clear();
    if (newinventory) {
      std::string ignored;
      objects_.erase(std::remove_if(objects_.begin(), objects_.end(),
                                    [&](const auto &o) {
                                      return o.object == newinventory;
                                    }),
                     objects_.end());
      registry_->retire_object(newinventory, ignored);
    }
    poisoned_ = true;
    return false;
  };
  for (auto role : data_->load_roles()) {
    const auto *o = inv.role(role);
    if (role == 0) {
      if (!registry_->allocate_object(newinventory, e))
        return abort();
      objects_.push_back({newinventory,
                          0,
                          data_->first_character(),
                          2,
                          0,
                          data_->normal_inventory_defaults(),
                          {},
                          {},
                          {},
                          {}});
    }
    auto row = std::find_if(next.items.inventories.begin(),
                            next.items.inventories.end(),
                            [&](const auto &r) { return r.owner == o->id; });
    if (row == next.items.inventories.end()) {
      e = "globalData inventory source row missing";
      return abort();
    }
    std::vector<FieldOwnedItem> source;
    if (saved)
      source = row->items;
    else
      for (const auto &i : inv.initial())
        if (i.owner == o->id)
          source.push_back(i.item);
    row->items.clear();
    for (auto item : source) {
      std::vector<LoadUidAllocation> trace;
      if (!apply_load_uid_allocations(candidate_random, candidate_ledger,
                                      {{o->id, 1}}, clock, e, &trace))
        return abort();
      if (trace.size() != 1) {
        e = "globalData source UID trace missing";
        return abort();
      }
      if (!saved)
        item.uid = trace.front().generated_uid;
      FieldGlobalDataItemReference actual;
      if (!factory(inv, defs, *registry_, o->id, item, actual, e))
        return abort();
      if (actual.registry != registry_ || !actual.object ||
          !actual.actual_owner || actual.owner != o->id ||
          !same(actual.value, item) || object_exists(actual.object) ||
          std::any_of(newitems.begin(), newitems.end(), [&](const auto &v) {
            return v.object == actual.object;
          })) {
        e = "globalData real Item factory result rejected";
        return abort();
      }
      newitems.push_back(std::move(actual));
      row->items.push_back(item);
    }
  }
  FieldInventoryRuntime check;
  if (!check.initialize(inv, defs, next, e))
    return abort();
  if (!saved) {
    std::array<int64_t, 7> stats{};
    if (!check.effective_stats(stats, e))
      return abort();
    next.hp = std::clamp<int64_t>(inv.initial_hp(), 0, stats[0]);
    next.pp = std::clamp<int64_t>(inv.initial_pp(), 0, stats[1]);
    next.revision = 1;
    if (!check.validate(next, e))
      return abort();
  }
  auto character =
      std::find_if(objects_.begin(), objects_.end(), [&](const auto &o) {
        return o.declaration == data_->first_character();
      });
  if (character == objects_.end()) {
    e = "globalData first source character missing";
    return abort();
  }
  character->inventory = newinventory;
  for (auto &object : objects_) {
    if (object.kind != 2)
      continue;
    const auto *source_owner = inv.role(object.role);
    for (const auto &item : newitems)
      if (item.owner == source_owner->id)
        object.item_objects.push_back(item.object);
  }
  auto field = [&](const std::string &name) -> FieldGlobalDataDefault * {
    auto found =
        std::find_if(character->fields.begin(), character->fields.end(),
                     [&](const auto &f) { return f.name == name; });
    return found == character->fields.end() ? nullptr : &*found;
  };
  auto integer = [&](const std::string &name, int64_t value) {
    auto *slot = field(name);
    if (!slot || slot->kind != 2)
      return false;
    slot->integer_value = value;
    return true;
  };
  auto string = [&](const std::string &name, const std::string &value) {
    auto *slot = field(name);
    if (!slot || slot->kind != 1)
      return false;
    slot->string_value = value;
    return true;
  };
  auto exp =
      std::min(std::max(policy.exp, policy.exp_levels[inv.initial_level() - 1]),
               policy.exp_levels.back());
  uint32_t level = 1;
  while (level < policy.exp_levels.size() && exp >= policy.exp_levels[level])
    ++level;
  if (level != next.level || !string(policy.slots[0], policy.name) ||
      !integer(policy.slots[2], exp) || !integer(policy.slots[3], level)) {
    e = "globalData Ninten source experience/fields rejected";
    return abort();
  }
  std::array<int64_t, 7> effective{};
  if (!check.initialize(inv, defs, next, e) ||
      !check.effective_stats(effective, e))
    return abort();
  const auto &base = inv.levels()[level - 1];
  std::array<int64_t, 2> intermediate{};
  for (size_t stat = 0; stat < base.size(); ++stat) {
    if (!integer(policy.stat_slots[stat], base[stat])) {
      e = "globalData source stat field missing";
      return abort();
    }
    if (stat < intermediate.size() && base[stat] > 0) {
      intermediate[stat] = std::clamp<int64_t>(base[stat], 0, effective[stat]);
      if (!integer(policy.slots[4 + stat], intermediate[stat]) ||
          !signal(character->object, policy.slots[4 + stat].substr(1),
                  base[stat], effective[stat], e))
        return abort();
    }
    if (base[stat] != 0 &&
        !signal(character->object, inv.stats()[stat], base[stat], 0, e))
      return abort();
  }
  // New-game source has an empty status list: no Status Reference allocations.
  // Constructor defaults provide empty permanent boosts; source overrides
  // provide exact affinity values and skill strings, then HP/PP and sorting.
  if (!string(policy.slots[1], policy.nickname)) {
    e = "globalData nickname field missing";
    return abort();
  }
  character->learned_skills = policy.skills;
  character->affinities = policy.affinities;
  for (size_t stat = 0; stat < 2; ++stat) {
    auto raw = stat ? inv.initial_pp() : inv.initial_hp();
    auto value = stat ? next.pp : next.hp;
    if (!integer(policy.slots[4 + stat], value)) {
      e = "globalData final vitals field missing";
      return abort();
    }
    if (raw != intermediate[stat] &&
        !signal(character->object, policy.slots[4 + stat].substr(1), raw,
                effective[stat], e))
      return abort();
  }
  std::stable_sort(
      character->learned_skills.begin(), character->learned_skills.end(),
      [&](const auto &a, const auto &b) {
        return std::find(policy.skills_order.begin(), policy.skills_order.end(),
                         a) < std::find(policy.skills_order.begin(),
                                        policy.skills_order.end(), b);
      });
  state_ = std::move(next);
  items_ = std::move(newitems);
  random = candidate_random;
  ledger.swap(candidate_ledger);
  prefix_ = true;
  e.clear();
  return true;
}
} // namespace encore::upstream

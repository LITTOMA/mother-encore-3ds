#include "encore/field_character_load.hpp"
#include "encore/field_node_sort.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *v) {
  e = v;
  return false;
}
bool reference(const FieldCharacterOwnedReference &r, FieldGlobalRegistry *g) {
  if (r.registry != g || !r.object || !r.actual_owner)
    return false;
  auto native = g->native_reference(r.object);
  return native && !native.owner_before(r.actual_owner) &&
         !r.actual_owner.owner_before(native);
}
bool same_state(const FieldCharacterLoadState &a,
                const FieldCharacterLoadState &b) {
  if (a.object != b.object || a.declaration != b.declaration ||
      a.role != b.role || a.name != b.name || a.nickname != b.nickname ||
      a.level != b.level || a.exp != b.exp || a.hp != b.hp || a.pp != b.pp ||
      a.stats != b.stats || a.permanent != b.permanent ||
      a.permanent_fields != b.permanent_fields || a.skills != b.skills ||
      a.affinities != b.affinities || a.status != b.status ||
      a.inventory.object != b.inventory.object ||
      a.items.size() != b.items.size() ||
      a.enemy_skills.size() != b.enemy_skills.size() ||
      a.untargetable != b.untargetable)
    return false;
  for (size_t i = 0; i < a.items.size(); ++i) {
    const auto &x = a.items[i];
    const auto &y = b.items[i];
    if (x.object != y.object || x.owner != y.owner ||
        x.value.definition != y.value.definition ||
        x.value.uid != y.value.uid || x.value.doses != y.value.doses ||
        x.value.equipped != y.value.equipped)
      return false;
  }
  for (size_t i = 0; i < a.enemy_skills.size(); ++i)
    if (a.enemy_skills[i].object != b.enemy_skills[i].object)
      return false;
  return true;
}
struct SkillComparator {
  const std::vector<std::string> *order = nullptr;
  bool operator()(const std::string &a, const std::string &b) const {
    auto x = std::find(order->begin(), order->end(), a),
         y = std::find(order->begin(), order->end(), b);
    if (x == order->end())
      return false;
    if (y == order->end())
      return true;
    return x < y;
  }
};
} // namespace
bool FieldCharacterLoadRuntime::initialize(
    const FieldCharacterLoadData &d, const FieldGlobalDataData &owner_data,
    const FieldGlobalDataRuntime &owner, FieldGlobalRegistry &registry,
    GlobalYamlCachesRuntime &caches, GlobalItemCache &items, SourceRandom &rng,
    std::vector<uint32_t> &ledger, LoadRngClockProvider clock,
    FieldCharacterLoadHost host, std::string &e) {
  if (data_ || !d.valid() || !owner_data.valid() ||
      !owner.character_load_bound_to(d) || !owner.globaldata_object() ||
      !owner.god_storage_complete() ||
      caches.owner() != owner.globaldata_object() ||
      items.owner() != owner.globaldata_object() ||
      !caches.init_caches_complete() || !items.directory_admitted() ||
      !items.definitions() || !items.definitions()->valid() ||
      !items.definitions()->global_constructor_scope() ||
      d.identity().upstream_commit != items.definitions()->source_pin() ||
      !host.read || !host.publish || !host.new_inventory || !host.new_item ||
      !host.new_enemy_skill || !host.stat_changed || !clock)
    return fail(
        e, "Character LOAD actual cache/owner/factory prerequisites rejected");
  std::array<uint8_t, 32> a{}, b{};
  for (const auto &p : items.definitions()->constructor_sources())
    if (!d.source_hash(p, a) || !items.definitions()->source_hash(p, b) ||
        a != b)
      return fail(e, "Character LOAD Item constructor source mismatch");
  for (const auto &decl : owner_data.declarations()) {
    if (decl.kind == 1 && (!d.source_hash(decl.script, a) ||
                           !owner_data.source_hash(decl.script, b) || a != b))
      return fail(e, "Character LOAD declaration script source mismatch");
  }
  size_t i = 0;
  std::set<FieldObjectId> identities;
  std::vector<FieldCharacterLoadState> cold;
  for (const auto &decl : owner_data.declarations()) {
    if (decl.kind != 1)
      continue;
    if (i >= d.rows().size())
      return fail(e, "Character LOAD omitted existing dictionary owner");
    const auto &row = d.rows()[i++];
    if (row.role == 0 && row.hp <= 0)
      return fail(e, "Cold Character LOAD requires a real unconscious Status "
                     "owner for zero HP");
    FieldCharacterLoadState state;
    FieldCharacterLoadState live;
    FieldGlobalDataObject actual;
    if (row.id != decl.id || row.role != decl.role || row.name != decl.name ||
        !host.read(row.id, state, e) ||
        !owner.read_character_load(row.id, live, e) ||
        !same_state(state, live) ||
        !owner.read_constructed_object(state.object, actual, e) ||
        actual.declaration != row.id || actual.kind != 1 ||
        actual.role != row.role || state.declaration != row.id ||
        state.role != row.role || !registry.object_exists(state.object) ||
        !identities.insert(state.object).second || state.level != 0 ||
        state.exp != 0 || state.hp != 0 || state.pp != 0 ||
        !state.name.empty() || state.stats.size() != d.stats().size() ||
        state.permanent.size() != d.stats().size() ||
        std::any_of(state.stats.begin(), state.stats.end(),
                    [](int64_t v) { return v != 0; }) ||
        std::any_of(state.permanent.begin(), state.permanent.end(),
                    [](int64_t v) { return v != 0; }) ||
        !state.status.empty() || state.inventory.object ||
        !state.items.empty() || !state.skills.empty() ||
        !state.enemy_skills.empty() || !state.affinities.empty())
      return fail(e, "Character LOAD requires actual uninitialized cold "
                     "declaration fields");
    for (const auto &item : row.items)
      if (!items.definitions()->definition(item.name))
        return fail(e, "Character LOAD references foreign Item definition");
    cold.push_back(std::move(state));
  }
  if (i != d.rows().size())
    return fail(e, "Character LOAD foreign owner coverage");
  data_ = &d;
  owner_ = &owner;
  registry_ = &registry;
  caches_ = &caches;
  items_ = &items;
  random_ = &rng;
  ledger_ = &ledger;
  clock_ = std::move(clock);
  host_ = std::move(host);
  cold_ = std::move(cold);
  return true;
}
bool FieldCharacterLoadRuntime::publish(FieldCharacterLoadState &s,
                                        std::string &e) {
  FieldCharacterLoadState live;
  if (!host_.publish(s, e) ||
      !owner_->read_character_load(s.declaration, live, e) ||
      !same_state(s, live)) {
    poisoned_ = true;
    return fail(e,
                "Character LOAD publication did not mutate the same live body");
  }
  s = std::move(live);
  return true;
}
bool FieldCharacterLoadRuntime::refresh(FieldCharacterLoadState &s,
                                        std::string &e) {
  FieldCharacterLoadState observed, actual;
  if (!host_.read(s.declaration, observed, e) ||
      !owner_->read_character_load(s.declaration, actual, e) ||
      !same_state(observed, actual)) {
    poisoned_ = true;
    return fail(e, "Character synchronous signal lost its live source owner");
  }
  s = std::move(actual);
  return true;
}
bool FieldCharacterLoadRuntime::maximum(FieldCharacterLoadState &s, bool hp,
                                        int64_t &out, std::string &e) {
  size_t k = hp ? 0 : 1;
  out = s.stats[k];
  int64_t boost = s.permanent[k];
  // Source get_equipment_boosts queries the first equipped Item per source
  // slot.
  for (const auto &slot : data_->slots()) {
    for (const auto &r : s.items) {
      if (!r.value.equipped)
        continue;
      auto *d = items_->get_item_data(r.value.definition);
      if (!d)
        return fail(e, "Character max stat actual cache getter rejected");
      if (d->slot == slot) {
        boost += d->boost[k];
        break;
      }
    }
  }
  out += boost;
  if (out < 0 || out > 2147483647)
    return fail(e, "Character max stat conversion bound rejected");
  return true;
}
bool FieldCharacterLoadRuntime::set_current(FieldCharacterLoadState &s, bool hp,
                                            int64_t value, std::string &e) {
  auto &current = hp ? s.hp : s.pp;
  auto old = current;
  if (value == old)
    return true;
  int64_t max = 0;
  if (!maximum(s, hp, max, e)) {
    poisoned_ = true;
    return false;
  }
  if (hp && (value <= 0 || max <= 0)) {
    poisoned_ = true;
    return fail(e,
                "Character HP setter needs actual unconscious Status consumer");
  }
  current = std::clamp<int64_t>(value, 0, max);
  s.write = hp ? FieldCharacterLoadWrite::Hp : FieldCharacterLoadWrite::Pp;
  if (!publish(s, e))
    return false;
  // Empty cold source Status makes PartyMember conscious guard and refresh a
  // no-op. Signal value is the requested value, not its clamped storage value.
  if (!host_.stat_changed(s.object, hp ? data_->hp() : data_->pp(), value, max,
                          e)) {
    poisoned_ = true;
    return false;
  }
  return refresh(s, e);
}
bool FieldCharacterLoadRuntime::set_stat(FieldCharacterLoadState &s, size_t k,
                                         int64_t value, std::string &e) {
  auto difference = value - s.stats[k];
  if (!difference)
    return true;
  s.stats[k] = value;
  s.write = FieldCharacterLoadWrite::Stat;
  s.stat_index = k;
  if (!publish(s, e))
    return false;
  if (difference > 0 && k < 2)
    if (!set_current(s, k == 0, (k == 0 ? s.hp : s.pp) + difference, e))
      return false;
  if (!host_.stat_changed(s.object, data_->stats()[k], value, 0, e)) {
    poisoned_ = true;
    return false;
  }
  return refresh(s, e);
}
bool FieldCharacterLoadRuntime::load_cold_default(std::string &e) {
  if (!data_ || complete_ || poisoned_ || !caches_->init_caches_complete() ||
      !items_->directory_admitted())
    return fail(e, "Character LOAD cursor unavailable/already consumed");
  // Fail before the first constructor if an actual owner was mutated since
  // bind.
  for (const auto &s : cold_) {
    FieldCharacterLoadState now;
    FieldCharacterLoadState live;
    if (!host_.read(s.declaration, now, e) || now.object != s.object ||
        !owner_->read_character_load(s.declaration, live, e) ||
        !same_state(now, live) || now.level || now.exp || now.hp || now.pp ||
        now.inventory.object || !now.name.empty() || !now.status.empty() ||
        !now.items.empty() || !now.skills.empty() ||
        !now.enemy_skills.empty() || now.stats != s.stats ||
        now.permanent != s.permanent || !now.affinities.empty())
      return fail(
          e,
          "Character cold LOAD ownership/state changed before source cursor");
  }
  for (size_t n = 0; n < data_->rows().size(); ++n) {
    const auto &row = data_->rows()[n];
    auto s = cold_[n];
    if (row.role == 0) {
      FieldCharacterOwnedReference inv;
      if (!host_.new_inventory(row.id, inv, e) || !reference(inv, registry_)) {
        poisoned_ = true;
        return fail(e, "Character Inventory actual Reference factory rejected");
      }
      // Inventory._init builds its local inv_content, then publishes _items and
      // the caller assigns _inventory only once its constructor has returned.
      std::vector<FieldGlobalDataItemReference> loaded;
      for (const auto &saved : row.items) {
        std::vector<LoadUidAllocation> trace;
        if (!apply_load_uid_allocations(*random_, *ledger_, {{row.id, 1}},
                                        clock_, e, &trace)) {
          poisoned_ = true;
          return false;
        }
        auto *d = items_->definitions()->definition(saved.name);
        FieldOwnedItem value{
            d->id, saved.has_uid ? saved.uid : trace.front().generated_uid,
            saved.doses, saved.equipped};
        FieldGlobalDataItemReference r;
        FieldOwnedItem actual_item;
        if (!host_.new_item(inv.object, value, r, e) ||
            r.registry != registry_ || !r.object || !r.actual_owner ||
            !r.source_owner || !r.source_owner->read_item(actual_item, e) ||
            actual_item.definition != value.definition ||
            actual_item.uid != value.uid || actual_item.doses != value.doses ||
            actual_item.equipped != value.equipped ||
            !registry_->object_exists(r.object) || r.owner != row.id ||
            r.value.definition != value.definition ||
            r.value.uid != value.uid || r.value.doses != value.doses ||
            r.value.equipped != value.equipped) {
          poisoned_ = true;
          return fail(e,
                      "Character Item actual Reference factory/value rejected");
        }
        loaded.push_back(std::move(r));
      }
      s.inventory = std::move(inv);
      s.items = std::move(loaded);
      s.write = FieldCharacterLoadWrite::Inventory;
      if (!publish(s, e))
        return false;
      s.name = row.display_name;
      s.write = FieldCharacterLoadWrite::Name;
      if (!publish(s, e))
        return false;
      auto cappedlevel =
          std::min<int64_t>(row.level, data_->exp_floors().size());
      s.exp = std::min(
          std::max(row.exp, data_->exp_floors()[size_t(cappedlevel - 1)]),
          data_->exp_floors().back());
      s.write = FieldCharacterLoadWrite::Exp;
      if (!publish(s, e))
        return false;
      int64_t level = 1;
      while (size_t(level) < data_->exp_floors().size() &&
             s.exp >= data_->exp_floors()[size_t(level)])
        ++level;
      s.level = level;
      s.write = FieldCharacterLoadWrite::Level;
      if (!publish(s, e))
        return false;
      for (size_t k = 0; k < row.targets.size(); ++k) {
        const auto &t = row.targets[k];
        auto tens = std::min<size_t>(size_t(s.level / 10), t.size() - 2);
        auto units = s.level - int64_t(tens) * 10;
        // GDScript scalar lerp calls the double overload (3.6.2 functions.cpp);
        // Character typed int conversion truncates only after that calculation.
        double value =
            double(t[tens]) +
            (double(t[tens + 1]) - double(t[tens])) * (double(units) / 10.0);
        if (!std::isfinite(value) || double(value) < 0 ||
            double(value) > 2147483647) {
          poisoned_ = true;
          return fail(e, "Character source lerp integer conversion rejected");
        }
        if (!set_stat(s, k, int64_t(value), e))
          return false;
      }
      // _status_init_from_dict assigns the actual empty source array, without
      // status_changed or HP refresh; never constructs fictitious Status nodes.
      s.status.clear();
      s.write = FieldCharacterLoadWrite::Status;
      if (!publish(s, e))
        return false;
      s.nickname = row.nickname;
      s.write = FieldCharacterLoadWrite::Nickname;
      if (!publish(s, e))
        return false;
      s.skills = row.skills;
      s.write = FieldCharacterLoadWrite::LearnedSkills;
      if (!publish(s, e))
        return false;
      s.permanent = row.permanent;
      s.permanent_fields = row.permanent_fields;
      s.write = FieldCharacterLoadWrite::PermanentBoosts;
      if (!publish(s, e))
        return false;
      s.affinities = row.affinities;
      s.write = FieldCharacterLoadWrite::Affinities;
      if (!publish(s, e))
        return false;
      if (!set_current(s, true, row.hp, e) || !set_current(s, false, row.pp, e))
        return false;
      FieldNodeSort<std::string, SkillComparator> sort;
      sort.compare.order = &data_->skills_order();
      sort.sort(s.skills.data(), int(s.skills.size()));
      s.write = FieldCharacterLoadWrite::SortSkills;
      if (!publish(s, e))
        return false;
    } else {
      s.name = row.display_name;
      s.write = FieldCharacterLoadWrite::Name;
      if (!publish(s, e))
        return false;
      s.level = row.level;
      s.write = FieldCharacterLoadWrite::Level;
      if (!publish(s, e))
        return false;
      s.exp = row.exp;
      s.write = FieldCharacterLoadWrite::Exp;
      if (!publish(s, e))
        return false;
      s.hp = row.hp;
      s.write = FieldCharacterLoadWrite::Hp;
      if (!publish(s, e))
        return false;
      // Character.init_from_dict places maxHP before PP, then maxPP/stat
      // fields. These are direct assignments, not the PartyMember setters.
      s.stats[0] = row.npc_stats[0];
      s.write = FieldCharacterLoadWrite::Stat;
      s.stat_index = 0;
      // Publish PP only at its own source cursor after maxHP.
      s.pp = cold_[n].pp;
      if (!publish(s, e))
        return false;
      s.pp = row.pp;
      s.write = FieldCharacterLoadWrite::Pp;
      if (!publish(s, e))
        return false;
      for (size_t k = 1; k < row.npc_stats.size(); ++k) {
        s.stats[k] = row.npc_stats[k];
        s.stat_index = k;
        s.write = FieldCharacterLoadWrite::Stat;
        if (!publish(s, e))
          return false;
      }
      s.status.clear();
      s.write = FieldCharacterLoadWrite::Status;
      if (!publish(s, e))
        return false;
      s.untargetable = row.untargetable;
      s.write = FieldCharacterLoadWrite::Untargetable;
      if (!publish(s, e))
        return false;
      s.enemy_skills.clear();
      s.write = FieldCharacterLoadWrite::NpcSkillsReset;
      if (!publish(s, e))
        return false;
      for (const auto &skill : row.npc_skills) {
        std::shared_ptr<GlobalYamlValue> exists;
        if (!caches_->call(data_->npc_skill_getter(), {skill.skill}, exists,
                           e) ||
            !exists || exists->kind != 1) {
          poisoned_ = true;
          return false;
        }
        if (!exists->boolean)
          continue; // Explicit original source membership guard.
        FieldCharacterOwnedReference r;
        if (!host_.new_enemy_skill(skill, r, e) || !reference(r, registry_)) {
          poisoned_ = true;
          return fail(e,
                      "Character EnemySkill actual Reference factory rejected");
        }
        FieldCharacterEnemySkill actual;
        int64_t remaining = -1;
        const auto &bindings = data_->source_bindings();
        auto initial = std::find_if(
            bindings.enemy_skill_defaults.begin(),
            bindings.enemy_skill_defaults.end(), [&](const auto &v) {
              return v.name == bindings.enemy_remaining_field && v.kind == 2;
            });
        if (!r.enemy_skill_owner ||
            initial == bindings.enemy_skill_defaults.end() ||
            !r.enemy_skill_owner->read_skill(actual, remaining, e) ||
            actual.skill != skill.skill || actual.weight != skill.weight ||
            actual.cooldown != skill.cooldown ||
            remaining != initial->integer_value) {
          poisoned_ = true;
          return fail(
              e,
              "EnemySkill factory did not initialize its actual source body");
        }
        s.enemy_skills.push_back(std::move(r));
        s.write = FieldCharacterLoadWrite::NpcSkillAppend;
        if (!publish(s, e))
          return false;
      }
    }
    cold_[n] = std::move(s);
  }
  complete_ = true;
  return true;
}
} // namespace encore::upstream

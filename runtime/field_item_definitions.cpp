#include "encore/field_item_definitions.hpp"
#include <algorithm>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
FieldItemInventory *owner(FieldItemSnapshot &s, uint32_t id) {
  for (auto &r : s.inventories)
    if (r.owner == id)
      return &r;
  return nullptr;
}
bool full(const FieldItemDefinitions &d, const FieldItemInventory &r) {
  return !d.unbounded(r.role) && r.items.size() >= d.capacity(r.role);
}
} // namespace
bool FieldItemDefinitionsRuntime::initialize(const FieldItemDefinitions &d,
                                             SourceRandom &r,
                                             std::vector<uint32_t> &ledger,
                                             LoadRngClockProvider clock,
                                             FieldItemDefinitionsHost h,
                                             std::string &e) {
  if (!d.valid() || d.global_constructor_scope() || !clock || !h.bind || !h.read || !h.commit)
    return fail(e, "Field items complete source host/clock required");
  std::set<uint32_t> ids;
  for (auto uid : ledger)
    if (!ids.insert(uid).second)
      return fail(e, "Generated-only UID ledger contains duplicate draws");
  if (!h.bind(d, e))
    return false;
  data_ = &d;
  random_ = &r;
  ledger_ = &ledger;
  clock_ = std::move(clock);
  host_ = std::move(h);
  FieldItemSnapshot state;
  if (!read(state, e)) {
    data_ = nullptr;
    random_ = nullptr;
    ledger_ = nullptr;
    return false;
  }
  e.clear();
  return true;
}
bool FieldItemDefinitionsRuntime::validate_snapshot(const FieldItemSnapshot &s,
                                                    std::string &e) const {
  if (!data_ || !data_->valid() || s.inventories.size() > 1024 ||
      s.party_order.size() > 1024)
    return fail(e, "Field item snapshot scope rejected");
  std::set<uint32_t> owners, uids;
  unsigned keys = 0, storage = 0;
  for (const auto &r : s.inventories) {
    if (!r.owner || !owners.insert(r.owner).second || r.role > 2 ||
        r.items.size() > 65535 ||
        (!data_->unbounded(r.role) && r.items.size() > data_->capacity(r.role)))
      return fail(e, "Field item inventory owner/type/capacity rejected");
    keys += r.role == 1;
    storage += r.role == 2;
    for (const auto &i : r.items) {
      const auto *d = data_->definition(i.definition);
      if (!d || !uids.insert(i.uid).second || !i.doses || i.doses > d->doses ||
          (i.equipped && d->slot.empty()))
        return fail(e, "Field item definition/UID/doses/equipment rejected");
    }
  }
  if (keys != 1 || storage > 1)
    return fail(e, "Field item key/storage ownership coverage rejected");
  std::set<uint32_t> party;
  for (auto id : s.party_order) {
    auto r = std::find_if(s.inventories.begin(), s.inventories.end(),
                          [&](const auto &row) { return row.owner == id; });
    if (!party.insert(id).second || r == s.inventories.end() || r->role != 0)
      return fail(e, "Field item actual party order rejected");
  }
  e.clear();
  return true;
}
bool FieldItemDefinitionsRuntime::read(FieldItemSnapshot &s,
                                       std::string &e) const {
  if (!data_ || !host_.read)
    return fail(e, "Field item source host unbound");
  return host_.read(s, e) && validate_snapshot(s, e);
}
bool FieldItemDefinitionsRuntime::inventory_space(bool &space,
                                                  std::string &e) const {
  space = false;
  FieldItemSnapshot s;
  if (!read(s, e))
    return false;
  for (auto id : s.party_order) {
    auto r = owner(s, id);
    if (r && !full(*data_, *r)) {
      space = true;
      break;
    }
  }
  e.clear();
  return true;
}
bool FieldItemDefinitionsRuntime::construct(const FieldItemBinding &b,
                                            bool give, bool full_transient,
                                            FieldItemResult &result,
                                            std::string &e) {
  result = {};
  const auto *d = data_ ? data_->definition(b.definition) : nullptr;
  if (!d || !random_ || !ledger_)
    return fail(e, "Field item source construction identity rejected");
  FieldItemSnapshot before;
  if (!read(before, e))
    return false;
  auto after = before;
  FieldItemInventory *target = nullptr;
  if (give) {
    if (d->keyitem()) {
      for (auto &r : after.inventories)
        if (r.role == 1) {
          target = &r;
          break;
        }
    } else
      for (auto id : after.party_order) {
        auto r = owner(after, id);
        if (r && !full(*data_, *r)) {
          target = r;
          break;
        }
      }
    if (!target)
      return fail(
          e, "Source Inventory.add_item_available found no available owner");
  } else if (full_transient) {
    if (d->keyitem())
      return fail(e,
                  "Source key item cannot enter full-inventory holder branch");
    for (auto id : after.party_order) {
      auto r = owner(after, id);
      if (r && !full(*data_, *r))
        return fail(e,
                    "Source full-holder transient branch has inventory space");
    }
  }
  // Native Reference allocation precedes Item._init's default UID expression.
  // Keep its owning token local so every failed candidate releases the slot.
  std::shared_ptr<void> construction;
  if (construction_.reserve) {
    if (!construction_.reserve(b, target ? target->owner : 0, construction, e))
      return false;
    if (!construction)
      return fail(e, "Actual Item reservation missing");
  }
  SourceRandom next = *random_;
  auto ledger = *ledger_;
  std::vector<LoadUidAllocation> trace;
  if (!apply_load_uid_allocations(next, ledger, {{b.object_id, 1}}, clock_, e,
                                  &trace) ||
      trace.size() != 1)
    return false;
  FieldItemResult r;
  r.kind = give ? FieldItemResultKind::Owned : FieldItemResultKind::Transient;
  r.item = {d->id, trace[0].generated_uid, d->doses, false};
  r.owner = target ? target->owner : 0;
  r.seed = trace[0].seed;
  r.raw_draws = trace[0].raw_draws;
  if (target) {
    target->items.push_back(r.item);
    if (!validate_snapshot(after, e)) {
      e = "Source generated UID conflicts with preserved saved identity or "
          "scoped inventory: " +
          e;
      return false;
    }
  }
  if (construction_.finish &&
      !construction_.finish(b, r.item, construction, e))
    return false;
  if (!host_.commit(before, after, r, e))
    return false;
  *random_ = next;
  ledger_->swap(ledger);
  result = r;
  e.clear();
  return true;
}
bool FieldItemDefinitionsRuntime::bind_source_construction(
    FieldItemConstructionHost h, std::string &e) {
  if (!data_ || construction_.reserve || construction_.finish || !h.reserve ||
      !h.finish)
    return fail(e, "Actual Item constructor missing/already bound");
  construction_ = std::move(h);
  e.clear();
  return true;
}
bool FieldItemDefinitionsRuntime::select_holder(FieldItemBindingKind kind,
                                                const std::string &scene,
                                                uint32_t id, bool give,
                                                FieldItemResult &result,
                                                std::string &e) {
  auto b = data_ ? data_->binding(kind, scene, id) : nullptr;
  if (!b ||
      (kind != FieldItemBindingKind::Present &&
       kind != FieldItemBindingKind::Dropped) ||
      b->operation != 1)
    return fail(e, "Field item holder source binding rejected");
  return construct(*b, give, !give, result, e);
}
bool FieldItemDefinitionsRuntime::grant_programme(const std::string &program,
                                                  const std::string &label,
                                                  FieldItemResult &result,
                                                  std::string &e) {
  auto b = data_ ? data_->programme(program, label) : nullptr;
  if (!b || b->operation != 4)
    return fail(e, "Field item programme source binding rejected");
  return construct(*b, true, false, result, e);
}
bool FieldItemDefinitionsRuntime::query(FieldItemBindingKind kind,
                                        const std::string &scene, uint32_t id,
                                        bool &found, FieldItemResult &result,
                                        std::string &e) const {
  found = false;
  result = {};
  auto b = data_ ? data_->binding(kind, scene, id) : nullptr;
  if (!b || (kind != FieldItemBindingKind::Door &&
             kind != FieldItemBindingKind::Payphone))
    return fail(e, "Field item query source binding rejected");
  FieldItemSnapshot s;
  if (!read(s, e))
    return false;
  if (!b->definition) {
    e.clear();
    return true;
  }
  std::vector<FieldItemInventory *> ordered;
  for (auto id : s.party_order)
    ordered.push_back(owner(s, id));
  for (auto &r : s.inventories)
    if (r.role == 1)
      ordered.push_back(&r);
  for (auto *r : ordered)
    for (const auto &i : r->items)
      if (i.definition == b->definition) {
        found = true;
        result.kind = FieldItemResultKind::Owned;
        result.item = i;
        result.owner = r->owner;
        e.clear();
        return true;
      }
  e.clear();
  return true;
}
bool FieldItemDefinitionsRuntime::remove(uint32_t uid, bool dose,
                                         FieldItemResult &result,
                                         std::string &e) {
  result = {};
  FieldItemSnapshot before;
  if (!read(before, e))
    return false;
  auto after = before;
  std::vector<FieldItemInventory *> ordered;
  for (auto &r : after.inventories)
    if (r.role == 1)
      ordered.push_back(&r);
  for (auto &r : after.inventories)
    if (r.role == 0)
      ordered.push_back(&r);
  for (auto *r : ordered) {
    auto i = std::find_if(r->items.begin(), r->items.end(),
                          [&](const auto &i) { return i.uid == uid; });
    if (i == r->items.end())
      continue;
    FieldItemResult res;
    res.owner = res.previous_owner = r->owner;
    if (dose && i->doses > data_->dose_drop_threshold()) {
      i->doses -= data_->dose_step();
      res.kind = FieldItemResultKind::Reduced;
      res.item = *i;
    } else {
      res.kind = FieldItemResultKind::Dropped;
      res.item = *i;
      res.item.equipped = false;
      r->items.erase(i);
    }
    if (!validate_snapshot(after, e) || !host_.commit(before, after, res, e))
      return false;
    result = res;
    e.clear();
    return true;
  }
  return fail(
      e, "Source get_item_owner rejected missing/non-character/storage UID");
}
bool FieldItemDefinitionsRuntime::reduce_or_drop(uint32_t uid,
                                                 FieldItemResult &r,
                                                 std::string &e) {
  return remove(uid, true, r, e);
}
bool FieldItemDefinitionsRuntime::drop(uint32_t uid, FieldItemResult &r,
                                       std::string &e) {
  return remove(uid, false, r, e);
}
bool FieldItemDefinitionsRuntime::transfer(uint32_t uid, uint32_t target_id,
                                           FieldItemResult &result,
                                           std::string &e) {
  result = {};
  FieldItemSnapshot before;
  if (!read(before, e))
    return false;
  auto after = before;
  auto target = owner(after, target_id);
  if (!target)
    return fail(e, "Source transfer target inventory missing");
  for (auto &source : after.inventories) {
    auto i = std::find_if(source.items.begin(), source.items.end(),
                          [&](const auto &i) { return i.uid == uid; });
    if (i == source.items.end())
      continue;
    if (source.owner == target->owner) {
      e.clear();
      return true;
    }
    FieldItemResult res;
    res.item = *i;
    res.item.equipped = false;
    res.previous_owner = source.owner;
    source.items.erase(i);
    if (full(*data_, *target)) {
      res.kind = FieldItemResultKind::TransferDropped;
      res.owner = 0;
    } else {
      target->items.push_back(res.item);
      res.kind = FieldItemResultKind::Transferred;
      res.owner = target->owner;
    }
    if (!validate_snapshot(after, e) || !host_.commit(before, after, res, e))
      return false;
    result = res;
    e.clear();
    return true;
  }
  return fail(e, "Source transfer item UID not owned");
}
} // namespace encore::upstream

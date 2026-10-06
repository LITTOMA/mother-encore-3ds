#include "encore/field_inventory.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
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
bool same(const FieldItemSnapshot &a, const FieldItemSnapshot &b) {
  if (a.party_order != b.party_order ||
      a.inventories.size() != b.inventories.size())
    return false;
  for (size_t n = 0; n < a.inventories.size(); ++n) {
    const auto &x = a.inventories[n];
    const auto &y = b.inventories[n];
    if (x.owner != y.owner || x.role != y.role ||
        x.items.size() != y.items.size())
      return false;
    for (size_t k = 0; k < x.items.size(); ++k)
      if (!same(x.items[k], y.items[k]))
        return false;
  }
  return true;
}
FieldItemInventory *inventory(FieldItemSnapshot &s, uint32_t id) {
  for (auto &r : s.inventories)
    if (r.owner == id)
      return &r;
  return nullptr;
}
const FieldItemInventory *inventory(const FieldItemSnapshot &s, uint32_t id) {
  for (const auto &r : s.inventories)
    if (r.owner == id)
      return &r;
  return nullptr;
}
FieldOwnedItem *item(FieldItemSnapshot &s, uint32_t uid,
                     uint32_t *owner = nullptr) {
  for (auto &r : s.inventories)
    for (auto &i : r.items)
      if (i.uid == uid) {
        if (owner)
          *owner = r.owner;
        return &i;
      }
  return nullptr;
}
const FieldOwnedItem *item(const FieldItemSnapshot &s, uint32_t uid,
                           uint32_t *owner = nullptr) {
  for (const auto &r : s.inventories)
    for (const auto &i : r.items)
      if (i.uid == uid) {
        if (owner)
          *owner = r.owner;
        return &i;
      }
  return nullptr;
}
bool remove(FieldItemSnapshot &s, uint32_t uid,
            FieldOwnedItem *removed = nullptr) {
  for (auto &r : s.inventories)
    for (auto it = r.items.begin(); it != r.items.end(); ++it)
      if (it->uid == uid) {
        if (removed) {
          *removed = *it;
          removed->equipped = false;
        }
        r.items.erase(it);
        return true;
      }
  return false;
}
bool add(int64_t a, int64_t b, int64_t &out) {
  if ((b > 0 && a > std::numeric_limits<int64_t>::max() - b) ||
      (b < 0 && a < std::numeric_limits<int64_t>::min() - b))
    return false;
  out = a + b;
  return true;
}
bool full(const FieldItemDefinitions &d, const FieldItemInventory &i) {
  return !d.unbounded(i.role) && i.items.size() >= d.capacity(i.role);
}
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  for (size_t i = 0; i < n; ++i) {
    c ^= i >= 16 && i < 20 ? 0 : p[i];
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
uint32_t u32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
void put(std::vector<uint8_t> &b, uint32_t v) {
  for (unsigned n = 0; n < 4; ++n)
    b.push_back(uint8_t(v >> (8 * n)));
}
void putq(std::vector<uint8_t> &b, uint64_t v) {
  for (unsigned n = 0; n < 8; ++n)
    b.push_back(uint8_t(v >> (8 * n)));
}
void patch(std::vector<uint8_t> &b, size_t at, uint32_t v) {
  for (unsigned n = 0; n < 4; ++n)
    b[at + n] = uint8_t(v >> (8 * n));
}
void text(std::vector<uint8_t> &b, const std::string &s) {
  put(b, uint32_t(s.size()));
  b.insert(b.end(), s.begin(), s.end());
}
} // namespace
bool FieldInventoryRuntime::initialize(const FieldInventoryData &data,
                                       const FieldItemDefinitions &defs,
                                       const FieldInventoryState &s,
                                       std::string &e) {
  if (!data.bind_definitions(defs, e))
    return false;
  FieldInventoryRuntime candidate;
  candidate.data_ = &data;
  candidate.defs_ = &defs;
  if (!candidate.validate(s, e))
    return false;
  candidate.state_ = s;
  *this = std::move(candidate);
  e.clear();
  return true;
}
bool FieldInventoryRuntime::stats(const FieldInventoryState &s,
                                  std::array<int64_t, 7> &out,
                                  std::string &e) const {
  if (!data_ || !defs_ || !s.level || s.level > data_->levels().size())
    return fail(e, "Inventory level source domain rejected");
  auto base = data_->levels()[s.level - 1];
  for (size_t n = 0; n < out.size(); ++n)
    if (s.permanent[n] < 0 || !add(base[n], s.permanent[n], out[n]))
      return fail(e, "Inventory permanent stat overflow");
  auto *i = inventory(s.items, data_->role(0)->id);
  if (!i)
    return fail(e, "Inventory Ninten owner absent");
  for (const auto &v : i->items)
    if (v.equipped) {
      auto *d = defs_->definition(v.definition);
      if (!d)
        return fail(e, "Unknown equipment source definition");
      for (size_t n = 0; n < out.size(); ++n)
        if (!add(out[n], d->boost[n], out[n]) || out[n] < 0)
          return fail(e, "Inventory equipment stat overflow");
    }
  return true;
}
bool FieldInventoryRuntime::effective_stats(std::array<int64_t, 7> &out,
                                            std::string &e) const {
  return validate(state_, e) && stats(state_, out, e);
}
bool FieldInventoryRuntime::validate(const FieldInventoryState &s,
                                     std::string &e) const {
  if (!data_ || !defs_ ||
      s.items.inventories.size() != data_->owners().size() ||
      s.items.party_order != std::vector<uint32_t>{data_->role(0)->id} ||
      s.cash < 0 || s.hp < 0 || s.pp < 0 ||
      s.statuses.size() > data_->statuses().size())
    return fail(e, "Inventory scoped state/party/vitals rejected");
  std::set<uint32_t> owners, uids;
  std::set<std::string> equipped;
  for (const auto &r : s.items.inventories) {
    auto *o = data_->owner(r.owner);
    if (!o || o->role != r.role || !owners.insert(r.owner).second ||
        r.items.size() > 4096 ||
        (!defs_->unbounded(r.role) && r.items.size() > defs_->capacity(r.role)))
      return fail(e, "Inventory source owner/capacity rejected");
    for (const auto &i : r.items) {
      auto *d = defs_->definition(i.definition);
      auto *p = data_->policy(i.definition);
      if (!d || !p || !uids.insert(i.uid).second || !i.doses ||
          i.doses > d->doses || ((r.role == 1) != d->keyitem()))
        return fail(
            e, "Inventory source definition/UID/doses/key ownership rejected");
      if (i.equipped) {
        bool equip = std::any_of(
            p->actions.begin(), p->actions.end(), [](const auto &a) {
              return a.function == FieldItemFunction::Equip;
            });
        if (r.role != 0 || !p->use_allowed || !equip || d->slot.empty() ||
            std::find(data_->slots().begin(), data_->slots().end(), d->slot) ==
                data_->slots().end() ||
            !equipped.insert(d->slot).second)
          return fail(e, "Inventory source equipment slot/permission rejected");
      }
    }
  }
  int32_t priority = std::numeric_limits<int32_t>::max();
  std::set<std::string> statuses;
  for (const auto &srow : s.statuses) {
    auto *p = data_->status(srow.id);
    if (!p || !statuses.insert(srow.id).second || srow.passive_turns < 0 ||
        (!p->passive && srow.passive_turns) || p->priority > priority ||
        (p->exclusive && s.statuses.size() != 1) ||
        (p->unconscious && s.hp != 0))
      return fail(e, "Inventory checked status/persistence/order rejected");
    priority = p->priority;
  }
  std::array<int64_t, 7> values{};
  if (!stats(s, values, e))
    return false;
  if (s.hp > values[0] || s.pp > values[1])
    return fail(e, "Inventory saved HP/PP exceeds admitted source maximum");
  e.clear();
  return true;
}
FieldItemDefinitionsHost FieldInventoryRuntime::definitions_host() {
  FieldItemDefinitionsHost h;
  h.bind = [this](const FieldItemDefinitions &d, std::string &e) {
    return data_ && defs_ == &d && data_->bind_definitions(d, e);
  };
  h.read = [this](FieldItemSnapshot &s, std::string &e) {
    if (!validate(state_, e))
      return false;
    s = state_.items;
    return true;
  };
  h.commit = [this](const auto &b, const auto &a, const auto &r,
                    std::string &e) { return commit_definitions(b, a, r, e); };
  return h;
}
bool FieldInventoryRuntime::commit_definitions(const FieldItemSnapshot &before,
                                               const FieldItemSnapshot &after,
                                               const FieldItemResult &r,
                                               std::string &e) {
  if (!validate(state_, e) || !same(before, state_.items) ||
      r.kind == FieldItemResultKind::None ||
      !defs_->definition(r.item.definition))
    return fail(e, "Inventory definition commit before/operation rejected");
  auto expected = before;
  auto *d = defs_->definition(r.item.definition);
  if (r.kind == FieldItemResultKind::Owned) {
    auto *target = inventory(expected, r.owner);
    if (!target || full(*defs_, *target) || item(expected, r.item.uid) ||
        r.item.doses != d->doses || r.item.equipped || r.previous_owner ||
        !r.raw_draws || ((target->role == 1) != d->keyitem()))
      return fail(e, "Inventory source grant identity/owner rejected");
    target->items.push_back(r.item);
  } else if (r.kind == FieldItemResultKind::Transient) {
    if (r.owner || r.previous_owner || r.item.doses != d->doses ||
        r.item.equipped || !r.raw_draws)
      return fail(e, "Inventory source temporary item rejected");
  } else {
    uint32_t oldowner = 0;
    auto *old = item(expected, r.item.uid, &oldowner);
    if (!old || old->definition != r.item.definition || r.raw_draws || r.seed)
      return fail(e, "Inventory mutation source UID missing");
    if (r.kind == FieldItemResultKind::Reduced) {
      auto reduced = *old;
      if (reduced.doses <= defs_->dose_drop_threshold())
        return fail(e, "Inventory reduce crossed source drop boundary");
      reduced.doses -= defs_->dose_step();
      if (!same(reduced, r.item) || r.owner != oldowner ||
          r.previous_owner != oldowner)
        return fail(e, "Inventory source reduction result rejected");
      *old = reduced;
    } else if (r.kind == FieldItemResultKind::Dropped) {
      FieldOwnedItem removed;
      remove(expected, r.item.uid, &removed);
      if (!same(removed, r.item) || r.owner != oldowner ||
          r.previous_owner != oldowner)
        return fail(e, "Inventory source drop result rejected");
    } else if (r.kind == FieldItemResultKind::Transferred ||
               r.kind == FieldItemResultKind::TransferDropped) {
      if (r.previous_owner != oldowner || r.owner == oldowner)
        return fail(e, "Inventory transfer owner rejected");
      FieldOwnedItem removed;
      remove(expected, r.item.uid, &removed);
      if (!same(removed, r.item))
        return fail(e, "Inventory transfer retained object mismatch");
      if (r.kind == FieldItemResultKind::TransferDropped) {
        if (r.owner ||
            !std::any_of(expected.inventories.begin(),
                         expected.inventories.end(), [&](const auto &v) {
                           return v.owner != oldowner &&
                                  ((v.role == 1) == d->keyitem()) &&
                                  full(*defs_, v);
                         }))
          return fail(e, "Inventory source transfer-full target unavailable");
      } else {
        auto *target = inventory(expected, r.owner);
        if (!target || full(*defs_, *target) ||
            ((target->role == 1) != d->keyitem()))
          return fail(e, "Inventory transfer target/capacity rejected");
        target->items.push_back(removed);
      }
    } else
      return fail(e, "Unknown typed field inventory result");
  }
  if (!same(expected, after))
    return fail(e,
                "Inventory definition callback did not match typed operation");
  auto next = state_;
  next.items = after;
  if (!validate(next, e) ||
      next.revision == std::numeric_limits<uint64_t>::max())
    return fail(e, "Inventory commit revision rejected");
  ++next.revision;
  state_ = std::move(next);
  context_ = r.item;
  has_context_ = true;
  e.clear();
  return true;
}
FieldShopHost FieldInventoryRuntime::shop_host(
    const FieldShopData &shop,
    std::function<bool(const std::string &, std::string &)> sound,
    std::function<bool(const std::string &, std::string &)> close) {
  FieldShopHost h;
  h.bind = [this, &shop](const FieldShopData &d,
                         const FieldItemDefinitions &items, std::string &e) {
    if (&d != &shop || !d.valid() || defs_ != &items || !data_ ||
        d.source_pin() != data_->source_pin())
      return fail(e, "Inventory shop source binding rejected");
    for (const auto &p : d.policies()) {
      auto *def = defs_->definition(p.id);
      std::array<uint8_t, 32> hash{};
      if (!def || def->source != p.source || def->doses != p.doses ||
          def->cost != p.cost || def->value != p.value ||
          def->keyitem() != p.key || def->slot != p.slot ||
          !defs_->source_hash(p.source, hash) || hash != p.source_sha)
        return fail(e, "Inventory shop item policy mismatch");
    }
    return data_->bind_definitions(items, e);
  };
  h.read = [this](FieldShopSnapshot &s, std::string &e) {
    if (!validate(state_, e))
      return false;
    s.items = state_.items;
    s.cash = state_.cash;
    s.natural_order = state_.items.party_order;
    s.revision = state_.revision;
    return true;
  };
  h.commit = [this, &shop](const auto &b, const auto &a, const auto &r,
                           std::string &e) {
    return commit_shop(shop, b, a, r, e);
  };
  h.sound = std::move(sound);
  h.close = std::move(close);
  return h;
}
bool FieldInventoryRuntime::bind_payphone_inventory(
    const FieldPayphoneData &phone, FieldPayphoneHost &host, std::string &e) {
  if (!validate(state_, e) || !phone.valid() ||
      phone.source_pin() != data_->source_pin() || !host.bind)
    return fail(e, "Inventory payphone concrete source host unbound");
  auto *card = defs_->definition(phone.card_name());
  std::array<uint8_t, 32> hash{};
  if (!card || card->keyitem() || card->doses != phone.card_max_doses() ||
      phone.card_step() != defs_->dose_step() ||
      !phone.source_hash(card->source, hash) ||
      !defs_->source_hash(card->source, hash))
    return fail(e, "Inventory phone-card source policy mismatch");
  std::array<uint8_t, 32> phone_hash{};
  if (!phone.source_hash(card->source, phone_hash) || hash != phone_hash)
    return fail(e, "Inventory phone-card YAML identity mismatch");
  const auto original_bind = host.bind;
  const uint32_t definition = card->id;
  auto candidate = host;
  candidate.bind = [this, &phone, original_bind](const FieldPayphoneData &d,
                                                 std::string &error) {
    if (&d != &phone || !validate(state_, error) || !original_bind(d, error))
      return false;
    return true;
  };
  auto lookup = [this, definition](uint64_t uid, FieldPayphoneCard &out,
                                   std::string &error) {
    if (!validate(state_, error) || uid > std::numeric_limits<uint32_t>::max())
      return fail(error, "Inventory payphone UID outside source uint32");
    uint32_t owner = 0;
    auto *i = item(state_.items, uint32_t(uid), &owner);
    auto *row = inventory(state_.items, owner);
    if (!i || !row || row->role == 2)
      return fail(error, "Inventory payphone item not party/key owned");
    auto *d = defs_->definition(i->definition);
    out = {i->uid, i->doses, d->item_name};
    if (i->definition == definition && !i->doses)
      return fail(error, "Inventory phone-card has no actual doses");
    error.clear();
    return true;
  };
  candidate.lookup_card = lookup;
  candidate.find_card = [this, &phone, definition](std::string_view name,
                                                   FieldPayphoneCard &out,
                                                   std::string &error) {
    if (!validate(state_, error) || name != phone.card_name())
      return fail(error, "Inventory phone-card source name rejected");
    std::vector<uint32_t> owners = state_.items.party_order;
    owners.push_back(data_->role(1)->id);
    for (auto owner : owners)
      for (const auto &i : inventory(state_.items, owner)->items)
        if (i.definition == definition) {
          out = {i.uid, i.doses, phone.card_name()};
          error.clear();
          return true;
        }
    out = {};
    error.clear();
    return true;
  };
  candidate.reduce_or_drop = [this, &phone, definition](uint64_t uid,
                                                        uint32_t step,
                                                        std::string &error) {
    if (!validate(state_, error) ||
        uid > std::numeric_limits<uint32_t>::max() || step != phone.card_step())
      return fail(error,
                  "Inventory phone-card reduce source arguments rejected");
    uint32_t owner = 0;
    auto *i = item(state_.items, uint32_t(uid), &owner);
    auto *row = inventory(state_.items, owner);
    if (!i || !row || row->role == 2 || i->definition != definition)
      return fail(error, "Inventory phone-card owner/definition rejected");
    auto next = state_;
    auto *used = item(next.items, uint32_t(uid));
    if (used->doses > defs_->dose_drop_threshold())
      used->doses -= step;
    else
      remove(next.items, uint32_t(uid));
    if (!validate(next, error) ||
        next.revision == std::numeric_limits<uint64_t>::max())
      return false;
    ++next.revision;
    if (has_context_ && context_.uid == uid &&
        context_.definition == definition) {
      if (auto *remaining = item(next.items, uint32_t(uid)))
        context_ = *remaining;
      else
        context_.equipped = false;
    }
    state_ = std::move(next);
    error.clear();
    return true;
  };
  candidate.read_cash = [this](int64_t &cash, std::string &error) {
    if (!validate(state_, error))
      return false;
    cash = state_.cash;
    return true;
  };
  candidate.add_cash = [this, &phone, definition](int64_t delta,
                                                  std::string &error) {
    if (!validate(state_, error) || delta != -int64_t(phone.cash_cost()) ||
        state_.cash < phone.cash_cost() ||
        state_.revision == std::numeric_limits<uint64_t>::max())
      return fail(error, "Inventory payphone cash source debit rejected");
    for (const auto &row : state_.items.inventories)
      if (row.role != 2)
        for (const auto &i : row.items)
          if (i.definition == definition)
            return fail(
                error, "Inventory source cash call has an available PhoneCard");
    auto next = state_;
    next.cash += delta;
    ++next.revision;
    state_ = std::move(next);
    error.clear();
    return true;
  };
  host = std::move(candidate);
  e.clear();
  return true;
}
bool FieldInventoryRuntime::commit_shop(const FieldShopData &shop,
                                        const FieldShopSnapshot &before,
                                        const FieldShopSnapshot &after,
                                        const FieldShopResult &r,
                                        std::string &e) {
  if (!validate(state_, e) || !shop.valid() ||
      shop.source_pin() != data_->source_pin() ||
      before.revision != state_.revision || before.cash != state_.cash ||
      !same(before.items, state_.items) ||
      before.natural_order != state_.items.party_order)
    return fail(e, "Inventory shop before ownership/cash revision mismatch");
  auto expected = before;
  bool publish = false;
  if (r.action == FieldShopAction::Ready) {
    if (r.item.definition || r.context_item.definition || r.owner ||
        !r.last_purchased.empty())
      return fail(e, "Inventory shop Ready side effects rejected");
  } else if (r.action == FieldShopAction::Preview) {
    auto *p = shop.policy(r.item.definition);
    auto *inv = inventory(expected.items, r.owner);
    if (!p || !inv || inv->role != 0 || !same(r.item, r.context_item))
      return fail(e, "Inventory shop preview owner/context rejected");
    auto *owned = item(expected.items, r.item.uid);
    bool soldrow = owned && same(*owned, r.item);
    bool offer = std::find(shop.offers().begin(), shop.offers().end(), p->id) !=
                 shop.offers().end();
    if (!soldrow && (!offer || r.item.equipped || r.item.doses != p->doses))
      return fail(e, "Inventory shop preview not source offer/owned row");
    publish = true;
  } else if (r.action == FieldShopAction::Purchase) {
    auto *p = shop.policy(r.item.definition);
    auto *inv = inventory(expected.items, r.owner);
    if (!p || !inv || inv->role != 0 || p->key || !p->slot.empty() ||
        std::find(shop.offers().begin(), shop.offers().end(), p->id) ==
            shop.offers().end() ||
        full(*defs_, *inv) || item(expected.items, r.item.uid) ||
        r.item.equipped || r.item.doses != p->doses ||
        expected.cash < p->cost || r.context_item.definition != p->id ||
        r.context_item.doses != p->doses || r.context_item.equipped ||
        r.last_purchased != p->name)
      return fail(e, "Inventory source shop purchase rejected");
    inv->items.push_back(r.item);
    expected.cash -= p->cost;
    publish = true;
  } else if (r.action == FieldShopAction::Sale) {
    auto *p = shop.policy(r.item.definition);
    uint32_t owner = 0;
    auto *owned = item(expected.items, r.item.uid, &owner);
    if (!p || !owned || owner != r.owner || owner != data_->role(0)->id ||
        p->key || p->value <= 0 || r.item.equipped ||
        r.item.definition != owned->definition ||
        r.item.doses != owned->doses || !same(r.item, r.context_item) ||
        !r.last_purchased.empty())
      return fail(e, "Inventory source shop sale rejected");
    int64_t amount = int64_t(p->value) * owned->doses;
    if (!add(expected.cash, amount, expected.cash))
      return fail(e, "Inventory source sale cash overflow");
    remove(expected.items, r.item.uid);
    publish = true;
  } else
    return fail(e, "Unimplemented source shop commit operation");
  if (after.revision != expected.revision ||
      after.natural_order != expected.natural_order ||
      after.cash != expected.cash || !same(after.items, expected.items))
    return fail(
        e, "Inventory shop callback bypassed typed cash/ownership operation");
  auto next = state_;
  next.items = expected.items;
  next.cash = expected.cash;
  if (!validate(next, e))
    return false;
  if (r.action != FieldShopAction::Ready) {
    if (next.revision == std::numeric_limits<uint64_t>::max())
      return fail(e, "Inventory shop revision overflow");
    ++next.revision;
  }
  state_ = std::move(next);
  if (publish) {
    context_ = r.context_item;
    has_context_ = true;
  }
  e.clear();
  return true;
}
bool FieldInventoryRuntime::source_initial(SourceRandom &random,
                                           std::vector<uint32_t> &ledger,
                                           LoadRngClockProvider clock,
                                           std::string &e) {
  if (!data_ || !defs_)
    return fail(e, "Inventory source initial domain unbound");
  FieldInventoryState next;
  next.level = data_->initial_level();
  next.hp = data_->initial_hp();
  next.pp = data_->initial_pp();
  next.cash = data_->initial_cash();
  for (const auto &o : data_->owners())
    next.items.inventories.push_back({o.id, o.role, {}});
  next.items.party_order = {data_->role(0)->id};
  SourceRandom rng = random;
  auto uidledger = ledger;
  std::vector<LoadInventoryAllocation> rows;
  for (auto role : {1u, 2u, 0u}) {
    auto *o = data_->role(role);
    uint32_t count = 0;
    for (const auto &i : data_->initial())
      count += i.owner == o->id;
    rows.push_back({o->id, count});
  }
  std::vector<LoadUidAllocation> trace;
  if (!apply_load_uid_allocations(rng, uidledger, rows, clock, e, &trace))
    return false;
  size_t n = 0;
  for (auto role : {1u, 2u, 0u})
    for (const auto &v : data_->initial())
      if (v.owner == data_->role(role)->id) {
        if (n >= trace.size())
          return fail(e, "Initial inventory allocation count mismatch");
        auto i = v.item;
        i.uid = trace[n++].generated_uid;
        inventory(next.items, v.owner)->items.push_back(i);
      }
  std::array<int64_t, 7> max{};
  if (!stats(next, max, e))
    return false;
  next.hp = std::clamp<int64_t>(next.hp, 0, max[0]);
  next.pp = std::clamp<int64_t>(next.pp, 0, max[1]);
  if (!validate(next, e) ||
      state_.revision == std::numeric_limits<uint64_t>::max())
    return false;
  next.revision = state_.revision + 1;
  state_ = std::move(next);
  random = rng;
  ledger.swap(uidledger);
  has_context_ = false;
  e.clear();
  return true;
}
bool FieldInventoryRuntime::equip(uint32_t uid, bool on,
                                  FieldInventoryResult &result,
                                  std::string &e) {
  if (!validate(state_, e))
    return false;
  uint32_t owner = 0;
  auto *i = item(state_.items, uid, &owner);
  const auto *p = i ? data_->policy(i->definition) : nullptr;
  const auto *d = i ? defs_->definition(i->definition) : nullptr;
  if (!p || !d || owner != data_->role(0)->id || d->keyitem() ||
      !p->use_allowed || d->slot.empty() ||
      !std::any_of(p->actions.begin(), p->actions.end(), [](const auto &a) {
        return a.function == FieldItemFunction::Equip;
      }))
    return fail(e, "Inventory equip requires admitted source action/owner");
  auto next = state_;
  auto *target = item(next.items, uid);
  if (on)
    for (auto &v : inventory(next.items, owner)->items)
      if (v.equipped && defs_->definition(v.definition)->slot == d->slot)
        v.equipped = false;
  target->equipped = on;
  if (!validate(next, e) ||
      next.revision == std::numeric_limits<uint64_t>::max())
    return false;
  ++next.revision;
  FieldInventoryResult out;
  out.performed = true;
  out.doses = target->doses;
  out.events.push_back({FieldInventoryEventKind::Sound,
                        data_->sound(on ? 1 : 2),
                        {},
                        0,
                        *target,
                        owner});
  state_ = std::move(next);
  result = std::move(out);
  e.clear();
  return true;
}
bool FieldInventoryRuntime::consume(uint32_t uid, uint32_t action,
                                    uint32_t target,
                                    FieldInventoryResult &result,
                                    std::string &e) {
  if (!validate(state_, e))
    return false;
  uint32_t owner = 0;
  auto *original = item(state_.items, uid, &owner);
  const auto *p = original ? data_->policy(original->definition) : nullptr;
  const auto *d = original ? defs_->definition(original->definition) : nullptr;
  if (!p || !d || owner != data_->role(0)->id || target != owner ||
      action >= p->actions.size() ||
      p->actions[action].function != FieldItemFunction::Consume)
    return fail(e, "Inventory consume source action/target rejected");
  auto unconscious = [this](const FieldInventoryState &s) {
    for (const auto &v : s.statuses)
      if (data_->status(v.id)->unconscious)
        return true;
    return false;
  };
  if (unconscious(state_))
    return fail(e, "Inventory target omitted by source unconscious filter");
  FieldInventoryResult out;
  out.doses = original->doses;
  auto message = [&](const std::string &key, int64_t value,
                     const std::string &stat, double delay, bool update) {
    out.events.push_back({FieldInventoryEventKind::Message, key, stat, value,
                          *original, target, delay, update});
  };
  bool permitted = p->consume_allowed;
  std::string failure = p->actions[action].fail;
  auto matches = [&](uint32_t selector) {
    return selector == 1   ? d->is_food()
           : selector == 2 ? !d->status_heals.empty()
           : selector == 3 ? d->heal_hp > 0
                           : d->heal_pp > 0;
  };
  // is_in_state_to_receive_item assigns (rather than ANDs) food/status_heals in
  // actual source status/effect order. The separate failure method also tests
  // HP/PP.
  bool receive = true;
  for (const auto &sts : state_.statuses)
    for (const auto &b : data_->status(sts.id)->blocks)
      if (b.selector <= 2)
        receive = !matches(b.selector);
  if (!receive) {
    permitted = false;
    bool can = true;
    for (const auto &sts : state_.statuses) {
      for (const auto &b : data_->status(sts.id)->blocks) {
        can = !matches(b.selector);
        if (!can) {
          failure = b.message;
          break;
        }
      }
      if (!can)
        break;
    }
  }
  if (!permitted) {
    message(failure, 0, {}, 0, false);
    result = std::move(out);
    e.clear();
    return true;
  }
  auto next = state_;
  std::array<int64_t, 7> values{};
  if (!stats(next, values, e))
    return false;
  std::vector<FieldInventoryEvent> queued;
  bool success = false;
  auto sethp = [&](int64_t v) {
    if (v != next.hp && !(unconscious(next) && v > 0))
      next.hp = std::clamp<int64_t>(v, 0, values[0]);
  };
  auto setpp = [&](int64_t v) {
    if (v != next.pp)
      next.pp = std::clamp<int64_t>(v, 0, values[1]);
  };
  int64_t value = 0;
  if (d->heal_hp > 0) {
    if (!add(next.hp, d->heal_hp, value))
      return fail(e, "Inventory source HP addition overflow");
    sethp(value);
    queued.push_back({FieldInventoryEventKind::Message,
                      data_->feedback(next.hp >= values[0] ? 1 : 0),
                      {},
                      next.hp >= values[0] ? 1 : d->heal_hp,
                      *original,
                      target});
    success = true;
  }
  if (d->heal_pp > 0 && values[1] > 0) {
    if (!add(next.pp, d->heal_pp, value))
      return fail(e, "Inventory source PP addition overflow");
    setpp(value);
    queued.push_back({FieldInventoryEventKind::Message,
                      data_->feedback(next.pp >= values[1] ? 3 : 2),
                      {},
                      next.pp >= values[1] ? 1 : d->heal_pp,
                      *original,
                      target});
    success = true;
  }
  // Scoped definitions contain at most one literal status cure; the shared
  // producer refuses unknown/all selectors instead of silently dropping them.
  for (const auto &sts : d->status_heals) {
    auto it = std::find_if(next.statuses.begin(), next.statuses.end(),
                           [&](const auto &v) { return v.id == sts; });
    auto *policy = data_->status(sts);
    if (!policy)
      return fail(e, "Inventory status cure not bound");
    if (it != next.statuses.end()) {
      next.statuses.erase(it);
      if (unconscious(next) && next.hp > 0)
        sethp(0);
      else if (!unconscious(next) && next.hp == 0)
        sethp(data_->refresh_hp());
      message(policy->heal, 0, {}, 0, false);
      success = true;
    } else
      message(policy->fail, 0, {}, 0, false);
  }
  for (auto index : p->boost_order)
    if (d->boost[index] > 0) {
      if (!add(next.permanent[index], d->boost[index], next.permanent[index]) ||
          !stats(next, values, e))
        return fail(e, "Inventory permanent boost overflow");
      message(data_->feedback(4), d->boost[index], data_->stats()[index], 0,
              false);
      if (index == 0) {
        if (!add(next.hp, d->boost[index], value))
          return fail(e, "Inventory boost HP overflow");
        sethp(value);
      } else if (index == 1) {
        if (!add(next.pp, d->boost[index], value))
          return fail(e, "Inventory boost PP overflow");
        setpp(value);
      }
      success = true;
    }
  if (!d->reusable()) {
    auto *i = item(next.items, uid);
    if (i->doses > defs_->dose_drop_threshold()) {
      i->doses -= defs_->dose_step();
      out.doses = i->doses;
    } else {
      remove(next.items, uid);
      out.removed = true;
      out.doses = 0;
    }
  }
  if (!validate(next, e) ||
      next.revision == std::numeric_limits<uint64_t>::max())
    return false;
  ++next.revision;
  out.performed = success;
  if (out.events.empty() && queued.empty())
    message(p->actions[action].fail, 0, {}, 0, false);
  if (success)
    out.events.push_back({FieldInventoryEventKind::Sound,
                          data_->sound(0),
                          {},
                          0,
                          *original,
                          target});
  for (size_t n = 0; n < queued.size(); ++n) {
    queued[n].after_seconds = data_->message_delay() * n;
    queued[n].update = n != 0;
    out.events.push_back(std::move(queued[n]));
  }
  context_ = *original;
  if (!out.removed)
    context_.doses = out.doses;
  has_context_ = true;
  state_ = std::move(next);
  result = std::move(out);
  e.clear();
  return true;
}
bool FieldInventoryRuntime::use(uint32_t uid, uint32_t action,
                                FieldInventoryResult &result, std::string &e) {
  if (!validate(state_, e))
    return false;
  uint32_t owner = 0;
  auto *i = item(state_.items, uid, &owner);
  auto *p = i ? data_->policy(i->definition) : nullptr;
  if (!p || action >= p->actions.size() ||
      p->actions[action].function != FieldItemFunction::Use ||
      !p->use_allowed || owner != data_->role(0)->id)
    return fail(e, "Inventory source use-item action rejected");
  FieldInventoryResult out;
  out.performed = true;
  out.doses = i->doses;
  out.events.push_back(
      {FieldInventoryEventKind::UseItem, {}, {}, 0, *i, owner});
  context_ = *i;
  has_context_ = true;
  result = std::move(out);
  e.clear();
  return true;
}
bool FieldInventoryRuntime::switch_items(uint32_t owner, uint32_t first,
                                         uint32_t second, std::string &e) {
  if (!validate(state_, e))
    return false;
  auto next = state_;
  auto *i = inventory(next.items, owner);
  if (!i || first >= i->items.size() || second >= i->items.size() ||
      next.revision == std::numeric_limits<uint64_t>::max())
    return fail(e, "Inventory source switch indices rejected");
  std::swap(i->items[first], i->items[second]);
  ++next.revision;
  state_ = std::move(next);
  e.clear();
  return true;
}
bool FieldInventoryRuntime::sort_auto(uint32_t owner, bool chinese,
                                      std::string &e) {
  if (!validate(state_, e))
    return false;
  auto next = state_;
  auto *inv = inventory(next.items, owner);
  if (!inv || next.revision == std::numeric_limits<uint64_t>::max())
    return fail(e, "Goods source sort owner/revision rejected");
  struct Score {
    FieldOwnedItem item;
    int64_t score;
    std::string name;
  };
  std::vector<Score> rows;
  for (const auto &i : inv->items) {
    auto *d = defs_->definition(i.definition);
    auto *p = data_->policy(i.definition);
    int64_t boost = 0;
    for (size_t n = 0; n < d->boost.size(); ++n)
      boost += int64_t(d->boost[n]) * data_->sort_weights()[n];
    auto &s = data_->sort_scores();
    int64_t score = 0;
    bool equippable =
        std::any_of(p->actions.begin(), p->actions.end(), [](const auto &a) {
          return a.function == FieldItemFunction::Equip;
        });
    if (d->keyitem())
      score = s[10];
    else if (!d->status_heals.empty())
      score = s[2];
    else if (equippable) {
      score = s[i.equipped ? 9 : 8];
      auto slot =
          std::find(data_->slots().begin(), data_->slots().end(), d->slot);
      if (slot == data_->slots().end())
        return fail(e, "Source auto-sort equipment slot unknown");
      score += int64_t(data_->slots().end() - slot) * data_->sort_slot_step() +
               boost;
    } else if (boost > 0)
      score = s[7] + boost;
    else if (d->heal_pp > 0)
      score = s[1] + d->heal_pp;
    else if (d->heal_hp > 0)
      score = s[0] + d->heal_hp;
    else if (!p->actions.empty() &&
             p->actions.front().function == FieldItemFunction::Consume)
      score = s[3];
    else if (!p->actions.empty() &&
             p->actions.front().function == FieldItemFunction::Use)
      score = s[5];
    else
      score = s[6];
    auto *t = data_->text(d->sorting_key);
    if (!t)
      return fail(e, "Goods source sort translation missing");
    rows.push_back({i, score, chinese ? t->zh : t->en});
  }
  // Source comparator has no order among equal-score/equal-name objects. Keep
  // their existing identity order; this adaptation performs no UID operation.
  std::stable_sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) {
    return a.score != b.score ? a.score > b.score : a.name < b.name;
  });
  inv->items.clear();
  for (const auto &v : rows)
    inv->items.push_back(v.item);
  if (!validate(next, e))
    return false;
  ++next.revision;
  state_ = std::move(next);
  e.clear();
  return true;
}
bool FieldInventoryRuntime::rows(uint32_t owner,
                                 std::vector<FieldGoodsRow> &out,
                                 std::string &e) const {
  if (!validate(state_, e))
    return false;
  auto *i = inventory(state_.items, owner);
  if (!i)
    return fail(e, "Goods inventory owner absent");
  std::vector<FieldGoodsRow> rows;
  for (const auto &v : i->items)
    rows.push_back({v, defs_->definition(v.definition)->name_key, v.equipped});
  out.swap(rows);
  return true;
}
bool FieldInventoryRuntime::actions(uint32_t uid,
                                    std::vector<FieldGoodsAction> &out,
                                    std::string &e) const {
  if (!validate(state_, e))
    return false;
  uint32_t owner = 0;
  auto *i = item(state_.items, uid, &owner);
  if (!i)
    return fail(e, "Goods selected UID missing");
  auto *p = data_->policy(i->definition);
  auto *d = defs_->definition(i->definition);
  std::vector<FieldGoodsAction> rows;
  for (size_t n = 0; n < p->actions.size(); ++n) {
    const auto &a = p->actions[n];
    if (a.function == FieldItemFunction::Equip) {
      if (owner == data_->role(0)->id && p->use_allowed && !d->keyitem())
        rows.push_back({i->equipped ? FieldGoodsActionKind::Unequip
                                    : FieldGoodsActionKind::Equip,
                        uint32_t(n), data_->action_label(i->equipped ? 1 : 0)});
    } else
      rows.push_back({a.function == FieldItemFunction::Consume
                          ? FieldGoodsActionKind::Consume
                          : FieldGoodsActionKind::Use,
                      uint32_t(n), a.name});
  }
  rows.push_back({FieldGoodsActionKind::Sort, 0, data_->action_label(2)});
  if (!d->keyitem() && d->value != 0)
    rows.push_back({FieldGoodsActionKind::Drop, 0, data_->action_label(3)});
  out.swap(rows);
  return true;
}
bool FieldInventoryRuntime::encode_save(std::vector<uint8_t> &out,
                                        std::string &e) const {
  if (!validate(state_, e))
    return false;
  std::vector<uint8_t> b(96);
  std::memcpy(b.data(), "ENCFISV1", 8);
  patch(b, 8, 1);
  patch(b, 20, 1);
  patch(b, 24, 1);
  patch(b, 28, 0x454e0043);
  std::copy(data_->source_pin().begin(), data_->source_pin().end(),
            b.begin() + 32);
  std::copy(data_->identity().begin(), data_->identity().end(), b.begin() + 64);
  put(b, state_.level);
  putq(b, uint64_t(state_.hp));
  putq(b, uint64_t(state_.pp));
  putq(b, uint64_t(state_.cash));
  for (auto v : state_.permanent)
    putq(b, uint64_t(v));
  put(b, uint32_t(state_.statuses.size()));
  for (const auto &v : state_.statuses) {
    text(b, v.id);
    putq(b, uint64_t(v.passive_turns));
  }
  put(b, uint32_t(state_.items.inventories.size()));
  for (const auto &i : state_.items.inventories) {
    put(b, i.owner);
    put(b, i.role);
    put(b, uint32_t(i.items.size()));
    for (const auto &v : i.items) {
      put(b, v.definition);
      put(b, v.uid);
      put(b, v.doses);
      put(b, uint32_t(v.equipped));
    }
  }
  patch(b, 12, uint32_t(b.size()));
  patch(b, 16, crc(b.data(), b.size()));
  out.swap(b);
  e.clear();
  return true;
}
bool FieldInventoryRuntime::decode_save(const uint8_t *p, size_t n,
                                        FieldInventoryState &out,
                                        std::string &e) const {
  if (!data_ || !defs_ || !p || n < 96 || n > 1024 * 1024 ||
      std::memcmp(p, "ENCFISV1", 8) || u32(p + 8) != 1 || u32(p + 12) != n ||
      u32(p + 16) != crc(p, n) || u32(p + 20) != 1 || u32(p + 24) != 1 ||
      u32(p + 28) != 0x454e0043 ||
      !std::equal(data_->source_pin().begin(), data_->source_pin().end(),
                  p + 32) ||
      !std::equal(data_->identity().begin(), data_->identity().end(), p + 64) ||
      u32(p + 52) || u32(p + 56) || u32(p + 60))
    return fail(e,
                "Unknown field inventory save schema/capability/rules/source");
  size_t at = 96;
  bool ok = true;
  auto u = [&]() -> uint32_t {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0u;
    }
    auto v = u32(p + at);
    at += 4;
    return v;
  };
  auto q = [&]() {
    if (!ok || at > n || n - at < 8) {
      ok = false;
      return int64_t(0);
    }
    uint64_t v = 0;
    for (unsigned k = 0; k < 8; ++k)
      v |= uint64_t(p[at + k]) << (8 * k);
    at += 8;
    int64_t x;
    std::memcpy(&x, &v, 8);
    return x;
  };
  FieldInventoryState s;
  s.level = u();
  s.hp = q();
  s.pp = q();
  s.cash = q();
  for (auto &v : s.permanent)
    v = q();
  auto count = u();
  if (count > 256)
    ok = false;
  for (uint32_t k = 0; ok && k < count; ++k) {
    auto len = u();
    if (len > 8192 || at > n || len > n - at) {
      ok = false;
      break;
    }
    FieldInventoryStatus v;
    v.id = std::string(reinterpret_cast<const char *>(p + at), len);
    at += len;
    v.passive_turns = q();
    s.statuses.push_back(std::move(v));
  }
  count = u();
  if (count != data_->owners().size())
    ok = false;
  for (uint32_t k = 0; ok && k < count; ++k) {
    FieldItemInventory i;
    i.owner = u();
    i.role = u();
    auto entries = u();
    if (entries > 4096) {
      ok = false;
      break;
    }
    for (uint32_t j = 0; ok && j < entries; ++j) {
      FieldOwnedItem v;
      v.definition = u();
      v.uid = u();
      v.doses = u();
      auto equipped = u();
      if (equipped > 1)
        ok = false;
      v.equipped = equipped == 1;
      i.items.push_back(v);
    }
    s.items.inventories.push_back(std::move(i));
  }
  s.items.party_order = {data_->role(0)->id};
  if (!ok || at != n || !validate(s, e))
    return fail(e, "Field inventory save domain/content rejected");
  out = std::move(s);
  e.clear();
  return true;
}
bool FieldInventoryRuntime::restore(const uint8_t *p, size_t n,
                                    SourceRandom &random,
                                    std::vector<uint32_t> &ledger,
                                    LoadRngClockProvider clock,
                                    std::string &e) {
  FieldInventoryState next;
  if (!decode_save(p, n, next, e) ||
      state_.revision == std::numeric_limits<uint64_t>::max())
    return false;
  std::vector<LoadInventoryAllocation> rows;
  for (auto role : {1u, 2u, 0u}) {
    auto *o = data_->role(role);
    auto *i = inventory(next.items, o->id);
    rows.push_back({o->id, uint32_t(i->items.size())});
  }
  SourceRandom rng = random;
  auto uidledger = ledger;
  if (!apply_load_uid_allocations(rng, uidledger, rows, clock, e))
    return false;
  next.revision = state_.revision + 1;
  state_ = std::move(next);
  random = rng;
  ledger.swap(uidledger);
  has_context_ = false;
  e.clear();
  return true;
}
} // namespace encore::upstream

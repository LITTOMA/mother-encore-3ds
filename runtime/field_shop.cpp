#include "encore/field_shop.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool confirming(FieldShopPhase p) {
  return p == FieldShopPhase::ConfirmBuy || p == FieldShopPhase::ConfirmSell ||
         p == FieldShopPhase::Insufficient;
}
} // namespace
FieldItemInventory *FieldShopRuntime::inventory(FieldShopSnapshot &s) const {
  for (auto &i : s.items.inventories)
    if (i.owner == owner_ && i.role == 0)
      return &i;
  return nullptr;
}
const FieldItemInventory *
FieldShopRuntime::inventory(const FieldShopSnapshot &s) const {
  for (const auto &i : s.items.inventories)
    if (i.owner == owner_ && i.role == 0)
      return &i;
  return nullptr;
}
bool FieldShopRuntime::validate(const FieldShopSnapshot &s,
                                std::string &e) const {
  if (s.cash < 0 || s.natural_order.empty() || s.natural_order.size() > 64 ||
      s.natural_order.size() != s.items.party_order.size())
    return fail(e, "Shop cash/party admission rejected");
  std::set<uint32_t> a(s.natural_order.begin(), s.natural_order.end()),
      b(s.items.party_order.begin(), s.items.party_order.end());
  if (a != b || a.size() != s.natural_order.size())
    return fail(e, "Shop natural party identity rejected");
  std::set<uint32_t> owners, uids;
  for (const auto &i : s.items.inventories) {
    if (i.role > 3 || !owners.insert(i.owner).second ||
        (!items_->unbounded(i.role) &&
         i.items.size() > items_->capacity(i.role)))
      return fail(e, "Shop inventory role/capacity rejected");
    for (const auto &v : i.items) {
      const auto *d = items_->definition(v.definition);
      const auto *p = data_->policy(v.definition);
      if (!d || !p || !v.doses || v.doses > d->doses ||
          !uids.insert(v.uid).second || d->keyitem() != (i.role == 1) ||
          (v.equipped && (i.role != 0 || d->slot.empty())))
        return fail(e, "Shop item ownership/dose/source rejected");
    }
  }
  for (auto id : s.natural_order) {
    auto it = std::find_if(
        s.items.inventories.begin(), s.items.inventories.end(),
        [id](const auto &i) { return i.owner == id && i.role == 0; });
    if (it == s.items.inventories.end())
      return fail(e, "Shop natural member inventory absent");
  }
  return true;
}
bool FieldShopRuntime::initialize(const FieldShopData &d,
                                  const FieldItemDefinitions &i,
                                  SourceRandom &r, std::vector<uint32_t> &l,
                                  LoadRngClockProvider c, FieldShopHost h,
                                  std::string &e) {
  data_ = nullptr;
  phase_ = FieldShopPhase::Closed;
  if (!d.valid() || !i.valid() || d.source_pin() != i.source_pin() || !c ||
      !h.bind || !h.read || !h.commit || !h.sound || !h.close)
    return fail(e, "Shop typed source host unbound");
  for (const auto &p : d.policies()) {
    const auto *x = i.definition(p.id);
    std::array<uint8_t, 32> hash{};
    if (!x || !i.source_hash(p.source, hash) || hash != p.source_sha ||
        x->cost != p.cost || x->value != p.value || x->doses != p.doses ||
        x->keyitem() != p.key || x->item_name != p.name || x->slot != p.slot ||
        x->name_key != p.name_key || x->description_key != p.description_key ||
        x->article_key != p.article_key)
      return fail(e, "Shop shared item definition/source mismatch");
  }
  for (auto id : d.offers()) {
    auto *p = d.policy(id);
    if (!p || p->key || !p->slot.empty())
      return fail(e, "Shop equipment/key offer mechanism not admitted");
  }
  if (!h.bind(d, i, e))
    return false;
  data_ = &d;
  items_ = &i;
  random_ = &r;
  ledger_ = &l;
  clock_ = std::move(c);
  host_ = std::move(h);
  return true;
}
bool FieldShopRuntime::audio(uint32_t k, std::string &e) {
  const auto &p = data_->sound(k);
  return p.empty() ? fail(e, "Shop source sound unbound") : host_.sound(p, e);
}
bool FieldShopRuntime::open(const std::string &name, std::string &e) {
  if (!data_ || phase_ != FieldShopPhase::Closed || name != data_->name())
    return fail(e, "Shop open source/state rejected");
  FieldShopSnapshot s;
  if (!host_.read(s, e) || !validate(s, e))
    return false;
  auto r = *random_;
  auto l = *ledger_;
  std::vector<LoadUidAllocation> t;
  if (!apply_load_uid_allocations(r, l, {{0, uint32_t(data_->offers().size())}},
                                  clock_, e, &t))
    return false;
  std::vector<FieldOwnedItem> preview;
  for (size_t n = 0; n < t.size(); ++n)
    preview.push_back({data_->offers()[n], t[n].generated_uid,
                       data_->policy(data_->offers()[n])->doses, false});
  FieldShopResult result;
  result.action = FieldShopAction::Ready;
  if (host_.source_items && !host_.source_items(preview, e))
    return false;
  if (!host_.commit(s, s, result, e))
    return false;
  *random_ = r;
  *ledger_ = std::move(l);
  previews_ = std::move(preview);
  snapshot_ = std::move(s);
  owner_ = snapshot_.natural_order.front();
  last_.clear();
  main_selected_ = 0;
  warning_left_ = 0;
  return enter(FieldShopPhase::BuySell, true, e);
}
bool FieldShopRuntime::refresh(std::string &e) {
  if (!data_ || phase_ == FieldShopPhase::Closed)
    return fail(e, "Shop refresh outside active source menu");
  FieldShopSnapshot s;
  if (!host_.read(s, e) || !validate(s, e))
    return false;
  if (std::find(s.natural_order.begin(), s.natural_order.end(), owner_) ==
      s.natural_order.end())
    return fail(e, "Shop selected member disappeared");
  snapshot_ = std::move(s);
  if (phase_ == FieldShopPhase::Buy)
    rows_ = previews_;
  else if (phase_ == FieldShopPhase::Sell)
    rows_ = inventory(snapshot_)->items;
  else
    return true;
  if (rows_.empty())
    selected_ = page_ = 0;
  else {
    selected_ = std::min(selected_, uint32_t(rows_.size() - 1));
    const auto maxpage = rows_.size() > data_->lines()
                             ? uint32_t(rows_.size() - data_->lines())
                             : 0;
    page_ = std::min(page_, maxpage);
    if (selected_ < page_)
      page_ = selected_;
    if (selected_ >= page_ + data_->lines())
      page_ = selected_ - data_->lines() + 1;
  }
  return true;
}
bool FieldShopRuntime::publish_context(std::string &e) {
  if (rows_.empty())
    return true;
  FieldShopResult r;
  r.action = FieldShopAction::Preview;
  r.item = rows_[selected_];
  r.context_item = r.item;
  r.owner = owner_;
  return host_.commit(snapshot_, snapshot_, r, e);
}
bool FieldShopRuntime::enter(FieldShopPhase p, bool reset, std::string &e) {
  phase_ = p;
  yes_ = true;
  if (reset)
    selected_ = page_ = 0;
  warning_left_ = 0;
  if (p == FieldShopPhase::BuySell) {
    rows_.clear();
    selected_item_ = {};
    return refresh(e);
  }
  if (!refresh(e))
    return false;
  return publish_context(e);
}
const std::string &FieldShopRuntime::prompt_key() const {
  static const std::string empty;
  if (!data_)
    return empty;
  if (warning())
    return data_->text_key(FieldShopTextRole::Warning);
  if (phase_ == FieldShopPhase::ConfirmBuy)
    return data_->text_key(FieldShopTextRole::PromptBuy);
  if (phase_ == FieldShopPhase::ConfirmSell)
    return data_->text_key(FieldShopTextRole::PromptSell);
  if (phase_ == FieldShopPhase::Insufficient)
    return data_->text_key(FieldShopTextRole::PromptCash);
  return empty;
}
int64_t FieldShopRuntime::price(uint32_t index) const {
  if (!data_ || index >= rows_.size())
    return 0;
  const auto *p = data_->policy(rows_[index].definition);
  return (phase_ == FieldShopPhase::Sell ||
          phase_ == FieldShopPhase::ConfirmSell)
             ? int64_t(p->value) * rows_[index].doses
             : p->cost;
}
bool FieldShopRuntime::restricted(uint32_t index) const {
  if (!data_ || index >= rows_.size())
    return true;
  const auto *p = data_->policy(rows_[index].definition);
  if (phase_ == FieldShopPhase::Sell || phase_ == FieldShopPhase::ConfirmSell)
    return p->value <= 0;
  auto *i = inventory(snapshot_);
  return !i ||
         (!items_->unbounded(i->role) &&
          i->items.size() >= items_->capacity(i->role)) ||
         p->cost > snapshot_.cash;
}
bool FieldShopRuntime::move(int dir, bool bypage, std::string &e) {
  if (!data_ || phase_ == FieldShopPhase::Closed || dir < -1 || dir > 1)
    return fail(e, "Shop cursor input rejected");
  if (!dir)
    return true;
  if (confirming(phase_)) {
    bool next = dir > 0 ? false : true;
    if (next != yes_) {
      yes_ = next;
      return audio(0, e);
    }
    return true;
  }
  if (phase_ == FieldShopPhase::BuySell) {
    uint32_t next = data_->can_sell() ? 1 - main_selected_ : 0;
    if (next != main_selected_) {
      main_selected_ = next;
      return audio(0, e);
    }
    return true;
  }
  if (!refresh(e))
    return false;
  if (rows_.empty())
    return true;
  uint32_t old = selected_;
  int64_t next = int64_t(selected_) + dir * (bypage ? data_->lines() : 1);
  bool loop =
      phase_ == FieldShopPhase::Buy ? data_->buy_loop() : data_->sell_loop();
  if (!bypage && loop) {
    if (next < 0)
      next = rows_.size() - 1;
    if (next >= int64_t(rows_.size()))
      next = 0;
  }
  next = std::clamp<int64_t>(next, 0, rows_.size() - 1);
  selected_ = uint32_t(next);
  if (old == selected_)
    return true;
  warning_left_ = 0;
  if (selected_ < page_)
    page_ = selected_;
  if (selected_ >= page_ + data_->lines())
    page_ = selected_ - data_->lines() + 1;
  return audio(0, e) && publish_context(e);
}
bool FieldShopRuntime::character(int dir, std::string &e) {
  if (!data_ || phase_ == FieldShopPhase::Closed || dir < -1 || dir > 1)
    return fail(e, "Shop member input rejected");
  if (confirming(phase_))
    return true;
  if (!refresh(e))
    return false;
  auto it = std::find(snapshot_.natural_order.begin(),
                      snapshot_.natural_order.end(), owner_);
  int64_t n = it - snapshot_.natural_order.begin();
  n = (n + dir + snapshot_.natural_order.size()) %
      snapshot_.natural_order.size();
  owner_ = snapshot_.natural_order[size_t(n)];
  if (phase_ == FieldShopPhase::Sell)
    return enter(phase_, true, e);
  if (phase_ == FieldShopPhase::Buy)
    return enter(phase_, false, e);
  return true;
}
bool FieldShopRuntime::select(std::string &e) {
  if (!data_ || phase_ == FieldShopPhase::Closed)
    return fail(e, "Shop selection outside menu");
  if (confirming(phase_)) {
    if (!audio(1, e))
      return false;
    return answer(yes_, e);
  }
  if (phase_ == FieldShopPhase::BuySell) {
    if (!audio(1, e))
      return false;
    return enter(main_selected_ ? FieldShopPhase::Sell : FieldShopPhase::Buy,
                 true, e);
  }
  if (!refresh(e))
    return false;
  if (rows_.empty())
    return true;
  const bool denied = restricted(selected_);
  if (!audio(denied ? 2 : 1, e))
    return false;
  selected_item_ = rows_[selected_];
  auto *p = data_->policy(selected_item_.definition);
  if (phase_ == FieldShopPhase::Sell) {
    if (p->value <= 0)
      return true;
    phase_ = FieldShopPhase::ConfirmSell;
  } else {
    auto *i = inventory(snapshot_);
    if (!items_->unbounded(i->role) &&
        i->items.size() >= items_->capacity(i->role)) {
      warning_left_ = data_->warning_seconds();
      return true;
    }
    phase_ = p->cost > snapshot_.cash ? FieldShopPhase::Insufficient
                                      : FieldShopPhase::ConfirmBuy;
  }
  yes_ = true;
  prompt_revision_ = snapshot_.revision;
  return true;
}
bool FieldShopRuntime::cancel(std::string &e) {
  if (!data_ || phase_ == FieldShopPhase::Closed)
    return fail(e, "Shop cancel outside menu");
  if (confirming(phase_)) {
    if (!audio(3, e))
      return false;
    return answer(false, e);
  }
  if (phase_ == FieldShopPhase::BuySell) {
    if (!host_.close(last_, e))
      return false;
    phase_ = FieldShopPhase::Closed;
    rows_.clear();
    return true;
  }
  if (!audio(3, e))
    return false;
  return enter(FieldShopPhase::BuySell, true, e);
}
bool FieldShopRuntime::answer(bool yes, std::string &e) {
  if (!confirming(phase_))
    return fail(e, "Shop answer has no source callback");
  const auto old = phase_;
  if (yes) {
    if (old == FieldShopPhase::ConfirmBuy && !purchase(e))
      return false;
    if (old == FieldShopPhase::ConfirmSell && !sell(e))
      return false;
    if (old == FieldShopPhase::Insufficient) {
      if (!audio(3, e))
        return false;
      return enter(FieldShopPhase::Sell, true, e);
    }
  }
  return enter(old == FieldShopPhase::ConfirmSell ? FieldShopPhase::Sell
                                                  : FieldShopPhase::Buy,
               false, e);
}
bool FieldShopRuntime::purchase(std::string &e) {
  FieldShopSnapshot before;
  if (!host_.read(before, e) || !validate(before, e))
    return false;
  if (before.revision != prompt_revision_)
    return fail(e, "Shop purchase source pause snapshot changed");
  auto after = before;
  auto *i = inventory(after);
  const auto *p = data_->policy(selected_item_.definition);
  if (!i || p->key || !p->slot.empty() || p->cost > before.cash ||
      (!items_->unbounded(i->role) &&
       i->items.size() >= items_->capacity(i->role)))
    return fail(e, "Shop purchase source preconditions changed");
  if (!audio(4, e))
    return false;
  auto random = *random_;
  auto ledger = *ledger_;
  std::vector<LoadUidAllocation> trace;
  if (!apply_load_uid_allocations(random, ledger, {{owner_, 1}}, clock_, e,
                                  &trace))
    return false;
  FieldOwnedItem bought{p->id, trace.front().generated_uid, p->doses, false};
  i->items.push_back(bought);
  after.cash -= p->cost;
  FieldShopResult result;
  result.action = FieldShopAction::Purchase;
  result.item = bought;
  result.context_item = selected_item_;
  result.owner = owner_;
  result.last_purchased = p->name;
  if (!host_.commit(before, after, result, e))
    return false;
  *random_ = random;
  *ledger_ = std::move(ledger);
  last_ = p->name;
  return true;
}
bool FieldShopRuntime::sell(std::string &e) {
  FieldShopSnapshot before;
  if (!host_.read(before, e) || !validate(before, e))
    return false;
  if (before.revision != prompt_revision_)
    return fail(e, "Shop sale source pause snapshot changed");
  auto after = before;
  auto *i = inventory(after);
  if (!i)
    return fail(e, "Shop sale owner missing");
  auto it =
      std::find_if(i->items.begin(), i->items.end(), [this](const auto &v) {
        return v.uid == selected_item_.uid;
      });
  const auto *p = data_->policy(selected_item_.definition);
  if (it == i->items.end() || it->definition != p->id ||
      it->doses != selected_item_.doses || p->value <= 0)
    return fail(e, "Shop selected sale item changed");
  const int64_t amount = int64_t(p->value) * it->doses;
  if (before.cash > std::numeric_limits<int64_t>::max() - amount)
    return fail(e, "Shop cash integer overflow");
  FieldShopResult result;
  result.action = FieldShopAction::Sale;
  result.item = *it;
  result.item.equipped = false;
  result.context_item = result.item;
  result.owner = owner_;
  i->items.erase(it);
  after.cash += amount;
  return audio(4, e) && host_.commit(before, after, result, e);
}
bool FieldShopRuntime::idle(double dt, std::string &e) {
  if (!data_ || !std::isfinite(dt) || dt < 0)
    return fail(e, "Shop idle time invalid");
  warning_left_ = std::max(0., warning_left_ - dt);
  return true;
}
} // namespace encore::upstream

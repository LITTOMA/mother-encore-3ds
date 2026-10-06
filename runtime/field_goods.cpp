#include "encore/field_goods.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
bool FieldGoodsMenu::initialize(const FieldGoodsData &d,
                                FieldInventoryRuntime &i,
                                FieldItemDefinitionsRuntime &definitions,
                                FieldGoodsHost host, std::string &e) {
  if (!i.data() || !d.bind_inventory(*i.data(), e) ||
      definitions.data() != i.definitions() || !host.bind || !host.format ||
      !host.sound || !host.description || !host.key_name ||
      !i.validate(i.state(), e) || !host.bind(d, *i.data(), e))
    return fail(e, "Goods concrete owning/typed UI host absent");
  const auto grid = i.data()->parameter("Grid");
  if (!grid || (*grid)[0] != 2 || (*grid)[1] < 1 || (*grid)[2] <= 0 ||
      (*grid)[3] <= 0 || !i.data()->description_rows() || !i.data()->role(0) ||
      !i.data()->role(1))
    return fail(e, "Goods source two-column owner schema rejected");
  *this = FieldGoodsMenu{};
  data_ = &d;
  inventory_ = &i;
  definitions_ = &definitions;
  host_ = std::move(host);
  e.clear();
  return true;
}
bool FieldGoodsMenu::play(uint32_t n, std::string &e) {
  return host_.sound(data_->sound(n), e);
}
void FieldGoodsMenu::set_phase(FieldGoodsPhase p) {
  phase_ = p;
  phase_started_ = time_;
  sub_ = 0;
  if (p == FieldGoodsPhase::Actions || p == FieldGoodsPhase::Targets ||
      p == FieldGoodsPhase::DropConfirm || p == FieldGoodsPhase::SortType)
    bounce_started_ = time_;
}
bool FieldGoodsMenu::selected_rows(std::vector<FieldGoodsRow> &rows,
                                   std::string &e) const {
  return inventory_ && inventory_->rows(owner_, rows, e);
}
bool FieldGoodsMenu::synchronize(bool reset, std::string &e) {
  std::vector<FieldGoodsRow> rows;
  if (!selected_rows(rows, e))
    return false;
  if (reset)
    selected_ = scroll_ = 0;
  if (rows.empty()) {
    selected_ = scroll_ = 0;
    has_selected_ = false;
  } else {
    selected_ = std::min(selected_, uint32_t(rows.size() - 1));
    selected_item_ = rows[selected_].item;
    has_selected_ = true;
    const auto columns = uint32_t((*inventory_->data()->parameter("Grid"))[0]);
    const auto visible = inventory_->data()->description_rows();
    if (description_) {
      if (selected_ < columns * scroll_)
        scroll_ = selected_ / columns;
      else if (selected_ >= columns * (scroll_ + visible))
        scroll_ = selected_ / columns - visible + 1;
    } else
      scroll_ = 0;
  }
  opened_revision_ = inventory_->state().revision;
  e.clear();
  return true;
}
bool FieldGoodsMenu::open(const std::string &nickname, bool description,
                          bool chinese, std::string &e) {
  if (!data_ || nickname.empty() || visible())
    return fail(e, "Goods open scope rejected");
  if (inventory_->state().items.party_order !=
      std::vector<uint32_t>{inventory_->data()->role(0)->id})
    return fail(e, "Goods other party targets pending");
  nickname_ = nickname;
  chinese_ = chinese;
  description_preference_ = description_ = description;
  owner_ = inventory_->data()->role(0)->id;
  time_ = phase_started_ = open_started_ = message_started_ = stats_started_ =
      message_close_started_ = move_ready_ = 0;
  bounce_started_ = -1;
  stats_visible_ = stats_ever_ = message_ever_ = message_closing_ = false;
  stats_current_ = stats_projected_ = {};
  message_.clear();
  actions_.clear();
  target_present_ = target_all_ = false;
  idle_epoch_ = 0;
  scheduled_.clear();
  events_.clear();
  set_phase(FieldGoodsPhase::Items);
  if (!synchronize(false, e))
    return false;
  return play(0, e);
}
bool FieldGoodsMenu::source_label(const std::string &key, std::string &out,
                                  std::string &e) const {
  if (!data_ || !data_->text(key))
    return fail(e, "Goods locale source key absent");
  FieldGoodsTextContext c;
  c.key = key;
  c.nickname = nickname_;
  c.target = inventory_->data()->role(0)->id;
  return host_.format(c, out, e);
}
bool FieldGoodsMenu::key_label(const std::string &key, std::string &out,
                               std::string &e) const {
  if (key != "ui_focus_next" && key != "ui_focus_prev" && key != "ui_scope")
    return fail(e, "Goods unknown source key hint");
  return host_.key_name(key, out, e);
}
bool FieldGoodsMenu::row_name(const FieldGoodsRow &row, std::string &out,
                              std::string &e) const {
  if (!data_ || !data_->text(row.name_key))
    return fail(e, "Goods item source name absent");
  FieldGoodsTextContext c;
  c.key = row.name_key;
  c.nickname = nickname_;
  c.item = row.item;
  c.target = inventory_->data()->role(0)->id;
  return host_.format(c, out, e);
}
bool FieldGoodsMenu::close(bool used, std::string &e) {
  if (!play(4, e))
    return false;
  set_phase(FieldGoodsPhase::Closing);
  if (!used)
    events_.push_back(
        {FieldGoodsEventKind::Back, {}, owner_, description_preference_});
  return true;
}
bool FieldGoodsMenu::apply_result(const FieldInventoryResult &result,
                                  std::string &e) {
  const auto target = inventory_->data()->role(0)->id;
  for (const auto &ev : result.events) {
    if (ev.kind == FieldInventoryEventKind::Sound) {
      if (!host_.sound(ev.key, e))
        return false;
    } else if (ev.kind == FieldInventoryEventKind::UseItem)
      events_.push_back({FieldGoodsEventKind::UseItem, ev.item, owner_,
                         description_preference_});
    else if (ev.kind == FieldInventoryEventKind::Message)
      scheduled_.push_back({ev, time_ + ev.after_seconds, idle_epoch_ + 1});
    else
      return fail(e, "Goods unknown inventory event");
  }
  (void)target;
  events_.push_back({FieldGoodsEventKind::InventoryChanged,
                     {},
                     owner_,
                     description_preference_});
  return synchronize(false, e);
}
bool FieldGoodsMenu::perform_consume(std::string &e) {
  if (!has_selected_ || action_index_ >= actions_.size())
    return fail(e, "Goods source action lost");
  FieldInventoryResult r;
  if (!inventory_->consume(selected_item_.uid,
                           actions_[action_index_].source_action,
                           inventory_->data()->role(0)->id, r, e) ||
      !apply_result(r, e))
    return false;
  set_phase(FieldGoodsPhase::Items);
  return true;
}
bool FieldGoodsMenu::select(std::string &e) {
  if (!has_selected_)
    return true;
  if (phase_ == FieldGoodsPhase::ManualSort) {
    if (!inventory_->switch_items(owner_, sort_source_, selected_, e))
      return false;
    description_ = description_preference_;
    set_phase(FieldGoodsPhase::Items);
    events_.push_back({FieldGoodsEventKind::InventoryChanged,
                       {},
                       owner_,
                       description_preference_});
    return synchronize(false, e);
  }
  if (!inventory_->actions(selected_item_.uid, actions_, e))
    return false;
  set_phase(FieldGoodsPhase::Actions);
  return true;
}
bool FieldGoodsMenu::input(FieldGoodsInput input, std::string &e) {
  if (!data_ || uint32_t(input) < 1 || uint32_t(input) > 9)
    return fail(e, "Goods unknown input opcode");
  if (!visible() || phase_ == FieldGoodsPhase::Closing)
    return true;
  if (inventory_->state().revision != opened_revision_) {
    if (phase_ != FieldGoodsPhase::Items)
      return fail(e, "Goods selected source UID stale during action");
    if (!synchronize(false, e))
      return false;
  }
  if (input == FieldGoodsInput::Cancel) {
    if (phase_ == FieldGoodsPhase::Message) {
      message_closing_ = true;
      message_close_started_ = time_;
      set_phase(FieldGoodsPhase::Items);
      return synchronize(false, e);
    }
    if (phase_ == FieldGoodsPhase::Items)
      return close(false, e);
    if (!play(3, e))
      return false;
    if (phase_ == FieldGoodsPhase::Targets ||
        phase_ == FieldGoodsPhase::DropConfirm ||
        phase_ == FieldGoodsPhase::SortType) {
      set_phase(FieldGoodsPhase::Actions);
      sub_ = action_index_;
      return true;
    }
    if (phase_ == FieldGoodsPhase::ManualSort)
      description_ = description_preference_;
    set_phase(FieldGoodsPhase::Items);
    return synchronize(false, e);
  }
  if (input == FieldGoodsInput::Scope) {
    if (phase_ == FieldGoodsPhase::Items && has_selected_) {
      description_preference_ = !description_preference_;
      if (!host_.description(description_preference_, e))
        return false;
      description_ = description_preference_;
      events_.push_back({FieldGoodsEventKind::DescriptionChanged,
                         {},
                         owner_,
                         description_preference_});
      return synchronize(false, e);
    }
    return true;
  }
  if (input == FieldGoodsInput::OwnerNext ||
      input == FieldGoodsInput::OwnerPrev) {
    if (phase_ == FieldGoodsPhase::Items) {
      owner_ = owner_ == inventory_->data()->role(0)->id
                   ? inventory_->data()->role(1)->id
                   : inventory_->data()->role(0)->id;
      if (!play(0, e))
        return false;
      return synchronize(true, e);
    }
    return true;
  }
  if (input == FieldGoodsInput::Accept) {
    if (phase_ == FieldGoodsPhase::Message) {
      message_closing_ = true;
      message_close_started_ = time_;
      set_phase(FieldGoodsPhase::Items);
      return synchronize(false, e);
    }
    if (phase_ == FieldGoodsPhase::Items ||
        phase_ == FieldGoodsPhase::ManualSort) {
      if (!has_selected_)
        return true;
      if (!play(2, e))
        return false;
      return select(e);
    }
    if (phase_ == FieldGoodsPhase::Actions) {
      if (sub_ >= actions_.size())
        return fail(e, "Goods action selection invalid");
      if (!play(2, e))
        return false;
      action_index_ = sub_;
      const auto a = actions_[sub_];
      FieldInventoryResult result;
      switch (a.kind) {
      case FieldGoodsActionKind::Equip:
      case FieldGoodsActionKind::Unequip:
        if (!inventory_->equip(selected_item_.uid,
                               a.kind == FieldGoodsActionKind::Equip, result,
                               e) ||
            !apply_result(result, e))
          return false;
        set_phase(FieldGoodsPhase::Items);
        return true;
      case FieldGoodsActionKind::Use:
        if (!inventory_->use(selected_item_.uid, a.source_action, result, e) ||
            !apply_result(result, e))
          return false;
        return close(true, e);
      case FieldGoodsActionKind::Consume: {
        const auto *d =
            inventory_->definitions()->definition(selected_item_.definition);
        if (!d)
          return fail(e, "Goods definition absent");
        target_all_ = d->target_all();
        target_present_ = std::none_of(
            inventory_->state().statuses.begin(),
            inventory_->state().statuses.end(), [&](const auto &s) {
              return inventory_->data()->status(s.id)->unconscious;
            });
        set_phase(FieldGoodsPhase::Targets);
        return true;
      }
      case FieldGoodsActionKind::Sort:
        set_phase(FieldGoodsPhase::SortType);
        return true;
      case FieldGoodsActionKind::Drop:
        set_phase(FieldGoodsPhase::DropConfirm);
        return true;
      }
      return fail(e, "Goods unsupported action");
    }
    if (phase_ == FieldGoodsPhase::Targets) {
      if (!target_present_)
        return true;
      if (!play(2, e))
        return false;
      return perform_consume(e);
    }
    if (phase_ == FieldGoodsPhase::DropConfirm) {
      if (!play(2, e))
        return false;
      if (sub_ == 0) {
        FieldItemResult r;
        if (!definitions_->drop(selected_item_.uid, r, e))
          return false;
        events_.push_back({FieldGoodsEventKind::InventoryChanged,
                           {},
                           owner_,
                           description_preference_});
        set_phase(FieldGoodsPhase::Items);
        return synchronize(false, e);
      }
      set_phase(FieldGoodsPhase::Actions);
      sub_ = action_index_;
      return true;
    }
    if (phase_ == FieldGoodsPhase::SortType) {
      if (!play(2, e))
        return false;
      if (sub_ == 0) {
        sort_source_ = selected_;
        description_ = false;
        set_phase(FieldGoodsPhase::ManualSort);
        return synchronize(false, e);
      }
      if (!inventory_->sort_auto(owner_, chinese_, e))
        return false;
      set_phase(FieldGoodsPhase::Items);
      events_.push_back({FieldGoodsEventKind::InventoryChanged,
                         {},
                         owner_,
                         description_preference_});
      return synchronize(false, e);
    }
    return true;
  }
  if (phase_ == FieldGoodsPhase::Message)
    return true;
  if (time_ < move_ready_)
    return true;
  const int x = input == FieldGoodsInput::Left    ? -1
                : input == FieldGoodsInput::Right ? 1
                                                  : 0,
            y = input == FieldGoodsInput::Up     ? -1
                : input == FieldGoodsInput::Down ? 1
                                                 : 0;
  if (!x && !y)
    return true;
  if (phase_ == FieldGoodsPhase::Items ||
      phase_ == FieldGoodsPhase::ManualSort) {
    std::vector<FieldGoodsRow> rows;
    if (!selected_rows(rows, e))
      return false;
    if (rows.empty())
      return true;
    const auto columns = uint32_t((*inventory_->data()->parameter("Grid"))[0]);
    if (columns != 2)
      return fail(e, "Goods sparse-column movement capability mismatch");
    uint32_t c = selected_ % columns, row = selected_ / columns;
    const auto n = uint32_t(rows.size());
    auto count = [&](uint32_t col) {
      return n / columns + (n % columns > col ? 1 : 0);
    };
    auto previous = selected_;
    if (y > 0) {
      if (c == 1 && row + 1 == count(1) && count(1) < count(0)) {
        c = 0;
        row = count(0) - 1;
      } else {
        ++row;
        if (row >= count(c))
          row = 0;
      }
    }
    if (y < 0) {
      if (!row) {
        if (count(0) > count(1)) {
          c = 0;
          row = count(0) - 1;
        } else
          row = count(c) - 1;
      } else
        --row;
    }
    if (x) {
      if (c == 0 && row + 1 == count(0) && count(0) > count(1)) {
        c = 1;
        row = count(1) ? count(1) - 1 : 0;
      } else {
        c = (c + 1) % columns;
        if (row >= count(c))
          row = count(c) ? count(c) - 1 : 0;
      }
    }
    auto index = row * columns + c;
    selected_ = index < n ? index : n - 1;
    if (!synchronize(false, e))
      return false;
    if (selected_ != previous && !play(1, e))
      return false;
  } else if (y && (phase_ == FieldGoodsPhase::Actions ||
                   phase_ == FieldGoodsPhase::DropConfirm ||
                   phase_ == FieldGoodsPhase::SortType)) {
    auto count =
        phase_ == FieldGoodsPhase::Actions ? uint32_t(actions_.size()) : 2u;
    if (!count)
      return true;
    auto old = sub_;
    sub_ = y < 0 ? (sub_ ? sub_ - 1 : count - 1) : (sub_ + 1) % count;
    if (old != sub_ && !play(1, e))
      return false;
  }
  move_ready_ = time_ + (*data_->parameter("Input"))[0];
  e.clear();
  return true;
}
bool FieldGoodsMenu::idle(double dt, std::string &e) {
  if (!data_ || !std::isfinite(dt) || dt < 0 || dt > 60)
    return fail(e, "Goods invalid idle delta");
  time_ += dt;
  ++idle_epoch_;
  if (!std::isfinite(time_))
    return fail(e, "Goods clock exhausted");
  for (auto i = scheduled_.begin(); i != scheduled_.end();) {
    if (i->due > time_ || i->earliest_idle > idle_epoch_) {
      ++i;
      continue;
    }
    FieldGoodsTextContext c;
    c.key = i->event.key;
    c.nickname = nickname_;
    c.item = i->event.item;
    c.target = i->event.target;
    c.value = i->event.value;
    c.stat = i->event.stat;
    std::string text;
    if (!data_->text(c.key) || !host_.format(c, text, e))
      return fail(e, "Goods source result formatting rejected");
    if (!i->event.update) {
      set_phase(FieldGoodsPhase::Message);
      message_started_ = time_;
      message_ever_ = true;
      message_closing_ = false;
    }
    message_ = std::move(text);
    i = scheduled_.erase(i);
  }
  bool visible = false;
  std::array<int64_t, 7> current{}, projected{};
  if (!equipment_preview(visible, current, projected, e))
    return false;
  if (visible) {
    stats_ever_ = true;
    stats_current_ = current;
    stats_projected_ = projected;
  }
  if (visible != stats_visible_) {
    stats_visible_ = visible;
    stats_started_ = time_;
  }
  if (phase_ == FieldGoodsPhase::Closing &&
      time_ - phase_started_ >= data_->clip("Close")->length)
    phase_ = FieldGoodsPhase::Closed;
  e.clear();
  return true;
}
bool FieldGoodsMenu::equipment_preview(bool &visible,
                                       std::array<int64_t, 7> &current,
                                       std::array<int64_t, 7> &projected,
                                       std::string &e) const {
  visible = false;
  if (!inventory_ || !inventory_->effective_stats(current, e))
    return false;
  projected = current;
  if (phase_ != FieldGoodsPhase::Actions || sub_ >= actions_.size())
    return true;
  const auto &a = actions_[sub_];
  if (a.kind != FieldGoodsActionKind::Equip &&
      a.kind != FieldGoodsActionKind::Unequip)
    return true;
  const auto *d =
      inventory_->definitions()->definition(selected_item_.definition);
  if (!d || d->slot.empty())
    return fail(e, "Goods equipment source slot absent");
  std::vector<FieldGoodsRow> rows;
  if (!inventory_->rows(inventory_->data()->role(0)->id, rows, e))
    return false;
  for (const auto &r : rows)
    if (r.equipped) {
      auto *old = inventory_->definitions()->definition(r.item.definition);
      if (old->slot == d->slot)
        for (size_t n = 0; n < projected.size(); ++n)
          projected[n] -= old->boost[n];
    }
  if (a.kind == FieldGoodsActionKind::Equip)
    for (size_t n = 0; n < projected.size(); ++n)
      projected[n] += d->boost[n];
  visible = true;
  e.clear();
  return true;
}
bool FieldGoodsMenu::portrait(bool &suitable, bool &equipped, int &comparison,
                              std::string &e) const {
  suitable = equipped = false;
  comparison = 0;
  if (!has_selected_)
    return true;
  const auto *d =
      inventory_->definitions()->definition(selected_item_.definition);
  const auto *p = inventory_->data()->policy(selected_item_.definition);
  if (!d || !p)
    return fail(e, "Goods selected portrait definition absent");
  if (d->slot.empty() || d->keyitem() || !p->use_allowed)
    return true;
  std::vector<FieldGoodsRow> rows;
  if (!inventory_->rows(inventory_->data()->role(0)->id, rows, e))
    return false;
  std::array<int32_t, 7> old{};
  for (const auto &r : rows)
    if (r.equipped) {
      const auto *i = inventory_->definitions()->definition(r.item.definition);
      if (i->slot == d->slot) {
        old = i->boost;
        equipped = r.item.uid == selected_item_.uid;
      }
    }
  suitable = !equipped;
  long double diff = 0;
  for (size_t n = 0; n < old.size(); ++n)
    diff += (static_cast<long double>(d->boost[n]) - old[n]) *
            inventory_->data()->sort_weights()[n];
  comparison = equipped ? 0 : diff > 0 ? 1 : diff < 0 ? -1 : 0;
  e.clear();
  return true;
}
double FieldGoodsMenu::bounce_offset() const {
  if (!data_ || bounce_started_ < 0)
    return 0;
  auto p = *data_->parameter("Bounce");
  auto t = std::clamp((time_ - bounce_started_) / p[0], 0., 1.);
  return -p[1] * (1 - t);
}
float FieldGoodsMenu::open_offset() const {
  if (!data_)
    return 0;
  return phase_ == FieldGoodsPhase::Closing
             ? data_->clip("Close")->value(time_ - phase_started_)
             : data_->clip("Open")->value(time_ - open_started_);
}
float FieldGoodsMenu::message_offset() const {
  return data_ ? data_->clip(message_closing_ ? "MessageClose" : "MessageOpen")
                     ->value(time_ - (message_closing_ ? message_close_started_
                                                       : message_started_))
               : 0;
}
float FieldGoodsMenu::stats_offset() const {
  return data_ ? data_->clip(stats_visible_ ? "StatsOpen" : "StatsClose")
                     ->value(time_ - stats_started_)
               : 0;
}
bool FieldGoodsMenu::message_visual() const {
  return data_ && message_ever_ &&
         (!message_closing_ ||
          time_ - message_close_started_ < data_->clip("MessageClose")->length);
}
bool FieldGoodsMenu::equipment_visual(bool &show, std::array<int64_t, 7> &cur,
                                      std::array<int64_t, 7> &projected,
                                      std::string &e) const {
  bool active = false;
  if (!equipment_preview(active, cur, projected, e))
    return false;
  show = active || (stats_ever_ &&
                    time_ - stats_started_ < data_->clip("StatsClose")->length);
  if (show && !active) {
    cur = stats_current_;
    projected = stats_projected_;
  }
  return true;
}
std::vector<FieldGoodsEvent> FieldGoodsMenu::take_events() {
  std::vector<FieldGoodsEvent> out;
  out.swap(events_);
  return out;
}
} // namespace encore::upstream

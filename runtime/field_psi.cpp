#include "encore/field_psi.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
namespace encore::upstream {
namespace {
constexpr int64_t numeric_limit =
    INT32_MAX; // Host statistic structural bound, not game tuning.
const FieldPsiMember *member(const FieldPsiSnapshot &s, std::string_view id) {
  for (const auto &m : s.party)
    if (m.character.character_id == id)
      return &m;
  return nullptr;
}
bool status(const FieldPsiData &d, const FieldPsiMember &m, unsigned role) {
  for (const auto &s : m.character.status) {
    auto *p = d.status(s.status_id);
    if (p && ((role == 1 && p->forgetful) || (role == 2 && p->unconscious) ||
              (role == 3 && p->incapacitated)))
      return true;
  }
  return false;
}
bool learned(const FieldPsiMember &m, std::string_view key) {
  return std::find(m.effective_skills.begin(), m.effective_skills.end(), key) !=
         m.effective_skills.end();
}
std::string substitute(std::string s, std::string_view key,
                       const std::string &value) {
  size_t at = 0;
  while ((at = s.find(key, at)) != s.npos) {
    s.replace(at, key.size(), value);
    at += value.size();
  }
  return s;
}
float ease(float t, float c) {
  if (c > 0)
    return c < 1 ? 1 - std::pow(1 - t, 1 / c) : std::pow(t, c);
  if (c < 0)
    return t < .5f ? std::pow(t * 2, -c) * .5f
                   : (1 - std::pow(1 - (t - .5f) * 2, -c)) * .5f + .5f;
  return 0;
}
} // namespace
bool validate_field_psi_snapshot(const FieldPsiData &d,
                                 const FieldPsiSnapshot &s, std::string &e) {
  auto bad = [&](const char *m) {
    e = m;
    return false;
  };
  if (!d.valid() || s.party.empty() || s.party.size() > 32)
    return bad("PSI source party absent");
  std::set<std::string> ids;
  for (const auto &m : s.party) {
    const auto &c = m.character;
    if (c.character_id.empty() || c.nickname.empty() ||
        !ids.insert(c.character_id).second ||
        std::find(d.party_order().begin(), d.party_order().end(),
                  c.character_id) == d.party_order().end() ||
        m.maximum_hp <= 0 || m.maximum_hp > numeric_limit || m.maximum_pp < 0 ||
        m.maximum_pp > numeric_limit || m.iq < 0 || m.iq > numeric_limit ||
        m.guts < 0 || m.guts > numeric_limit || c.hp < 0 ||
        c.hp > m.maximum_hp || c.pp < 0 || c.pp > m.maximum_pp)
      return bad("PSI live character/stat bounds");
    for (const auto &key : m.effective_skills)
      if (!d.skill(key))
        return bad("PSI learned skill outside checked source categories");
    for (const auto &key : c.learned_skills)
      if (!d.skill(key) || !learned(m, key))
        return bad("PSI effective skills omit actual learned identity");
    std::set<std::string> status_ids;
    for (const auto &t : c.status)
      if (!d.status(t.status_id) || !status_ids.insert(t.status_id).second)
        return bad("PSI status outside checked source policy");
  }
  if (!member(s, s.player))
    return bad("PSI source Player party identity");
  e.clear();
  return true;
}
bool field_psi_targetable(const FieldPsiData &d, const FieldPsiSkill &s,
                          const FieldPsiMember &m, std::string &e) {
  for (const auto &t : m.character.status)
    if (!d.status(t.status_id)) {
      e = "PSI target unknown status";
      return false;
    }
  e.clear();
  return (!status(d, m, 2) || s.target_unconscious) &&
         (!status(d, m, 3) || s.target_incapacitated);
}
bool prepare_field_psi(const FieldPsiData &d, const FieldPsiSnapshot &snapshot,
                       std::string_view user, uint32_t id,
                       std::string_view target, const SourceRandom &rng,
                       FieldPsiCandidate &out, std::string &e) {
  if (!validate_field_psi_snapshot(d, snapshot, e))
    return false;
  auto bad = [&](const char *t) {
    e = t;
    return false;
  };
  auto *s = d.skill(id);
  auto *c = member(snapshot, user);
  if (!s || !c || !learned(*c, s->key) || c->maximum_pp <= 0 ||
      status(d, *c, 1) || c->character.pp < int64_t(s->pp))
    return bad("PSI caster/learned/forgetful/PP rejected");
  if (s->operation != FieldPsiOperation::Telepathy &&
      s->operation != FieldPsiOperation::Heal)
    return bad("PSI field skill execution unsupported");
  FieldPsiCandidate candidate;
  candidate.snapshot = snapshot;
  candidate.random = rng;
  auto caster = std::find_if(
      candidate.snapshot.party.begin(), candidate.snapshot.party.end(),
      [&](const auto &m) { return m.character.character_id == user; });
  caster->character.pp -= s->pp;
  if (s->operation == FieldPsiOperation::Telepathy) {
    if (!target.empty() && target != user)
      return bad("PSI SELF target identity");
  } else {
    auto *t = member(snapshot, target);
    if (!t || !field_psi_targetable(d, *s, *t, e))
      return bad("PSI source target unavailable");
    if (!s->iq_divisor)
      return bad("PSI normal heal divisor absent");
    for (auto &m : candidate.snapshot.party)
      if (m.character.character_id == target) {
        // Source spec + integer IQ division followed by floor(global
        // randf*variance). All operands are checked before double -> signed
        // narrowing; arithmetic uses int64_t and source HP setter clamps only
        // the stored value, not feedback.
        const int64_t base = int64_t(s->heal) + c->iq / int64_t(s->iq_divisor);
        const double value =
            std::floor(double(base) + candidate.random.randf() * s->variance -
                       double(s->variance) / 2.0);
        if (!std::isfinite(value) ||
            value < double(std::numeric_limits<int32_t>::min()) ||
            value > double(std::numeric_limits<int32_t>::max()))
          return bad("PSI heal numeric conversion rejected");
        const int64_t amount = std::max<int64_t>(0, int64_t(value));
        if (amount > INT64_MAX - m.character.hp)
          return bad("PSI HP addition overflow");
        FieldPsiResult result;
        result.skill = id;
        result.target = m.character.character_id;
        result.nickname = m.character.nickname;
        result.old_hp = m.character.hp;
        result.heal = amount;
        result.unclamped_hp = result.old_hp + amount;
        result.maximum = result.unclamped_hp >= m.maximum_hp;
        result.hp = std::min(result.unclamped_hp, m.maximum_hp);
        m.character.hp = result.hp;
        candidate.results.push_back(std::move(result));
      }
  }
  candidate.random_state = candidate.random.state();
  out = std::move(candidate);
  e.clear();
  return true;
}
bool FieldPsiMenu::fail(const char *m) {
  error_ = m;
  return false;
}
bool FieldPsiMenu::initialize(const FieldPsiData *d, FieldPsiHost h,
                              std::string_view code) {
  if (!d || !d->valid() || !d->locale(code) || !h.read || !h.commit ||
      !h.measure || !h.admit_telepathy || !h.close_commands_for_telepathy ||
      !h.telepathy)
    return fail("PSI requires complete checked session/world/font Host");
  data_ = d;
  host_ = std::move(h);
  locale_ = std::string(code);
  phase_ = FieldPsiPhase::Closed;
  rows_ = uint32_t(data_->layout(FieldPsiLayout::DescriptionPolicy).x);
  error_.clear();
  return true;
}
bool FieldPsiMenu::refresh() {
  FieldPsiSnapshot s;
  if (!host_.read(s, error_) || !validate_field_psi_snapshot(*data_, s, error_))
    return false;
  snapshot_ = std::move(s);
  return true;
}
const FieldPsiMember *FieldPsiMenu::caster() const {
  return member(snapshot_, caster_);
}
const FieldPsiSkill *FieldPsiMenu::selected() const {
  if (page_ + row_ >= groups_.size() || column_ >= groups_[page_ + row_].size())
    return nullptr;
  return data_->skill(groups_[page_ + row_][column_]);
}
bool FieldPsiMenu::selectable(const FieldPsiSkill &s) const {
  auto *c = caster();
  return c && !status(*data_, *c, 1) && c->character.pp >= int64_t(s.pp);
}
void FieldPsiMenu::update_page(int dir) {
  const int max = int(groups_.size()) - int(rows_);
  page_ = max <= 0 ? 0
                   : uint32_t((int(page_) + dir) % (max + 1) +
                              (int(page_) + dir < 0 ? max + 1 : 0));
}
bool FieldPsiMenu::describe(std::string text) {
  float width = 0, height = 0, line = 0;
  auto rect = data_->layout(FieldPsiLayout::Description);
  if (!host_.measure(text, rect.z, width, height, line, error_) ||
      !std::isfinite(width) || !std::isfinite(height) || !std::isfinite(line) ||
      width < 0 || height < 0 || line <= 0)
    return fail("PSI source font measurement rejected");
  description_ = std::move(text);
  wraps_ = width > rect.z;
  auto policy = data_->layout(FieldPsiLayout::DescriptionPolicy);
  auto next = uint32_t(wraps_ && height > line ? policy.y : policy.x);
  if (!next || next > 32)
    return fail("PSI row policy invalid");
  if (next != rows_) {
    rows_ = next;
    if (row_ >= rows_) {
      update_page(1);
      row_ = row_ - 1;
    } else if (page_ + rows_ > uint32_t(policy.x)) {
      update_page(-1);
      row_ = std::min(row_ + 1, rows_ - 1);
    } else
      update_page(0);
    if (page_ + row_ >= groups_.size()) {
      row_ = 0;
      update_page(0);
    }
    if (selected() == nullptr)
      column_ = 0;
  }
  return true;
}
bool FieldPsiMenu::rebuild() {
  groups_.clear();
  page_ = row_ = column_ = 0;
  auto *c = caster();
  if (!c)
    return fail("PSI caster disappeared");
  for (const auto &key : c->effective_skills) {
    auto *s = data_->skill(key);
    if (!s)
      return fail("PSI source skill category missing");
    if (s->operation == FieldPsiOperation::Excluded)
      continue;
    auto it = std::find_if(groups_.begin(), groups_.end(), [&](const auto &g) {
      return data_->skill(g.front())->name_key == s->name_key;
    });
    if (it == groups_.end())
      groups_.push_back({s->id});
    else if (std::find(it->begin(), it->end(), s->id) == it->end())
      it->push_back(s->id);
  }
  for (auto &g : groups_)
    std::stable_sort(g.begin(), g.end(), [&](uint32_t a, uint32_t b) {
      return data_->skill(a)->level < data_->skill(b)->level;
    });
  update_page(0);
  auto *s = selected();
  return describe(s ? (locale_ == "en" ? s->desc_en : s->desc_zh)
                    : std::string());
}
bool FieldPsiMenu::open() {
  if (!data_ || active())
    return fail("PSI menu lifecycle");
  if (!refresh())
    return false;
  caster_.clear();
  for (const auto &id : data_->party_order()) {
    auto *m = member(snapshot_, id);
    if (m && m->maximum_pp > 0) {
      caster_ = id;
      break;
    }
  }
  if (caster_.empty())
    return fail("PSI source party has no MAXPP caster");
  rows_ = uint32_t(data_->layout(FieldPsiLayout::DescriptionPolicy).x);
  phase_ = FieldPsiPhase::Skills;
  closing_ = false;
  animation_time_ = cursor_time_ = message_time_ = 0;
  messages_.clear();
  targets_.clear();
  back_ = telepathy_closed_ = false;
  if (!rebuild()) {
    phase_ = FieldPsiPhase::Closed;
    return false;
  }
  sounds_.push_back(FieldPsiSound::Open);
  return true;
}
bool FieldPsiMenu::close(bool silent) {
  if (!visible())
    return true;
  phase_ = FieldPsiPhase::Closing;
  closing_ = true;
  animation_time_ = 0;
  back_ = true;
  if (!silent)
    sounds_.push_back(FieldPsiSound::Close);
  highlight();
  return true;
}
bool FieldPsiMenu::set_locale(std::string_view code) {
  if (!data_ || !data_->locale(code))
    return fail("PSI locale outside source binding");
  locale_ = std::string(code);
  if (phase_ == FieldPsiPhase::Skills) {
    auto *s = selected();
    return describe(s ? (locale_ == "en" ? s->desc_en : s->desc_zh)
                      : std::string());
  }
  return true;
}
bool FieldPsiMenu::move_row(int delta) {
  if (groups_.empty())
    return true;
  int i = int(row_) + delta;
  if (i >= 0 && i < int(rows_) && i < int(groups_.size()))
    row_ = uint32_t(i);
  else if (i + int(page_) >= int(groups_.size())) {
    row_ = 0;
    update_page(1);
  } else if (i + int(page_) < 0) {
    row_ = uint32_t(std::min<size_t>(groups_.size(), rows_) - 1);
    update_page(-1);
  } else if (i < 0)
    update_page(-1);
  else if (i >= int(data_->layout(FieldPsiLayout::DescriptionPolicy).x))
    update_page(1);
  if (page_ + row_ >= groups_.size())
    return fail("PSI source page index unavailable");
  auto &g = groups_[page_ + row_];
  column_ = std::min<uint32_t>(column_, uint32_t(g.size() - 1));
  auto *s = selected();
  sounds_.push_back(FieldPsiSound::Move);
  return describe(locale_ == "en" ? s->desc_en : s->desc_zh);
}
void FieldPsiMenu::highlight() {
  if (host_.highlight) {
    std::vector<std::string> ids;
    if (phase_ == FieldPsiPhase::Targets && target_ < targets_.size())
      ids.push_back(targets_[target_]);
    host_.highlight(ids);
  }
}
bool FieldPsiMenu::confirm_skill() {
  auto *s = selected();
  if (!s)
    return true;
  if (!selectable(*s)) {
    phase_ = FieldPsiPhase::Message;
    messages_.clear();
    sounds_.push_back(FieldPsiSound::Restricted);
    return describe(status(*data_, *caster(), 1) ? locale().forgetful
                                                 : locale().insufficient);
  }
  if (s->operation == FieldPsiOperation::Pending)
    return fail(
        "PSI selected field skill has no admitted execution capability");
  if (s->operation == FieldPsiOperation::Telepathy)
    return cast(caster_);
  targets_.clear();
  for (const auto &id : data_->party_order()) {
    auto *m = member(snapshot_, id);
    if (m && field_psi_targetable(*data_, *s, *m, error_))
      targets_.push_back(id);
  }
  if (targets_.empty())
    return fail("PSI source target popup has no legal party target");
  phase_ = FieldPsiPhase::Targets;
  target_ = 0;
  sounds_.push_back(FieldPsiSound::Confirm);
  highlight();
  return true;
}
bool FieldPsiMenu::show_next_message() {
  if (messages_.empty())
    return true;
  auto message = messages_.front();
  messages_.erase(messages_.begin());
  message_time_ = data_->message_delay();
  return describe(std::move(message));
}
bool FieldPsiMenu::cast(std::string_view target) {
  auto *s = selected();
  if (!s)
    return fail("PSI selected skill missing");
  if (s->operation == FieldPsiOperation::Telepathy &&
      !host_.admit_telepathy(error_))
    return false;
  FieldPsiCandidate candidate;
  if (!host_.commit(caster_, s->id, target, candidate, error_))
    return false;
  if (!validate_field_psi_snapshot(*data_, candidate.snapshot, error_))
    return false;
  snapshot_ = std::move(candidate.snapshot);
  sounds_.push_back(FieldPsiSound::Confirm);
  if (s->operation == FieldPsiOperation::Telepathy) {
    close(true);
    telepathy_closed_ = true;
    if (!host_.close_commands_for_telepathy(error_) || !host_.telepathy(error_))
      return false;
    return true;
  }
  phase_ = FieldPsiPhase::Message;
  highlight();
  messages_.clear();
  for (const auto &r : candidate.results) {
    sounds_.push_back(FieldPsiSound::Heal);
    auto text = r.maximum ? locale().hp_max : locale().hp_up;
    text = substitute(std::move(text), "{target}", r.nickname);
    text = substitute(std::move(text), "{value}", std::to_string(r.heal));
    messages_.push_back(std::move(text));
  }
  return show_next_message();
}
bool FieldPsiMenu::input(int x, int y, bool confirm, bool cancel,
                         int character_step) {
  if (x < -1 || x > 1 || y < -1 || y > 1 || character_step < -1 ||
      character_step > 1)
    return fail("PSI input domain");
  if (phase_ == FieldPsiPhase::Closed || phase_ == FieldPsiPhase::Closing)
    return true;
  if (phase_ == FieldPsiPhase::Message) {
    if ((confirm || cancel) && messages_.empty()) {
      phase_ = FieldPsiPhase::Skills;
      auto *s = selected();
      highlight();
      return describe(s ? (locale_ == "en" ? s->desc_en : s->desc_zh)
                        : std::string());
    }
    return true;
  }
  if (phase_ == FieldPsiPhase::Targets) {
    if (cancel) {
      phase_ = FieldPsiPhase::Skills;
      sounds_.push_back(FieldPsiSound::Cancel);
      highlight();
      return true;
    }
    if (y && !targets_.empty()) {
      target_ = uint32_t((int(target_) + y + int(targets_.size())) %
                         int(targets_.size()));
      sounds_.push_back(FieldPsiSound::Move);
      highlight();
    }
    if (confirm)
      return cast(targets_[target_]);
    return true;
  }
  if (cancel)
    return close();
  if (character_step) {
    std::vector<std::string> casters;
    for (const auto &id : data_->party_order()) {
      auto *m = member(snapshot_, id);
      if (m && m->maximum_pp > 0)
        casters.push_back(id);
    }
    if (casters.size() > 1) {
      auto it = std::find(casters.begin(), casters.end(), caster_);
      const auto i = int(it - casters.begin());
      caster_ = casters[size_t((i + character_step + int(casters.size())) %
                               int(casters.size()))];
      sounds_.push_back(FieldPsiSound::Open);
      if (!rebuild())
        return false;
    }
  }
  if (y && !move_row(y))
    return false;
  if (x && !groups_.empty()) {
    auto &g = groups_[page_ + row_];
    auto next = uint32_t((int(column_) + x + int(g.size())) % int(g.size()));
    if (next != column_) {
      column_ = next;
      sounds_.push_back(FieldPsiSound::Move);
      auto *s = selected();
      if (!describe(locale_ == "en" ? s->desc_en : s->desc_zh))
        return false;
    }
  }
  return !confirm || confirm_skill();
}
bool FieldPsiMenu::idle_frame(double dt) {
  if (!std::isfinite(dt) || dt < 0 || dt > 3600)
    return fail("PSI idle delta");
  cursor_time_ += dt;
  animation_time_ += dt;
  if (phase_ == FieldPsiPhase::Closing &&
      animation_time_ >= data_->animation(true).length)
    phase_ = FieldPsiPhase::Closed;
  // Source SceneTreeTimer resumes after this frame's idle animation processing,
  // once only; a newly yielded 0.5s timer does not consume overshoot this
  // frame.
  if (message_time_ > 0) {
    message_time_ -= dt;
    if (message_time_ <= 0) {
      message_time_ = 0;
      if (!messages_.empty() && !show_next_message())
        return false;
    }
  }
  return true;
}
float FieldPsiMenu::cost_animation_y() const {
  if (!data_)
    return 0;
  const auto &a = data_->cost_animation();
  const float t = float(std::min<double>(cursor_time_, a.length));
  for (size_t i = 1; i < a.keys.size(); ++i)
    if (t < a.keys[i].time) {
      const auto &p = a.keys[i - 1];
      const auto &q = a.keys[i];
      return p.value +
             (q.value - p.value) *
                 ease(std::clamp((t - p.time) / (q.time - p.time), 0.f, 1.f),
                      p.ease);
    }
  return a.keys.back().value;
}
float FieldPsiMenu::animation_y() const {
  if (!data_)
    return 0;
  const auto &a = data_->animation(closing_);
  const float t = float(std::min<double>(animation_time_, a.length));
  if (a.keys.empty())
    return 0;
  for (size_t i = 1; i < a.keys.size(); ++i)
    if (t < a.keys[i].time) {
      const auto &p = a.keys[i - 1];
      const auto &q = a.keys[i];
      const float x = (t - p.time) / (q.time - p.time);
      return p.value +
             (q.value - p.value) * ease(std::clamp(x, 0.f, 1.f), p.ease);
    }
  return a.keys.back().value;
}
} // namespace encore::upstream

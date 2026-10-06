#include "encore/field_character_load.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *v) {
  e = v;
  return false;
}
uint32_t u32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  while (n--) {
    c ^= *p++;
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
struct Reader {
  const uint8_t *p;
  size_t n;
  bool ok = true;
  uint32_t u() {
    if (n < 4) {
      ok = false;
      return 0;
    }
    auto x = u32(p);
    p += 4;
    n -= 4;
    return x;
  }
  int64_t q() {
    uint64_t lo = u(), hi = u();
    return int64_t(lo | hi << 32);
  }
  int64_t bounded() {
    auto x = q();
    if (x < 0 || x > 2147483647)
      ok = false;
    return x;
  }
  bool boolean() {
    auto x = u();
    if (x > 1)
      ok = false;
    return x != 0;
  }
  uint32_t count(uint32_t maximum = 4096) {
    auto x = u();
    if (x > maximum)
      ok = false;
    return ok ? x : 0;
  }
  std::string s() {
    auto k = u();
    if (k > 4096 || k > n) {
      ok = false;
      return {};
    }
    std::string x(reinterpret_cast<const char *>(p), k);
    p += k;
    n -= k;
    size_t chars = 0;
    if (x.find('\0') != x.npos || !encore::utf8_count(x, chars))
      ok = false;
    return x;
  }
  std::vector<std::string> strings() {
    std::vector<std::string> v;
    auto k = count();
    while (k-- && ok)
      v.push_back(s());
    return v;
  }
  std::vector<int64_t> ints() {
    std::vector<int64_t> v;
    auto k = count();
    while (k-- && ok)
      v.push_back(bounded());
    return v;
  }
  double number() {
    uint64_t bits = uint64_t(u());
    bits |= uint64_t(u()) << 32;
    double x;
    std::memcpy(&x, &bits, 8);
    if (!std::isfinite(x) || x < 0)
      ok = false;
    return x;
  }
  std::array<uint8_t, 32> hash() {
    std::array<uint8_t, 32> v{};
    if (n < 32) {
      ok = false;
      return v;
    }
    std::memcpy(v.data(), p, 32);
    p += 32;
    n -= 32;
    if (std::all_of(v.begin(), v.end(), [](uint8_t b) { return b == 0; }))
      ok = false;
    return v;
  }
};
bool unique(const std::vector<std::string> &v) {
  return !v.empty() &&
         std::set<std::string>(v.begin(), v.end()).size() == v.size() &&
         std::none_of(v.begin(), v.end(),
                      [](const auto &s) { return s.empty(); });
}

bool source_identifier(const std::string &s) {
  if (s.empty())
    return false;
  for (unsigned char c : s)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_'))
      return false;
  return !((s.front() >= '0' && s.front() <= '9'));
}
bool source_script(const std::string &s) {
  return !s.empty() && s.front() != '/' && s.find_first_of("\\:") == s.npos &&
         s.find("..") == s.npos;
}
bool default_mapping(
    const std::vector<FieldGlobalDataDefault> &fields,
    const std::vector<std::pair<std::string, uint32_t>> &expected) {
  if (fields.size() != expected.size())
    return false;
  std::set<std::string> seen;
  for (const auto &entry : expected) {
    if (!source_identifier(entry.first) || !seen.insert(entry.first).second)
      return false;
    auto found =
        std::find_if(fields.begin(), fields.end(), [&](const auto &field) {
          return field.name == entry.first;
        });
    if (found == fields.end() || found->kind != entry.second ||
        found->integer_value || !found->string_value.empty())
      return false;
  }
  return true;
}
bool validate_bindings(const FieldCharacterLoadBindings &b) {
  if (b.normal_inventory_type != 0 || !b.enemy_skill_id ||
      !b.item_constructor_id || b.enemy_skill_id == b.item_constructor_id ||
      b.item_native != "Reference" || b.enemy_skill_native != "Reference" ||
      b.stat_fields.size() != 7 || !unique(b.stat_fields) ||
      !source_identifier(b.inventory_getter_method) ||
      b.npc_nickname_prefix.empty() || b.npc_nickname_suffix.empty())
    return false;
  for (const auto *path :
       {&b.character_script, &b.member_script, &b.npc_script,
        &b.inventory_script, &b.enemy_skill_script, &b.item_script})
    if (!source_script(*path))
      return false;
  std::vector<std::pair<std::string, uint32_t>> charfields = {
      {b.name, 1}, {b.level, 2}, {b.exp, 2},       {b.status, 3},
      {b.hp, 2},   {b.pp, 2},    {b.affinities, 4}};
  for (const auto &field : b.stat_fields)
    charfields.emplace_back(field, 2);
  if (!default_mapping(b.character_defaults, charfields) ||
      !default_mapping(b.member_defaults, {{b.nickname, 1},
                                           {b.learned_skills, 3},
                                           {b.inventory, 5},
                                           {b.permanent_boosts, 4},
                                           {b.inventory_getter, 5}}) ||
      !default_mapping(b.npc_defaults,
                       {{b.untargetable, 6}, {b.npc_skills, 3}}) ||
      !default_mapping(b.inventory_defaults, {{b.inventory_type_field, 2},
                                              {b.inventory_items_field, 3}}) ||
      !default_mapping(b.enemy_skill_defaults,
                       {{b.enemy_id_field, 1},
                        {b.enemy_weight_field, 2},
                        {b.enemy_cooldown_field, 2},
                        {b.enemy_remaining_field, 2}}) ||
      !default_mapping(b.item_defaults, {{b.item_name_field, 1},
                                         {b.item_uid_field, 2},
                                         {b.item_equipped_field, 6},
                                         {b.item_doses_field, 2}}))
    return false;
  if (b.enemy_constructor_defaults.size() != 3 ||
      b.enemy_constructor_defaults[0].name != b.enemy_id_field ||
      b.enemy_constructor_defaults[0].kind != 1 ||
      b.enemy_constructor_defaults[0].string_value.empty() ||
      b.enemy_constructor_defaults[0].integer_value ||
      b.enemy_constructor_defaults[1].name != b.enemy_weight_field ||
      b.enemy_constructor_defaults[1].kind != 2 ||
      !b.enemy_constructor_defaults[1].string_value.empty() ||
      b.enemy_constructor_defaults[2].name != b.enemy_cooldown_field ||
      b.enemy_constructor_defaults[2].kind != 2 ||
      !b.enemy_constructor_defaults[2].string_value.empty())
    return false;
  using Step = FieldCharacterLoadStep;
  const std::vector<Step> member = {
      Step::NormalInventory, Step::Name,       Step::ExpFromLevel,
      Step::Status,          Step::Nickname,   Step::LearnedSkills,
      Step::PermanentBoosts, Step::Affinities, Step::SetHp,
      Step::SetPp,           Step::SortSkills};
  const std::vector<Step> character = {
      Step::Name,       Step::LevelRaw, Step::ExpRaw,   Step::HpRaw,
      Step::MaxHpRaw,   Step::PpRaw,    Step::MaxPpRaw, Step::OffenseRaw,
      Step::DefenseRaw, Step::SpeedRaw, Step::IqRaw,    Step::GutsRaw,
      Step::Status};
  const std::vector<Step> npc = {Step::Untargetable, Step::ResetEnemySkills,
                                 Step::FilterEnemySkills, Step::NewEnemySkill,
                                 Step::AppendEnemySkill};
  if (b.member_load_order != member || b.character_load_order != character ||
      b.npc_load_order != npc || b.item_argument_order.size() != 14)
    return false;
  for (size_t n = 0; n < b.item_argument_order.size(); ++n)
    if (uint32_t(b.item_argument_order[n]) != n + 1)
      return false;
  return true;
}
} // namespace
bool FieldCharacterLoadData::source_hash(std::string_view p,
                                         std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(p));
  if (i == sources_.end())
    return false;
  out = i->second;
  return true;
}
bool FieldCharacterLoadData::load(const uint8_t *p, size_t n,
                                  const FieldIdentity &expected,
                                  std::string &e) {
  if (!p || n < 128 || n > 1024 * 1024 || std::memcmp(p, "ENCCHAR1", 8) ||
      u32(p + 8) != 2 || u32(p + 12) != 128 || u32(p + 16) != n ||
      u32(p + 20) != crc(p + 128, n - 128) || u32(p + 24) != 0x454e0050 ||
      u32(p + 28) != 2 || u32(p + 32) != 1 ||
      u32(p + 36) != expected.scene_id ||
      std::memcmp(p + 40, expected.upstream_commit.data(), 20) ||
      std::memcmp(p + 60, expected.source_sha256.data(), 32) || u32(p + 124))
    return fail(e, "Character LOAD format/pin/identity/capability rejected");
  FieldCharacterLoadData d;
  d.identity_ = expected;
  std::memcpy(d.ir_.data(), p + 92, 32);
  Reader r{p + 128, n - 128};
  auto &bindings = d.bindings_;
  std::string *text_fields[] = {&bindings.name,
                                &bindings.level,
                                &bindings.exp,
                                &bindings.status,
                                &bindings.hp,
                                &bindings.pp,
                                &bindings.nickname,
                                &bindings.inventory,
                                &bindings.inventory_getter,
                                &bindings.learned_skills,
                                &bindings.permanent_boosts,
                                &bindings.affinities,
                                &bindings.untargetable,
                                &bindings.npc_skills,
                                &bindings.character_script,
                                &bindings.member_script,
                                &bindings.npc_script,
                                &bindings.inventory_script,
                                &bindings.inventory_type_field,
                                &bindings.inventory_items_field,
                                &bindings.inventory_getter_method,
                                &bindings.enemy_skill_script,
                                &bindings.enemy_id_field,
                                &bindings.enemy_weight_field,
                                &bindings.enemy_cooldown_field,
                                &bindings.enemy_remaining_field,
                                &bindings.item_script,
                                &bindings.item_native,
                                &bindings.enemy_skill_native,
                                &bindings.item_name_field,
                                &bindings.item_uid_field,
                                &bindings.item_equipped_field,
                                &bindings.item_doses_field,
                                &bindings.npc_nickname_prefix,
                                &bindings.npc_nickname_suffix};
  for (auto *field : text_fields) {
    *field = r.s();
    if (field->empty())
      r.ok = false;
  }
  bindings.normal_inventory_type = r.u();
  bindings.enemy_skill_id = r.u();
  bindings.item_constructor_id = r.u();
  bindings.stat_fields = r.strings();
  std::vector<FieldGlobalDataDefault> *default_groups[] = {
      &bindings.character_defaults,
      &bindings.member_defaults,
      &bindings.npc_defaults,
      &bindings.inventory_defaults,
      &bindings.enemy_skill_defaults,
      &bindings.item_defaults,
      &bindings.enemy_constructor_defaults};
  for (auto *group : default_groups) {
    auto count = r.count(128);
    std::set<std::string> names;
    if (!count)
      r.ok = false;
    while (count-- && r.ok) {
      FieldGlobalDataDefault f;
      f.name = r.s();
      f.kind = r.u();
      f.string_value = r.s();
      f.integer_value = r.bounded();
      if (f.name.empty() || !names.insert(f.name).second || f.kind < 1 ||
          f.kind > 6 ||
          ((f.kind == 2 || f.kind == 6) && !f.string_value.empty()) ||
          ((f.kind != 2 && f.kind != 6) && f.integer_value) ||
          (f.kind == 6 && f.integer_value > 1) ||
          (f.kind >= 3 && f.kind <= 5 && !f.string_value.empty()))
        r.ok = false;
      group->push_back(std::move(f));
    }
  }
  for (auto *group :
       {&bindings.member_load_order, &bindings.character_load_order,
        &bindings.npc_load_order}) {
    auto count = r.count(64);
    if (!count)
      r.ok = false;
    while (count-- && r.ok) {
      auto op = r.u();
      if (op < 1 || op > 27)
        r.ok = false;
      group->push_back(static_cast<FieldCharacterLoadStep>(op));
    }
  }
  {
    auto count = r.count(64);
    if (!count)
      r.ok = false;
    while (count-- && r.ok) {
      auto op = r.u();
      if (op < 1 || op > 14)
        r.ok = false;
      bindings.item_argument_order.push_back(
          static_cast<FieldCharacterItemArgument>(op));
    }
  }
  if (!validate_bindings(bindings))
    r.ok = false;
  d.stats_ = r.strings();
  d.hp_ = r.s();
  d.pp_ = r.s();
  d.npc_skill_getter_ = r.s();
  d.slots_ = r.strings();
  d.skills_ = r.strings();
  d.floors_ = r.ints();
  if (d.stats_.size() != 7 || !unique(d.stats_) || !unique(d.slots_) ||
      !unique(d.skills_) || d.npc_skill_getter_.empty() || d.hp_.empty() ||
      d.pp_.empty() || d.hp_ == d.pp_ || d.floors_.size() < 2 ||
      d.floors_.front() != 0 ||
      !std::is_sorted(d.floors_.begin(), d.floors_.end()) ||
      std::adjacent_find(d.floors_.begin(), d.floors_.end()) != d.floors_.end())
    r.ok = false;
  auto count = r.count();
  if (!count)
    r.ok = false;
  while (count-- && r.ok) {
    auto path = r.s();
    auto h = r.hash();
    if (path.empty() || !d.sources_.emplace(path, h).second)
      r.ok = false;
  }
  count = r.count(128);
  if (!count)
    r.ok = false;
  std::set<uint32_t> ids;
  std::set<std::string> names;
  while (count-- && r.ok) {
    FieldCharacterLoadRow x;
    x.id = r.u();
    x.role = r.u();
    x.name = r.s();
    x.display_name = r.s();
    x.nickname = r.s();
    x.level = r.bounded();
    x.exp = r.bounded();
    x.hp = r.bounded();
    x.pp = r.bounded();
    x.untargetable = r.boolean();
    x.permanent = r.ints();
    {
      auto count = r.count(7);
      std::set<std::string> keys;
      while (count-- && r.ok) {
        auto key = r.s();
        auto value = r.bounded();
        if (!keys.insert(key).second ||
            std::find(d.stats_.begin(), d.stats_.end(), key) == d.stats_.end())
          r.ok = false;
        x.permanent_fields.emplace_back(std::move(key), value);
      }
    }
    for (size_t i = 0; i < d.stats_.size(); ++i) {
      int64_t value = 0;
      for (const auto &field : x.permanent_fields)
        if (field.first == d.stats_[i])
          value = field.second;
      if (i >= x.permanent.size() || x.permanent[i] != value)
        r.ok = false;
    }

    x.skills = r.strings();
    auto c = r.count();
    std::set<std::string> aff;
    while (c-- && r.ok) {
      auto key = r.s();
      auto value = r.number();
      if (key.empty() || !aff.insert(key).second)
        r.ok = false;
      x.affinities.emplace_back(key, value);
    }
    c = r.count(7);
    while (c-- && r.ok)
      x.targets.push_back(r.ints());
    x.npc_stats = r.ints();
    c = r.count(256);
    while (c-- && r.ok) {
      FieldCharacterSavedItem a;
      a.name = r.s();
      a.equipped = r.boolean();
      a.doses = r.u();
      a.has_uid = r.boolean();
      a.uid = r.u();
      if (a.name.empty() || !a.doses || a.doses > 65535 ||
          (!a.has_uid && a.uid))
        r.ok = false;
      x.items.push_back(a);
    }
    c = r.count(256);
    while (c-- && r.ok) {
      FieldCharacterEnemySkill a;
      a.skill = r.s();
      a.weight = r.bounded();
      a.cooldown = r.bounded();
      if (a.skill.empty())
        r.ok = false;
      x.npc_skills.push_back(a);
    }
    if (!x.id || !ids.insert(x.id).second || x.role > 1 || x.name.empty() ||
        !names.insert(x.name).second || x.display_name.empty() ||
        x.permanent.size() != 7)
      r.ok = false;
    if (x.role == 0) {
      if (x.targets.size() != 7 || !x.npc_stats.empty() ||
          !x.npc_skills.empty() || x.untargetable || x.level < 1)
        r.ok = false;
      for (const auto &t : x.targets)
        if (t.size() < 2)
          r.ok = false;
    } else if (x.npc_stats.size() != 7 || !x.targets.empty() ||
               !x.items.empty() || !x.skills.empty() || !x.affinities.empty() ||
               !x.permanent_fields.empty())
      r.ok = false;
    d.rows_.push_back(std::move(x));
  }
  for (const auto *source :
       {&bindings.character_script, &bindings.member_script,
        &bindings.npc_script, &bindings.inventory_script,
        &bindings.enemy_skill_script, &bindings.item_script})
    if (!d.sources_.count(*source))
      r.ok = false;
  bool default_hash = false;
  for (const auto &v : d.sources_)
    if (!std::memcmp(v.second.data(), p + 60, 32))
      default_hash = true;
  if (!r.ok || r.n || !default_hash ||
      std::all_of(d.ir_.begin(), d.ir_.end(), [](uint8_t b) { return b == 0; }))
    return fail(e, "Character LOAD schema/coverage/source values rejected");
  d.valid_ = true;
  *this = std::move(d);
  return true;
}
bool FieldCharacterLoadData::load_file(const char *path,
                                       const FieldIdentity &expected,
                                       std::string &e) {
  auto *f = path ? std::fopen(path, "rb") : nullptr;
  if (!f)
    return fail(e, "Cannot open character LOAD resource");
  bool ok = std::fseek(f, 0, SEEK_END) == 0;
  auto length = std::ftell(f);
  ok = ok && length >= 128 && length <= 1024 * 1024 &&
       std::fseek(f, 0, SEEK_SET) == 0;
  std::vector<uint8_t> b(ok ? size_t(length) : 0);
  ok = ok && std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  if (!ok)
    return fail(e, "Character LOAD read rejected");
  return load(b.data(), b.size(), expected, e);
}
} // namespace encore::upstream

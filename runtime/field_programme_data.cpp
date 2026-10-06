#include "encore/field_programme.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
namespace encore::upstream {
namespace {
uint32_t u32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  for (size_t i = 0; i < n; ++i) {
    c ^= i >= 16 && i < 20 ? 0 : p[i];
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int32_t(c & 1)));
  }
  return ~c;
}
bool path(std::string_view p) {
  return !p.empty() && p.front() != '/' && p.back() != '/' &&
         p.find("..") == p.npos && p.find(':') == p.npos &&
         p.find('\\') == p.npos;
}
bool symbol(std::string_view s) {
  if (s.empty())
    return false;
  for (auto c : s)
    if (!(c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9')))
      return false;
  return true;
}
bool translation_key(std::string_view s) {
  // CSV option keys use a hyphen before OPT; source labels and language IDs
  // remain ordinary symbols. This is a key grammar, not a general path rule.
  if (s.empty()) return false;
  for (auto c : s)
    if (!(c == '-' || c == '_' || (c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
      return false;
  return true;
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 64;
  bool ok = true;
  uint32_t integer() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto x = u32(p + at);
    at += 4;
    return x;
  }
  uint32_t count(uint32_t maximum) {
    auto x = integer();
    if (x > maximum)
      ok = false;
    return ok ? x : 0;
  }
  bool boolean() {
    auto x = integer();
    if (x > 1)
      ok = false;
    return x != 0;
  }
  double scalar() {
    if (!ok || at > n || n - at < 8) {
      ok = false;
      return 0;
    }
    uint64_t bits = uint64_t(u32(p + at)) | (uint64_t(u32(p + at + 4)) << 32);
    at += 8;
    double x;
    std::memcpy(&x, &bits, 8);
    if (!std::isfinite(x) || std::abs(x) > 1e6)
      ok = false;
    return x;
  }
  std::string text() {
    auto len = count(65536);
    if (!ok || at > n || len > n - at) {
      ok = false;
      return {};
    }
    std::string x(reinterpret_cast<const char *>(p + at), len);
    at += len;
    size_t i = 0;
    uint32_t cp;
    while (i < x.size())
      if (!encore::utf8_next(x, i, cp) || cp == 0 || (cp < 32 && cp != 10)) {
        ok = false;
        break;
      }
    return x;
  }
  std::array<uint8_t, 32> hash() {
    std::array<uint8_t, 32> x{};
    if (!ok || at > n || n - at < 32) {
      ok = false;
      return x;
    }
    std::copy(p + at, p + at + 32, x.begin());
    at += 32;
    if (std::all_of(x.begin(), x.end(), [](uint8_t b) { return !b; }))
      ok = false;
    return x;
  }
};
bool supported(uint16_t raw) {
  using K = DialogueActionKind;
  switch (K(raw)) {
  case K::BeginCutscene:
  case K::GrantKeyItem:
  case K::ShowDialogue:
  case K::PlaySound:
  case K::SetFlag:
  case K::AwaitChoices:
  case K::AwaitDialogue:
  case K::Jump:
  case K::StopInteraction:
  case K::SetTalker:
  case K::CutsceneEnded:
  case K::DialogueDone:
    return true;
  default:
    return false;
  }
}
} // namespace
RoomProgram FieldProgrammeData::program(uint32_t i) const {
  return i < programs_.size() ? programs_[i].table : RoomProgram{};
}
RoomCommand FieldProgrammeData::command(uint32_t i) const {
  return i < commands_.size() ? commands_[i] : RoomCommand{};
}
std::string_view FieldProgrammeData::string(uint32_t i) const {
  return i < strings_.size() ? std::string_view(strings_[i])
                             : std::string_view{};
}
const FieldProgrammeRecord *FieldProgrammeData::record(uint32_t i) const {
  return i < programs_.size() ? &programs_[i] : nullptr;
}
const FieldProgrammeText *FieldProgrammeData::text(uint32_t i) const {
  for (const auto &x : texts_)
    if (x.id == i)
      return &x;
  return nullptr;
}
const FieldProgrammeChoice *FieldProgrammeData::choice(uint32_t i) const {
  return i < choices_.size() ? &choices_[i] : nullptr;
}
const BasementKeyItem *FieldProgrammeData::key(uint32_t i) const {
  for (const auto &x : keys_)
    if (x.id == i)
      return &x;
  return nullptr;
}
const std::string *FieldProgrammeData::flag(uint32_t i) const {
  return i < flags_.size() ? &flags_[i] : nullptr;
}
const std::string *FieldProgrammeData::sound(uint32_t i) const {
  return i < sounds_.size() ? &sounds_[i] : nullptr;
}
const std::string *FieldProgrammeData::source_label(uint32_t i) const {
  return i < labels_.size() ? &labels_[i] : nullptr;
}
bool FieldProgrammeData::find_program(std::string_view s, uint32_t &out) const {
  for (uint32_t i = 0; i < programs_.size(); ++i)
    if (programs_[i].path == s) {
      out = i;
      return true;
    }
  return false;
}
bool FieldProgrammeData::source_hash(std::string_view s,
                                     std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(s));
  if (i == sources_.end())
    return false;
  out = i->second;
  return true;
}
bool FieldProgrammeData::localize(
    uint32_t id, std::string_view lang,
    const std::function<bool(std::string_view, std::string &, std::string &)>
        &input_label,
    LocalizedHouseSpan &out, std::string &e) const {
  const auto *t = text(id);
  if (!valid_ || !t) {
    e = "Field programme text unavailable";
    return false;
  }
  auto i = t->locales.find(std::string(lang));
  if (i == t->locales.end()) {
    e = "Field programme locale unavailable";
    return false;
  }
  LocalizedHouseSpan result = out;
  result.speaker = i->second.speaker;
  result.segments.clear();
  for (size_t j = 0; j < i->second.segments.size(); ++j) {
    const auto &s = i->second.segments[j];
    LocalizedHouseSegment segment;
    segment.flags =
        (s.bullet ? 1u : 0u) | (j + 1 < i->second.segments.size() ? 2u : 4u);
    for (const auto &token : s.tokens) {
      auto kind = token.kind;
      auto value = token.text;
      if (kind == 12) {
        std::string resolved;
        if (!input_label || !input_label(value, resolved, e))
          return false;
        value = std::move(resolved);
        kind = uint32_t(HouseTokenKind::Literal);
      }
      segment.tokens.push_back({kind, std::move(value)});
    }
    result.segments.push_back(std::move(segment));
  }
  out = std::move(result);
  e.clear();
  return true;
}
bool FieldProgrammeData::load_file(const char *p, std::string &e) {
  if (!p) {
    e = "Field programme path absent";
    return false;
  }
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    e = "Field programme file unavailable";
    return false;
  }
  f.seekg(0, std::ios::end);
  auto n = f.tellg();
  if (n < 64 || n > 4 * 1024 * 1024) {
    e = "Field programme file size";
    return false;
  }
  f.seekg(0);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  if (!f.read(reinterpret_cast<char *>(b.data()), n)) {
    e = "Field programme file read";
    return false;
  }
  return load(b.data(), b.size(), e);
}
bool FieldProgrammeData::load(const uint8_t *p, size_t n, std::string &e) {
  auto reject = [&](const char *m) {
    e = m;
    return false;
  };
  if (!p || n < 64 || n > 4 * 1024 * 1024)
    return reject("Field programme pack size");
  if (std::memcmp(p, "ENCFPG01", 8) || u32(p + 8) != 1 || u32(p + 12) != n ||
      u32(p + 20) != 1 || u32(p + 24) != 1 || !u32(p + 28) ||
      u32(p + 28) > 1024 || u32(p + 16) != crc(p, n))
    return reject("Field programme header/version/capability/rules/CRC");
  for (size_t i = 52; i < 64; ++i)
    if (p[i])
      return reject("Field programme header reserved");
  FieldProgrammeData d;
  std::copy(p + 32, p + 52, d.pin_.begin());
  if (std::all_of(d.pin_.begin(), d.pin_.end(), [](uint8_t x) { return !x; }))
    return reject("Field programme source pin absent");
  Reader r{p, n};
  d.scene_ = r.text();
  d.hint_color_ = r.text();
  d.flag_emit_ = r.boolean();
  d.npc_.id = r.integer();
  d.npc_.ready_ordinal = r.integer();
  d.npc_.node = r.text();
  if (!r.ok || !path(d.scene_) || !path(d.npc_.node) || !d.npc_.id ||
      !d.npc_.ready_ordinal || d.hint_color_.size() != 6 ||
      d.hint_color_.find_first_not_of("0123456789abcdef") != std::string::npos)
    return reject("Field programme NPC source/format");
  auto count = r.count(4096);
  if (!count)
    return reject("Field programme NPC rows absent");
  std::set<std::pair<bool, uint32_t>> defaults;
  std::pair<bool, uint32_t> previous{};
  uint32_t prev_ordinal = 0;
  for (uint32_t i = 0; i < count; ++i) {
    FieldProgrammeNpcRow row;
    row.thoughts = r.boolean();
    row.group = r.integer();
    row.ordinal = r.integer();
    row.last = r.boolean();
    row.supported = r.boolean();
    row.flag = r.text();
    row.program = r.text();
    row.source = r.text();
    row.sha = r.hash();
    auto group = std::make_pair(row.thoughts, row.group);
    if (!r.ok || !row.ordinal || (!row.flag.empty() && !symbol(row.flag)) ||
        !path(row.program) || !path(row.source) ||
        (i && (group < previous ||
               (group == previous && row.ordinal <= prev_ordinal))))
      return reject("Field programme ordered NPC row");
    if (row.flag.empty())
      defaults.insert(group);
    previous = group;
    prev_ordinal = row.ordinal;
    d.npc_.rows.push_back(std::move(row));
  }
  if (defaults.size() != 2 || !defaults.count({false, 0}) ||
      !defaults.count({true, 0}))
    return reject("Field programme NPC source defaults");
  count = r.count(4096);
  std::set<std::string> names;
  for (uint32_t i = 0; i < count; ++i) {
    auto s = r.text();
    if (!r.ok || !symbol(s) || !names.insert(s).second)
      return reject("Field programme source flag");
    d.flags_.push_back(std::move(s));
  }
  count = r.count(4096);
  names.clear();
  for (uint32_t i = 0; i < count; ++i) {
    auto s = r.text();
    if (!r.ok || !path(s) || !names.insert(s).second)
      return reject("Field programme audio source");
    d.sounds_.push_back(std::move(s));
  }
  count = r.count(256);
  std::set<uint32_t> ids;
  for (uint32_t i = 0; i < count; ++i) {
    BasementKeyItem k;
    k.id = r.integer();
    k.doses = r.integer();
    k.grant = r.boolean();
    k.source = r.text();
    k.name_key = r.text();
    if (!r.ok || !k.id || !ids.insert(k.id).second || !k.doses ||
        k.doses > 65535 || !k.grant || !symbol(k.source) || !symbol(k.name_key))
      return reject("Field programme key source policy");
    d.keys_.push_back(std::move(k));
  }
  count = r.count(4096);
  ids.clear();
  for (uint32_t i = 0; i < count; ++i) {
    FieldProgrammeText t;
    t.id = r.integer();
    t.source = r.text();
    t.label = r.text();
    t.key = r.text();
    t.speaker_key = r.text();
    t.voice = r.text();
    if (!r.ok || !t.id || !ids.insert(t.id).second || !path(t.source) ||
        !symbol(t.label) || !symbol(t.key) ||
        (!t.speaker_key.empty() && !symbol(t.speaker_key)) ||
        (!t.voice.empty() && !path(t.voice)))
      return reject("Field programme text source identity");
    auto langs = r.count(16);
    if (!langs)
      return reject("Field programme text locale count");
    for (uint32_t j = 0; j < langs; ++j) {
      auto lang = r.text();
      FieldProgrammeLocale l;
      l.speaker = r.text();
      auto segments = r.count(64);
      if (!symbol(lang) || !segments)
        return reject("Field programme locale/segment count");
      for (uint32_t q = 0; q < segments; ++q) {
        FieldProgrammeSegment s;
        s.bullet = r.boolean();
        auto tokens = r.count(4096);
        bool hint = false;
        for (uint32_t z = 0; z < tokens; ++z) {
          FieldProgrammeToken token;
          token.kind = r.integer();
          token.text = r.text();
          if (!r.ok ||
              !(token.kind == 1 || token.kind == 2 || token.kind == 3 ||
                token.kind == 4 || token.kind == 11 || token.kind == 12) ||
              (token.kind == 12 && !symbol(token.text)) ||
              (token.kind == 3 && token.text != d.hint_color_) ||
              ((token.kind == 2 || token.kind == 4 || token.kind == 11) &&
               !token.text.empty()))
            return reject("Field programme text token capability");
          if (token.kind == 3) {
            if (hint)
              return reject("Field programme nested hint");
            hint = true;
          }
          if (token.kind == 4) {
            if (!hint)
              return reject("Field programme unmatched hint");
            hint = false;
          }
          s.tokens.push_back(std::move(token));
        }
        if (hint)
          return reject("Field programme unclosed hint");
        l.segments.push_back(std::move(s));
      }
      if (!r.ok || !t.locales.emplace(std::move(lang), std::move(l)).second)
        return reject("Field programme duplicate locale");
    }
    d.texts_.push_back(std::move(t));
  }
  count = r.count(4096);
  names.clear();
  for (uint32_t i = 0; i < count; ++i) {
    FieldProgrammeChoice c;
    c.identity = r.text();
    c.program = r.text();
    c.label = r.text();
    c.initial_selection = r.integer();
    c.cancel_target_pc = r.integer();
    auto options = r.count(64);
    if (!r.ok || c.identity.empty() || !names.insert(c.identity).second ||
        !path(c.program) || !symbol(c.label) || !options ||
        c.initial_selection >= options)
      return reject("Field programme choice schema");
    std::set<std::string> option_keys;
    for (uint32_t j = 0; j < options; ++j) {
      FieldProgrammeOption o;
      o.key = r.text();
      o.target_pc = r.integer();
      auto langs = r.count(16);
      if (!r.ok || !translation_key(o.key) ||
          !option_keys.insert(o.key).second || !langs)
        return reject("Field programme choice translation key");
      for (uint32_t q = 0; q < langs; ++q) {
        auto lang = r.text();
        auto value = r.text();
        if (!r.ok || !symbol(lang) || value.empty() ||
            !o.texts.emplace(std::move(lang), std::move(value)).second)
          return reject("Field programme choice locale");
      }
      c.options.push_back(std::move(o));
    }
    d.choices_.push_back(std::move(c));
  }
  count = r.count(1024);
  if (count != u32(p + 28))
    return reject("Field programme source count");
  names.clear();
  ids.clear();
  for (uint32_t i = 0; i < count; ++i) {
    FieldProgrammeRecord pr;
    pr.path = r.text();
    pr.source = r.text();
    pr.table.stable_id = r.integer();
    pr.table.first_command = r.integer();
    pr.table.command_count = r.count(65535);
    pr.table.phrase_count = r.count(4096);
    pr.table.source_path_string = uint32_t(d.strings_.size());
    d.strings_.push_back(pr.path);
    if (!r.ok || !path(pr.path) || !path(pr.source) || !pr.table.stable_id ||
        !ids.insert(pr.table.stable_id).second ||
        !names.insert(pr.path).second ||
        pr.table.first_command != d.commands_.size() ||
        !pr.table.command_count || !pr.table.phrase_count ||
        d.commands_.size() + pr.table.command_count > 65535)
      return reject("Field programme instruction span");
    for (uint32_t j = 0; j < pr.table.command_count; ++j) {
      RoomCommand c;
      auto op = r.integer();
      auto actor = r.integer();
      c.opcode = uint16_t(op);
      c.actor_index = uint16_t(actor);
      c.phrase = r.integer();
      c.target_index = r.integer();
      c.flags = r.integer();
      c.auxiliary_index = r.integer();
      c.value = r.scalar();
      c.duration = r.scalar();
      auto label = r.text();
      if (!r.ok || op > 65535 || actor != kRoomNoActor ||
          !supported(c.opcode) || c.phrase >= pr.table.phrase_count ||
          c.auxiliary_index != kRoomNoIndex || !symbol(label))
        return reject("Field programme opcode/actor/source label");
      using K = DialogueActionKind;
      auto k = K(c.opcode);
      bool target = k == K::GrantKeyItem || k == K::ShowDialogue ||
                    k == K::PlaySound || k == K::SetFlag ||
                    k == K::AwaitChoices || k == K::Jump;
      if (!target && c.target_index != kRoomNoIndex)
        return reject("Field programme unused target");
      if ((k == K::SetFlag ? c.value != 1 : c.value != 0) ||
          (k == K::DialogueDone ? c.duration <= 0 || c.duration > 60
                                : c.duration != 0) ||
          (k == K::ShowDialogue || k == K::StopInteraction ? c.flags != 1
                                                           : c.flags != 0))
        return reject("Field programme typed payload");
      if ((k == K::ShowDialogue && !d.text(c.target_index)) ||
          (k == K::GrantKeyItem && !d.key(c.target_index)) ||
          (k == K::SetFlag && !d.flag(c.target_index)) ||
          (k == K::PlaySound && !d.sound(c.target_index)) ||
          (k == K::AwaitChoices && !d.choice(c.target_index)) ||
          (k == K::Jump &&
           (c.target_index <= j || c.target_index >= pr.table.command_count)))
        return reject("Field programme command resource reference");
      d.commands_.push_back(c);
      d.labels_.push_back(std::move(label));
    }
    auto first = pr.table.first_command,
         last = first + pr.table.command_count - 1;
    if (d.commands_[first].opcode !=
            uint16_t(DialogueActionKind::BeginCutscene) ||
        d.commands_[last].opcode != uint16_t(DialogueActionKind::DialogueDone))
      return reject("Field programme entry/terminator");
    d.programs_.push_back(std::move(pr));
  }
  count = r.count(4096);
  if (!count)
    return reject("Field programme source closure absent");
  for (uint32_t i = 0; i < count; ++i) {
    auto source = r.text();
    auto hash = r.hash();
    if (!r.ok || !path(source) ||
        !d.sources_.emplace(std::move(source), hash).second)
      return reject("Field programme source receipt");
  }
  if (!r.ok || r.at != n || !d.sources_.count(d.scene_))
    return reject("Field programme trailing bytes/source scene");
  for (const auto &row : d.npc_.rows) {
    uint32_t program = 0;
    auto h = d.sources_.find(row.source);
    if (h == d.sources_.end() || h->second != row.sha ||
        row.supported != d.find_program(row.program, program))
      return reject("Field programme NPC source hash/admission");
  }
  for (const auto &s : d.sounds_)
    if (!d.sources_.count(s))
      return reject("Field programme audio source closure");
  for (const auto &t : d.texts_)
    if (!d.sources_.count(t.source) ||
        (!t.voice.empty() && !d.sources_.count(t.voice)))
      return reject("Field programme text source closure");
  for (const auto &pr : d.programs_) {
    if (!d.sources_.count(pr.source))
      return reject("Field programme programme source closure");
    for (uint32_t j = 0; j < pr.table.command_count; ++j) {
      auto index = pr.table.first_command + j;
      const auto &c = d.commands_[index];
      if (c.opcode == uint16_t(DialogueActionKind::ShowDialogue)) {
        auto t = d.text(c.target_index);
        if (!t || t->source != pr.source || t->label != d.labels_[index])
          return reject("Field programme text/phrase ownership");
      }
      if (c.opcode == uint16_t(DialogueActionKind::Jump)) {
        auto target = pr.table.first_command + c.target_index;
        if (!c.target_index ||
            d.commands_[target - 1].phrase == d.commands_[target].phrase)
          return reject("Field programme jump phrase entry");
      }
      if (c.opcode == uint16_t(DialogueActionKind::AwaitChoices)) {
        auto choice = d.choice(c.target_index);
        if (!choice || choice->program != pr.path ||
            choice->label != d.labels_[index])
          return reject("Field programme choices ownership");
      }
    }
  }
  for (const auto &c : d.choices_) {
    uint32_t pi = 0;
    if (!d.find_program(c.program, pi))
      return reject("Field programme choice program");
    auto pr = d.program(pi);
    auto target = [&](uint32_t pc) {
      return pc < pr.command_count &&
             (!pc || d.commands_[pr.first_command + pc - 1].phrase !=
                         d.commands_[pr.first_command + pc].phrase);
    };
    if (!target(c.cancel_target_pc))
      return reject("Field programme cancel target");
    for (const auto &o : c.options)
      if (!target(o.target_pc))
        return reject("Field programme option target");
  }
  // Bounded reviewed phrase structure, independent of game strings/content IDs.
  for (const auto &pr : d.programs_) {
    using K = DialogueActionKind;
    uint32_t pc = 1;
    auto base = pr.table.first_command;
    while (pc < pr.table.command_count) {
      auto read_kind = [&]() {
        return pc < pr.table.command_count ? K(d.commands_[base + pc].opcode)
                                           : K::BeginCutscene;
      };
      auto phrase = d.commands_[base + pc].phrase;
      auto label = d.labels_[base + pc];
      auto consume = [&](K expected) {
        if (pc >= pr.table.command_count || read_kind() != expected ||
            d.commands_[base + pc].phrase != phrase ||
            d.labels_[base + pc] != label)
          return false;
        ++pc;
        return true;
      };
      if (read_kind() == K::GrantKeyItem && !consume(K::GrantKeyItem))
        return reject("Field programme item phrase");
      if (!consume(K::ShowDialogue))
        return reject("Field programme source phrase text entry");
      if (read_kind() == K::PlaySound && !consume(K::PlaySound))
        return reject("Field programme phrase sound order");
      if (read_kind() == K::SetFlag && !consume(K::SetFlag))
        return reject("Field programme phrase flag order");
      if (read_kind() == K::AwaitChoices) {
        if (!consume(K::AwaitChoices))
          return reject("Field programme choice phrase gate");
      } else {
        if (!consume(K::AwaitDialogue))
          return reject("Field programme phrase dialogue gate");
        if (read_kind() == K::Jump) {
          if (!consume(K::Jump))
            return reject("Field programme phrase goto");
        } else if (!consume(K::StopInteraction) || !consume(K::SetTalker) ||
                   !consume(K::CutsceneEnded) || !consume(K::DialogueDone))
          return reject("Field programme source ending order");
      }
      if (pc < pr.table.command_count &&
          d.commands_[base + pc].phrase == phrase)
        return reject("Field programme overlapping phrase boundary");
    }
  }
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream

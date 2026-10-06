#include "encore/item_details.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>

namespace encore::upstream {
namespace {
uint32_t u32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
uint16_t u16(const uint8_t *p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
float real(const uint8_t *p) {
  const auto bits = u32(p);
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}
bool reject(std::string &e, const char *s) {
  e = s;
  return false;
}
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = ~uint32_t(0);
  for (size_t i = 0; i < n; ++i) {
    c ^= i >= 16 && i < 20 ? 0 : p[i];
    for (int j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int32_t(c & 1)));
  }
  return ~c;
}
size_t scalar_size(unsigned char c) {
  return c < 128 ? 1 : c < 224 ? 2 : c < 240 ? 3 : 4;
}
bool utf8(std::string_view s, bool newline = false) {
  size_t i = 0;
  while (i < s.size()) {
    uint32_t c = uint8_t(s[i++]);
    if (c < 128) {
      if ((c < 32 && !(newline && c == 10)) || c == 127)
        return false;
      continue;
    }
    uint32_t n = 0, min = 0;
    if (c >= 0xc2 && c <= 0xdf) {
      n = 1;
      c &= 31;
      min = 128;
    } else if (c >= 0xe0 && c <= 0xef) {
      n = 2;
      c &= 15;
      min = 2048;
    } else if (c >= 0xf0 && c <= 0xf4) {
      n = 3;
      c &= 7;
      min = 65536;
    } else
      return false;
    if (i + n > s.size())
      return false;
    while (n--) {
      auto b = uint8_t(s[i++]);
      if ((b & 0xc0) != 0x80)
        return false;
      c = (c << 6) | (b & 63);
    }
    if (c < min || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
      return false;
  }
  return true;
}
bool safe_path(std::string_view s) {
  if (s.empty() || s.front() == '/' || s.back() == '/' ||
      s.find_first_of("\\:") != s.npos ||
      std::any_of(s.begin(), s.end(),
                  [](unsigned char c) { return c < 32 || c == 127; }))
    return false;
  size_t i = 0;
  while (i < s.size()) {
    auto end = s.find('/', i);
    if (end == s.npos)
      end = s.size();
    auto part = s.substr(i, end - i);
    if (part.empty() || part == "." || part == "..")
      return false;
    i = end + 1;
  }
  return true;
}
bool format(std::string_view s) {
  auto at = s.find("%s");
  return at != s.npos && s.find('%') == at && s.find('%', at + 2) == s.npos &&
         s.find_first_of("[]{}\n") == s.npos;
}
std::string number_phrase(std::string_view pattern, uint32_t n) {
  std::string s(pattern);
  const auto at = s.find("%s");
  s.replace(at, 2, std::to_string(n));
  return s;
}
} // namespace
bool ItemDetailsView::field_family() const {
  return valid() && !std::memcmp(bytes_, "ENCFID01", 8);
}
uint32_t ItemDetailsView::count(ItemDetailsSection s) const {
  const auto k = uint32_t(s);
  return valid() && k >= 1 && k <= 7 ? u32(bytes_ + 64 + (k - 1) * 16 + 8) : 0;
}
const uint8_t *ItemDetailsView::record(ItemDetailsSection s, uint32_t i) const {
  if (i >= count(s))
    return nullptr;
  const auto *d = bytes_ + 64 + (uint32_t(s) - 1) * 16;
  return bytes_ + u32(d + 4) + size_t(i) * u16(d + 2);
}
std::string_view ItemDetailsView::string(uint32_t i) const {
  const auto *p = record(ItemDetailsSection::Strings, i);
  if (!p || (i && p[-1]))
    return {};
  const auto *end =
      bytes_ + u32(bytes_ + 68) + count(ItemDetailsSection::Strings);
  const auto *q = std::find(p, end, uint8_t(0));
  return {reinterpret_cast<const char *>(p), size_t(q - p)};
}
ItemDetailsDefinition ItemDetailsView::definition(uint32_t i) const {
  auto *p = record(ItemDetailsSection::Definitions, i);
  return p ? ItemDetailsDefinition{u32(p),
                                   u32(p + 4),
                                   u32(p + 8),
                                   u32(p + 12),
                                   u32(p + 16),
                                   field_family() ? u32(p + 20) : item_no_index}
           : ItemDetailsDefinition{};
}
ItemDetailsLocale ItemDetailsView::locale(uint32_t i) const {
  auto *p = record(ItemDetailsSection::Locales, i);
  return p ? ItemDetailsLocale{u32(p),      u32(p + 4),  u32(p + 8),
                               u32(p + 12), u32(p + 16), u32(p + 20),
                               u32(p + 24), u32(p + 28)}
           : ItemDetailsLocale{};
}
ItemDetailsPresentation ItemDetailsView::presentation(uint32_t i) const {
  auto *p = record(ItemDetailsSection::Presentations, i);
  return p ? ItemDetailsPresentation{u32(p), u32(p + 4), u32(p + 8),
                                     u32(p + 12)}
           : ItemDetailsPresentation{};
}
ItemDetailsToken ItemDetailsView::token(uint32_t i) const {
  auto *p = record(ItemDetailsSection::Tokens, i);
  return p ? ItemDetailsToken{ItemDetailsTokenKind(u32(p)), u32(p + 4),
                              u32(p + 8), u32(p + 12)}
           : ItemDetailsToken{};
}
ItemDetailsResource ItemDetailsView::resource(uint32_t i) const {
  ItemDetailsResource r;
  auto *p = record(ItemDetailsSection::Resources, i);
  if (p) {
    r.id = u32(p);
    r.path = u32(p + 4);
    r.kind = u32(p + 8);
    r.width = u32(p + 12);
    r.height = u32(p + 16);
    r.columns = u32(p + 20);
    r.rows = u32(p + 24);
    std::memcpy(r.sha256, p + 28, 32);
    r.bytes = u32(p + 60);
    r.crc32 = u32(p + 64);
  }
  return r;
}
float ItemDetailsView::parameter(ItemDetailsParameter k) const {
  auto *p = record(ItemDetailsSection::Parameters, uint32_t(k) - 1);
  return p && u32(p) == uint32_t(k) ? real(p + 4) : 0;
}
std::string ItemDetailsView::reviewed_commit() const {
  std::string s;
  const char *h = "0123456789abcdef";
  if (valid())
    for (size_t i = 32; i < 52; ++i) {
      s += h[bytes_[i] >> 4];
      s += h[bytes_[i] & 15];
    }
  return s;
}
bool ItemDetailsView::bind_items(ItemView items, std::string &e) const {
  if (!valid() || field_family() || !items.valid() ||
      reviewed_commit() != items.reviewed_commit() ||
      count(ItemDetailsSection::Definitions) !=
          items.count(ItemSection::Definitions))
    return reject(e, "Item details source/definition count binding rejected");
  for (uint32_t i = 0; i < count(ItemDetailsSection::Definitions); ++i) {
    auto d = definition(i);
    if (d.definition >= items.count(ItemSection::Definitions))
      return reject(e, "Item details definition binding rejected");
    const auto original = items.definition(d.definition);
    if (string(d.source) != items.string(original.source) ||
        string(d.raw_description) != items.string(original.description))
      return reject(e, "Item details source/raw description binding rejected");
  }
  e.clear();
  return true;
}
bool ItemDetailsView::bind_field_items(const FieldItemDefinitions &items,
                                       std::string &e) const {
  if (!valid() || !field_family() || !items.valid() ||
      count(ItemDetailsSection::Definitions) != items.definitions().size())
    return reject(e, "Field item details source/count binding rejected");
  const char *h = "0123456789abcdef";
  std::string pin;
  for (auto b : items.source_pin()) {
    pin += h[b >> 4];
    pin += h[b & 15];
  }
  if (pin != reviewed_commit())
    return reject(e, "Field item details reviewed pin rejected");
  for (uint32_t i = 0; i < count(ItemDetailsSection::Definitions); ++i) {
    const auto d = definition(i);
    const auto *original = items.definition(d.definition);
    if (!original || string(d.source) != original->source ||
        string(d.raw_description) != original->description_key ||
        d.max_doses != original->doses)
      return reject(e,
                    "Field item details identity/description/doses rejected");
    int32_t value = 0;
    for (auto v : original->boost)
      if (v > 0) {
        value = v;
        break;
      }
    if (!value)
      value = original->heal_pp > 0   ? original->heal_pp
              : original->heal_hp > 0 ? original->heal_hp
                                      : 0;
    if (d.item_value != uint32_t(value))
      return reject(e, "Field item details value binding rejected");
  }
  e.clear();
  return true;
}
bool ItemDetailsView::verify_resources(const char *root, std::string &e) const {
  if (!valid() || !root || !*root)
    return reject(e, "Item details resource root missing");
  for (uint32_t i = 0; i < count(ItemDetailsSection::Resources); ++i) {
    const auto r = resource(i);
    std::string path(root);
    if (path.back() != '/')
      path += '/';
    path += string(r.path);
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
      return reject(e, "Item details inline image unavailable");
    uint8_t block[4096];
    bool ok = true;
    size_t total = 0;
    uint32_t hash = ~uint32_t(0);
    for (;;) {
      size_t n = std::fread(block, 1, sizeof block, f);
      if (total + n > r.bytes) {
        ok = false;
        break;
      }
      total += n;
      hash = encore::crc32_update(hash, block, n);
      if (n < sizeof block) {
        ok = !std::ferror(f);
        break;
      }
    }
    if (std::fclose(f))
      ok = false;
    if (!ok || total != r.bytes || ~hash != r.crc32)
      return reject(e, "Item details converted texture size/CRC mismatch");
  }
  e.clear();
  return true;
}
bool ItemDetailsData::load(const uint8_t *p, size_t n, std::string &e) {
  const bool field = p && n >= 8 && !std::memcmp(p, "ENCFID01", 8);
  if (!p || n < 176 || n > 1024 * 1024 ||
      (!field && std::memcmp(p, "ENCITD01", 8)) || u32(p + 8) != 1 ||
      u32(p + 12) != n || u32(p + 16) != crc(p, n) || u32(p + 20) != 7 ||
      u32(p + 24) != 1 || u32(p + 28) != 1)
    return reject(e,
                  "Item details header/version/capability/rules/CRC rejected");
  bool pin = false;
  for (size_t i = 32; i < 52; ++i)
    pin |= p[i] != 0;
  if (!pin)
    return reject(e, "Item details reviewed pin missing");
  for (size_t i = 52; i < 64; ++i)
    if (p[i])
      return reject(e, "Item details reserved header rejected");
  const uint32_t strides[] = {1, field ? 24u : 20u, 32, 16, 16, 68, 8};
  size_t end = 176;
  for (uint32_t i = 0; i < 7; ++i) {
    const auto *d = p + 64 + i * 16;
    const auto count = u32(d + 8);
    const size_t aligned = (end + 3) & ~size_t(3);
    if (aligned > n)
      return reject(e, "Item details truncated padding");
    for (size_t j = end; j < aligned; ++j)
      if (p[j])
        return reject(e, "Item details padding rejected");
    end = aligned;
    if (u16(d) != i + 1 || u16(d + 2) != strides[i] || u32(d + 4) != end ||
        count > 65536 || u32(d + 12) != size_t(count) * strides[i] ||
        size_t(count) * strides[i] > n - end)
      return reject(e, "Item details directory/bounds rejected");
    end += size_t(count) * strides[i];
  }
  if (end != n)
    return reject(e, "Item details trailing bytes rejected");
  ItemDetailsView v;
  v.bytes_ = p;
  v.size_ = n;
  std::set<uint32_t> strings;
  const auto ns = v.count(ItemDetailsSection::Strings);
  if (!ns || p[u32(p + 68) + ns - 1])
    return reject(e, "Item details unterminated strings");
  for (uint32_t i = 0; i < ns;) {
    strings.insert(i);
    auto s = v.string(i);
    if (s.size() > 8192 || !utf8(s, true))
      return reject(e, "Item details UTF-8/control bytes rejected");
    i += uint32_t(s.size()) + 1;
  }
  auto str = [&](uint32_t i, bool empty = false) {
    return strings.count(i) != 0 && (empty || !v.string(i).empty());
  };
  const auto nd = v.count(ItemDetailsSection::Definitions),
             np = v.count(ItemDetailsSection::Presentations),
             nr = v.count(ItemDetailsSection::Resources);
  if ((field ? (nd < 1 || nd > 256) : (nd != 2)) ||
      v.count(ItemDetailsSection::Locales) != 2 || np != nd * 2 ||
      !v.count(ItemDetailsSection::Tokens) ||
      v.count(ItemDetailsSection::Tokens) > (field ? 32768u : 1024u) ||
      (field ? (nr < 2 || nr > 258) : (nr != 1)) ||
      v.count(ItemDetailsSection::Parameters) != 4)
    return reject(e, "Item details scoped section counts rejected");
  std::set<uint32_t> defs;
  std::set<std::string_view> sources;
  for (uint32_t i = 0; i < nd; ++i) {
    const auto d = v.definition(i);
    if (!defs.insert(d.definition).second ||
        !sources.insert(v.string(d.source)).second || !str(d.source) ||
        !safe_path(v.string(d.source)) || !str(d.raw_description) ||
        !d.max_doses || d.max_doses > 65535 || d.item_value > 65535 ||
        (field && (!d.definition ||
                   (d.item_icon != item_no_index && d.item_icon >= nr))))
      return reject(e, "Item details definition rejected");
  }
  std::set<std::string_view> locales;
  for (uint32_t i = 0; i < 2; ++i) {
    auto l = v.locale(i);
    auto name = v.string(l.locale), separator = v.string(l.separator);
    if (!str(l.locale) || !locales.insert(name).second ||
        (name != "en" && name != "zh_Hans_CN") || !str(l.separator, true) ||
        !utf8(separator) ||
        (!separator.empty() &&
         scalar_size(uint8_t(separator.front())) != separator.size()) ||
        !str(l.total) || !str(l.left_singular) || !str(l.left_plural) ||
        !str(l.image_measure) || !format(v.string(l.total)) ||
        !format(v.string(l.left_singular)) ||
        !format(v.string(l.left_plural)) || !utf8(v.string(l.image_measure)) ||
        v.string(l.image_measure).size() > 32)
      return reject(e, "Item details locale/phrase rejected");
    for (auto c : {l.base_color, l.hint_color})
      if ((c >> 24) != 255)
        return reject(e, "Item details text alpha rejected");
  }
  for (uint32_t i = 0; i < 4; ++i) {
    auto *r = v.record(ItemDetailsSection::Parameters, i);
    const auto f = real(r + 4);
    if (u32(r) != i + 1 || !std::isfinite(f))
      return reject(e, "Item details finite parameter rejected");
    if (i == 0   ? (f <= 0 || f > 128)
        : i == 1 ? (std::abs(f) > 128)
                 : (f < 1 || f > 64 || f != std::floor(f)))
      return reject(e, "Item details parameter range rejected");
  }
  std::set<uint32_t> resources;
  for (uint32_t i = 0; i < v.count(ItemDetailsSection::Resources); ++i) {
    auto r = v.resource(i);
    auto path = v.string(r.path);
    if (!r.id || !resources.insert(r.id).second || !str(r.path) ||
        !safe_path(path) || path.substr(0, 9) != "graphics/" ||
        path.size() < 4 || path.substr(path.size() - 4) != ".t3x" ||
        r.kind != 1 || !r.width || !r.height || r.width > 8192 ||
        r.height > 8192 || r.columns != 1 || r.rows != 1 || !r.bytes ||
        r.bytes > 4 * 1024 * 1024 ||
        !std::any_of(r.sha256, r.sha256 + 32, [](uint8_t b) { return b != 0; }))
      return reject(e, "Item details graphic source rejected");
  }
  std::set<std::pair<uint32_t, uint32_t>> pairs;
  uint32_t next = 0;
  for (uint32_t i = 0; i < np; ++i) {
    auto a = v.presentation(i);
    if (!defs.count(a.definition) || a.locale >= 2 ||
        !pairs.insert({a.definition, a.locale}).second ||
        a.first_token != next || !a.token_count ||
        a.token_count > v.count(ItemDetailsSection::Tokens) - next)
      return reject(e, "Item details presentation range rejected");
    next += a.token_count;
    bool nickname = false, doses = false;
    for (uint32_t j = 0; j < a.token_count; ++j) {
      auto t = v.token(a.first_token + j);
      const auto kind = uint32_t(t.kind);
      if (kind < 1 || kind > 6 || t.reserved || t.color > 1)
        return reject(e, "Item details token/opcode rejected");
      if (t.kind == ItemDetailsTokenKind::Text) {
        if (!str(t.value) || !utf8(v.string(t.value)) ||
            v.string(t.value).find_first_of(field ? "[]{}" : "[]{}%") !=
                std::string_view::npos)
          return reject(e, "Item details literal controls rejected");
      } else if (t.kind == ItemDetailsTokenKind::InlineImage) {
        if (t.value >= v.count(ItemDetailsSection::Resources))
          return reject(e, "Item details inline image reference rejected");
      } else if (t.value)
        return reject(e, "Item details unused token operand rejected");
      if (t.kind == ItemDetailsTokenKind::Nickname) {
        if (nickname)
          return reject(e, "Item details repeated nickname rejected");
        nickname = true;
      }
      if (t.kind == ItemDetailsTokenKind::Doses) {
        if (doses)
          return reject(e, "Item details repeated dose rejected");
        doses = true;
      }
    }
    ItemDetailsDefinition d;
    for (uint32_t j = 0; j < nd; ++j)
      if (v.definition(j).definition == a.definition)
        d = v.definition(j);
    if ((d.max_doses > 1) != doses)
      return reject(e, "Item details dose policy binding rejected");
  }
  if (next != v.count(ItemDetailsSection::Tokens))
    return reject(e, "Item details unowned tokens rejected");
  std::vector<uint8_t> candidate(p, p + n);
  bytes_.swap(candidate);
  e.clear();
  return true;
}
bool ItemDetailsData::load_file(const char *path, std::string &e) {
  if (!path || !*path)
    return reject(e, "Item details path missing");
  FILE *f = std::fopen(path, "rb");
  if (!f)
    return reject(e, "Item details file unavailable");
  std::vector<uint8_t> b;
  uint8_t block[4096];
  bool ok = true;
  for (;;) {
    size_t n = std::fread(block, 1, sizeof block, f);
    if (b.size() + n > 1024 * 1024) {
      ok = false;
      break;
    }
    b.insert(b.end(), block, block + n);
    if (n < sizeof block) {
      ok = !std::ferror(f);
      break;
    }
  }
  if (std::fclose(f))
    ok = false;
  if (!ok)
    return reject(e, "Item details file read rejected");
  return load(b.data(), b.size(), e);
}

bool ItemDetailsView::compose(uint32_t id, uint32_t doses,
                              std::string_view nickname,
                              std::string_view language, float width,
                              const ItemDetailsMeasure &measure,
                              ItemDetailsComposition &out,
                              std::string &e) const {
  if (!valid() || !measure || !std::isfinite(width) || width <= 0 ||
      width > 8192 || nickname.size() > 256 || !utf8(nickname))
    return reject(e, "Item details presentation input rejected");
  ItemDetailsDefinition d;
  uint32_t di = item_no_index, li = item_no_index, pi = item_no_index;
  for (uint32_t i = 0; i < count(ItemDetailsSection::Definitions); ++i)
    if (definition(i).definition == id) {
      d = definition(i);
      di = i;
    }
  for (uint32_t i = 0; i < count(ItemDetailsSection::Locales); ++i)
    if (string(locale(i).locale) == language)
      li = i;
  for (uint32_t i = 0; i < count(ItemDetailsSection::Presentations); ++i) {
    auto a = presentation(i);
    if (a.definition == id && a.locale == li)
      pi = i;
  }
  if (di == item_no_index || li == item_no_index || pi == item_no_index ||
      doses > d.max_doses)
    return reject(e, "Item details identity/locale/doses rejected");
  const auto loc = locale(li);
  const auto spec = presentation(pi);
  const auto sep = string(loc.separator);
  const auto image_measure = string(loc.image_measure);
  std::string cut;
  const uint32_t limit =
      uint32_t(parameter(language == "en" ? ItemDetailsParameter::EnNameLimit
                                          : ItemDetailsParameter::ZhNameLimit));
  for (size_t i = 0, count = 0; i < nickname.size() && count < limit; ++count) {
    const size_t size = scalar_size(uint8_t(nickname[i]));
    cut.append(nickname.substr(i, size));
    i += size;
  }
  struct Unit {
    std::string text;
    uint32_t image = item_no_index, color = 0;
    bool newline = false;
  };
  std::vector<Unit> units;
  auto append = [&](std::string_view s, uint32_t color) {
    for (size_t i = 0; i < s.size();) {
      const size_t n = scalar_size(uint8_t(s[i]));
      units.push_back(
          {std::string(s.substr(i, n)), item_no_index, color, false});
      i += n;
    }
  };
  for (uint32_t i = 0; i < spec.token_count; ++i) {
    auto t = token(spec.first_token + i);
    const auto color = t.color ? loc.hint_color : loc.base_color;
    switch (t.kind) {
    case ItemDetailsTokenKind::Text:
      append(string(t.value), color);
      break;
    case ItemDetailsTokenKind::Nickname:
      append(cut, color);
      break;
    case ItemDetailsTokenKind::Doses: {
      const bool left = doses < d.max_doses;
      append(number_phrase(string(left ? (doses == 1 ? loc.left_singular
                                                     : loc.left_plural)
                                       : loc.total),
                           left ? doses : d.max_doses),
             color);
      break;
    }
    case ItemDetailsTokenKind::ItemValue:
      append(std::to_string(d.item_value), color);
      break;
    case ItemDetailsTokenKind::InlineImage:
      units.push_back({{}, t.value, color, false});
      break;
    case ItemDetailsTokenKind::Newline:
      units.push_back({{}, item_no_index, color, true});
      break;
    }
  }
  if (units.size() > 16384)
    return reject(e, "Item details expanded text capacity rejected");
  ItemDetailsComposition candidate;
  std::vector<Unit> line;
  float y = 0;
  bool metrics_ok = true;
  auto measured = [&](const std::vector<Unit> &v) {
    std::string plain;
    for (const auto &u : v)
      plain += u.image == item_no_index ? u.text : std::string(image_measure);
    const float n = measure(plain);
    if (!std::isfinite(n) || n < 0 || n > 1048576)
      metrics_ok = false;
    return n;
  };
  auto flush = [&]() {
    if (line.empty())
      return;
    float x = 0;
    for (size_t i = 0; i < line.size();) {
      const auto &u = line[i];
      ItemDetailsAtom atom;
      atom.color = u.color;
      atom.x = x;
      atom.y = y;
      if (u.image != item_no_index) {
        const auto r = resource(u.image);
        atom.kind = ItemDetailsTokenKind::InlineImage;
        atom.resource = u.image;
        atom.width = float(r.width);
        atom.height = float(r.height);
        atom.y = y + parameter(ItemDetailsParameter::ImageBaseline);
        ++i;
      } else {
        atom.kind = ItemDetailsTokenKind::Text;
        atom.height = parameter(ItemDetailsParameter::LineHeight);
        const auto color = u.color;
        while (i < line.size() && line[i].image == item_no_index &&
               line[i].color == color)
          atom.text += line[i++].text;
        atom.width = measure(atom.text);
        if (!std::isfinite(atom.width) || atom.width < 0 ||
            atom.width > 1048576)
          metrics_ok = false;
      }
      x += atom.width;
      candidate.atoms.push_back(std::move(atom));
    }
    line.clear();
    y += parameter(ItemDetailsParameter::LineHeight);
  };
  // Equivalent source word assembly: the first word is retained even if wider
  // than the panel; subsequent words measure the complete stripped candidate.
  // Empty WORD_SEPARATOR uses UTF-8 scalar units, never byte fragments.
  size_t at = 0;
  while (at < units.size()) {
    struct Word {
      std::vector<Unit> value;
      uint32_t separator_color = 0;
    };
    std::vector<Word> words(1);
    words.front().separator_color = loc.base_color;
    while (at < units.size() && !units[at].newline) {
      const auto &u = units[at++];
      if (sep.empty()) {
        if (!words.back().value.empty())
          words.push_back({{}, u.color});
        words.back().value.push_back(u);
      } else if (u.image == item_no_index && u.text == sep)
        words.push_back({{}, u.color});
      else
        words.back().value.push_back(u);
    }
    line = std::move(words.front().value);
    for (size_t i = 1; i < words.size(); ++i) {
      std::vector<Unit> proposed = line;
      if (!sep.empty())
        proposed.push_back(
            {std::string(sep), item_no_index, words[i].separator_color, false});
      proposed.insert(proposed.end(), words[i].value.begin(),
                      words[i].value.end());
      if (measured(proposed) > width) {
        flush();
        line = std::move(words[i].value);
      } else
        line = std::move(proposed);
    }
    flush();
    if (at < units.size() && units[at].newline)
      ++at;
  }
  flush();
  if (!metrics_ok)
    return reject(e, "Item details font metrics rejected");
  candidate.height = y;
  out = std::move(candidate);
  e.clear();
  return true;
}
} // namespace encore::upstream

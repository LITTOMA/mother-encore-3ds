#include "encore/field_prompts.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
namespace encore::upstream {
namespace {
uint32_t word(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
  }
  return ~c;
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 80;
  bool ok = true;
  uint32_t integer() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = word(p + at);
    at += 4;
    return v;
  }
  float scalar() {
    auto v = integer();
    float f;
    std::memcpy(&f, &v, 4);
    if (!std::isfinite(f) || std::abs(f) > 100000)
      ok = false;
    return f;
  }
  std::array<float, 4> value() {
    return {scalar(), scalar(), scalar(), scalar()};
  }
  std::string text() {
    auto len = integer();
    if (!ok || len > 4096 || at > n || len > n - at) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), len);
    at += len;
    size_t i = 0;
    uint32_t cp;
    while (i < s.size())
      if (!encore::utf8_next(s, i, cp) || cp < 32) {
        ok = false;
        break;
      }
    return s;
  }
  bool hash(std::array<uint8_t, 32> &out) {
    if (!ok || at > n || n - at < 32) {
      ok = false;
      return false;
    }
    std::copy_n(p + at, 32, out.begin());
    at += 32;
    ok = !std::all_of(out.begin(), out.end(), [](uint8_t v) { return !v; });
    return ok;
  }
};
bool path(const std::string &s) {
  return !s.empty() && s.front() != '/' && s.back() != '/' &&
         s.find("..") == s.npos && s.find(':') == s.npos &&
         s.find('\\') == s.npos;
}
bool semantic_value(uint32_t role, const std::array<float, 4> &v) {
  if (role == 7 || role == 11)
    return (v[0] == 0 || v[0] == 1) && v[1] == 0 && v[2] == 0 && v[3] == 0;
  if (role == 8 || role == 10)
    return v[0] >= 0 && v[0] <= 1 && v[1] == 0 && v[2] == 0 && v[3] == 0;
  if (role == 4 || role == 5 || role == 6 || role == 9)
    return std::all_of(v.begin(), v.end(),
                       [](float f) { return f >= 0 && f <= 1; });
  return v[2] == 0 && v[3] == 0;
}
} // namespace
const FieldPromptDescriptor *FieldPromptData::record(uint32_t id) const {
  for (const auto &r : records_)
    if (r.id == id)
      return &r;
  return nullptr;
}
const FieldPromptClip *FieldPromptData::clip(FieldPromptClipRole role) const {
  for (const auto &r : clips_)
    if (r.role == role)
      return &r;
  return nullptr;
}
bool FieldPromptData::load_file(const char *name, std::string &e) {
  if (!name) {
    e = "Field prompt pack path absent";
    return false;
  }
  std::ifstream f(name, std::ios::binary);
  if (!f) {
    e = "Field prompt pack unavailable";
    return false;
  }
  f.seekg(0, std::ios::end);
  auto n = f.tellg();
  if (n < 80 || n > 1024 * 1024) {
    e = "Field prompt pack size";
    return false;
  }
  f.seekg(0);
  std::vector<uint8_t> b(static_cast<size_t>(n));
  if (!f.read(reinterpret_cast<char *>(b.data()), n)) {
    e = "Field prompt pack read";
    return false;
  }
  return load(b.data(), b.size(), e);
}
bool FieldPromptData::load(const uint8_t *p, size_t n, std::string &e) {
  auto reject = [&](const char *t) {
    e = t;
    return false;
  };
  if (!p || n < 80 || n > 1024 * 1024 || std::memcmp(p, "ENCFPR01", 8) ||
      word(p + 8) != 1 || word(p + 12) != n || word(p + 20) != 1 ||
      word(p + 76) || word(p + 16) != crc(p + 80, n - 80))
    return reject("Field prompt header/version/capability/CRC rejected");
  FieldPromptData next;
  std::copy_n(p + 24, 20, next.pin_.begin());
  std::copy_n(p + 44, 32, next.script_.begin());
  if (std::all_of(next.pin_.begin(), next.pin_.end(),
                  [](uint8_t v) { return !v; }) ||
      std::all_of(next.script_.begin(), next.script_.end(),
                  [](uint8_t v) { return !v; }))
    return reject("Field prompt source identity absent");
  Reader r{p, n};
  if (!r.hash(next.scene_))
    return reject("Field prompt scene hash absent");
  next.scene_id_ = r.integer();
  if (!next.scene_id_)
    return reject("Field prompt scene ID absent");
  for (uint32_t i = 0; i < 10; ++i) {
    next.initial_[i] = r.value();
    if (!r.ok || !semantic_value(i + 1, next.initial_[i]))
      return reject("Field prompt initial Canvas values rejected");
  }
  auto &a = next.art_;
  a.path = r.text();
  r.hash(a.sha256);
  a.bytes = r.integer();
  a.crc32 = r.integer();
  a.width = r.integer();
  a.height = r.integer();
  for (auto &v : a.label_crop)
    v = r.integer();
  for (auto &v : a.arrow_crop)
    v = r.integer();
  a.label_origin = {r.scalar(), r.scalar()};
  a.arrow_origin = {r.scalar(), r.scalar()};
  a.label = r.text();
  r.hash(a.layout_sha256);
  for (auto &v : a.engine_hash) {
    if (r.at >= n) {
      r.ok = false;
      break;
    }
    v = p[r.at++];
  }
  auto crop = [&](const auto &c) {
    return c[2] && c[3] && c[0] < a.width && c[1] < a.height &&
           c[2] <= a.width - c[0] && c[3] <= a.height - c[1];
  };
  if (!r.ok || !path(a.path) || a.path.rfind("graphics/", 0) != 0 ||
      a.path.size() < 4 || a.path.substr(a.path.size() - 4) != ".t3x" ||
      !a.bytes || a.bytes > 1024 * 1024 || !a.width || !a.height ||
      a.width > 1024 || a.height > 1024 || !crop(a.label_crop) ||
      !crop(a.arrow_crop) || a.label.empty() ||
      std::all_of(a.engine_hash.begin(), a.engine_hash.end(),
                  [](uint8_t v) { return !v; }))
    return reject("Field prompt converted art/layout rejected");
  std::set<std::string> names;
  for (auto &name : next.choices_) {
    name = r.text();
    if (!r.ok ||
        (name != "Objects" && name != "NPCs" && name != "Both" &&
         name != "None") ||
        !names.insert(name).second)
      return reject("Field prompt settings choice rejected");
  }
  auto count = r.integer();
  if (!count || count > 4096)
    return reject("Field prompt count rejected");
  std::set<uint32_t> ids;
  names.clear();
  uint32_t last = 0;
  bool had = false;
  for (uint32_t i = 0; i < count; ++i) {
    FieldPromptDescriptor d;
    d.id = r.integer();
    d.parent_id = r.integer();
    d.ready_ordinal = r.integer();
    d.category = r.integer();
    auto enabled = r.integer();
    d.enabled = enabled != 0;
    d.node = r.text();
    d.key = r.text();
    d.offset = {r.scalar(), r.scalar()};
    if (!r.ok || !d.id || !d.parent_id || !ids.insert(d.id).second ||
        !names.insert(d.node).second || !path(d.node) || d.category >= 4 ||
        (next.choices_[d.category] != "Objects" &&
         next.choices_[d.category] != "NPCs") ||
        enabled > 1 ||
        (d.key != "ui_accept" && d.key != "ui_select" &&
         d.key != "ui_toggle") ||
        (had && last >= d.ready_ordinal))
      return reject("Field prompt source record rejected");
    last = d.ready_ordinal;
    had = true;
    next.records_.push_back(std::move(d));
  }
  count = r.integer();
  if (count != 5)
    return reject("Field prompt clip coverage rejected");
  ids.clear();
  names.clear();
  for (uint32_t i = 0; i < count; ++i) {
    FieldPromptClip c;
    auto role = r.integer();
    c.role = FieldPromptClipRole(role);
    c.name = r.text();
    c.length = r.scalar();
    auto loop = r.integer();
    c.loop = loop != 0;
    auto tracks = r.integer();
    if (!r.ok || role < 1 || role > 5 || !ids.insert(role).second ||
        c.name.empty() || !names.insert(c.name).second || c.length <= 0 ||
        c.length > 10 || loop > 1 || !tracks || tracks > 16 ||
        (c.loop != (c.role == FieldPromptClipRole::Float)))
      return reject("Field prompt clip rejected");
    std::set<uint32_t> props;
    for (uint32_t j = 0; j < tracks; ++j) {
      FieldPromptTrack t;
      auto prop = r.integer();
      t.property = FieldPromptProperty(prop);
      t.update = r.integer();
      auto keys = r.integer();
      if (!r.ok || prop < 1 || prop > 11 || !props.insert(prop).second ||
          t.update > 1 || !keys || keys > 64 ||
          ((prop == 7 || prop == 11) && t.update != 1))
        return reject("Field prompt animation track rejected");
      float prior = -1;
      for (uint32_t k = 0; k < keys; ++k) {
        FieldPromptKey q;
        q.time = r.scalar();
        q.ease = r.scalar();
        q.value = r.value();
        if (!r.ok || q.time < 0 || q.time < prior || q.time > c.length ||
            !semantic_value(prop, q.value) || (prop == 11 && q.value[0] != 1))
          return reject("Field prompt animation key rejected");
        prior = q.time;
        t.keys.push_back(q);
      }
      c.tracks.push_back(std::move(t));
    }
    next.clips_.push_back(std::move(c));
  }
  if (!r.ok || r.at != n)
    return reject("Field prompt trailing data rejected");
  next.valid_ = true;
  *this = std::move(next);
  e.clear();
  return true;
}
} // namespace encore::upstream

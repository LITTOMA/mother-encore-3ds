#include "encore/field_dialogue_ui.hpp"
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
  while (n--) {
    c ^= *p++;
    for (unsigned i = 0; i < 8; ++i)
      c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0u);
  }
  return ~c;
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 128;
  bool ok = true;
  uint32_t u() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = word(p + at);
    at += 4;
    return v;
  }
  uint32_t count(uint32_t limit) {
    auto x = u();
    if (x > limit)
      ok = false;
    return ok ? x : 0;
  }
  int32_t i() {
    auto x = u();
    int32_t v;
    std::memcpy(&v, &x, 4);
    return v;
  }
  float f() {
    auto x = u();
    float v;
    std::memcpy(&v, &x, 4);
    if (!std::isfinite(v) || std::abs(v) > 1e6)
      ok = false;
    return v;
  }
  Vec2 v() {
    float x = f(), y = f();
    return {x, y};
  }
  BattleValue rect() {
    float x = f(), y = f(), z = f(), w = f();
    return {x, y, z, w};
  }
  std::string t() {
    auto length = count(65536);
    if (!ok || at > n || length > n - at) {
      ok = false;
      return {};
    }
    std::string v(reinterpret_cast<const char *>(p + at), length);
    at += length;
    size_t chars = 0;
    if (v.find('\0') != v.npos || !utf8_count(v, chars))
      ok = false;
    return v;
  }
  std::array<uint8_t, 32> hash() {
    std::array<uint8_t, 32> h{};
    if (!ok || at > n || n - at < 32) {
      ok = false;
      return h;
    }
    std::copy_n(p + at, 32, h.begin());
    at += 32;
    if (std::all_of(h.begin(), h.end(), [](auto x) { return !x; }))
      ok = false;
    return h;
  }
};
bool safe(std::string_view p) {
  return !p.empty() && p.front() != '/' && p.find("..") == p.npos &&
         p.find_first_of(":\\") == p.npos;
}
} // namespace
const FieldDialogueUiNode *FieldDialogueUiData::node(uint32_t id) const {
  for (const auto &n : nodes_)
    if (n.id == id)
      return &n;
  return nullptr;
}
const FieldDialogueUiNode *
FieldDialogueUiData::role(FieldDialogueUiRole role) const {
  for (const auto &n : nodes_)
    if (n.role == role)
      return &n;
  return nullptr;
}
const FieldDialogueUiControl *FieldDialogueUiData::control(uint32_t id) const {
  for (const auto &n : controls_)
    if (n.id == id)
      return &n;
  return nullptr;
}
bool FieldDialogueUiData::load_file(const char *p, const FieldIdentity &id,
                                    std::string &e) {
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  if (!f) {
    e = "Cannot open checked dialogue UI pack";
    return false;
  }
  auto n = f.tellg();
  if (n < 0 || uint64_t(n) > 4u * 1024u * 1024u) {
    e = "Dialogue UI file bound";
    return false;
  }
  std::vector<uint8_t> b(static_cast<size_t>(n));
  f.seekg(0);
  if (!b.empty() &&
      !f.read(reinterpret_cast<char *>(b.data()), std::streamsize(b.size()))) {
    e = "Dialogue UI file truncated";
    return false;
  }
  return load(b.data(), b.size(), id, e);
}
bool FieldDialogueUiData::load(const uint8_t *p, size_t n,
                               const FieldIdentity &expected, std::string &e) {
  auto reject = [&](const char *s) {
    e = s;
    return false;
  };
  if (!p || n < 128 || n > 4u * 1024u * 1024u)
    return reject("Dialogue UI pack bound");
  if (std::memcmp(p, "ENCDUI01", 8) || word(p + 8) != 1 ||
      word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 20) != crc(p + 128, n - 128) || word(p + 24) != 0x454e0041 ||
      word(p + 28) != 1 || word(p + 32) != 20)
    return reject("Dialogue UI version/family/capability/CRC rejected");
  for (unsigned i = 124; i < 128; ++i)
    if (p[i])
      return reject("Dialogue UI reserved header");
  if (std::all_of(p + 92, p + 124, [](uint8_t x) { return !x; }))
    return reject("Dialogue UI IR proof missing");
  FieldDialogueUiData d;
  d.identity_.scene_id = word(p + 36);
  std::copy_n(p + 40, 20, d.identity_.upstream_commit.begin());
  std::copy_n(p + 60, 32, d.identity_.source_sha256.begin());
  if (!expected.scene_id || d.identity_.scene_id != expected.scene_id ||
      d.identity_.upstream_commit != expected.upstream_commit ||
      d.identity_.source_sha256 != expected.source_sha256)
    return reject("Dialogue UI original source identity differs");
  Reader r{p, n};
  d.recipe_ = r.hash();
  d.house_ = r.hash();
  d.display_ = r.rect();
  d.name_tween_ = r.rect();
  d.started_ = r.t();
  d.finished_ = r.t();
  d.rect_ = r.t();
  d.range_ = r.t();
  d.value_ = r.t();
  d.delay_ = r.t();
  d.wait_ = r.t();
  size_t delay_chars = 0, wait_chars = 0;
  if (!utf8_count(d.delay_, delay_chars) || !utf8_count(d.wait_, wait_chars) ||
      delay_chars != 1 || wait_chars != 1 || d.range_.empty() ||
      d.value_.empty())
    return reject("Dialogue UI native Range/text marker codec");
  if (d.display_.x <= 0 || d.display_.y <= 0 || d.display_.z < 0 ||
      d.display_.z > 1 || d.display_.w < 0 || d.display_.w > 1 ||
      d.name_tween_.x < 0 || d.name_tween_.y <= 0 || d.name_tween_.z < 1 ||
      d.name_tween_.w <= 0 || d.started_.empty() || d.finished_.empty() ||
      d.rect_.empty())
    return reject("Dialogue UI viewport/tween/signal policy");
  auto count = r.count(47);
  if (count != 47)
    return reject("Dialogue UI full recipe node facts absent");
  std::set<uint32_t> ids, roles;
  for (uint32_t i = 0; i < count; ++i) {
    FieldDialogueUiNode x;
    x.id = r.u();
    x.parent = r.u();
    x.ready = r.u();
    auto role = r.u(), kind = r.u();
    x.path = r.t();
    x.native_class = r.t();
    x.script = r.t();
    if (!x.id || !ids.insert(x.id).second || x.ready >= 47 || role > 20 ||
        kind > 9 || (!role) != (kind == 0) ||
        (!x.parent && x.id != expected.scene_id) || x.path.empty() ||
        x.native_class.empty() || (role && !roles.insert(role).second))
      return reject("Dialogue UI native role/source binding");
    x.role = FieldDialogueUiRole(role);
    x.kind = FieldDialogueUiKind(kind);
    const char *classes[] = {"",
                             "CanvasLayer",
                             "Control",
                             "NinePatchRect",
                             "GridContainer",
                             "HBoxContainer",
                             "Label",
                             "RichTextLabel",
                             "VScrollBar",
                             "AnimationPlayer"};
    if (kind && x.native_class != classes[kind])
      return reject("Dialogue UI native class/schema mismatch");
    d.nodes_.push_back(std::move(x));
  }
  if (roles.size() != 20)
    return reject("Dialogue UI native scope incomplete");
  count = r.count(17);
  if (count != 17)
    return reject("Dialogue UI control scope incomplete");
  std::set<uint32_t> cids;
  for (uint32_t i = 0; i < count; ++i) {
    FieldDialogueUiControl c;
    c.id = r.u();
    c.flags = r.u();
    c.align = r.u();
    c.valign = r.u();
    c.columns = r.u();
    c.visible_characters = r.i();
    c.hseparation = r.f();
    c.vseparation = r.f();
    c.line_separation = r.f();
    c.font_height = r.f();
    c.font_ascent = r.f();
    c.font_descent = r.f();
    c.minimum = r.v();
    c.patch = r.rect();
    for (float &v : c.range)
      v = r.f();
    c.style = r.rect();
    c.font = r.t();
    c.texture = r.t();
    c.material = r.t();
    c.text = r.t();
    c.bbcode = r.t();
    auto *node = d.node(c.id);
    if (!node || node->kind <= FieldDialogueUiKind::CanvasLayer ||
        node->kind >= FieldDialogueUiKind::AnimationPlayer ||
        !cids.insert(c.id).second || c.flags > 63 || c.align > 3 ||
        c.valign > 3 || c.columns > 6 || c.minimum.x < 0 || c.minimum.y < 0 ||
        c.visible_characters < -1 || (!c.font.empty() && !safe(c.font)) ||
        (!c.texture.empty() && !safe(c.texture)) ||
        (!c.material.empty() && !safe(c.material)) ||
        ((node->kind == FieldDialogueUiKind::Label ||
          node->kind == FieldDialogueUiKind::RichTextLabel) &&
         (c.font.empty() || c.font_height <= 0)))
      return reject("Dialogue UI control property codec");
    if (node->kind == FieldDialogueUiKind::VScrollBar &&
        (c.range[2] < 0 || c.range[1] < c.range[0] || c.range[3] < 0 ||
         c.range[4] < c.range[0] || c.range[4] > c.range[1]))
      return reject("Dialogue UI native Range bounds");
    d.controls_.push_back(std::move(c));
  }
  count = r.count(6);
  if (count != 6)
    return reject("Dialogue UI source animation closure incomplete");
  std::set<std::pair<uint32_t, std::string>> clips;
  for (uint32_t i = 0; i < count; ++i) {
    FieldDialogueUiAnimation a;
    a.owner = r.u();
    a.target = r.u();
    a.clip.resource = r.u();
    a.clip.interpolation = r.u();
    a.clip.length = r.f();
    a.clip.step = r.f();
    a.clip.name = r.t();
    auto keys = r.count(64);
    auto *owner = d.node(a.owner);
    if (!owner || owner->kind != FieldDialogueUiKind::AnimationPlayer ||
        !d.control(a.target) || !a.clip.resource ||
        (a.clip.interpolation != 1 && a.clip.interpolation != 2) ||
        a.clip.length <= 0 || a.clip.step <= 0 || a.clip.name.empty() ||
        !clips.emplace(a.owner, a.clip.name).second || !keys)
      return reject("Dialogue UI source animation codec");
    for (uint32_t j = 0; j < keys; ++j) {
      FieldDialogueKey k;
      k.time = r.f();
      k.transition = r.f();
      k.value = r.v();
      if (k.time < 0 || k.time > a.clip.length || k.transition <= 0 ||
          (!a.clip.keys.empty() && k.time <= a.clip.keys.back().time))
        return reject("Dialogue UI animation key ordering");
      a.clip.keys.push_back(k);
    }
    d.animations_.push_back(std::move(a));
  }
  count = r.count(2);
  if (count != 2)
    return reject("Dialogue UI borrowed original art missing");
  ids.clear();
  for (uint32_t i = 0; i < count; ++i) {
    FieldDialogueUiResource a;
    a.id = r.u();
    a.width = r.u();
    a.height = r.u();
    a.path = r.t();
    a.source = r.t();
    a.sha = r.hash();
    if (!a.id || !ids.insert(a.id).second || !a.width || !a.height ||
        a.width > 4096 || a.height > 4096 || !safe(a.path) || !safe(a.source))
      return reject("Dialogue UI texture binding");
    d.resources_.push_back(std::move(a));
  }
  count = r.count(2048);
  for (uint32_t i = 0; i < count; ++i) {
    auto pth = r.t();
    auto h = r.hash();
    if (!safe(pth) || !d.sources_.emplace(pth, h).second)
      return reject("Dialogue UI duplicate/unsafe source proof");
  }
  if (!r.ok || r.at != n)
    return reject("Dialogue UI malformed payload");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool FieldDialogueUiData::verify_house(HouseView house, std::string &e) const {
  if (!valid_ || !house.valid()) {
    e = "Dialogue UI actual checked House owner absent";
    return false;
  }
  for (const auto &art : resources_) {
    bool found = false;
    for (uint32_t i = 0; i < house.count(HouseSection::Resources); ++i) {
      auto r = house.resource(i);
      if (r.id != art.id)
        continue;
      found = house.string(r.path) == art.path && r.width == art.width &&
              r.height == art.height &&
              std::equal(art.sha.begin(), art.sha.end(), r.sha256);
    }
    if (!found) {
      e = "Dialogue UI actual borrowed source texture differs";
      return false;
    }
  }
  const auto *p = control(role(FieldDialogueUiRole::Box)->id);
  const auto *q = control(role(FieldDialogueUiRole::NameBox)->id);
  const auto bm = house.parameter(HouseParameter::DialogueMargins),
             nm = house.parameter(HouseParameter::NameMargins),
             nt = house.parameter(HouseParameter::NameSizing),
             nr = house.parameter(HouseParameter::NameRect),
             dr = house.parameter(HouseParameter::DisplayReference);
  auto equal = [](BattleValue a, BattleValue b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
  };
  if (!p || !q || !equal(p->patch, bm) || !equal(q->patch, nm) ||
      !equal(dr, display_) || nt.x != name_tween_.x || nt.y != name_tween_.y ||
      nt.z != name_tween_.z || nr.w != name_tween_.w) {
    e = "Dialogue UI actual source House layout/tween tuning differs";
    return false;
  }
  e.clear();
  return true;
}
} // namespace encore::upstream

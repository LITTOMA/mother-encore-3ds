#include "encore/field_dialogue_visual.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
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
      c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0);
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
  int32_t i() {
    auto v = u();
    int32_t out;
    std::memcpy(&out, &v, 4);
    return out;
  }
  float f() {
    auto v = u();
    float out;
    std::memcpy(&out, &v, 4);
    if (!std::isfinite(out) || std::abs(out) >= 1000000)
      ok = false;
    return out;
  }
  Vec2 v() {
    float x = f(), y = f();
    return {x, y};
  }
  bool b() {
    auto v = u();
    if (v > 1)
      ok = false;
    return v != 0;
  }
  std::string t() {
    auto count = u();
    if (!ok || count > 65536 || at > n || count > n - at) {
      ok = false;
      return {};
    }
    std::string v(reinterpret_cast<const char *>(p + at), count);
    at += count;
    size_t c = 0;
    if (v.find('\0') != v.npos || !utf8_count(v, c))
      ok = false;
    return v;
  }
  std::array<uint8_t, 32> h() {
    std::array<uint8_t, 32> h{};
    if (!ok || at > n || n - at < 32) {
      ok = false;
      return h;
    }
    std::copy_n(p + at, 32, h.begin());
    at += 32;
    if (std::all_of(h.begin(), h.end(), [](uint8_t v) { return !v; }))
      ok = false;
    return h;
  }
};
bool path(std::string_view p) {
  if (p.empty() || p.front() == '/' || p.back() == '/' ||
      p.find('\\') != p.npos || p.find(':') != p.npos)
    return false;
  size_t start = 0;
  while (start < p.size()) {
    auto end = p.find('/', start);
    if (end == p.npos)
      end = p.size();
    auto q = p.substr(start, end - start);
    if (q.empty() || q == "." || q == "..")
      return false;
    start = end + 1;
  }
  return true;
}
} // namespace
const FieldDialogueVisualNode *
FieldDialogueVisualData::node(uint32_t id) const {
  for (const auto &n : nodes_)
    if (n.id == id)
      return &n;
  return nullptr;
}
const FieldDialogueCursor *FieldDialogueVisualData::cursor(uint32_t id) const {
  for (const auto &n : cursors_)
    if (n.id == id)
      return &n;
  return nullptr;
}
bool FieldDialogueVisualData::load_file(const char *p, const FieldIdentity &id,
                                        std::string &e) {
  if (!p) {
    e = "Dialogue visuals path missing";
    return false;
  }
  FILE *f = std::fopen(p, "rb");
  if (!f) {
    e = "Cannot open dialogue visuals";
    return false;
  }
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    e = "Dialogue visuals seek rejected";
    return false;
  }
  auto n = std::ftell(f);
  if (n < 128 || n > 4 * 1024 * 1024 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    e = "Dialogue visuals size rejected";
    return false;
  }
  std::vector<uint8_t> b(static_cast<size_t>(n));
  auto got = std::fread(b.data(), 1, b.size(), f);
  bool close = std::fclose(f) == 0;
  if (got != b.size() || !close) {
    e = "Dialogue visuals read rejected";
    return false;
  }
  return load(b.data(), b.size(), id, e);
}
bool FieldDialogueVisualData::load(const uint8_t *p, size_t n,
                                   const FieldIdentity &id, std::string &e) {
  auto reject = [&](const char *s) {
    e = s;
    return false;
  };
  if (!p || n < 128 || n > 4 * 1024 * 1024)
    return reject("Dialogue visual size rejected");
  if (std::memcmp(p, "ENCFDVS1", 8) || word(p + 8) != 1 ||
      word(p + 12) != 128 || word(p + 16) != n || word(p + 24) != 0x454e0046 ||
      word(p + 28) != 1 || word(p + 32) != 20 || !id.scene_id ||
      word(p + 36) != id.scene_id ||
      std::memcmp(p + 40, id.upstream_commit.data(), 20) ||
      std::memcmp(p + 60, id.source_sha256.data(), 32) || word(p + 124) ||
      crc(p + 128, n - 128) != word(p + 20) ||
      std::all_of(p + 92, p + 124, [](uint8_t v) { return !v; }))
    return reject("Dialogue visual identity/CRC/version/capability rejected");
  FieldDialogueVisualData d;
  Reader r{p, n};
  d.identity_ = id;
  d.recipe_ = r.h();
  d.tween_ = r.f();
  d.timer_ = r.f();
  d.transition_ = r.t();
  d.ease_ = r.t();
  if (d.transition_.empty() || d.ease_.empty())
    return reject("Cursor native tween enums missing");
  if (d.tween_ <= 0 || d.tween_ > 1 || d.timer_ <= 0 || d.timer_ > 1)
    return reject("Cursor duration rejected");
  for (auto *seq : {&d.actions_, &d.sounds_, &d.signals_}) {
    auto count = r.u();
    if (!r.ok || count == 0 || count > 32)
      return reject("Cursor source names rejected");
    std::set<std::string> names;
    for (uint32_t i = 0; i < count; ++i) {
      auto s = r.t();
      if (s.empty() || !names.insert(s).second)
        return reject("Cursor duplicate source names rejected");
      seq->push_back(s);
    }
  }
  if (d.actions_.size() != 3 || d.sounds_.size() != 3 ||
      d.signals_.size() != 11)
    return reject("Cursor source name roster rejected");
  const std::set<std::string> native_classes{
      "AnimatedSprite", "Sprite",           "AnimationPlayer", "Camera2D",
      "Area2D",         "CollisionShape2D", "Node2D"};
  std::set<uint32_t> ids;
  std::set<std::string> paths;
  for (uint32_t i = 0; i < 20; ++i) {
    FieldDialogueVisualNode v;
    v.id = r.u();
    v.parent = r.u();
    v.ready = r.u();
    v.pause = r.u();
    v.priority = r.i();
    v.path = r.t();
    v.native_class = r.t();
    v.script = r.t();
    if (!r.ok || !v.id || !v.parent || v.id == v.parent || v.ready >= 47 ||
        v.pause > 2 || !ids.insert(v.id).second ||
        !paths.insert(v.path).second || !native_classes.count(v.native_class) ||
        !path(v.path) || (!v.script.empty() && !path(v.script)))
      return reject("Dialogue visual node source rejected");
    d.nodes_.push_back(v);
  }
  if (r.u() != 2)
    return reject("Cursor roster rejected");
  for (uint32_t j = 0; j < 2; ++j) {
    FieldDialogueCursor c;
    c.id = r.u();
    c.sprite = r.u();
    c.player = r.u();
    c.timer = r.u();
    c.flags = r.u();
    c.frame = r.u();
    c.visible = r.b();
    c.playing = r.b();
    c.centered = r.b();
    c.menu = r.t();
    c.offset = r.v();
    c.size = r.v();
    c.position = r.v();
    c.rotation = r.f();
    c.scale = r.v();
    c.drawing_offset = r.v();
    c.fps = r.f();
    c.sprite_flags = r.u();
    c.sprite_position = r.v();
    c.sprite_offset = r.v();
    c.sprite_scale = r.v();
    c.sprite_rotation = r.f();
    auto *body = d.node(c.id);
    auto *sprite = d.node(c.sprite);
    auto *player = d.node(c.player);
    if (!r.ok || !body || body->native_class != "AnimatedSprite" || !sprite ||
        sprite->native_class != "Sprite" || sprite->parent != c.id || !player ||
        player->native_class != "AnimationPlayer" || player->parent != c.id ||
        !c.timer || ids.count(c.timer) || c.flags > 8191 ||
        (c.flags & ~uint32_t(1 | 16 | 32 | 128 | 2048)) || c.frame >= 4 ||
        c.fps <= 0 || c.fps > 1000 || c.size.x <= 0 || c.size.y <= 0 ||
        c.scale.x <= 0 || c.scale.y <= 0 || c.sprite_flags > 15 ||
        c.sprite_scale.x <= 0 || c.sprite_scale.y <= 0)
      return reject("Cursor source identity/policy rejected");
    if (r.u() != 3)
      return reject("Cursor clip roster rejected");
    for (uint32_t i = 0; i < 3; ++i) {
      FieldArrowClip clip;
      clip.role = r.u();
      clip.name = r.t();
      clip.length = r.f();
      clip.loop = r.b();
      auto tracks = r.u();
      if (!r.ok || clip.role != i + 1 || clip.name.empty() ||
          clip.length <= 0 || clip.length > 1 || clip.loop || !tracks ||
          tracks > 3)
        return reject("Cursor clip rejected");
      std::set<uint32_t> properties;
      for (uint32_t k = 0; k < tracks; ++k) {
        FieldArrowTrack tr;
        auto prop = r.u();
        tr.property = static_cast<FieldArrowProperty>(prop);
        tr.update = r.u();
        auto keys = r.u();
        if (!r.ok || prop < 1 || prop > 3 || !properties.insert(prop).second ||
            tr.update > 1 || !keys || keys > 64)
          return reject("Cursor track rejected");
        float previous = -1;
        for (uint32_t m = 0; m < keys; ++m) {
          FieldArrowKey key;
          key.time = r.f();
          key.transition = r.f();
          key.value = r.v();
          if (!r.ok || key.time < 0 || key.time > clip.length ||
              key.time <= previous ||
              (prop != 2 &&
               (key.value.y != 0 || std::floor(key.value.x) != key.value.x)) ||
              (prop == 1 && (key.value.x < 0 || key.value.x >= 4)) ||
              (prop == 3 && (key.value.x < 0 || key.value.x > 1)))
            return reject("Cursor key rejected");
          previous = key.time;
          tr.keys.push_back(key);
        }
        clip.tracks.push_back(tr);
      }
      c.clips.push_back(clip);
    }
    d.cursors_.push_back(c);
  }
  for (uint32_t kind = 0; kind < 2; ++kind) {
    auto count = r.u();
    if (!r.ok || count < 128 || count > 1024 * 1024 || r.at > n ||
        count > n - r.at)
      return reject("Dialogue nested native pack rejected");
    bool loaded = kind ? d.camera_.load(p + r.at, count, id, e)
                       : d.arrows_.load(p + r.at, count, id, e);
    if (!loaded)
      return false;
    r.at += count;
  }
  if (d.camera_.records().size() != 1 || d.arrows_.records().size() != 1 ||
      !d.node(d.camera_.records()[0].id) || !d.node(d.arrows_.records()[0].id))
    return reject("Dialogue camera/arrow nested source rejected");
  auto sources = r.u();
  if (!r.ok || sources < 10 || sources > 256)
    return reject("Dialogue source proof count rejected");
  std::map<std::string, std::array<uint8_t, 32>> proofs;
  for (uint32_t i = 0; i < sources; ++i) {
    auto path_ = r.t();
    auto hash = r.h();
    if (!r.ok || !path(path_) || !proofs.emplace(path_, hash).second)
      return reject("Dialogue visual source proof rejected");
  }
  auto proof = proofs.find(d.camera_.source_scene());
  if (!r.ok || r.at != n || proof == proofs.end() ||
      proof->second != id.source_sha256)
    return reject("Dialogue visual source/trailing bytes rejected");
  for (const auto &body : d.nodes_)
    if (!body.script.empty()) {
      std::array<uint8_t, 32> hash{};
      auto it = proofs.find(body.script);
      if (it == proofs.end() || !d.camera_.source_hash(body.script, hash) ||
          hash != it->second)
        return reject("Dialogue visual script proof mismatch");
    }
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream

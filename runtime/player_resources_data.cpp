#include "encore/player_resources.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
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
bool hash(const std::array<uint8_t, 32> &h) {
  return std::any_of(h.begin(), h.end(), [](uint8_t x) { return x; });
}
bool path(std::string_view p, std::string_view prefix,
          std::string_view suffix) {
  return p.size() > prefix.size() + suffix.size() &&
         p.substr(0, prefix.size()) == prefix &&
         p.substr(p.size() - suffix.size()) == suffix &&
         p.find("..") == p.npos && p.find('\\') == p.npos;
}
struct R {
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
  std::string s() {
    auto k = u();
    if (k > 4096 || k > n) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p), k);
    p += k;
    n -= k;
    size_t chars = 0;
    if (s.find('\0') != s.npos || !encore::utf8_count(s, chars))
      ok = false;
    return s;
  }
  float f() {
    auto raw = u();
    float x = 0;
    std::memcpy(&x, &raw, 4);
    if (!std::isfinite(x))
      ok = false;
    return x;
  }
  std::array<uint8_t, 32> h() {
    std::array<uint8_t, 32> x{};
    if (n < 32) {
      ok = false;
      return x;
    }
    std::copy(p, p + 32, x.begin());
    p += 32;
    n -= 32;
    if (!hash(x))
      ok = false;
    return x;
  }
};
} // namespace

const PlayerResourceImage *PlayerResourcesData::image(uint32_t id) const {
  for (const auto &x : images_)
    if (x.id == id)
      return &x;
  return nullptr;
}
const PlayerResource *PlayerResourcesData::resource(uint32_t id) const {
  for (const auto &x : resources_)
    if (x.id == id)
      return &x;
  return nullptr;
}
const PlayerResourceAudio *PlayerResourcesData::audio(uint32_t id) const {
  for (const auto &x : audios_)
    if (x.id == id)
      return &x;
  return nullptr;
}
const PlayerResourceNode *PlayerResourcesData::node(std::string_view p) const {
  for (const auto &x : nodes_)
    if (x.path == p)
      return &x;
  return nullptr;
}
bool PlayerResourcesData::source_hash(std::string_view p,
                                      std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(p));
  if (!valid_ || i == sources_.end())
    return false;
  out = i->second;
  return true;
}
bool PlayerResourcesData::load(const uint8_t *p, size_t n,
                               const PlayerInitializationData &init,
                               const PlayerGraphicsData &gfx, std::string &e) {
  if (!init.valid() || !gfx.valid() || !p || n < 128 || n > 4 * 1024 * 1024 ||
      std::memcmp(p, "ENCPRES1", 8) || u32(p + 8) != 1 || u32(p + 12) != 128 ||
      u32(p + 16) != n || u32(p + 20) != crc(p + 128, n - 128) ||
      u32(p + 24) != 0x454e005f || u32(p + 28) != 1 || u32(p + 32) != 1 ||
      u32(p + 124))
    return fail(e, "Player resources header/version/capability rejected");
  const auto &id = init.identity();
  if (u32(p + 36) != id.scene_id ||
      std::memcmp(p + 40, id.upstream_commit.data(), 20) ||
      std::memcmp(p + 60, id.source_sha256.data(), 32) ||
      gfx.identity().upstream_commit != id.upstream_commit)
    return fail(e, "Player resources source scene differs");
  PlayerResourcesData d;
  d.identity_ = id;
  std::copy(p + 92, p + 124, d.ir_.begin());
  if (!hash(d.ir_))
    return fail(e, "Player resources missing IR hash");
  R r{p + 128, n - 128};
  d.init_ir_ = r.h();
  d.gfx_ir_ = r.h();
  if (d.init_ir_ != init.ir_sha256() || d.gfx_ir_ != gfx.ir_sha256())
    return fail(e, "Player resources source dependency differs");
  auto count = r.u();
  if (!count || count > 4096)
    return fail(e, "Player resources source closure count rejected");
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    auto s = r.s();
    auto h = r.h();
    std::array<uint8_t, 32> known{};
    if (s.empty() || s.find("..") != s.npos || s.find('\\') != s.npos ||
        !d.sources_.emplace(s, h).second ||
        (init.source_hash(s, known) && known != h))
      return fail(e, "Player resources source closure conflict");
  }
  count = r.u();
  if (!count || count > 4096)
    return fail(e, "Player resources image count rejected");
  std::set<uint32_t> ids;
  std::set<std::string> paths;
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    PlayerResourceImage a;
    a.id = r.u();
    a.width = r.u();
    a.height = r.u();
    a.bytes = r.u();
    a.source = r.s();
    a.path = r.s();
    a.source_sha = r.h();
    a.import_sha = r.h();
    a.output_sha = r.h();
    if (!a.id || !a.width || !a.height || a.width > 1024 || a.height > 1024 ||
        !a.bytes || a.bytes > 16 * 1024 * 1024 ||
        !path(a.source, "Graphics/", ".png") ||
        !path(a.path, "graphics/", ".t3x") || !ids.insert(a.id).second ||
        !paths.insert(a.path).second || d.sources_[a.source] != a.source_sha ||
        d.sources_[a.source + ".import"] != a.import_sha)
      return fail(e, "Player resources image provenance rejected");
    d.images_.push_back(a);
  }
  count = r.u();
  if (count > 64)
    return fail(e, "Player audio resource count rejected");
  ids.clear();
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    PlayerResourceAudio a;
    a.id = r.u();
    a.kind = r.u();
    a.rate = r.u();
    a.channels = r.u();
    a.frames = r.u();
    a.bytes = r.u();
    auto loop = r.u();
    a.loop = loop != 0;
    a.source = r.s();
    a.path = r.s();
    a.source_sha = r.h();
    a.import_sha = r.h();
    a.output_sha = r.h();
    if (a.kind < 1 || a.kind > 2 || !a.rate || a.rate > 192000 || !a.channels ||
        a.channels > 2 || !a.frames ||
        uint64_t(a.frames) * a.channels * 2 != a.bytes ||
        a.bytes > 16 * 1024 * 1024 || loop > 1 ||
        !path(a.path, "sound/effects/", ".pcm") ||
        a.source.find("..") != a.source.npos || !ids.insert(a.id).second ||
        !paths.insert(a.path).second || d.sources_[a.source] != a.source_sha ||
        d.sources_[a.source + ".import"] != a.import_sha)
      return fail(e, "Player actual PCM resource format/provenance rejected");
    d.audios_.push_back(a);
  }
  count = r.u();
  if (!count || count > 4096)
    return fail(e, "Player native resource count rejected");
  ids.clear();
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    PlayerResource a;
    a.id = r.u();
    a.kind = r.u();
    a.texture = r.u();
    auto local = r.u();
    a.local = (local & 1) != 0;
    a.instanced = (local & 2) != 0;
    a.shader = r.u();
    a.source = r.s();
    for (auto &v : a.rect)
      v = r.f();
    auto pc = r.u();
    if (a.kind < 1 || a.kind > 3 || local > 3 || !ids.insert(a.id).second ||
        a.source.empty() || pc > 4)
      return fail(e, "Player native resource schema rejected");
    if (a.kind < 3) {
      const auto *im = d.image(a.texture);
      if (!im || a.local || a.instanced || a.shader || pc || a.rect[0] < 0 ||
          a.rect[1] < 0 || a.rect[2] <= 0 || a.rect[3] <= 0 ||
          a.rect[0] + a.rect[2] > im->width ||
          a.rect[1] + a.rect[3] > im->height)
        return fail(e, "Player Atlas/native texture region rejected");
    } else if (a.texture || a.shader < 1 || a.shader > 2 || pc != 4)
      return fail(e, "Player material kernel/schema rejected");
    for (uint32_t j = 0; j < pc && r.ok; ++j) {
      PlayerResourceParameter v;
      v.role = r.u();
      v.kind = r.u();
      v.name = r.s();
      for (auto &f : v.value)
        f = r.f();
      const uint32_t expected = a.shader == 1 ? (j < 2 ? 2 : 1)
                                              : (j == 0   ? 2
                                                 : j == 1 ? 1
                                                 : j == 2 ? 4
                                                          : 3);
      if (v.role != j + 1 || v.kind != expected || v.name.empty() ||
          std::any_of(a.parameters.begin(), a.parameters.end(),
                      [&](const auto &p) { return p.name == v.name; }))
        return fail(e, "Player material parameter source mapping rejected");
      a.parameters.push_back(v);
    }
    d.resources_.push_back(a);
  }
  count = r.u();
  if (count > 4096)
    return fail(e, "Player source sprite binding count rejected");
  paths.clear();
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    PlayerResourceNode a;
    a.path = r.s();
    a.texture = r.u();
    a.material = r.u();
    a.columns = r.u();
    a.rows = r.u();
    a.frame = r.u();
    auto tex = d.resource(a.texture);
    auto mat = d.resource(a.material);
    if (a.path.empty() || !paths.insert(a.path).second || !a.columns ||
        !a.rows || a.columns > 4096 || a.rows > 4096 ||
        uint64_t(a.frame) >= uint64_t(a.columns) * a.rows ||
        (a.texture != 0xffffffff &&
         (!tex || tex->kind == 3 || std::fmod(tex->rect[2], float(a.columns)) ||
          std::fmod(tex->rect[3], float(a.rows)))) ||
        (a.material != 0xffffffff && (!mat || mat->kind != 3)))
      return fail(e, "Player Sprite actual resource/grid binding rejected");
    d.nodes_.push_back(a);
  }
  if (!r.ok || r.n)
    return fail(e, "Player resources truncated/trailing/nonfinite bytes");
  // Coverage belongs to the actual audited native PackedScene graph. Removing
  // a supported source resource from the packet cannot turn it into "unused".
  auto actual_resources =
      init.native_source() ? init.native_source()->get("resources") : nullptr;
  if (!actual_resources || actual_resources->kind != 5)
    return fail(e, "Player resources actual native graph unavailable");
  size_t expected_resources = 0, expected_audio = 0;
  for (const auto &actual : actual_resources->array) {
    auto cls = actual ? actual->get("class") : nullptr;
    auto id = actual ? actual->get("id") : nullptr;
    auto path = actual ? actual->get("path") : nullptr;
    if (!cls || cls->kind != 4 || !id || id->kind != 2 || id->integer < 0 ||
        id->integer > 0xffffffffll || !path || path->kind != 4)
      return fail(e, "Player resources malformed actual source record");
    uint32_t kind = cls->string == "StreamTexture"    ? 1
                    : cls->string == "AtlasTexture"   ? 2
                    : cls->string == "ShaderMaterial" ? 3
                                                      : 0;
    if (kind) {
      ++expected_resources;
      auto record = d.resource(uint32_t(id->integer));
      if (!record || record->kind != kind ||
          record->instanced != (kind == 3 && path->string.empty()))
        return fail(e, "Player resources native source coverage/class/local "
                       "clone differs");
      if (kind == 1) {
        const auto *image = d.image(record->texture);
        if (!image || path->string != "res://" + image->source ||
            record->source != image->source)
          return fail(e, "Player StreamTexture native source path differs");
      } else {
        std::string expected = path->string.empty()
                                   ? init.recipe().source_scene()
                                   : path->string.substr(6);
        const auto sep = expected.find("::");
        if (sep != expected.npos)
          expected.resize(sep);
        if (record->source != expected)
          return fail(e, "Player Atlas/Material source scene path differs");
      }
      if (kind == 3) {
        auto properties = actual->get("properties");
        auto local =
            properties ? properties->get("resource_local_to_scene") : nullptr;
        if (!local || local->kind != 1 || record->local != local->boolean)
          return fail(
              e, "Player ShaderMaterial local-to-scene source policy differs");
      }
    }
    uint32_t audio_kind = cls->string == "AudioStreamSample" ? 1
                          : cls->string == "AudioStreamMP3"  ? 2
                                                             : 0;
    if (audio_kind) {
      ++expected_audio;
      auto record = d.audio(uint32_t(id->integer));
      if (!record || record->kind != audio_kind ||
          path->string != "res://" + record->source)
        return fail(e, "Player Audio Resource native source coverage differs");
    }
  }
  for (const auto &field : init.fields())
    if (field.adapter == 2) {
      ++expected_audio;
      auto found =
          std::find_if(d.audios_.begin(), d.audios_.end(), [&](const auto &a) {
            return a.source == field.resource &&
                   field.native ==
                       (a.kind == 1 ? "AudioStreamSample" : "AudioStreamMP3");
          });
      if (found == d.audios_.end())
        return fail(e, "Player constructor Audio Resource missing");
    }
  if (expected_resources != d.resources_.size() ||
      expected_audio != d.audios_.size())
    return fail(
        e, "Player native Resource closure contains unknown/missing record");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool PlayerResourcesData::load_file(const char *path,
                                    const PlayerInitializationData &init,
                                    const PlayerGraphicsData &gfx,
                                    std::string &e) {
  FILE *f = path ? std::fopen(path, "rb") : nullptr;
  if (!f)
    return fail(e, "Player resources cannot open");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "Player resources seek failed");
  }
  auto n = std::ftell(f);
  if (n < 0 || n > 4 * 1024 * 1024) {
    std::fclose(f);
    return fail(e, "Player resources file size rejected");
  }
  std::rewind(f);
  std::vector<uint8_t> b(size_t(n), 0);
  bool ok = std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  return ok ? load(b.data(), b.size(), init, gfx, e)
            : fail(e, "Player resources short read");
}
} // namespace encore::upstream
